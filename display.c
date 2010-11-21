/*
 * Copyright 2007, Intel Corporation
 *
 * This file is part of PowerTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * Authors:
 * 	Arjan van de Ven <arjan@linux.intel.com>
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <dirent.h>
#include <ncurses.h>
#include <time.h>
#include <wchar.h>
#include <sys/time.h>

#include "batcop.h"

static WINDOW *title_bar_window;
static WINDOW *cstate_window;
static WINDOW *wakeup_window;
static WINDOW *battery_power_window;
static WINDOW *timerstat_window;
static WINDOW *suggestion_window;
static WINDOW *status_bar_window;

#define print(win, y, x, fmt, args...) do { mvwprintw(win, y, x, fmt, ## args); } while (0)

char status_bar_slots[10][40];

static int mapcount = 0;
static char* string_list[1000];
static double value_list[1000] = {0};

static void cleanup_curses(void) {
	endwin();
}

static void zap_windows(void)
{
	if (title_bar_window) {
		delwin(title_bar_window);
		title_bar_window = NULL;
	}
	if (cstate_window) {
		delwin(cstate_window);
		cstate_window = NULL;
	}
	if (wakeup_window) {
		delwin(wakeup_window);
		wakeup_window = NULL;
	}
	if (battery_power_window) {
		delwin(battery_power_window);
		battery_power_window = NULL;
	}
	if (timerstat_window) {
		delwin(timerstat_window);
		timerstat_window = NULL;
	}
	if (suggestion_window) {
		delwin(suggestion_window);
		suggestion_window = NULL;
	}
	if (status_bar_window) {
		delwin(status_bar_window);
		status_bar_window = NULL;
	}
}


int maxx, maxy;

int maxtimerstats = 50;
int maxwidth = 200;

void setup_windows(void) 
{
	getmaxyx(stdscr, maxy, maxx);

	zap_windows();	

	title_bar_window = subwin(stdscr, 1, maxx, 0, 0);
	cstate_window = subwin(stdscr, 7, maxx, 2, 0);
	wakeup_window = subwin(stdscr, 1, maxx, 9, 0);
	battery_power_window = subwin(stdscr, 2, maxx, 10, 0);
	timerstat_window = subwin(stdscr, maxy-16, maxx, 12, 0);
	maxtimerstats = maxy-16  -2;
	maxwidth = maxx - 18;
	suggestion_window = subwin(stdscr, 3, maxx, maxy-4, 0);	
	status_bar_window = subwin(stdscr, 1, maxx, maxy-1, 0);

	strcpy(status_bar_slots[0], _(" Q - Quit "));
	strcpy(status_bar_slots[1], _(" R - Refresh "));

	werase(stdscr);
	refresh();
}

void initialize_curses(void) 
{
	initscr();
	start_color();
	keypad(stdscr, TRUE);	/* enable keyboard mapping */
	nonl();			/* tell curses not to do NL->CR/NL on output */
	cbreak();		/* take input chars one at a time, no wait for \n */
	noecho();		/* dont echo input */
	curs_set(0);		/* turn off cursor */
	use_default_colors();

	init_pair(PT_COLOR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
	init_pair(PT_COLOR_HEADER_BAR, COLOR_BLACK, COLOR_WHITE);
	init_pair(PT_COLOR_ERROR, COLOR_BLACK, COLOR_RED);
	init_pair(PT_COLOR_RED, COLOR_WHITE, COLOR_RED);
	init_pair(PT_COLOR_YELLOW, COLOR_WHITE, COLOR_YELLOW);
	init_pair(PT_COLOR_GREEN, COLOR_WHITE, COLOR_GREEN);
	init_pair(PT_COLOR_BLUE, COLOR_WHITE, COLOR_BLUE);
	init_pair(PT_COLOR_BRIGHT, COLOR_WHITE, COLOR_BLACK);
	
	atexit(cleanup_curses);
}

void show_title_bar(void) 
{
	int i;
	int x;
	wattrset(title_bar_window, COLOR_PAIR(PT_COLOR_HEADER_BAR));
	wbkgd(title_bar_window, COLOR_PAIR(PT_COLOR_HEADER_BAR));   
	werase(title_bar_window);

	print(title_bar_window, 0, 0,  "     PowerTOP version %s      (C) 2007 Intel Corporation", VERSION);

	wrefresh(title_bar_window);

	werase(status_bar_window);

	x = 0;
	for (i=0; i<10; i++) {
		if (strlen(status_bar_slots[i])==0)
			continue;
		wattron(status_bar_window, A_REVERSE);
		print(status_bar_window, 0, x, status_bar_slots[i]);
		wattroff(status_bar_window, A_REVERSE);			
		x+= strlen(status_bar_slots[i])+1;
	}
	wrefresh(status_bar_window);
}

void show_cstates(void) 
{
	int i, count = 0;
	werase(cstate_window);

	for (i=0; i < 10; i++) {
		if (i == topcstate+1)
			wattron(cstate_window, A_BOLD);
		else
			wattroff(cstate_window, A_BOLD);			
		if (strlen(cstate_lines[i]) && count <= 6) {
			print(cstate_window, count, 0, "%s", cstate_lines[i]);
			count++;
		}
	}

	for (i=0; i<6; i++) {
		if (i == topfreq+1)
			wattron(cstate_window, A_BOLD);
		else
			wattroff(cstate_window, A_BOLD);			
		print(cstate_window, i, 38, "%s", cpufreqstrings[i]);
	}

	wrefresh(cstate_window);
}


void show_acpi_power_line(double rate, double cap, double capdelta, time_t ti)
{
	char buffer[1024];

	sprintf(buffer,  _("no ACPI power usage estimate available") );

	werase(battery_power_window);
	if (rate > 0.001) {
		char *c;
		sprintf(buffer, _("Power usage (ACPI estimate): %3.1fW (%3.1f hours)"), rate, cap/rate);
		strcat(buffer, " ");
		c = &buffer[strlen(buffer)];
		if (ti>180 && capdelta > 0)
			sprintf(c, _("(long term: %3.1fW,/%3.1fh)"), 3600*capdelta / ti, cap / (3600*capdelta/ti+0.01));
	} 
	else if (ti>120 && capdelta > 0.001)
		sprintf(buffer, _("Power usage (5 minute ACPI estimate) : %5.1f W (%3.1f hours left)"), 3600*capdelta / ti, cap / (3600*capdelta/ti+0.01));

	print(battery_power_window, 0, 0, "%s\n", buffer);	
	wrefresh(battery_power_window);
}

void show_pmu_power_line(unsigned sum_voltage_mV,
                         unsigned sum_charge_mAh, unsigned sum_max_charge_mAh,
                         int sum_discharge_mA)
{
	char buffer[1024];

	if (sum_discharge_mA != 0)
	{
		unsigned remaining_charge_mAh;

		if (sum_discharge_mA < 0)
		{
			/* we are currently discharging */
			sum_discharge_mA = -sum_discharge_mA;
			remaining_charge_mAh = sum_charge_mAh;
		}
		else
		{
			/* we are currently charging */
			remaining_charge_mAh = (sum_max_charge_mAh
						- sum_charge_mAh);
		}

		snprintf(buffer, sizeof(buffer),
			 _("Power usage: %3.1fW (%3.1f hours)"),
			 sum_voltage_mV * sum_discharge_mA / 1e6,
			 (double)remaining_charge_mAh / sum_discharge_mA);
	}
	else
		snprintf(buffer, sizeof(buffer),
			 _("no power usage estimate available") );

	werase(battery_power_window);
	print(battery_power_window, 0, 0, "%s\n", buffer);
	wrefresh(battery_power_window);
}


void show_wakeups(double d, double interval, double C0time)
{
//	printf("Wakeups-from-idle per second : %4.1f\tinterval: %0.1fs\n", d, interval);
}

void leave (int sig)
{
  FILE *fp;
  int i;
  char str1[1023];
  char hostname[1023];

  if (runmode != MONITOR_ONLY)
    {
      gethostname (hostname, 1023);
      sprintf (str1, "traces_%s_%d", hostname, getpid ());

      fp = fopen (str1, "w");
      fprintf (stderr, "\nPreparing trace file %s\n", str1);
      for (i = 0; i < mapcount; i++)
        {
          fprintf (fp, "%5.1f %s\n", value_list[i], string_list[i]);
        }

      fclose (fp);
    }

  exit (sig);
}

/* FIXME: Need to make this more clean */
/* FIXME: Floating point bug */
void monitor_mode_init (char *tracefile)
{
  FILE *fp;
  char *p = NULL;
  int c;
  float x;

  fp = fopen (tracefile, "r");

  while (!feof (fp))
    {
      fscanf (fp, "%f", &x);
      char *tmp = NULL;
      tmp = (char *) malloc (sizeof (char) * 1023);

      // Skip as many spaces as necessary
      while (1)
        {
          c = fgetc (fp);
          if (c != ' ')
            break;
        }

      sprintf (tmp, "%c", c); // First valid non-space character

      while ((c = fgetc (fp)) && !(c == EOF || c == '\n'))
        {
          sprintf (tmp, "%s%c",tmp,c);
        }
      string_list[mapcount] = tmp;
      value_list[mapcount] = x;
      mapcount++;
    } 
}

void show_timerstats(int nostats, int ticktime)
{
	int i;
  int flag = 0;

	if (!nostats) {
		int counter = 0;
		for (i = 0; i < linehead; i++)
			if ((lines[i].count > 0 || lines[i].disk_count > 0) && counter++ < maxtimerstats)
        {
          flag = 0;
          int k;
          for (k = 0; k < mapcount; k++)
            {
              if (strncmp (string_list[k], lines[i].string, sizeof (char) * strlen (lines[i].string)) == 0)
                {
                  flag = 1;
                  if ((lines[i].count - value_list[k]) * 1.0/ticktime > 100 && runmode != TRAIN_ONLY)
                    {
                      struct timeval tp;
                      gettimeofday (&tp, NULL);
                      printf ("--%5.1f: %5.1f %5.1f\t: %s\n", (double) tp.tv_sec, value_list[k] * 1.0/ticktime, (lines[i].count - value_list[k]) * 1.0/ticktime, string_list[k]);
                      printf ("\n");
                    }
                  if (runmode != MONITOR_ONLY)
                    {
                      value_list[k] =  0.1 * lines[i].count + (1 - 0.1) * value_list[k]; // Simple Exponential Smoothing
                    }
                  break;
                }
            }
          if (flag == 0 && runmode != MONITOR_ONLY)
            {
              string_list[mapcount] = (char *) malloc (sizeof(char) * (strlen (lines[i].string) + 1));
              strncpy (string_list[mapcount], lines[i].string, sizeof(char) * (strlen (lines[i].string) + 1));
              value_list[mapcount] = lines[i].count;// * 1.0/ticktime;
              mapcount++;
            } 
				}
	} else {
		if (geteuid() == 0) {
			printf("No detailed statistics available; please enable the CONFIG_TIMER_STATS kernel option\n");
			printf("This option is located in the Kernel Debugging section of menuconfig\n");
			printf("(which is CONFIG_DEBUG_KERNEL=y in the config file)\n");
			printf("Note: this is only available in 2.6.21 and later kernels\n");
		} else
			printf("No detailed statistics available; PowerTOP needs root privileges for that\n");
	}

}

void show_suggestion(char *sug)
{
	werase(suggestion_window);
	print(suggestion_window, 0, 0, "%s", sug);
	wrefresh(suggestion_window);
}

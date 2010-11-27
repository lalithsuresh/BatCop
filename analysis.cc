/*
 * Copyright 2007, Intel Corporation
 *
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
 *  Lalith Suresh <suresh.lalith@gmail.com>
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
#include "ap.h"
#include "alglibinternal.h"

static WINDOW *battery_power_window;

#define print(win, y, x, fmt, args...) do { mvwprintw(win, y, x, fmt, ## args); } while (0)

char status_bar_slots[10][40];

static int mapcount = 0;
static char* string_list[1000];
static double value_list[1000] = {0};

int maxtimerstats = 50;

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

#ifdef __cplusplus
extern "C"{
#endif

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

#ifdef __cplusplus
} //extern "C"
#endif

/* FIXME: Need to make this more clean */
/* FIXME: Floating point bug */
void monitor_mode_init (char *tracefile)
{
  FILE *fp;
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

void compute_timerstats(int nostats, int ticktime)
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
                  if (((double) lines[i].count - value_list[k]) * 1.0/ticktime > 100.0 && runmode != TRAIN_ONLY)
                    {
                      struct timeval tp;
                      gettimeofday (&tp, NULL);
                    }
                  if (runmode != MONITOR_ONLY)
                    {
                      value_list[k] =  0.1 * lines[i].count + (1 - 0.1) * value_list[k]; // Simple Exponential Smoothing
                      printf ("%5.1f %5.1f\t: %s\n", value_list[k] * 1.0/ticktime, lines[i].count * 1.0/ticktime, string_list[k]);
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

/*
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

#include <map>
#include <iostream>

#include "batcop.h"
#include "ap.h"
#include "alglibinternal.h"
#include "dataanalysis.h"

static WINDOW *battery_power_window;

#define print(win, y, x, fmt, args...) do { mvwprintw(win, y, x, fmt, ## args); } while (0)

char status_bar_slots[10][40];

struct data_tuple_ {
  int cpu;
  int disk;
  int irq;
  bool flag;
};

typedef struct data_tuple_ data_tuple;

static int mapcount = 0;

std::map <char *, std::vector<data_tuple> > datamap;
std::map <char *, int > countmap;
std::map <char *, alglib::real_2d_array > analysismap;

alglib::real_2d_array r2a;

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
  for (std::map<char *, std::vector<data_tuple> >::const_iterator i = datamap.begin ();
        i != datamap.end (); i++)
    {
      //std::cerr << i->first << " irq:" << i->second.irq << " cpu:" << i->second.cpu << " disk:" << i->second.disk << "\n";
      std::cerr << "\n" << i->first << "\n";
      for (std::vector<data_tuple>::const_iterator vect = i->second.begin ();
            vect != i->second.end(); vect++)
        {
          std::cerr << "[" << vect->cpu << "," << vect->disk << "," << vect->irq << "]\n";
        }
    }

  for (std::map<char *, alglib::real_2d_array >::const_iterator i = analysismap.begin ();
        i != analysismap.end (); i++)
    {
      alglib::ae_int_t info;
      alglib::real_2d_array C;
      alglib::integer_1d_array xyc;
      alglib::kmeansgenerate (i->second, 10, 2, 2, 10, info, C, xyc);

      if (info == 1)
        fprintf (stderr, "%s : %d: [%f %f],[%f %f]\n", i->first, info, C[0][0], C[0][1], C[1][0], C[1][1]);
    }
    
/*
  FILE *fp;
  int i;
  char str1[1023];
  char hostname[1023];

  alglib::ae_int_t x;
  alglib::real_1d_array w;
  alglib::fisherlda (r2a, 100, 2, 2, 10, x, w);

  printf ("\nFisher Algorithm has returned status: %d\n",x);
  for (int i = 0; i < 2; i++)
    {
      printf ("%f ", w[i]);
    }
  printf ("\n");

  if (runmode != MONITOR_ONLY)
    {
      gethostname (hostname, 1023);
      sprintf (str1, "traces_%s_%d", hostname, getpid ());

      fp = fopen (str1, "w");
      fprintf (stderr, "\nPreparing trace file %s\n", str1);
      for (i = 0; i < mapcount; i++)
        {
          fprintf (fp, "%d %s\n", value_list[i], string_list[i]);
        }

      fclose (fp);
    }
*/
  exit (sig);
}

#ifdef __cplusplus
} //extern "C"
#endif

void training_mode_init ()
{
  r2a.setlength (100,3);
}


/* FIXME: Need to make this more clean */
/* FIXME: Floating point bug */
void monitor_mode_init (char *tracefile)
{
/*
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
    */
}

void compute_timerstats(int nostats, int ticktime)
{
	int i;
  int flag = 0;
  static int numsamples = 0;

	if (!nostats) {
		int counter = 0;
		for (i = 0; i < linehead; i++)
			if ((lines[i].count > 0 || lines[i].disk_count > 0) && counter++ < maxtimerstats)
        {
          //printf(" %5.1f%% (%5.1f)  %s\n", lines[i].count * 100.0 / linectotal,
          //  lines[i].count * 1.0 / ticktime,
          //  lines[i].string);

          if (runmode == TRAIN_ONLY)
            {
              data_tuple temp;
              if (datamap.find (lines[i].string) == datamap.end ())
                {
                  temp.cpu = 0 * 1.0/ticktime;
                  temp.irq = lines[i].count * 1.0/ticktime;
                  temp.disk = lines[i].disk_count * 1.0/ticktime;
                  temp.flag = 1;
                  datamap[lines[i].string].push_back (temp);
                  countmap[lines[i].string] = 0;
                  analysismap[lines[i].string].setlength (100, 2);
                }
              else if (datamap[lines[i].string].size () == 1 && datamap[lines[i].string][0].flag == 1)
                {
                  datamap[lines[i].string].clear ();
                  temp.cpu = 0;
                  temp.irq = lines[i].count * 1.0/ticktime;
                  temp.disk = lines[i].disk_count * 1.0/ticktime;
                  temp.flag = 0;

                  datamap[lines[i].string].push_back (temp);
                  countmap[lines[i].string] = 0;
                  fprintf (stderr, "Second: %s: countmap: %d\n", lines[i].string, countmap[lines[i].string]);
//                  analysismap[lines[i].string][countmap[lines[i].string]][0] = temp.cpu;
                  analysismap[lines[i].string][0][countmap[lines[i].string]] = temp.irq;
                  analysismap[lines[i].string][1][countmap[lines[i].string]] = temp.disk;
                }
              else
                {
                  temp.cpu = 0;
                  temp.irq = lines[i].count * 1.0/ticktime;
                  temp.disk = lines[i].disk_count * 1.0/ticktime;
                  temp.flag = 0;
                  
                  datamap[lines[i].string].push_back (temp);
                  if (countmap[lines[i].string] < 10)
                    {
                      countmap[lines[i].string]++;
                      fprintf (stderr, "Third: %s: countmap: %d \n", lines[i].string, countmap[lines[i].string]);
//                      analysismap[lines[i].string][countmap[lines[i].string]][0] = temp.cpu;
                      analysismap[lines[i].string][0][countmap[lines[i].string]] = temp.irq;
                      analysismap[lines[i].string][1][countmap[lines[i].string]] = temp.disk;
                    }
                }

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

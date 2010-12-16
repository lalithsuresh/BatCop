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
#include <assert.h>

#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#include "batcop.h"
#include "whitelist.h"
#include "ap.h"
#include "alglibinternal.h"
#include "dataanalysis.h"
#include "cfm-callback.h"

static WINDOW *battery_power_window;

char status_bar_slots[10][40];

struct data_tuple_ {
  long long int cpu;
  int disk;
  int irq;
  bool flag;
};

typedef struct data_tuple_ data_tuple;

static int mapcount = 0;

//We don't need a vector here. I'll refactor later -Lalith
std::map <char *, std::vector<data_tuple> > datamap;

// When collecting data for training mode, we track how many 
// data-points we have per process with the below map.
std::map <char *, int > countmap;

// Data in this is finally processed
std::map <char *, alglib::real_2d_array > analysismap;

// This lets us calculate the running deltas
std::map <std::string, double > last_val_map;

// This is required exclusively for keeping track of CPU
// utilisation per process from /proc/pid/stat
std::map <char *, long long int > last_cpu;

// During monitor mode, this map stores the centroids
// calculated for each process from the k-means analysis
// performed during training mode
std::map <std::string, std::vector<double> > centroid_map;

alglib::real_2d_array r2a;

int maxtimerstats = 50;

// Function to collected CPU utilisation in a given period
// per application. This extracts the number of jiffies
// a process has used in both user space and kernel space
// combined.
long long int getTicksFromPid (char *inPid)
{
  long long int pid;
  char tcomm[PATH_MAX];
  char state;

  long long int ppid;
  long long int pgid;
  long long int sid;
  long long int tty_nr;
  long long int tty_pgrp;

  long long int flags;
  long long int min_flt;
  long long int cmin_flt;
  long long int maj_flt;
  long long int cmaj_flt;
  long long int utime;
  long long int stimev;


  FILE *input;

  input = NULL;
  std::stringstream filename;

  // Every process has a /proc/pid/stat entry
  filename << "/proc/" << inPid << "/stat";

  input = fopen (filename.str().c_str(), "r");

  // Need a more graceful way of handling this case
  if(!input) {
    fprintf (logfile, "File-open: Could not open stat file for %s\n", inPid);
    return 0;
  }

  fscanf(input, "%lld ", &pid);
  fscanf(input, "%s ", tcomm);
  fscanf(input, "%c ", &state);
  fscanf(input, "%lld ", &ppid);
  fscanf(input, "%lld ", &pgid);
  fscanf(input, "%lld ", &sid);
  fscanf(input, "%lld ", &tty_nr);
  fscanf(input, "%lld ", &tty_pgrp);
  fscanf(input, "%lld ", &flags);
  fscanf(input, "%lld ", &min_flt);
  fscanf(input, "%lld ", &cmin_flt);
  fscanf(input, "%lld ", &maj_flt);
  fscanf(input, "%lld ", &cmaj_flt);

  // We're interested only in these two,
  // but we need to go through the rest
  // of the file anyway.
  fscanf(input, "%lld ", &utime);
  fscanf(input, "%lld ", &stimev);
  
  fclose (input);

  return utime + stimev;
}

// FIXME: From PowerTOP. Unused for now. Remove later.
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

	//print(battery_power_window, 0, 0, "%s\n", buffer);	
	wrefresh(battery_power_window);
}

// FIXME: From PowerTOP. Unused for now. Remove later.
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
	//print(battery_power_window, 0, 0, "%s\n", buffer);
	wrefresh(battery_power_window);
}

// Uncomment this to display wakeups-from-idle per second
// (from PowerTOP)
void show_wakeups(double d, double interval, double C0time)
{
//	printf("Wakeups-from-idle per second : %4.1f\tinterval: %0.1fs\n", d, interval);
}

#ifdef __cplusplus
extern "C"{
#endif

void process_and_exit ()
{
  /*
  // Uncomment this section for debugging needs - Lalith
  for (std::map<char *, alglib::real_2d_array >::const_iterator i = analysismap.begin ();
        i != analysismap.end (); i++)
    {
      std::cerr << i->first << "\n";
      
      for (uint32_t j = 0; j < training_cycles; j++)
        {
          std::cerr << i->second[j][0] << " ";
        }
      std::cerr << "\n";
    }
  */

  // If anyone else calls this, die.
  assert (runmode == TRAIN_ONLY);
  std::stringstream filename;
  filename << "traces_" << getpid ();

  std::fstream file_op(filename.str().c_str(),std::ios::out);

  if (!file_op.is_open())
    {
      fprintf (logfile, "Cannot open trace file\n");
    }
  for (std::map<char *, alglib::real_2d_array >::const_iterator i = analysismap.begin ();
      i != analysismap.end (); i++)
    {
      alglib::ae_int_t info;
      alglib::real_2d_array C;
      alglib::integer_1d_array xyc;

      // Run k-means analysis on data with NPoints=training_cycles, NVars=3, NumClusters=2, NumRestarts=100
      alglib::kmeansgenerate (i->second, training_cycles, 3, 2, 100, info, C, xyc);

      if (info == 1)
        {
          // Need to test min-max consistency
          file_op << min(C[0][0], C[0][1]) << " "     // irq1
                  << max(C[0][0], C[0][1]) << " "     // irq2
                  << min(C[1][0], C[1][1]) << " "     // disk1
                  << max(C[1][0], C[1][1]) << " "     // disk2
                  << min(C[2][0], C[2][1]) << " "     // cpu1
                  << max(C[2][0], C[2][1]) << " "     // cpu2
                  << i->first << "\n";                // We add the name last to make it easier to read later
        }
    }

  fprintf (logfile, "Writing K-Means results to file: %s\n", filename.str().c_str());
  file_op.close ();

  exit (0);
}

#ifdef __cplusplus
} //extern "C"
#endif

// This method prepares the centroid maps from the tracefile
// (generated from a training run), reads the whitelist of
// processes to be ignored, and finally the list of callbacks
// for confirming suspicions from a particular process
void monitor_mode_init (char *tracefile, char *whitefile, char *cbfile)
{
  FILE *fp;
  int c;
  float x;

  fp = fopen (tracefile, "r");

  while (!feof (fp))
    {
      // Push centroid coordinates onto a vector.
      //                      0       1       2         3         4         5
      // centroid_vect -> {C[0][0], C[0][1], C[1][0], C[1][1], C[2][0], C[2][1]}
      std::vector<double> centroid_vect;
      for (uint32_t i =0; i < 6; i++)
        {
          fscanf (fp, "%f", &x);
          centroid_vect.push_back (x);
        }

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

      std::stringstream temp;
      temp << tmp;
      // By now we have the whole process name.
      // So add to centroid map
      centroid_map[temp.str()] = centroid_vect;
    } 

  // See if whitelist is defined
  if (whitefile != NULL)
    {
      read_whitelist (whitefile);
    }
  else
    {
      fprintf (logfile, "* No whitelist file defined. All processes will be now under watch.\n");
    }

  // Read confirmation callbacks
  std::string fname (cbfile);

  if (cbfile != NULL)
    {
      read_confirmation_callback_conf (fname.c_str());
    }
  else
    {
      fprintf (logfile, "* No callback list defined.\n");
    }
}

// The heart of batcop. This method puts together the
// data in training mode, and performs the analysis
// during monitor mode.
void compute_timerstats(int nostats, int ticktime)
{
	int i;
  int flag = 0;
  static int numsamples = 0;
  static bool complete = false;

	if (!nostats) {
		int counter = 0;

    /* Probably need to be careful about the disk utilisation params too */
		for (i = 0; i < linehead; i++)
      {
        if ((lines[i].count > 0 || lines[i].disk_count > 0) && counter++ < maxtimerstats)
          {
            //printf(" %5.1f%% (%5.1f)  %s\n", lines[i].count * 100.0 / linectotal,
            //  lines[i].count * 1.0 / ticktime,
            //  lines[i].string);

            if (runmode == TRAIN_ONLY)
              {
                data_tuple temp;

                // Let the data collectors warmup
                if (datamap.find (lines[i].string) == datamap.end ())
                  {
                    temp.cpu = 0;
                    temp.irq = lines[i].count;
                    temp.disk = lines[i].disk_count * 1.0/ticktime;
                    temp.flag = 1;

                    if (strcmp (lines[i].pid, "0") !=0 && strcmp (lines[i].pid, "") != 0)
                      {
                        last_cpu[lines[i].pid] = getTicksFromPid (lines[i].pid);
                      }
                    datamap[lines[i].string].push_back (temp); // We don't really need a vector here. I'll refactor later.
                    countmap[lines[i].string] = 0;
                    analysismap[lines[i].string].setlength (training_cycles, 3);
                  }

                // First entries actually goes in at this point
                else if (datamap[lines[i].string].size () == 1 && datamap[lines[i].string][0].flag == 1)
                  {
                    temp.cpu = 0;
                    temp.irq = (lines[i].count - datamap[lines[i].string][countmap[lines[i].string]].irq) * 1.0/ticktime;
                    temp.disk = lines[i].disk_count * 1.0/ticktime;
                    temp.flag = 0;

                    //fprintf (stderr, "%s: %d\n", lines[i].string, temp.irq); 
                    if (strcmp (lines[i].pid, "0") !=0 && strcmp (lines[i].pid, "") != 0)
                      {
                        long long int newcount = getTicksFromPid (lines[i].pid);
                        temp.cpu = (newcount - last_cpu[lines[i].pid]) * 1.0/ticktime;
                        last_cpu[lines[i].pid] = newcount;
                      }
                    countmap[lines[i].string] = 0;
                    datamap[lines[i].string].push_back (temp);
                    datamap[lines[i].string][datamap[lines[i].string].size () - 1].irq = lines[i].count;
                    analysismap[lines[i].string][countmap[lines[i].string]][0] = temp.irq;
                    analysismap[lines[i].string][countmap[lines[i].string]][1] = temp.disk;
                    analysismap[lines[i].string][countmap[lines[i].string]][2] = temp.cpu;
                  }

                // Second entry on goes in from here
                else
                  {
                    temp.cpu = 0;
                    temp.irq = (lines[i].count  - datamap[lines[i].string][countmap[lines[i].string]].irq)* 1.0/ticktime;
                    temp.disk = lines[i].disk_count * 1.0/ticktime;
                    temp.flag = 0;
                    
                    //fprintf (stderr, "%s: %d\n", lines[i].string, temp.irq); 
                    if (strcmp (lines[i].pid, "0") !=0 && strcmp (lines[i].pid, "") != 0)
                      {
                        long long int newcount = getTicksFromPid (lines[i].pid);
                        temp.cpu = (newcount - last_cpu[lines[i].pid]) * 1.0/ticktime;
                        last_cpu[lines[i].pid] = newcount;
                      }
                    datamap[lines[i].string].push_back (temp);
                    datamap[lines[i].string][datamap[lines[i].string].size () - 1].irq = lines[i].count;
                                      
                    if (countmap[lines[i].string] < training_cycles - 1)
                      {
                        complete = false; 
                        countmap[lines[i].string]++;
                        analysismap[lines[i].string][countmap[lines[i].string]][0] = temp.irq;
                        analysismap[lines[i].string][countmap[lines[i].string]][1] = temp.disk;
                        analysismap[lines[i].string][countmap[lines[i].string]][2] = temp.cpu;
                      }
                    else
                      {
                        complete = true;
                      }
                  }

                  if (complete == true)
                    {
                      process_and_exit ();
                    }
               }
            else if (runmode == MONITOR_ONLY)
              {
                std::stringstream pcharToString;
                pcharToString << lines[i].string;

                // Warmup mechanism
                if (last_val_map.find (pcharToString.str()) == last_val_map.end ())
                  {
                    if (centroid_map.find (pcharToString.str()) != centroid_map.end ())
                      {
                        data_tuple temp;
                        temp.cpu = 0;
                        temp.irq = lines[i].count * 1.0/ticktime;
                        temp.disk = lines[i].disk_count * 1.0/ticktime;
                        temp.flag = 0;

                        if (strcmp (lines[i].pid, "0") !=0 && strcmp (lines[i].pid, "") != 0)
                          {
                            long long int newcount = getTicksFromPid (lines[i].pid);
                            temp.cpu = (newcount - last_cpu[lines[i].pid]) * 1.0/ticktime;
                            last_cpu[lines[i].pid] = newcount;
                          }

                        last_val_map[pcharToString.str()] = lines[i].count;
                      }
                  }
                // Perform detection from here
                else if (centroid_map.find (pcharToString.str()) != centroid_map.end () && lookup_whitelist (pcharToString.str().c_str()) == 0)
                  {
                    double distance1, distance2; // distance from both centroids
                    double min;
                    std::vector<double> centroid_vect;
                    data_tuple temp;
                    centroid_vect = centroid_map[lines[i].string];

                    temp.cpu = 0;
                    temp.irq = (lines[i].count - last_val_map[pcharToString.str()] )* 1.0/ticktime;
                    temp.disk = lines[i].disk_count * 1.0/ticktime;
                    temp.flag = 0;

                    //fprintf (stderr, "Mon: %s: irq: %d\n", lines[i].string, temp.irq);
                    if (strcmp (lines[i].pid, "0") !=0 && strcmp (lines[i].pid, "") != 0)
                      {
                        long long int newcount = getTicksFromPid (lines[i].pid);
                        temp.cpu = (newcount - last_cpu[lines[i].pid]) * 1.0/ticktime;
                        last_cpu[lines[i].pid] = newcount;
                      }

                    // Calculate Euclidian distance to each centroid
                    // sqrt (dist(Cx,irq)^2 + dist(Cy,disk)^2 + dist(Cz,cpu)^2)
                    distance1 = sqrt (  (centroid_vect[0] - temp.irq) * (centroid_vect[0] - temp.irq) 
                                     +  (centroid_vect[2] - temp.disk) * (centroid_vect[2] - temp.disk)
                                     +  (centroid_vect[4] - temp.cpu) * (centroid_vect[4] - temp.cpu)  );

                    distance2 = sqrt (  (centroid_vect[1] - temp.irq) * (centroid_vect[1] - temp.irq) 
                                     +  (centroid_vect[3] - temp.disk) * (centroid_vect[3] - temp.disk)
                                     +  (centroid_vect[5] - temp.cpu) * (centroid_vect[5] - temp.cpu)  );

                    last_val_map[pcharToString.str()] = lines[i].count;

                    struct timeval tv;
                    gettimeofday (&tv, 0);

                    // If the new calculated point is further from
                    // Centroid1 (legitimate) than Centroid2 (malicious),
                    // then raise a suspicion
                    if (distance1 > distance2)
                      {
                        fprintf (logfile, "%ld: %s is acting suspicious! %f %f\n", tv.tv_sec, lines[i].string, distance1, distance2);
                        trigger_callbacks (lines[i].string);
                      }
                  }
              }
          }
      }
	  } 
  else 
    {
		  if (geteuid() == 0)
        {
    			fprintf(logfile, "No detailed statistics available; please enable the CONFIG_TIMER_STATS kernel option\n");
		    	fprintf(logfile, "This option is located in the Kernel Debugging section of menuconfig\n");
    			fprintf(logfile, "(which is CONFIG_DEBUG_KERNEL=y in the config file)\n");
    			fprintf(logfile, "Note: this is only available in 2.6.21 and later kernels\n");
		    }
      else
        {
    			fprintf(logfile, "No detailed statistics available; BatCop needs root privileges for that\n");
        }
  	}
}

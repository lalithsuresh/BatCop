;/*
 * Copyright 2007, Intel Corporation
 *
 * This file is part of PowerTOP, modified for BatCop
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
 *  Lalith Suresh <suresh.lalith@gmail.com>
 */

#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <libintl.h>
#include <ctype.h>
#include <assert.h>
#include <locale.h>
#include <time.h>
#include <limits.h>
#include <sys/stat.h>
#include <signal.h>

#include "batcop.h"

// PowerTOP was originally C, this
// allows BatCop to use C-C++ linkage.
#ifdef __cplusplus
extern "C"
{
#endif

uint64_t start_usage[8], start_duration[8];
uint64_t last_usage[8], last_duration[8];
char cnames[8][16];

double ticktime = 15.0;

int interrupt_0, total_interrupt;

int showpids = 1;
int training_cycles = 10;

static int maxcstate = 0;
int topcstate = 0;
FILE *logfile = NULL;

#define IRQCOUNT 150

struct irqdata {
	int active;
	int number;
	uint64_t count;
	char description[256];
};

struct irqdata interrupts[IRQCOUNT];

#define FREQ_ACPI 3579.545
static unsigned long FREQ;

int nostats;


struct line	*lines;
int		linehead;
int		linesize;
int		linectotal;
int   runmode = 0;


double last_bat_cap = 0;
double prev_bat_cap = 0;
time_t last_bat_time = 0;
time_t prev_bat_time = 0;

double displaytime = 0.0;

// This enables a graceful exit. Else logfiles
// get corrupted.
void leave (int sig)
{
  struct timeval tv;
  gettimeofday (&tv, 0);
  fprintf (logfile, "%ld Received SIGINT. Exiting.\n", tv.tv_sec);
  fclose (logfile);
  exit (sig);
}

// From PowerTOP. Modified for BatCop.
void push_line(char *string, int count)
{
	int i;

	assert(string != NULL);
	for (i = 0; i < linehead; i++)
		if (strcmp(string, lines[i].string) == 0) {
			lines[i].count += count;
			return;
		}
	if (linehead == linesize)
		lines = (struct line*) realloc (lines, (linesize ? (linesize *= 2) : (linesize = 64)) * sizeof (struct line));
	memset(&lines[linehead], 0, sizeof(&lines[0]));
	lines[linehead].string = strdup (string);
	lines[linehead].count = count;
	lines[linehead].disk_count = 0;
	lines[linehead].pid[0] = 0;
	linehead++;
}

// From PowerTOP. Modified for BatCop.
void push_line_pid(char *string, int cpu_count, int disk_count, char *pid) 
{
	int i;
	assert(string != NULL);
	assert(strlen(string) > 0);
	for (i = 0; i < linehead; i++)
		if (strcmp(string, lines[i].string) == 0) {
			lines[i].count += cpu_count;
			lines[i].disk_count += disk_count;
//			if (pid && strcmp(lines[i].pid, pid)!=0)
//				lines[i].pid[0] = 0;
			return;
		}
	if (linehead == linesize)
		lines = (struct line *) realloc (lines, (linesize ? (linesize *= 2) : (linesize = 64)) * sizeof (struct line));
	memset(&lines[linehead], 0, sizeof(&lines[0]));
	lines[linehead].string = strdup (string);
	lines[linehead].count = cpu_count;
	lines[linehead].disk_count = disk_count;
	if (pid)
		strcpy(lines[linehead].pid, pid);
	linehead++;
}

// From PowerTOP.
void count_lines(void)
{
	uint64_t q = 0;
	int i;
	for (i = 0; i < linehead; i++)
		q += lines[i].count;
	linectotal = q;
}

// From PowerTOP.
int update_irq(int irq, uint64_t count, char *name)
{
	int i;
	int firstfree = IRQCOUNT;

	if (!name)
		return 0;

	for (i = 0; i < IRQCOUNT; i++) {
		if (interrupts[i].active && interrupts[i].number == irq) {
			uint64_t oldcount;
			oldcount = interrupts[i].count;
			interrupts[i].count = count;
			return count - oldcount;
		}
		if (!interrupts[i].active && firstfree > i)
			firstfree = i;
	}

	interrupts[firstfree].active = 1;
	interrupts[firstfree].count = count;
	interrupts[firstfree].number = irq;
	strcpy(interrupts[firstfree].description, name);
	if (strcmp(name,"i8042\n")==0)
		strcpy(interrupts[firstfree].description, _("PS/2 keyboard/mouse/touchpad"));
	return count;
}

// From PowerTOP.
static int percpu_hpet_timer(char *name)
{
	static int timer_list_read;
	static int percpu_hpet_start = INT_MAX, percpu_hpet_end = INT_MIN;
	char *c;
	long hpet_chan;

	if (!timer_list_read) {
		char file_name[20];
		char ln[80];
		FILE *fp;

		timer_list_read = 1;
		snprintf(file_name, sizeof(file_name), "/proc/timer_list");
		fp = fopen(file_name, "r");
		if (fp == NULL)
			return 0;

		while (fgets(ln, sizeof(ln), fp) != NULL)
		{
			c = strstr(ln, "Clock Event Device: hpet");
			if (!c)
				continue;

			c += 24;
			if (!isdigit(c[0]))
				continue;

			hpet_chan = strtol(c, NULL, 10);
			if (hpet_chan < percpu_hpet_start)
				percpu_hpet_start = hpet_chan;
			if (hpet_chan > percpu_hpet_end)
				percpu_hpet_end = hpet_chan;
		}
		fclose(fp);
	}

	c = strstr(name, "hpet");
	if (!c)
		return 0;

	c += 4;
	if (!isdigit(c[0]))
		return 0;

	hpet_chan = strtol(c, NULL, 10);
	if (percpu_hpet_start <= hpet_chan && hpet_chan <= percpu_hpet_end)
		return 1;

	return 0;
}

// From PowerTOP.
static void do_proc_irq(void)
{
	FILE *file;
	char line[1024];
	char line2[1024];
	char *name;
	uint64_t delta;
	
	interrupt_0 = 0;
	total_interrupt  = 0;

	file = fopen("/proc/interrupts", "r");
	if (!file)
		return;
	while (!feof(file)) {
		char *c;
		int nr = -1;
		uint64_t count = 0;
		int special = 0;
		memset(line, 0, sizeof(line));
		if (fgets(line, 1024, file) == NULL)
			break;
		c = strchr(line, ':');
		if (!c)
			continue;
		/* deal with NMI and the like.. make up fake nrs */
		if (line[0] != ' ' && (line[0] < '0' || line[0] > '9')) {	
			if (strncmp(line,"NMI:", 4)==0)
				nr=20000;
			if (strncmp(line,"RES:", 4)==0)
				nr=20001;
			if (strncmp(line,"CAL:", 4)==0)
				nr=20002;
			if (strncmp(line,"TLB:", 4)==0)
				nr=20003;
			if (strncmp(line,"TRM:", 4)==0)
				nr=20004;
			if (strncmp(line,"THR:", 4)==0)
				nr=20005;
			if (strncmp(line,"SPU:", 4)==0)
				nr=20006;
			special = 1;
		} else
			nr = strtoull(line, NULL, 10);

		if (nr==-1)
			continue;
		*c = 0;
		c++;
		while (c && strlen(c)) {
			char *newc;
			count += strtoull(c, &newc, 10);
			if (newc == c)
				break;
			c = newc;
		}
		c = strchr(c, ' ');
		if (!c) 
			continue;
		while (c && *c == ' ')
			c++;
		if (!special) {
			c = strchr(c, ' ');
			if (!c) 
				continue;
			while (c && *c == ' ')
				c++;
		}
		name = c;
		delta = update_irq(nr, count, name);
		c = strchr(name, '\n');
		if (c)
			*c = 0;
		if (strcmp(name, "i8042")) { 
			if (special) 
				sprintf(line2, _("[%s] <kernel IPI>"), name);
			else
				sprintf(line2, _("[%s] <interrupt>"), name);
		}
		else
			sprintf(line2, _("%s interrupt"), _("PS/2 keyboard/mouse/touchpad"));

		/* skip per CPU timer interrupts */
		if (percpu_hpet_timer(name))
			delta = 0;

		if (nr > 0 && delta > 0)
			push_line(line2, delta);
		if (nr==0)
			interrupt_0 = delta;
		else
			total_interrupt += delta;
	}
	fclose(file);
}

// FIXME: From PowerTOP. Unused for now. Remove later.
static void read_data_acpi(uint64_t * usage, uint64_t * duration)
{
	DIR *dir;
	struct dirent *entry;
	FILE *file = NULL;
	char line[4096];
	char *c;
	int clevel = 0;

	memset(usage, 0, 64);
	memset(duration, 0, 64);

	dir = opendir("/proc/acpi/processor");
	if (!dir)
		return;
	while ((entry = readdir(dir))) {
		if (strlen(entry->d_name) < 3)
			continue;
		sprintf(line, "/proc/acpi/processor/%s/power", entry->d_name);
		file = fopen(line, "r");
		if (!file)
			continue;

		clevel = 0;

		while (!feof(file)) {
			memset(line, 0, 4096);
			if (fgets(line, 4096, file) == NULL)
				break;
			c = strstr(line, "age[");
			if (!c)
				continue;
			c += 4;
			usage[clevel] += 1+strtoull(c, NULL, 10);
			c = strstr(line, "ation[");
			if (!c)
				continue;
			c += 6;
			duration[clevel] += strtoull(c, NULL, 10);

			clevel++;
			if (clevel > maxcstate)
				maxcstate = clevel;

		}
		fclose(file);
	}
	closedir(dir);
}

// From PowerTOP.
static void read_data_cpuidle(uint64_t * usage, uint64_t * duration)
{
	DIR *cpudir;
	DIR *dir;
	struct dirent *entry;
	FILE *file = NULL;
	char line[4096];
	char filename[128], *f;
	int len, clevel = 0;

	memset(usage, 0, 64);
	memset(duration, 0, 64);

	cpudir = opendir("/sys/devices/system/cpu");
	if (!cpudir)
		return;

	/* Loop over cpuN entries */
	while ((entry = readdir(cpudir))) {
		if (strlen(entry->d_name) < 3)
			continue;

		if (!isdigit(entry->d_name[3]))
			continue;

		len = sprintf(filename, "/sys/devices/system/cpu/%s/cpuidle",
			      entry->d_name);

		dir = opendir(filename);
		if (!dir)
			return;

		clevel = 0;

		/* For each C-state, there is a stateX directory which
		 * contains a 'usage' and a 'time' (duration) file */
		while ((entry = readdir(dir))) {
			if (strlen(entry->d_name) < 3)
				continue;
			sprintf(filename + len, "/%s/desc", entry->d_name);
			file = fopen(filename, "r");
			if (file) {

				memset(line, 0, 4096);
				f = fgets(line, 4096, file);
				fclose(file);
				if (f == NULL)
					break;

			
				f = strstr(line, "MWAIT ");
				if (f) {
					f += 6;
					clevel = (strtoull(f, NULL, 16)>>4) + 1;
					sprintf(cnames[clevel], "C%i mwait", clevel);
				} else
					sprintf(cnames[clevel], "C%i\t", clevel);

				f = strstr(line, "POLL IDLE");
				if (f) {
					clevel = 0;
					sprintf(cnames[clevel], "%s\t", _("polling"));
				}

				f = strstr(line, "ACPI HLT");
				if (f) {
					clevel = 1;
					sprintf(cnames[clevel], "%s\t", "C1 halt");
				}
			}
			sprintf(filename + len, "/%s/usage", entry->d_name);
			file = fopen(filename, "r");
			if (!file)
				continue;

			memset(line, 0, 4096);
			f = fgets(line, 4096, file);
			fclose(file);
			if (f == NULL)
				break;

			usage[clevel] += 1+strtoull(line, NULL, 10);

			sprintf(filename + len, "/%s/time", entry->d_name);
			file = fopen(filename, "r");
			if (!file)
				continue;
		
			memset(line, 0, 4096);
			f = fgets(line, 4096, file);
			fclose(file);
			if (f == NULL)
				break;

			duration[clevel] += 1+strtoull(line, NULL, 10);

			clevel++;
			if (clevel > maxcstate)
				maxcstate = clevel;
		
		}
		closedir(dir);

	}
	closedir(cpudir);
}

// From PowerTOP.
static void read_data(uint64_t * usage, uint64_t * duration)
{
	int r;
	struct stat s;

	/* Then check for CPUidle */
	r = stat("/sys/devices/system/cpu/cpu0/cpuidle", &s);
	if (!r) {
		read_data_cpuidle(usage, duration);
		
		/* perform residency calculations based on usecs */
		FREQ = 1000;
		return;
	}

	/* First, check for ACPI */
	r = stat("/proc/acpi/processor", &s);
	if (!r) {
		read_data_acpi(usage, duration);

		/* perform residency calculations based on ACPI timer */
		FREQ = FREQ_ACPI;
		return;
	}
}

// From PowerTOP.
void stop_timerstats(void)
{
	FILE *file;
	file = fopen("/proc/timer_stats", "w");
	if (!file) {
		nostats = 1;
		return;
	}
	fprintf(file, "0\n");
	fclose(file);
}

// From PowerTOP.
void start_timerstats(void)
{
	FILE *file;
	file = fopen("/proc/timer_stats", "w");
	if (!file) {
		nostats = 1;
		return;
	}
	fprintf(file, "1\n");
	fclose(file);
}

// From PowerTOP.
int line_compare (const void *av, const void *bv)
{
	const struct line	*a = (const struct line *) av, *b = (const struct line *)bv;
	return (b->count + 50 * b->disk_count) - (a->count + 50 * a->disk_count);
}

// From PowerTOP.
void sort_lines(void)
{
	qsort (lines, linehead, sizeof (struct line), line_compare);
}

// FIXME: From PowerTOP. Unused for now. Remove later
int print_battery_proc_acpi(void)
{
	DIR *dir;
	struct dirent *dirent;
	FILE *file;
	double rate = 0;
	double cap = 0;

	char filename[256];

	dir = opendir("/proc/acpi/battery");
	if (!dir)
		return 0;

	while ((dirent = readdir(dir))) {
		int dontcount = 0;
		double voltage = 0.0;
		double amperes_drawn = 0.0;
		double watts_drawn = 0.0;
		double amperes_left = 0.0;
		double watts_left = 0.0;
		char line[1024];

		if (strlen(dirent->d_name) < 3)
			continue;

		sprintf(filename, "/proc/acpi/battery/%s/state", dirent->d_name);
		file = fopen(filename, "r");
		if (!file)
			continue;
		memset(line, 0, 1024);
		while (fgets(line, 1024, file) != NULL) {
			char *c;
			if (strstr(line, "present:") && strstr(line, "no"))
				break;

			if (strstr(line, "charging state:")
			    && !strstr(line, "discharging"))
				dontcount = 1;
			c = strchr(line, ':');
			if (!c)
				continue;
			c++;

			if (strstr(line, "present voltage")) 
				voltage = strtoull(c, NULL, 10) / 1000.0;
		
			if (strstr(line, "remaining capacity") && strstr(c, "mW"))
				watts_left = strtoull(c, NULL, 10) / 1000.0;

			if (strstr(line, "remaining capacity") && strstr(c, "mAh"))
				amperes_left = strtoull(c, NULL, 10) / 1000.0; 

			if (strstr(line, "present rate") && strstr(c, "mW"))
				watts_drawn = strtoull(c, NULL, 10) / 1000.0 ;

			if (strstr(line, "present rate") && strstr(c, "mA"))
				amperes_drawn = strtoull(c, NULL, 10) / 1000.0;

		}
		fclose(file);
	
		if (!dontcount) {
			rate += watts_drawn + voltage * amperes_drawn;
		}
		cap += watts_left + voltage * amperes_left;
		

	}
	closedir(dir);
	if (prev_bat_cap - cap < 0.001 && rate < 0.001)
		last_bat_time = 0;
	if (!last_bat_time) {
		last_bat_time = prev_bat_time = time(NULL);
		last_bat_cap = prev_bat_cap = cap;
	}
	if (time(NULL) - last_bat_time >= 400) {
		prev_bat_cap = last_bat_cap;
		prev_bat_time = last_bat_time;
		last_bat_time = time(NULL);
		last_bat_cap = cap;
	}

	show_acpi_power_line(rate, cap, prev_bat_cap - cap, time(NULL) - prev_bat_time);
	return 1;
}

// FIXME: From PowerTOP. Unused for now. Remove later
int print_battery_proc_pmu(void)
{
	char line[80];
	int i;
	int power_present = 0;
	int num_batteries = 0;
	/* unsigned rem_time_sec = 0; */
	unsigned charge_mAh = 0, max_charge_mAh = 0, voltage_mV = 0;
	int discharge_mA = 0;
	FILE *fd;

	fd = fopen("/proc/pmu/info", "r");
	if (fd == NULL)
		return 0;

	while ( fgets(line, sizeof(line), fd) != NULL )
	{
		if (strncmp("AC Power", line, strlen("AC Power")) == 0)
			sscanf(strchr(line, ':')+2, "%d", &power_present);
		else if (strncmp("Battery count", line, strlen("Battery count")) == 0)
			sscanf(strchr(line, ':')+2, "%d", &num_batteries);
	}
	fclose(fd);

	for (i = 0; i < num_batteries; ++i)
	{
		char file_name[20];
		int flags = 0;
		/* int battery_charging, battery_full; */
		/* unsigned this_rem_time_sec = 0; */
		unsigned this_charge_mAh = 0, this_max_charge_mAh = 0;
		unsigned this_voltage_mV = 0, this_discharge_mA = 0;

		snprintf(file_name, sizeof(file_name), "/proc/pmu/battery_%d", i);
		fd = fopen(file_name, "r");
		if (fd == NULL)
			continue;

		while (fgets(line, sizeof(line), fd) != NULL)
		{
			if (strncmp("flags", line, strlen("flags")) == 0)
				sscanf(strchr(line, ':')+2, "%x", &flags);
			else if (strncmp("charge", line, strlen("charge")) == 0)
				sscanf(strchr(line, ':')+2, "%d", &this_charge_mAh);
			else if (strncmp("max_charge", line, strlen("max_charge")) == 0)
				sscanf(strchr(line, ':')+2, "%d", &this_max_charge_mAh);
			else if (strncmp("voltage", line, strlen("voltage")) == 0)
				sscanf(strchr(line, ':')+2, "%d", &this_voltage_mV);
			else if (strncmp("current", line, strlen("current")) == 0)
				sscanf(strchr(line, ':')+2, "%d", &this_discharge_mA);
			/* else if (strncmp("time rem.", line, strlen("time rem.")) == 0) */
			/*   sscanf(strchr(line, ':')+2, "%d", &this_rem_time_sec); */
		}
		fclose(fd);

		if ( !(flags & 0x1) )
			/* battery isn't present */
			continue;

		/* battery_charging = flags & 0x2; */
		/* battery_full = !battery_charging && power_present; */

		charge_mAh += this_charge_mAh;
		max_charge_mAh += this_max_charge_mAh;
		voltage_mV += this_voltage_mV;
		discharge_mA += this_discharge_mA;
		/* rem_time_sec += this_rem_time_sec; */
	}
	show_pmu_power_line(voltage_mV, charge_mAh, max_charge_mAh,
	                    discharge_mA);
	return 1;
}

void print_battery_sysfs(void)
{
	DIR *dir;
	struct dirent *dirent;
	FILE *file;
	double rate = 0;
	double cap = 0;

	char filename[256];
	
	if (print_battery_proc_acpi())
		return;
	
	if (print_battery_proc_pmu())
		return;

	dir = opendir("/sys/class/power_supply");
	if (!dir) {
		return;
	}

	while ((dirent = readdir(dir))) {
		int dontcount = 0;
		double voltage = 0.0;
		double amperes_drawn = 0.0;
		double watts_drawn = 0.0;
		double watts_left = 0.0;
		char line[1024];

		if (strstr(dirent->d_name, "AC"))
			continue;

		sprintf(filename, "/sys/class/power_supply/%s/present", dirent->d_name);
		file = fopen(filename, "r");
		if (!file)
			continue;
		int s;
		if ((s = getc(file)) != EOF) {
			if (s == 0)
				break;
		}
		fclose(file);

		sprintf(filename, "/sys/class/power_supply/%s/status", dirent->d_name);
		file = fopen(filename, "r");
		if (!file)
			continue;
		memset(line, 0, 1024);
		if (fgets(line, 1024, file) != NULL) {
			if (!strstr(line, "Discharging"))
				dontcount = 1;
		}
		fclose(file);

		sprintf(filename, "/sys/class/power_supply/%s/voltage_now", dirent->d_name);
		file = fopen(filename, "r");
		if (!file)
			continue;
		memset(line, 0, 1024);
		if (fgets(line, 1024, file) != NULL) {
			voltage = strtoull(line, NULL, 10) / 1000000.0;
		}
		fclose(file);

		sprintf(filename, "/sys/class/power_supply/%s/energy_now", dirent->d_name);
		file = fopen(filename, "r");
		watts_left = 1;
		if (!file) {
			sprintf(filename, "/sys/class/power_supply/%s/charge_now", dirent->d_name);
			file = fopen(filename, "r");
			if (!file) 
				continue;

			/* W = A * V */
			watts_left = voltage;
		}
		memset(line, 0, 1024);
		if (fgets(line, 1024, file) != NULL) 
			watts_left *= strtoull(line, NULL, 10) / 1000000.0;
		fclose(file);

		sprintf(filename, "/sys/class/power_supply/%s/current_now", dirent->d_name);
		file = fopen(filename, "r");
		if (!file)
			continue;
		memset(line, 0, 1024);
		if (fgets(line, 1024, file) != NULL) {
			watts_drawn = strtoull(line, NULL, 10) / 1000000.0;
		}
		fclose(file);
	
		if (!dontcount) {
			rate += watts_drawn + voltage * amperes_drawn;
		}
		cap += watts_left;
		

	}
	closedir(dir);
	if (prev_bat_cap - cap < 0.001 && rate < 0.001)
		last_bat_time = 0;
	if (!last_bat_time) {
		last_bat_time = prev_bat_time = time(NULL);
		last_bat_cap = prev_bat_cap = cap;
	}
	if (time(NULL) - last_bat_time >= 400) {
		prev_bat_cap = last_bat_cap;
		prev_bat_time = last_bat_time;
		last_bat_time = time(NULL);
		last_bat_cap = cap;
	}

	show_acpi_power_line(rate, cap, prev_bat_cap - cap, time(NULL) - prev_bat_time);
}

char cstate_lines[12][200];

void usage()
{
	printf(_("Usage: powertop [OPTION...]\n"));
	printf(_("  -m, --mode            0 == TRAIN_ONLY, 1 == MONITOR_ONLY, 2 == DYNAMIC_ONLY\n"));
	printf(_("  -w, --whitefile       List of processes to ignore (monitor mode only)\n"));
	printf(_("  -b, --cbfile          List of confirmation callbacks (monitor mode only)\n"));
	printf(_("  -b, --logfile         Logfile (monitor mode only)\n"));
	printf(_("  -c, --cycles          Number of training cycles (training mode only)\n"));
	printf(_("  -f, --file            input file (monitor mode only)\n"));
	printf(_("  -t, --time=DOUBLE     default time to gather data in seconds\n"));
	printf(_("  -h, --help            Show this help message\n"));
	printf(_("  -v, --version         Show version information and exit\n"));
	exit(0);
}

void version()
{
	printf(_("BatCop version %s\n"), VERSION);
	exit(0);
}

// Heavily modified version of the PowerTOP main routine.
// Changes: All suggestion-code removed, all unnecessary
// monitors removed (and much more).
// Adds: More cmdline options, and calls initialisation
// routines for BatCop's training and monitor modes
int run_batcop(int argc, char **argv)
{
	char line[1024];
	FILE *file = NULL;
	uint64_t cur_usage[8], cur_duration[8];
	double wakeups_per_second = 0;
  char *tracefile = NULL;
  char *whitefile = NULL;
  char *cbfile = NULL;
  char *lfile = NULL;

	start_data_dirty_capture();


 	while (1) {
 		static struct option opts[] = {
 			{ "time", 1, NULL, 't' },
 			{ "pids", 0, NULL, 'p' },
 			{ "help", 0, NULL, 'h' },
 			{ "version", 0, NULL, 'v' },
 			{ "mode", 1, NULL, 'm' },
 			{ "file", 1, NULL, 'f' },
 			{ "cycles", 1, NULL, 'c' },
 			{ "whitefile", 1, NULL, 'w' },
 			{ "cbfile", 1, NULL, 'b' },
 			{ "logfile", 1, NULL, 'l' },
 			{ 0, 0, NULL, 0 }
 		};
 		int index2 = 0, c;
 		
 		c = getopt_long(argc, argv, "dt:phv", opts, &index2);
 		if (c == -1)
 			break;
 		switch (c) {
 		case 't':
 			ticktime = strtod(optarg, NULL);
 			break;
 		case 'p':
 			showpids = 1;
 			break;
 		case 'h':
 			usage();
 			break;
 		case 'v':
 			version();
 			break;
    case 'm':
      runmode = strtod (optarg, NULL);
      break;
    case 'f':
      tracefile = optarg;
      break;
    case 'c':
      training_cycles = strtod (optarg, NULL);
      break;
    case 'w':
      whitefile = optarg;
      break;
    case 'b':
      cbfile = optarg;
      break;
    case 'l':
      lfile = optarg;
      break;
 		default:
 			;
 		}
 	}

  if (lfile == NULL)
    {
      logfile = stderr;
    }
  else
    {
      const char *x = lfile;
      logfile = fopen ( x, "a");

      (void) signal(SIGINT,leave);

      if(logfile == NULL)
        {
          fprintf (logfile, "Error: Could not open logfile %s\n", lfile);
          exit (1);
        }
      struct timeval tv;
      gettimeofday (&tv, 0);
      fprintf (logfile, "%ld Running Batcop.\n", tv.tv_sec);
    }
  if (runmode == TRAIN_ONLY)
    {
      fprintf (logfile, "\nRunning in TRAIN_ONLY mode\n");
      if (tracefile != NULL)
        fprintf (logfile, "Input file will not be used\n");
    }
  else if (runmode == MONITOR_ONLY)
    {
      if (tracefile == NULL)
        {
          fprintf (logfile, "Error: Trace file required for MONITOR mode. Use --file to specify trace file.\n");
          exit (-1);
        }
      else if (access (tracefile, R_OK) != 0)
        {
          fprintf (logfile, "Error: Cannot access tracefile %s\n", tracefile);
          exit (-1);
        }
      fprintf (logfile, "\nRunning MONITOR_ONLY mode with trace file %s\n", tracefile);
      monitor_mode_init (tracefile, whitefile, cbfile);
    }
  else if (runmode == DYNAMIC)
    {
      fprintf (logfile, "\nRunning in DYNAMIC mode\n");
    }
  else
    {
      fprintf (logfile, "\nError: Mode not recognised\n");
      exit (-1);
    }

	system("/sbin/modprobe cpufreq_stats > /dev/null 2>&1");
	read_data(&start_usage[0], &start_duration[0]);


	memcpy(last_usage, start_usage, sizeof(last_usage));
	memcpy(last_duration, start_duration, sizeof(last_duration));

	do_proc_irq();
	do_proc_irq();

	memset(cur_usage, 0, sizeof(cur_usage));
	memset(cur_duration, 0, sizeof(cur_duration));
	if (geteuid() != 0)
		printf(_("BatCop needs to be run as root to collect enough information\n"));
	printf(_("Collecting data for %i seconds \n"), (int)ticktime);
	printf("\n\n");
	stop_timerstats();

	while (1) {
		double maxsleep = 0.0;
		int64_t totalticks;
		int64_t totalevents;
		fd_set rfds;
		struct timeval tv;
		int key = 0;

		int i = 0;
		double c0 = 0;
		char *c;


		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		tv.tv_sec = ticktime;
		tv.tv_usec = (ticktime - tv.tv_sec) * 1000000;
		do_proc_irq();
		start_timerstats();


		key = select(1, &rfds, NULL, NULL, &tv);

		if (key && tv.tv_sec) ticktime = ticktime - tv.tv_sec - tv.tv_usec/1000000.0;


		stop_timerstats();
		do_proc_irq();
		read_data(&cur_usage[0], &cur_duration[0]);

		totalticks = 0;
		totalevents = 0;
		for (i = 0; i < 8; i++)
			if (cur_usage[i]) {
				totalticks += cur_duration[i] - last_duration[i];
				totalevents += cur_usage[i] - last_usage[i];
			}

		memset(&cstate_lines, 0, sizeof(cstate_lines));
		topcstate = -4;

    if (totalevents == 0 && maxcstate <= 1) {
      sprintf(cstate_lines[5],_("< Detailed C-state information is not available.>\n"));
    } else {
      double sleept, percentage;
      c0 = sysconf(_SC_NPROCESSORS_ONLN) * ticktime * 1000 * FREQ - totalticks;
      if (c0 < 0)
        c0 = 0; /* rounding errors in measurement might make c0 go slightly negative.. this is confusing */
      sprintf(cstate_lines[0], _("Cn\t          Avg residency\n"));

      percentage = c0 * 100.0 / (sysconf(_SC_NPROCESSORS_ONLN) * ticktime * 1000 * FREQ);
      sprintf(cstate_lines[1], _("C0 (cpu running)        (%4.1f%%)\n"), percentage);
      if (percentage > 50)
        topcstate = 0;
      for (i = 0; i < 8; i++)
        if (cur_usage[i]) {
          sleept = (cur_duration[i] - last_duration[i]) / (cur_usage[i] - last_usage[i]
                      + 0.1) / FREQ;
          percentage = (cur_duration[i] -
                last_duration[i]) * 100 /
               (sysconf(_SC_NPROCESSORS_ONLN) * ticktime * 1000 * FREQ);

          if (cnames[i][0]==0)
            sprintf(cnames[i],"C%i",i+1);
          sprintf
              (cstate_lines[2+i], _("%s\t%5.1fms (%4.1f%%)\n"),
               cnames[i], sleept, percentage);
          if (maxsleep < sleept)
            maxsleep = sleept;
          if (percentage > 50)
            topcstate = i+1;

        }
    }

		/* now the timer_stats info */
		memset(line, 0, sizeof(line));
		totalticks = 0;
		file = NULL;
		if (!nostats)
			file = fopen("/proc/timer_stats", "r");
		while (file && !feof(file)) {
			char *count, *pid, *process, *func;
			char line2[1024];
			int cnt;
			int deferrable = 0;
			memset(line, 0, 1024);
			if (fgets(line, 1024, file) == NULL)
				break;
			if (strstr(line, "total events"))
				break;
			c = count = &line[0];
			c = strchr(c, ',');
			if (!c)
				continue;
			*c = 0;
			c++;
			while (*c != 0 && *c == ' ')
				c++;
			pid = c;
			c = strchr(c, ' ');
			if (!c)
				continue;
			*c = 0;
			c++;
			while (*c != 0 && *c == ' ')
				c++;
			process = c;
			c = strchr(c, ' ');
			if (!c)
				continue;
			*c = 0;
			c++;
			while (*c != 0 && *c == ' ')
				c++;
			func = c;

			if (strcmp(process, "swapper")==0 &&
			    strcmp(func, "hrtimer_start_range_ns (tick_sched_timer)\n")==0) {
				process = _("[kernel scheduler]");
				func = _("Load balancing tick");
			}
			if (strcmp(process, "insmod") == 0)
				process = _("[kernel module]");
			if (strcmp(process, "modprobe") == 0)
				process = _("[kernel module]");
			if (strcmp(process, "swapper") == 0)
				process = _("[kernel core]");
			c = strchr(c, '\n');
			if (strncmp(func, "tick_nohz_", 10) == 0)
				continue;
			if (strncmp(func, "tick_setup_sched_timer", 20) == 0)
				continue;
			if (strcmp(process, "batcop") == 0)
				continue;
			if (c)
				*c = 0;
			cnt = strtoull(count, &c, 10);
			while (*c != 0) {
				if (*c++ == 'D')
					deferrable = 1;
			}

			if (deferrable)
				continue;
			if (strchr(process, '['))
				sprintf(line2, "%s %s", process, func);
			else
				sprintf(line2, "%s", process);
			push_line_pid(line2, cnt, 0, pid);
		}

		if (file)
			pclose(file);

		parse_data_dirty_buffer();

		if (strstr(line, "total events")) {
			int d;
			d = strtoull(line, NULL, 10) / sysconf(_SC_NPROCESSORS_ONLN);
			if (totalevents == 0) { /* No c-state info available, use timerstats instead */
				totalevents = d * sysconf(_SC_NPROCESSORS_ONLN) + total_interrupt;
				if (d < interrupt_0)
					totalevents += interrupt_0 - d;
			}
			if (d>0 && d < interrupt_0)
				push_line(_("[extra timer interrupt]"), interrupt_0 - d);
		}

	
		if (totalevents && ticktime) {
			wakeups_per_second = totalevents * 1.0 / ticktime / sysconf(_SC_NPROCESSORS_ONLN);
			show_wakeups(wakeups_per_second, ticktime, c0 * 100.0 / (sysconf(_SC_NPROCESSORS_ONLN) * ticktime * 1000 * FREQ) );
		}
		count_lines();

		displaytime = displaytime - ticktime;

		compute_timerstats(nostats, ticktime);

		if (maxsleep < 5.0)
			ticktime = 10;
		else if (maxsleep < 30.0)
			ticktime = 15;
		else if (maxsleep < 100.0)
			ticktime = 20;
		else if (maxsleep < 400.0)
			ticktime = 30;
		else
			ticktime = 45;

		if (wakeups_per_second < 0)
			ticktime = 2;

		read_data(&cur_usage[0], &cur_duration[0]);
		memcpy(last_usage, cur_usage, sizeof(last_usage));
		memcpy(last_duration, cur_duration, sizeof(last_duration));

	}

	end_data_dirty_capture();
	return 0;
}

#ifdef __cplusplus
}
#endif // extern "C"

// Technically, this never returns.
int main (int argc, char **argv)
{
  run_batcop (argc, argv);
  return 0;
}

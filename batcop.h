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
 *  Lalith Suresh <suresh.lalith@gmail.com>
 */




#ifndef __INCLUDE_GUARD_BATCOP_H_
#define __INCLUDE_GUARD_BATCOP_H_

#include <libintl.h>
#include "ap.h"

#define VERSION "1.12"

#ifdef __cplusplus
extern "C" {
#endif

struct line {
	char	*string;
	int	count;
	int	disk_count;
	char 	pid[12];
};

typedef void (suggestion_func)(void);

extern struct line     *lines;  
extern int             linehead;
extern int             linesize;
extern int             linectotal;
extern int             runmode;
extern int             training_cycles;

extern double displaytime;

void monitor_mode_init(char *tracefile);
void training_mode_init();
void show_wakeups(double d, double interval, double C0time);

/* min definition borrowed from the Linux kernel */
#define min(x,y) ({ \
        typeof(x) _x = (x);     \
        typeof(y) _y = (y);     \
        (void) (&_x == &_y);            \
        _x < _y ? _x : _y; })

#define max(x,y) ({ \
        typeof(x) _x = (x);     \
        typeof(y) _y = (y);     \
        (void) (&_x == &_y);            \
        _x > _y ? _x : _y; })



#define _(STRING)    gettext(STRING)


#define PT_COLOR_DEFAULT    1
#define PT_COLOR_HEADER_BAR 2
#define PT_COLOR_ERROR      3
#define PT_COLOR_RED        4
#define PT_COLOR_YELLOW     5
#define PT_COLOR_GREEN      6
#define PT_COLOR_BRIGHT     7
#define PT_COLOR_BLUE	    8


/* Battery IDS defines */
#define TRAIN_ONLY 0
#define MONITOR_ONLY 1
#define DYNAMIC 2


void show_acpi_power_line(double rate, double cap, double capdelta, time_t time);
void show_pmu_power_line(unsigned sum_voltage_mV,
                         unsigned sum_charge_mAh, unsigned sum_max_charge_mAh,
                         int sum_discharge_mA);

void compute_timerstats(int nostats, int ticktime);

void push_line(char *string, int count);
void push_line_pid(char *string, int cpu_count, int disk_count, char *pid);


void start_data_dirty_capture(void);
void end_data_dirty_capture(void);
void parse_data_dirty_buffer(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

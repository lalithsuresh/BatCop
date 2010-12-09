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

#include <iostream>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include "cfm-callback.h"
#include "stdlib.h"
#include "batcop.h"

std::map <std::string, std::vector<std::string> > callbacks;

// Registers a confirmation callback for a
// particular process
void register_callback (std::string script, std::string process)
{
  callbacks[process].push_back(script);
}

// Called by compute_timerstats() (analysis.cc)
// upon detection of any suspicion.
void trigger_callbacks (char *process)
{
  int pid;
  std::string str_name (process);

  // Retrieve vector of callbacks for the concerned process
  std::vector<std::string> vect = callbacks[process];
  for (std::vector<std::string>::const_iterator k = vect.begin ();
        k != vect.end (); k++)
    {
      fprintf (logfile, "Executing callback [%s] for process [%s]\n", k->c_str(), process);
      int pid = fork ();

      if (pid == 0)
        {
          system (k->c_str());
          exit (0);
        }
    }
}

// Read from conf file to populate the callbacks
// map with <process, actions> tuples.
void read_confirmation_callback_conf (const char *conffile)
{

  std::ifstream infile (conffile);

  if (!infile.is_open ())
    {
      fprintf (logfile, "Error: cannot open %s\n", conffile);
    }

  while (infile.good ())
    {
      std::string tmp;
      infile >> tmp;

      char name[1023];
      infile.getline (name, 1023);

      std::string process (name);
      process.erase (0,1); // Remove the darn space
      register_callback (tmp, process);
    }
}

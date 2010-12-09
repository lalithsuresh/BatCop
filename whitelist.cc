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
#include <fstream>
#include <set>

#include "batcop.h"
#include "whitelist.h"

static std::set < std::string > whitelist;

// A wrapper around the whitelist std::set
int lookup_whitelist (const char *name)
{
  std::string str_name (name);
  if (whitelist.find (str_name) != whitelist.end ())
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

void read_whitelist (const char *conffile)
{
  std::ifstream whitefile (conffile);
  if (!whitefile.is_open ())
    {
      std::cerr << "Error: read_whitelist: Cannot open " << conffile << "\n";
      exit (1);
    }

  std::cout << "Populating whitelist from file: " << conffile << "\n";
  while (whitefile.good ())
    {
      char name[1023];
      whitefile.getline (name, 1023);
  
      // Convert to std::string because std::string
      // is more reliable for searching than char *
      std::string str_name (name);

      // Populate std::set whitelist
      whitelist.insert (str_name);
    }
}

/*
 *      Copyright (C) 2025 Jean-Luc Barriere
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "chipinfo.h"
#include <cstdio>

namespace K150
{

void CHIPInfo::dumplist(const std::string& datfile, const std::string& filter)
{
  FILE * file = fopen(datfile.c_str(), "r");
  if (file == nullptr)
  {
    fprintf(stderr, "Opening DAT file '%s' failed.\n", datfile.c_str());
    return;
  }
  std::string _filter = upperStr(filter);
  for (;;)
  {
    char buf[1024];
    size_t sz = 0;
    bool eof = false;
    bool eol = false;
    bool blank = true;
    // read a line
    do
    {
      int c = fgetc(file);
      if (c < 0)
        eof = true;
      else if (c == 0x0a)
        eol = true;
      else if (c >= 0x20 && c <= 0x7f)
      {
        if (!blank || c != 0x20)
        {
          blank = false;
          buf[sz++] = c;
        }
      }
    } while (!eof && !eol && sz < 1024);

    std::vector<std::string> var = tokenize(std::string(buf, sz), '=', '"', false);
    if (var.size() > 1)
    {
      std::string vn = upperStr(var[0]);
      // looking for var CHIPNAME
      if (vn == "CHIPNAME")
      {
        std::string chipname = upperStr(unwrap(var[1]));
        if (_filter.empty() || chipname.find(_filter) != std::string::npos)
          fprintf(stdout, "%s\n", chipname.c_str());
      }
    }

    if (eof)
      break;
  }

  fclose(file);
}

bool CHIPInfo::loaddata(const std::string& datfile, const std::string& chipname)
{
  FILE * file = fopen(datfile.c_str(), "r");
  if (file == nullptr)
  {
    fprintf(stderr, "Opening DAT file '%s' failed.\n", datfile.c_str());
    return false;
  }
  bool chipfound = false;
  m_info.valid = false;
  m_info.chip_name = upperStr(chipname);
  for (;;)
  {
    char buf[1024];
    size_t sz = 0;
    bool eof = false;
    bool eol = false;
    bool blank = true;
    // read a line
    do
    {
      int c = fgetc(file);
      if (c < 0)
        eof = true;
      else if (c == 0x0a)
        eol = true;
      else if (c >= 0x20 && c <= 0x7f)
      {
        if (!blank || c != 0x20)
        {
          blank = false;
          buf[sz++] = c;
        }
      }
    } while (!eof && !eol && sz < 1024);

    std::vector<std::string> tokens = tokenize(std::string(buf, sz), ' ', '"', true);

    if (tokens.size() == 0)
    {
      // blank line
      if (chipfound)
        break;
    }
    else if (tokens[0].compare(0, 4, "LIST") != 0)
    {
      std::vector<std::string> var = tokenize(std::string(buf, sz), '=', '"', false);
      if (var.size() > 1)
      {
        std::string vn = upperStr(var[0]);
        if (!chipfound)
        {
          // looking for var CHIPNAME
          if (vn == "CHIPNAME" && upperStr(unwrap(var[1])) == m_info.chip_name)
            chipfound = true;
        }
        else
        {
          if (m_debug)
            fprintf(stderr, ">>> CHIPINFO::%s=%s\n", vn.c_str(), var[1].c_str());
          // set infos
          if (vn == "CHIPID")
            m_info.chip_id = unwrap(var[1]);
          else if (vn == "SOCKETIMAGE")
            m_info.socket_image = upperStr(unwrap(var[1]));
          else if (vn == "ERASEMODE")
            m_info.erase_mode = atoi(unwrap(var[1]).c_str());
          else if (vn == "POWERSEQUENCE")
            m_info.power_sequence = upperStr(unwrap(var[1]));
          else if (vn == "PROGRAMDELAY")
            m_info.program_delay = atoi(unwrap(var[1]).c_str());
          else if (vn == "PROGRAMTRIES")
            m_info.program_tries = atoi(unwrap(var[1]).c_str());
          else if (vn == "OVERPROGRAM")
            m_info.over_program = atoi(unwrap(var[1]).c_str());
          else if (vn == "CORETYPE")
            m_info.core_type = upperStr(unwrap(var[1]));
          else if (vn == "ROMSIZE")
            m_info.rom_size = (int) std::stod(std::string("0x").append(unwrap(var[1])));
          else if (vn == "EEPROMSIZE")
            m_info.eeprom_size = (int) std::stod(std::string("0x").append(unwrap(var[1])));
          else if (vn == "FUSEBLANK")
          {
            m_info.fuse_blank.clear();
            for (std::string& m : tokenize(unwrap(var[1]), ' ', '\0', true))
              m_info.fuse_blank.push_back((int) std::stod(std::string("0x").append(m)));
          }
          else if (vn == "INCLUDE")
            m_info.include = (upperStr(unwrap(var[1])) == "Y");
          else if (vn == "FLASHCHIP")
            m_info.flash_chip = (upperStr(unwrap(var[1])) == "Y");
          else if (vn == "CPWARN")
            m_info.cp_warn = (upperStr(unwrap(var[1])) == "Y");
          else if (vn == "CALWORD")
            m_info.cal_word = (upperStr(unwrap(var[1])) == "Y");
          else if (vn == "BANDGAP")
            m_info.band_gap = (upperStr(unwrap(var[1])) == "Y");
          else if (vn == "ICSPONLY")
            m_info.icsp_only = (upperStr(unwrap(var[1])) == "Y");
        }
      }
      else if (chipfound)
      {
        fprintf(stderr, ">>> PARSE ERROR: %s\n", var[0].c_str());
      }
    }

    if (eof)
      break;
  }

  fclose(file);
  m_info.valid = chipfound;
  return chipfound;
}

}

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

#ifndef CHIPINFO_H
#define CHIPINFO_H

#include <string>
#include <vector>

namespace K150
{

class CHIPInfo
{
public:
  CHIPInfo() : m_debug(false) { }
  ~CHIPInfo() { }

  void setDebug(bool debug) { m_debug = debug; }

  struct FUSE
  {
    std::string group;
    std::string name;
    std::vector<std::pair<std::string, int> > values;
  };

  void dumplist(const std::string& datfile, const std::string& filter);

  struct CHIP
  {
    bool valid                  = false;
    std::string chip_name;
    std::string chip_id;
    std::string socket_image;
    int erase_mode              = 0;
    std::string power_sequence;
    int program_delay           = 0;
    int program_tries           = 0;
    int over_program            = 0;
    std::string core_type;
    int rom_size                = 0;
    int eeprom_size             = 0;
    std::vector<int> fuse_blank;
    bool include                = false;
    bool flash_chip             = false;
    bool cp_warn                = false;
    bool cal_word               = false;
    bool band_gap               = false;
    bool icsp_only              = false;
    std::vector<FUSE> fuses;
  };

  bool loaddata(const std::string& datfile, const std::string& chipname);

  const CHIP& data() const { return m_info; }

private:
  bool m_debug;
  CHIP m_info;
  
  std::string upperStr(const std::string& buf)
  {
    std::string out;
    for (size_t n = 0; n < buf.size(); ++n)
    {
      out.push_back(::toupper(buf[n]));
    }
    return out;
  }

  std::string unwrap(const std::string& buf)
  {
    size_t f = buf.find_first_of('"');
    if (f == std::string::npos)
      return buf;
    size_t l = buf.find_last_of('"');
    return buf.substr(f + 1, l - f);
  }

  std::vector<std::string> tokenize(const std::string& str, const char sep, const char enc, bool trimnull)
  {
    std::vector<std::string> tokens;
    size_t pos = 0;
    size_t end = str.size();
    if (pos == end)
      return tokens;
    bool encaps = false;
    std::string token;
    while (pos != end)
    {
      if (!encaps && sep && str[pos] == sep)
      {
        // trim null token
        if (!trimnull || !token.empty())
        {
          tokens.push_back(std::move(token));
          token.clear();
        }
        pos += 1;
      }
      else
      {
        token.push_back(str[pos]);
        if (str[pos] == enc)
          encaps = !encaps;
        pos += 1;
      }
    }
    if (!trimnull || !token.empty())
      tokens.push_back(std::move(token));
    return tokens;
  }

};

}

#endif /* CHIPINFO_H */


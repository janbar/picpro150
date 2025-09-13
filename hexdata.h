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

#ifndef HEXDATA_H
#define HEXDATA_H

#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>

namespace K150
{

class HexData
{
private:
  static int hex_to_num(const char * str, int sz);
  static void u8_to_hex(std::string& str, uint8_t u);
  static void logdata(FILE * out, const std::vector<uint8_t>& data);
  static std::vector<uint8_t> readline(FILE * file);
  std::string hexrecord(int& ext_addr, int addr, const std::vector<uint8_t>& data);

public:
  HexData() : m_debug(false) { }
  ~HexData() { }

  void setDebug(bool debug) { m_debug = debug; }

  enum Endianness { LE, BE };
  
  bool loadHEX(const std::string& path);
  bool saveHEX(const std::string& path);
  bool loadRAW(int addr, const std::vector<uint8_t>& data, bool swap_bytes);
  bool loadRAW_LE8(int addr, const std::vector<uint8_t>& data);

  std::vector<uint8_t> rangeOfData(int lower_bound, int word_count, int blank_word, bool swap_bytes);

  void dumpSegments();

private:
  bool m_debug;
  std::map<int, std::vector<uint8_t> > m_segments;
};

}


#endif /* HEXDATA_H */


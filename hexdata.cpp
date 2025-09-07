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

#include "hexdata.h"

#include <cassert>

#define HEX_OUT_ENABLE_EXT_SEGMENT_ADDRESS 0

namespace K150
{

int HexData::hex_to_num(const char * str, int sz)
{
  int val = 0;
  for (int i = 0; i < sz; ++i)
  {
    if (*str >= '0' && *str <= '9')
      val = (val << 4) + (*str - '0');
    else if (*str >= 'A' && *str <= 'F')
      val = (val << 4) + (*str - 'A' + 10);
    else if (*str >= 'a' && *str <= 'f')
      val = (val << 4) + (*str - 'a' + 10);
    else
      break;
    ++str;
  }
  return val;
}

void HexData::u8_to_hex(std::string& str, uint8_t u)
{
  static const char g[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
  };
  str.push_back(g[0xf & (u >> 4)]);
  str.push_back(g[(0xf & u)]);
}

void HexData::logdata(const std::vector<uint8_t>& data)
{
  unsigned idx = 0, lno = 0;
  size_t sz = data.size();
  while (idx < sz)
  {
    ++lno;
    char str[24];
    int i;
    for (i = 0; i < 16 && idx < sz; ++i, ++idx)
    {
      fprintf(stdout, "%02x ", (unsigned char) data[idx]);
      str[i] = (data[idx] > 32 && data[idx] < 127 ? data[idx] : '.');
    }
    str[i] = '\0';
    while (i++ < 16) fputs("   ", stdout);
    fputc(' ', stdout);
    fputs(str, stdout);
    fputc('\n', stdout);
  }
}

std::vector<uint8_t> HexData::readline(FILE * file)
{
  std::vector<uint8_t> buf;
  bool eof = false;
  bool eol = false;
  bool blank = true;
  // read a line
  do
  {
    char c = fgetc(file);
    if (c < 0)
      eof = true;
    else if (c == 0x0a)
      eol = true;
    else if (c >= 0x20 && c <= 0x7f)
    {
      if (!blank || c != 0x20)
      {
        blank = false;
        buf.push_back(c);
      }
    }
  } while (!eof && !eol);

  return buf;
}

bool HexData::loadHEX(const std::string& path, bool le /*= true*/)
{
  FILE * file = fopen(path.c_str(), "r");
  if (file == nullptr)
    return false;

  int lno = 0;
  bool eof = false;

  m_segments.clear();
  int ext_address = 0;

  for (;;)
  {
    char hex[4];
    std::vector<uint8_t> line = readline(file);
    int sum = 0;
    lno += 1;

    if (line.size() < 3 || line[0] != ':')
    {
      logdata(line);
      fprintf(stderr, "Invalid format at line %d.\n", lno);
      break;
    }

    hex[0] = line[1];
    hex[1] = line[2];
    int reclen = hex_to_num(hex, 2);
    sum += reclen;

    if (line.size() != (2 * (reclen + 5) + 1))
    {
      logdata(line);
      fprintf(stderr, "Record size is invalid at line %d.\n", lno);
      break;
    }

    hex[0] = line[3];
    hex[1] = line[4];
    hex[2] = line[5];
    hex[3] = line[6];
    int recaddr = hex_to_num(hex, 4);
    sum += (recaddr >> 8) & 0xff;
    sum += (recaddr) & 0xff;
    recaddr |= ext_address;

    hex[0] = line[7];
    hex[1] = line[8];
    int rectype = hex_to_num(hex, 2);
    sum += rectype;

    if (rectype == 0)
    {
      std::vector<uint8_t> data;
      for (int i = 0; i < (2 * reclen); i += 4)
      {
        hex[0] = line[i + 9];
        hex[1] = line[i + 10];
        int b1 = hex_to_num(hex, 2);
        sum += b1;
        hex[0] = line[i + 11];
        hex[1] = line[i + 12];
        int b2 = hex_to_num(hex, 2);
        sum += b2;

        if (le)
        {
          data.push_back(b2);
          data.push_back(b1);
        }
        else
        {
          data.push_back(b1);
          data.push_back(b2);
        }
      }
      m_segments.insert(std::pair<int, std::vector<uint8_t> >(recaddr, data));
    }
    else if (rectype == 1)
    {
      if (reclen == 0)
        eof = true;
      break;
    }
    else if (rectype == 2)
    {
      // address uses BE bytes order
      hex[0] = line[9];
      hex[1] = line[10];
      hex[2] = line[11];
      hex[3] = line[12];
      int shift = hex_to_num(hex, 4);
      sum += (shift >> 8) & 0xff;
      sum += (shift) & 0xff;
      ext_address = shift << 4;
    }
    else if (rectype == 4)
    {
      // address uses BE bytes order
      hex[0] = line[9];
      hex[1] = line[10];
      hex[2] = line[11];
      hex[3] = line[12];
      int shift = hex_to_num(hex, 4);
      sum += (shift >> 8) & 0xff;
      sum += (shift) & 0xff;
      ext_address = shift << 16;
    }
    else
    {
      // not implemented
      logdata(line);
      fprintf(stderr, "Record type %d is not supported.\n", rectype);
      break;
    }

    hex[0] = line[line.size()-2];
    hex[1] = line[line.size()-1];
    int crc = hex_to_num(hex, 2);
    if (crc != ((~sum + 1) &0xff))
    {
      logdata(line);
      fprintf(stderr, "Bad CRC for record at line %d\n", lno);
      break;
    }
  }

  fclose(file);

  if (!eof)
    return false;

  if (m_debug)
  {
    for (std::map<int, std::vector<uint8_t> >::iterator it = m_segments.begin(); it != m_segments.end(); ++it)
    {
      fprintf(stdout, ">>> %04X : ", it->first);
      logdata(it->second);
    }
  }
  return true;
}

std::string HexData::hexrecord(int& ext_addr, int addr, const std::vector<uint8_t>& data, bool le /*= true*/)
{
  int sum = 0;
  uint8_t b = 0;
  std::string record;

  // handle address extension
  int ext = (addr >> 16) & 0xffff;
  if (ext != ext_addr)
  {
    if (ext <= 0xf && 1 == HEX_OUT_ENABLE_EXT_SEGMENT_ADDRESS)
    {
      record.append(":02000002");
      sum += 4;
      b = (addr >> 12) & 0xff;
      u8_to_hex(record, b);
      sum += b;
      b = (addr >> 4) & 0xff;
      u8_to_hex(record, b);
      sum += b;
      ext_addr = (addr >> 4) & 0xffff;
    }
    else
    {
      record.append(":02000004");
      sum += 6;
      b = (ext >> 8) & 0xff;
      u8_to_hex(record, b);
      sum += b;
      b = ext & 0xff;
      u8_to_hex(record, b);
      sum += b;
      ext_addr = ext << 16;
    }
    // CRC
    b = (~sum + 1) &0xff;
    u8_to_hex(record, b);
    sum = 0;
    record.push_back('\n');
  }

  record.push_back(':');
  // count words
  b = data.size();
  u8_to_hex(record, b);
  sum += b;
  // address BE
  b = (addr >> 8) & 0xff;
  u8_to_hex(record, b);
  sum += b;
  b = addr & 0xff;
  u8_to_hex(record, b);
  sum += b;
  // type
  b = 0;
  u8_to_hex(record, b);
  sum += b;
  // data
  for (int i = 0; i < data.size(); i += 2)
  {
    if (le)
    {
      u8_to_hex(record, data[i + 1]);
      u8_to_hex(record, data[i]);
    }
    else
    {
      u8_to_hex(record, data[i]);
      u8_to_hex(record, data[i + 1]);
    }
    sum += data[i];
    sum += data[i + 1];
  }
  // CRC
  b = (~sum + 1) &0xff;
  u8_to_hex(record, b);

  record.push_back('\n');
  return record;
}

bool HexData::saveHEX(const std::string& path, bool le /*= true*/)
{
  FILE * file = fopen(path.c_str(), "w");
  if (file == nullptr)
    return false;

  int ext_addr = 0;

  std::map<int, std::vector<uint8_t> >::iterator it = m_segments.begin();
  while (it != m_segments.end())
  {
    int addr = it->first;
    std::vector<uint8_t>::iterator ptr = it->second.begin();
    while (ptr != it->second.end())
    {
      size_t d = std::distance(ptr, it->second.end());
      if (d > 16)
        d = 16;
      std::string rec = hexrecord(ext_addr, addr, std::vector<uint8_t>(ptr, ptr + d), le);
      fputs(rec.c_str(), file);
      ptr += d;
      addr +=d;
    }
    ++it;
  }
  fputs(":00000001FF\n", file);
  fflush(file);
  fclose(file);
  return true;
}

bool HexData::loadRAW(int addr, const std::vector<uint8_t>& data)
{
  if ((data.size() % 2) != 0)
    return false;
  for (auto& e : m_segments)
    if ((addr >= e.first && addr < (e.first + e.second.size())) ||
            ((addr + data.size()) > e.first && (addr + data.size()) < (e.first + e.second.size())))
      return false;
  m_segments.insert(std::pair<int, std::vector<uint8_t> >(addr, data));
  return true;
}

std::vector<uint8_t> HexData::rangeOfData(int lower_bound, int word_count, int blank_word)
{
  assert((lower_bound % 2) == 0);

  std::vector<uint8_t> data;
  int upper_bound = lower_bound + 2 * word_count;
  int addr = lower_bound;
  uint8_t blank_msb = (blank_word >> 8) & 0xff;
  uint8_t blank_lsb = blank_word & 0xff;

  std::map<int, std::vector<uint8_t> >::iterator it = m_segments.lower_bound(addr);
  // if not aligned then start from the previous segment
  if (it != m_segments.end() && it != m_segments.begin() && it->first > addr)
    --it;

  for (;;)
  {
    // fill empty data until upper bound
    if (it == m_segments.end() || it->first >= upper_bound)
    {
      while (addr < upper_bound)
      {
        data.push_back(blank_msb);
        data.push_back(blank_lsb);
        addr += 2;
      }
      break;
    }
    if (it->first + it->second.size() > addr)
    {
      // fill empty data until first
      while (addr < it->first)
      {
        data.push_back(blank_msb);
        data.push_back(blank_lsb);
        addr += 2;
      }
      int shift = 0;
      while ((addr - shift) > it->first)
        shift += 2;
      while (shift < it->second.size() && addr < upper_bound)
      {
        data.push_back(it->second[shift++]);
        data.push_back(it->second[shift++]);
        addr += 2;
      }
    }
    if (addr == upper_bound)
      break;
    ++it;
  }
  return data;
}

void HexData::dumpSegments()
{
  for (auto& e : m_segments)
  {
    fprintf(stdout, "%06X : ", e.first);
    logdata(e.second);
  }
}

}

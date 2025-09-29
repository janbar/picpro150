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

#include "k150.h"

#include <cstdio>
#include <cstring>
#include <cassert>

#include <sys/ioctl.h> //ioctl() call defenitions
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

namespace K150
{

typedef struct
{
  const char * name;  // core type name
  int value;          // core type code
  int bits;           // bits wide instruction
  int rom_base;       // rom base address
  int eeprom_base;    // eeprom base address
  int config_base;    // config base address
} CORE_TYPE;
static CORE_TYPE CORE_TYPE_LIST[] = {
  { "BIT16_C",      0,    16,   0x000000,   0xf00000,   0x300000, },
  { "BIT16_A",      1,    16,   0x000000,   0xf00000,   0x300000, },
  { "BIT16_B",      2,    16,   0x000000,   0xf00000,   0x300000, },
  { "BIT14_G",      3,    14,   0x000000,   0x004200,   0x00400e, },
  { "BIT12_A",      4,    12,   0x000000,   0x004200,   0x00400e, },
  { "BIT14_A",      5,    14,   0x000000,   0x004200,   0x00400e, },
  { "BIT14_B",      6,    14,   0x000000,   0x004200,   0x00400e, },
  { "BIT14_C",      7,    14,   0x000000,   0x004200,   0x00400e, },
  { "BIT12_B",      8,    14,   0x000000,   0x004200,   0x00400e, },
  { "BIT14_E",      9,    14,   0x000000,   0x004200,   0x00400e, },
  { "BIT14_F",      10,   14,   0x000000,   0x004200,   0x00400e, },
  { "BIT12_C",      11,   12,   0x000000,   0x004200,   0x001ffe, },
  { nullptr, 0, 0, 0, 0, 0, }
};

typedef struct
{
  const char * name;  // power sequence name
  int value;          // power sequence code
  bool delay;         // vcc/vpp delay
} POWER_SEQUENCE;
static POWER_SEQUENCE POWER_SEQUENCE_LIST[] = {
  { "VCC",          0,    false, },
  { "VCCVPP1",      1,    false, },
  { "VCCVPP2",      2,    false, },
  { "VPP1VCC",      3,    false, },
  { "VPP2VCC",      4,    false, },
  { "VCCFASTVPP1",  1,    true, },
  { "VCCFASTVPP2",  2,    true, },
  { nullptr, 0, false }
};

typedef struct
{
  const char * name;  // dip name
  const char * value; // position pin 1
} SOCKET_HINT;
static SOCKET_HINT SOCKET_HINT_LIST[] = {
  { "0PIN",         "" },
  { "8PIN",         "socket pin 13", },
  { "14PIN",        "socket pin 13", },
  { "18PIN",        "socket pin 2", },
  { "28NPIN",       "socket pin 1", },
  { "40PIN",        "socket pin 1", },
  { nullptr, nullptr }
};

static inline void show_progress(FILE * out, unsigned current, unsigned total)
{
  static int c = 0;
  static const char PROGRESS[4] = { '|', '/', '-', '\\', };
  if ((current == 0 || (c % 10) == 0) && total != 0)
    fprintf(stderr, "%c  %u0%%\r", PROGRESS[(c / 10) % 4], (10 * current / total));
  c += 1;
}

static inline void clear_progress()
{
  fputs("       \r", stderr);
}


void Programmer::logbuffer(FILE * out)
{
  unsigned idx = 0, lno = 0;
  size_t sz = m_buffer.size();
  while (idx < sz)
  {
    ++lno;
    fprintf(out, "%08X:  ", idx);
    char str[24];
    int i;
    for (i = 0; i < 16 && idx < sz; ++i, ++idx)
    {
      fprintf(out, "%02x ", (unsigned char) m_buffer[idx]);
      str[i] = (m_buffer[idx] > 32 && m_buffer[idx] < 127 ? m_buffer[idx] : '.');
    }
    str[i] = '\0';
    while (i++ < 16) fputs("   ", out);
    fputc(' ', out);
    fputs(str, out);
    fputc('\n', out);
  }
}

Programmer::~Programmer()
{
  if (m_port != nullptr)
    m_port->close();
  m_port = nullptr;
}

bool Programmer::connect(COMPort * port)
{
  disconnect();
  m_port = port;
  if (m_port == nullptr)
    return false;
  m_port->open();
  if (!m_port->isopen())
    return false;

  m_port->reset();
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 2)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'B')
    return false;
  m_version = m_buffer[1];

  if (!commandStart())
    return false;

  std::vector<uint8_t> cmd = { 21 };
  m_port->writeData(cmd);
  try
  {
    m_buffer.clear();
    int attempt = 0;
    while (m_buffer.size() < 4 && attempt++ < 10)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  m_protocol.clear();
  for (unsigned char c : m_buffer)
    m_protocol.push_back(static_cast<char>(c));
  if (m_protocol != "P18A")
  {
    fprintf(stderr, "Unsupported protocol (%s).\n", m_protocol.c_str());
    return false;
  }

  commandEnd();

  fprintf(stderr, "Programmer %s speaks protocol %s.\n",
          getVersionName().c_str(), getProtocol().c_str());
  return true;
}

void Programmer::disconnect()
{
  if (m_port != nullptr)
    m_port->close();
}

std::string Programmer::getVersionName()
{
  switch (m_version)
  {
  case 0: return "K128";
  case 1: return "K149-A";
  case 2: return "K149-B";
  case 3: return "K150";
  default:
    return "";
  }
}

bool Programmer::configure(const CHIPInfo& info)
{
  fprintf(stderr, "Load setup for chip %s ... ", info.data().chip_name.c_str());
  fflush(stderr);

  m_props = Properties();

  if (!info.data().icsp_only)
  {
    SOCKET_HINT * ptr = SOCKET_HINT_LIST;
    for (;;)
    {
      if (info.data().socket_image.compare(ptr->name) == 0)
      {
        m_props.socket_hint.assign(ptr->value);
        break;
      }
      if ((++ptr)->name == nullptr)
        break;
    }
  }

  {
    CORE_TYPE * ptr = CORE_TYPE_LIST;
    for (;;)
    {
      if (info.data().core_type.compare(ptr->name) == 0)
      {
        m_props.core_type = ptr->value;
        m_props.core_bits = ptr->bits;
        // enable flag for type bit16_a
        m_props.flag_18f_single_panel_access_mode = (ptr->value == 1);

        m_props.rom_base = ptr->rom_base;
        m_props.eeprom_base = ptr->eeprom_base;
        m_props.config_base = ptr->config_base;
        break;
      }
      if ((++ptr)->name == nullptr)
      {
        fprintf(stderr, "Unsupported CORE TYPE (%s).\n", info.data().core_type.c_str());
        return false;
      }
    }
  }

  {
    POWER_SEQUENCE * ptr = POWER_SEQUENCE_LIST;
    for (;;)
    {
      if (info.data().power_sequence.compare(ptr->name) == 0)
      {
        m_props.power_sequence = ptr->value;
        m_props.flag_vcc_vpp_delay = ptr->delay;
        break;
      }
      if ((++ptr)->name == nullptr)
      {
        fprintf(stderr, "Unsupported POWER SEQUENCE (%s).\n", info.data().power_sequence.c_str());
        return false;
      }
    }
  }

  m_props.rom_size = info.data().rom_size;
  m_props.rom_blank = ~(0xFFFF << m_props.core_bits) & 0xFFFF;
  m_props.eeprom_size = info.data().eeprom_size;
  m_props.program_delay = info.data().program_delay;
  m_props.program_tries = info.data().program_tries;
  m_props.erase_mode = info.data().erase_mode;
  m_props.panel_sizing = info.data().panel_sizing;
  m_props.fuse_blank = info.data().fuse_blank;
  m_props.flag_flash_chip = info.data().flash_chip;
  m_props.flag_calibration_value_in_rom = info.data().cal_word;
  m_props.flag_band_gap_fuse = info.data().band_gap;

  fprintf(stderr, "OK\n");
  return true;
}

bool Programmer::commandStart()
{
  std::vector<uint8_t> msg;
  msg = { 1 }; // RETURNS
  m_port->writeData(msg);
  try
  {
    for (;;)
    {
      m_buffer.clear();
      while (m_buffer.size() < 1)
        m_port->readData(m_buffer);
      if (m_debug)
        logbuffer(stderr);
      if (m_buffer[0] == 'Q')
        break;
    }
  }
  catch (...)
  {
    return false;
  }

  // go to jump table
  msg = { 'P' };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);
  // check for acknowledgement
  if (m_buffer[0] != 'P')
  {
    fprintf(stderr, "No acknowledgement for command start.\n");
    return false;
  }

  m_buffer.clear();
  return true;
}

bool Programmer::commandEnd()
{
  std::vector<uint8_t> msg;
  msg = { 1 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'Q')
  {
    fprintf(stderr, "Unexpected response (%u) in command end.", m_buffer[0]);
    return false;
  }
  return true;
}

bool Programmer::waitUntilChipInSocket()
{
  if (m_props.socket_hint.empty())
    return true;
  fprintf(stderr, "Waiting for user to insert chip into socket with pin 1 at %s ... ", m_props.socket_hint.c_str());
  fflush(stderr);

  if (!commandStart())
    return false;

  std::vector<uint8_t> msg = { 18 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 2)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'A')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  bool ok = false;

  if (m_buffer[1] == 'Y')
  {
    fprintf(stderr, "OK\n");
    ok = true;
  }

  commandEnd();

  return ok;
}

bool Programmer::waitUntilChipOutOfSocket()
{
  if (m_props.socket_hint.empty())
    return true;

  if (!commandStart())
    return false;

  fprintf(stderr, "Waiting until chip out socket ... ");
  fflush(stderr);
  std::vector<uint8_t> msg = { 19 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 2)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'A')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  bool ok = false;

  if (m_buffer[1] == 'Y')
  {
    fprintf(stderr, "OK\n");
    ok = true;
  }

  commandEnd();

  return ok;
}

bool Programmer::initializeProgrammingVariables(bool icsp_mode /*= false*/)
{
  fprintf(stderr, "Initialize programming interface ... ");
  fflush(stderr);

  std::vector<uint8_t> msg = { 3 };
  msg.push_back((m_props.rom_size & 0xff00) >> 8);
  msg.push_back((m_props.rom_size & 0x00ff));
  msg.push_back((m_props.eeprom_size & 0xff00) >> 8);
  msg.push_back((m_props.eeprom_size & 0x00ff));
  msg.push_back(m_props.core_type);
  int flags = 0;
  if (m_props.flag_calibration_value_in_rom)
    flags += 1;
  if (m_props.flag_band_gap_fuse)
    flags += 2;
  if (m_props.flag_18f_single_panel_access_mode)
    flags += 4;
  if (m_props.flag_vcc_vpp_delay)
    flags += 8;
  msg.push_back(flags);
  msg.push_back(m_props.program_delay);

  if (!icsp_mode)
    msg.push_back(m_props.power_sequence);
  else
  {
    switch (m_props.power_sequence)
    {
    case 2:
      msg.push_back(1);
      break;
    case 4:
      msg.push_back(3);
      break;
    default:
      msg.push_back(m_props.power_sequence);
    }
  }
  msg.push_back(m_props.erase_mode);
  msg.push_back(m_props.program_tries);
  msg.push_back(m_props.panel_sizing);

  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'I')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  fprintf(stderr, "OK\n");
  return true;
}

bool Programmer::setProgrammingVoltages(bool onoff)
{
  m_buffer.clear();
  std::vector<uint8_t> msg;
  if (onoff)
    msg.push_back(4);
  else
    msg.push_back(5);
    m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if ((onoff && m_buffer[0] != 'V') || (!onoff && m_buffer[0] != 'v'))
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  // save state
  m_VPPEnabled = onoff;

  return true;
}

bool Programmer::cycleProgrammingVoltages()
{
  std::vector<uint8_t> msg = { 6 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'V')
  {
    // reset state
    commandEnd();
    m_VPPEnabled = false;
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  // save state
  m_VPPEnabled = true;

  return true;
}

bool Programmer::programROM(const std::vector<uint8_t>& data)
{
  assert(m_VPPEnabled == true);

  int wsz = data.size() / 2;
  if (wsz > m_props.rom_size || ((wsz * 2) % 32) != 0)
  {
    fprintf(stderr, "Invalid ROM size (%d).\n", wsz);
    return false;
  }

  std::vector<uint8_t> msg = { 7 };
  msg.push_back((wsz & 0xff00) >> 8);
  msg.push_back((wsz & 0x00ff));
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'Y')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  for (int v = 0; v < data.size(); v += 32)
  {
    msg.clear();
    for (int i = 0; i < 32; ++i)
      msg.push_back(data[v + i]);
    m_port->writeData(msg);
    try
    {
      m_buffer.clear();
      while (m_buffer.size() < 1)
        m_port->readData(m_buffer);
    }
    catch (...)
    {
      return false;
    }

    if (m_debug)
      logbuffer(stderr);

    if (m_buffer[0] != 'Y')
    {
      fprintf(stderr, "\nCommand failed.\n");
      return false;
    }

    show_progress(stderr, v, data.size());
  }

  clear_progress();

  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'P')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  return true;
}

bool Programmer::programEEPROM(const std::vector<uint8_t>& data)
{
  assert(m_VPPEnabled == true);

  if (data.size() > m_props.eeprom_size || (data.size() % 2) != 0)
  {
    fprintf(stderr, "Invalid EEPROM size (%d).\n", (int) data.size());
    return false;
  }

  std::vector<uint8_t> msg = { 8 };
  msg.push_back((data.size() & 0xff00) >> 8);
  msg.push_back((data.size() & 0x00ff));
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'Y')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  for (int v = 0; v < data.size(); v += 2)
  {
    msg.clear();
    msg.push_back(data[v]);
    msg.push_back(data[v + 1]);
    m_port->writeData(msg);
    try
    {
      m_buffer.clear();
      while (m_buffer.size() < 1)
        m_port->readData(m_buffer);
    }
    catch (...)
    {
      return false;
    }

    if (m_debug)
      logbuffer(stderr);

    if (m_buffer[0] != 'Y')
    {
      fprintf(stderr, "\nCommand failed.\n");
      return false;
    }

    show_progress(stderr, v, data.size());
  }

  clear_progress();

  msg.clear();
  msg.push_back(0);
  msg.push_back(0);
  m_port->writeData(msg);

  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'P')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  return true;
}

bool Programmer::programCONFIG(const std::vector<uint8_t>& id, const std::vector<int>& fuses)
{
  assert(m_VPPEnabled == true);

  std::vector<uint8_t> msg = { 9 };

  std::vector<uint8_t> id_data = id;
  switch (m_props.core_bits)
  {
  case 16:
    id_data.resize(8, 0);
    if (fuses.size() != 7)
    {
      fprintf(stderr, "Should have 7 fuses for %d bit core.\n", m_props.core_bits);
      return false;
    }
    msg.push_back('0');
    msg.push_back('0');
    for (int i = 0; i < 8; ++i)
      msg.push_back(id_data[i]);
    for (int i = 0; i < fuses.size(); ++i)
    {
      msg.push_back((fuses[i] & 0x00ff));
      msg.push_back((fuses[i] & 0xff00) >> 8);
    }
    break;

  default:
    id_data.resize(4, 0);
    // 16f88 is 14bit yet has two fuses
    if (fuses.empty() || fuses.size() > 2)
    {
      fprintf(stderr, "Should have one or two fuses for %d bit core.\n", m_props.core_bits);
      return false;
    }
    msg.push_back('0');
    msg.push_back('0');
    for (int i = 0; i < 4; ++i)
      msg.push_back(id_data[i]);
    msg.push_back('F');
    msg.push_back('F');
    msg.push_back('F');
    msg.push_back('F');
    msg.push_back((fuses[0] & 0x00ff));
    msg.push_back((fuses[0] & 0xff00) >> 8);
    for (int i = 0; i < 12; ++i)
      msg.push_back(0xff);
    break;
  }

  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'Y')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  return true;
}

bool Programmer::programCOMMIT_18FXXXX_FUSE()
{
  assert(m_VPPEnabled == true);

  if (m_props.core_bits != 16)
    return true;
  // core 16 bits (PIC18F) requires additional operations

  std::vector<uint8_t> msg = { 17 }; // PROGRAM 18FXXXX FUSE
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'Y')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  return true;
}

bool Programmer::programCalibration(int cal, int fuse)
{
  assert(m_VPPEnabled == true);

  std::vector<uint8_t> msg = { 10 };
  msg.push_back((cal & 0xff00) >> 8);
  msg.push_back((cal & 0x00ff));
  msg.push_back((fuse & 0xff00) >> 8);
  msg.push_back((fuse & 0x00ff));

  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'Y')
  {
    if (m_buffer[0] == 'C')
      fprintf(stderr, "Calibration failed.\n");
    else if (m_buffer[0] == 'F')
      fprintf(stderr, "Fuse failed.\n");
    else
      fprintf(stderr, "Command failed.\n");
    return false;
  }

  return true;
}

bool Programmer::eraseChip()
{
  assert(m_VPPEnabled == true);

  std::vector<uint8_t> msg = { 14 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'Y')
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  return true;
}

bool Programmer::isBlankROM()
{
  std::vector<uint8_t> msg = { 15 };
  msg.push_back((m_props.rom_blank >> 8) & 0xff);
  m_port->writeData(msg);

  try
  {
    for (;;)
    {
      fputc('.', stderr);
      fflush(stderr);
      m_buffer.clear();
      while (m_buffer.size() < 1)
        m_port->readData(m_buffer);
      if (m_buffer[0] != 'B')
        break;
    }
  }
  catch (...)
  {
    return false;
  }
  fputc('\n', stderr);
  fflush(stderr);

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] == 'Y')
    return true;
  else if (m_buffer[0] == 'N')
    return false;
  else if (m_buffer[0] == 'C')
    return false;

  fprintf(stderr, "Command failed.\n");
  return false;
}

bool Programmer::isBlankEEPROM()
{
  std::vector<uint8_t> msg = { 16 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] == 'Y')
    return true;
  else if (m_buffer[0] == 'N')
    return false;

  fprintf(stderr, "Command failed.\n");
  return false;
}

bool Programmer::readCONFIG(std::vector<int>& fuses)
{
  assert(m_VPPEnabled == true);

  std::vector<uint8_t> msg = { 13 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 1)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer[0] != 'C')
  {
    setProgrammingVoltages(false);
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  try
  {
    m_buffer.clear();
    while (m_buffer.size() < 26)
      m_port->readData(m_buffer);
  }
  catch (...)
  {
    return false;
  }

  if (m_debug)
    logbuffer(stderr);

  fprintf(stderr, "Chip ID: %02X%02X\n", m_buffer[1], m_buffer[0]);
  fprintf(stderr, "IDs    : %02X %02X %02X %02X %02X %02X %02X %02X\n",
          m_buffer[2], m_buffer[3], m_buffer[4], m_buffer[5],
          m_buffer[6], m_buffer[7], m_buffer[8], m_buffer[9]);
  if (m_props.flag_calibration_value_in_rom)
    fprintf(stderr, "Cal    : %02X%02X\n", m_buffer[25], m_buffer[24]);
  fprintf(stderr, "Fuses  :");
  for (int i = 0; i < m_props.fuse_blank.size(); i += 2)
  {
    fprintf(stderr, " %02X%02X", m_buffer[i + 11], m_buffer[i + 10]);
    fuses.push_back(m_buffer[i + 10] | (m_buffer[i + 11] << 8));
  }
  fputc('\n', stderr);

  return true;
}

bool Programmer::readROM(std::vector<uint8_t>& data)
{
  assert(m_VPPEnabled == true);

  int ds = m_props.rom_size * 2; // words to bytes

  std::vector<uint8_t> msg = { 11 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < ds)
    {
      m_port->readData(m_buffer);
      show_progress(stderr, m_buffer.size(), ds);
    }
  }
  catch (...)
  {
    return false;
  }

  clear_progress();

  if (m_debug)
    logbuffer(stderr);

  if (m_buffer.size() != ds)
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  data = m_buffer;

  return true;
}

bool Programmer::readEEPROM(std::vector<uint8_t>& data)
{
  assert(m_VPPEnabled == true);

  std::vector<uint8_t> msg = { 12 };
  m_port->writeData(msg);
  try
  {
    m_buffer.clear();
    while (m_buffer.size() < m_props.eeprom_size)
    {
      m_port->readData(m_buffer);
      show_progress(stderr, m_buffer.size(), m_props.eeprom_size);
    }
  }
  catch (...)
  {
    return false;
  }

  clear_progress();

  if (m_debug)
    logbuffer(stderr);

  data = m_buffer;

  if (m_buffer.size() != m_props.eeprom_size)
  {
    fprintf(stderr, "Command failed.\n");
    return false;
  }

  data = m_buffer;

  return true;
}

}

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

#ifndef K150_H
#define K150_H

#include "chipinfo.h"

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>

namespace K150
{

static constexpr uint8_t terminator    = 0x0a;

class COMPort
{
public:
  virtual void writeData(const std::vector<uint8_t>& data) = 0;
  virtual void readData(std::vector<uint8_t>& data) = 0;
  virtual void open() = 0;
  virtual void close() = 0;
  virtual bool isopen() = 0;
  virtual void reset() = 0;
};

class Callback
{
public:
  virtual void status(bool connected) = 0;
};

class Programmer
{
private:
  void logbuffer();

public:
  Programmer() { }

  virtual ~Programmer();

  struct Properties
  {
    std::string socket_hint;
    int rom_base                            = 0;
    int rom_size                            = 0;
    int rom_blank                           = 0;
    int eeprom_base                         = 0;
    int eeprom_size                         = 0;
    int core_type                           = 0;
    int core_bits                           = 0;
    int program_delay                       = 0;
    int power_sequence                      = 0;
    int erase_mode                          = 0;
    int program_tries                       = 0;
    int over_program                        = 0;
    int config_base                         = 0;
    std::vector<int> fuse_blank;
    bool flag_calibration_value_in_rom      = false;
    bool flag_band_gap_fuse                 = false;
    bool flag_18f_single_panel_access_mode  = false;
    bool flag_vcc_vpp_delay                 = false;
    bool flag_flash_chip                    = false;
  };

  void setDebug(bool on) { m_debug = on; }

  enum Mode { mode_recv, mode_tran, mode_both };

  bool connect(COMPort * port);
  void disconnect();

  int getVersion() { return m_version; }
  std::string getProtocol() { return m_protocol; }
  std::string getVersionName();

  bool configure(const CHIPInfo& info);
  const Properties& properties() { return m_props; }

  bool commandStart();
  bool commandEnd();

  bool waitUntilChipInSocket();
  bool waitUntilChipOutOfSocket();

  bool initializeProgrammingVariables(bool icsp_mode = false);
  bool setProgrammingVoltages(bool onoff);
  bool cycleProgrammingVoltages();
  bool programROM(const std::vector<uint8_t>& data);
  bool programEEPROM(const std::vector<uint8_t>& data);
  bool programCONFIG(const std::vector<uint8_t>& id, const std::vector<int>& fuses);
  bool programCOMMIT_18FXXXX_FUSE();
  bool programCalibration(int cal, int fuse);
  bool eraseChip();

  bool isBlankROM();
  bool isBlankEEPROM();

  bool readCONFIG();
  bool readROM(std::vector<uint8_t>& data);
  bool readEEPROM(std::vector<uint8_t>& data);
  
private:
  COMPort * m_port = nullptr;
  std::vector<uint8_t> m_buffer;
  bool m_debug = false;
  uint8_t m_version;
  std::string m_protocol;
  Properties m_props;
};

} // namespace K150

#endif /* K150_H */


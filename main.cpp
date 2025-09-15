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

#include <cstdlib>
#include <unistd.h>

#include "serialport/serialport.h"
#include "k150.h"
#include "chipinfo.h"
#include "hexdata.h"
#include "usage.h"

#ifdef VERSION_STRING
#define PP150_VERSION VERSION_STRING
#else
#define PP150_VERSION "UNDEFINED"
#endif
#define PP150_HEADER  "PIC Programmer version " PP150_VERSION " compiled on " __DATE__ " at " __TIME__

//
// declarations
//
static const char B16_CHARS[] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'a', 'b', 'c', 'd', 'e', 'f',
  'A', 'B', 'C', 'D', 'E', 'F',
  0
};

std::string dirname(
        const std::string& filepath
);

void logdata(
        FILE * out,
        const std::vector<uint8_t>& data
);

bool load_chip_info(
        K150::CHIPInfo& info,
        const std::string& datpath,
        const std::string& chipname
);

bool read_pic(
        K150::Programmer& programmer,
        bool icsp_mode,
        bool program_rom,
        bool program_eeprom,
        bool program_config,
        const std::string& outhex
);

bool erase_pic(
        K150::Programmer& programmer,
        bool icsp_mode
);

bool program_pic(
        K150::Programmer& programmer,
        K150::HexData& hex,
        const std::vector<uint8_t>& ID,
        bool icsp_mode,
        bool program,
        bool program_rom,
        bool program_eeprom,
        bool program_config
);

bool verify_pic(
        K150::Programmer& programmer,
        K150::HexData& hex,
        bool icsp_mode,
        bool program_rom,
        bool program_eeprom
);

bool isblank_pic(
        K150::Programmer& programmer,
        bool icsp_mode,
        bool program_rom,
        bool program_eeprom
);

//
// implement COMPort
//
class SerialPort : public K150::COMPort
{
  Serial::SerialPort& m_port;
public:
  SerialPort(Serial::SerialPort& port) : m_port(port) { }

  void writeData(const std::vector<uint8_t>& data) override
  {
    m_port.WriteBinary(data);
  }
  void readData(std::vector<uint8_t>& data) override
  {
      m_port.ReadBinary(data);
  }
  void open() override
  {
    m_port.Open();
  }
  void close() override
  {
    m_port.Close();
  }
  bool isopen() override
  {
    return (m_port.GetState() == Serial::State::OPEN);
  }
  void reset() override
  {
    m_port.ResetDevice();
  }
};

//
// main
//
enum Operation {
  NONE      = 0,
  CONVERT   = 1,
  LIST      = 2,
  DRYRUN    = 3,
  DUMP      = 4,
  ERASE     = 5,
  PROGRAM   = 6,
  VERIFY    = 7,
  ISBLANK   = 8,
  PING      = 9,
};

int main(int argc, char** argv)
{
  std::string exepath = dirname(argv[0]);
  std::string datpath = exepath + "picpro.dat";
  std::string serialdev = "/dev/ttyUSB0";
  std::string chipname;
  std::string newhex;
  std::string outhex;
  std::string list_filter;
  std::vector<uint8_t> ID;
  Operation op = NONE;
  bool debug = false;
  bool icsp = false;
  bool swab = false;
  bool program_rom = false;
  bool program_eeprom = false;
  bool program_config = false;
  bool convert_raw2hex = false;
  bool convert_hex2raw = false;
  int range_beg = 0;
  int range_end = 0;
  int range_blank = 0;

  int n = 1;
  while (n < argc)
  {
    if (::strcmp(argv[n], "--debug") == 0)
      debug = true;
    else if (::strcmp(argv[n], "-p") == 0 && n < argc-1)
      serialdev.assign(argv[++n]);
    else if (::strcmp(argv[n], "-t") == 0 && n < argc-1)
      chipname.assign(argv[++n]);
    else if (::strcmp(argv[n], "-i") == 0 && n < argc-1)
      newhex.assign(argv[++n]);
    else if (::strcmp(argv[n], "-o") == 0 && n < argc-1)
      outhex.assign(argv[++n]);
    else if (::strcmp(argv[n], "-d") == 0 && n < argc-1)
      datpath.assign(argv[++n]);
    else if (::strcmp(argv[n], "--icsp") == 0)
      icsp = true;
    else if (::strcmp(argv[n], "--swab") == 0)
      swab = true;
    else if (::strcmp(argv[n], "-h") == 0 || ::strcmp(argv[n], "--help") == 0)
    {
      fwrite(usage_txt, usage_txt_len, 1, stdout);
      return EXIT_SUCCESS;
    }
    else if (::strcmp(argv[n], "-v") == 0 || ::strcmp(argv[n], "--version") == 0)
    {
      fprintf(stdout, "%s\n", PP150_HEADER);
      return EXIT_SUCCESS;
    }
    else if (::strncmp(argv[n], "--id=", 5) == 0)
    {
      std::string buf(argv[n]+5);
      // should be 2 characters for byte
      if ((buf.size() % 2) != 0 || buf.size() > 16)
      {
        fprintf(stderr, "Invalid length for ID (%d).\n", (int) buf.size());
        return EXIT_FAILURE;
      }
      // should be hexa-decimal format
      for (int i = 0; i < buf.size(); ++i)
      {
        const char * p = B16_CHARS;
        while (*p && buf[i] != *p) ++p;
        if (*p == 0)
        {
          fprintf(stderr, "Invalid format for ID (%s).\n", buf.c_str());
          return EXIT_FAILURE;
        }
      }
      // fill raw data for ID
      for (int i = 0; i < buf.size(); i += 2)
      {
        std::string bb;
        bb.push_back(buf[i]);
        bb.push_back(buf[i + 1]);
        int val = (int) ::strtoul(bb.c_str(), nullptr, 16);
        ID.push_back((val) & 0xff);
      }
    }
    else if (op == NONE && ::strcmp(argv[n], "list") == 0 && n < argc-1)
    {
      n += 1;
      if (::strcmp(argv[n], "all") != 0)
        list_filter.assign(argv[n]);
      op = LIST;
    }
    else if (op == NONE && ::strcmp(argv[n], "dryrun") == 0 && n < argc-1)
    {
      n += 1;
      if (::strcmp(argv[n], "all") == 0)
        program_rom = program_eeprom = program_config = true;
      else if (::strcmp(argv[n], "rom") == 0)
        program_rom = true;
      else if (::strcmp(argv[n], "eeprom") == 0)
        program_eeprom = true;
      else if (::strcmp(argv[n], "config") == 0)
        program_config = true;
      else
      {
        fprintf(stderr, "Invalid argument (%s).\n", argv[n]);
        return EXIT_FAILURE;
      }
      op = DRYRUN;
    }
    else if (op == NONE && ::strcmp(argv[n], "dump") == 0 && n < argc-1)
    {
      n += 1;
      if (::strcmp(argv[n], "all") == 0)
        program_rom = program_eeprom = program_config = true;
      else if (::strcmp(argv[n], "rom") == 0)
        program_rom = true;
      else if (::strcmp(argv[n], "eeprom") == 0)
        program_eeprom = true;
      else if (::strcmp(argv[n], "config") == 0)
        program_config = true;
      else if (::strcmp(argv[n], "hex") != 0)
      {
        fprintf(stderr, "Invalid argument (%s).\n", argv[n]);
        return EXIT_FAILURE;
      }
      op = DUMP;
    }
    else if (op == NONE && ::strcmp(argv[n], "ping") == 0)
    {
      op = PING;
    }
    else if (op == NONE && ::strcmp(argv[n], "erase") == 0)
    {
      op = ERASE;
    }
    else if (op == NONE && ::strcmp(argv[n], "program") == 0 && n < argc-1)
    {
      n += 1;
      if (::strcmp(argv[n], "all") == 0)
        program_rom = program_eeprom = program_config = true;
      else if (::strcmp(argv[n], "rom") == 0)
        program_rom = true;
      else if (::strcmp(argv[n], "eeprom") == 0)
        program_eeprom = true;
      else if (::strcmp(argv[n], "config") == 0)
        program_config = true;
      else
      {
        fprintf(stderr, "Invalid argument (%s).\n", argv[n]);
        return EXIT_FAILURE;
      }
      op = PROGRAM;
    }
    else if (op == NONE && ::strcmp(argv[n], "verify") == 0 && n < argc-1)
    {
      n += 1;
      if (::strcmp(argv[n], "all") == 0)
        program_rom = program_eeprom = program_config = true;
      else if (::strcmp(argv[n], "rom") == 0)
        program_rom = true;
      else if (::strcmp(argv[n], "eeprom") == 0)
        program_eeprom = true;
      else
      {
        fprintf(stderr, "Invalid argument (%s).\n", argv[n]);
        return EXIT_FAILURE;
      }
      op = VERIFY;
    }
    else if (op == NONE && ::strcmp(argv[n], "isblank") == 0 && n < argc-1)
    {
      n += 1;
      if (::strcmp(argv[n], "rom") == 0)
        program_rom = true;
      else if (::strcmp(argv[n], "eeprom") == 0)
        program_eeprom = true;
      else
      {
        fprintf(stderr, "Invalid argument (%s).\n", argv[n]);
        return EXIT_FAILURE;
      }
      op = ISBLANK;
    }
    else if (op == NONE && ::strcmp(argv[n], "convert") == 0 && n < argc-1)
    {
      n += 1;
      if (::strcmp(argv[n], "raw2hex") == 0)
        convert_raw2hex = true;
      else if (::strcmp(argv[n], "hex2raw") == 0)
        convert_hex2raw = true;
      else
      {
        fprintf(stderr, "Invalid argument (%s).\n", argv[n]);
        return EXIT_FAILURE;
      }
      op = CONVERT;
    }
    else if (::strncmp(argv[n], "--range=", 8) == 0)
    {
      std::string buf(argv[n]+8);
      // should be in format %8x-%8x
      char * c = nullptr;
      range_beg = (int) strtoul(buf.c_str(), &c, 16);
      range_end = range_beg;
      if (c && *c == '-')
      {
        std::string tmp = buf.substr(buf.find(*c) + 1);
        if (!tmp.empty())
          range_end = (int) strtoul(tmp.c_str(), &c, 16);
        if (range_end <= range_beg)
        {
          fprintf(stderr, "Invalid range (%s).\n", buf.c_str());
          return EXIT_FAILURE;
        }
      }
      else
      {
        fprintf(stderr, "Invalid format for range (%s).\n", buf.c_str());
        return EXIT_FAILURE;
      }
    }
    else if (::strncmp(argv[n], "--blank=", 8) == 0)
    {
      std::string buf(argv[n]+8);
      // should be in format %4x
      char * c = nullptr;
      range_blank = ((int) strtoul(buf.c_str(), &c, 16)) & 0xffff;
      if (c && *c)
      {
        fprintf(stderr, "Invalid format for word blank (%s).\n", buf.c_str());
        return EXIT_FAILURE;
      }
    }
    else
    {
      fprintf(stderr, "Invalid argument (%s).\n", argv[n]);
      fputs("Use option -h or --help to print usage.\n", stdout);
      return EXIT_FAILURE;
    }

    n += 1;
  }

  if (debug)
  {
    fprintf(stderr, ">>> OPERATION=%d\n", op);
    fprintf(stderr, ">>> EXEPATH=%s\n", exepath.c_str());
    fprintf(stderr, ">>> DATPATH=%s\n", datpath.c_str());
    fprintf(stderr, ">>> SERIALDEV=%s\n", serialdev.c_str());
    fprintf(stderr, ">>> NEWHEX=%s\n", newhex.c_str());
    fprintf(stderr, ">>> OUTHEX=%s\n", outhex.c_str());
    fprintf(stderr, ">>> ICSP=%s\n", (icsp ? "true" : "false"));
    fprintf(stderr, ">>> SWAB=%s\n", (swab ? "true" : "false"));
    fprintf(stderr, ">>> RANGE_BEG=%08X\n", range_beg);
    fprintf(stderr, ">>> RANGE_END=%08X\n", range_end);
    fprintf(stderr, ">>> RANGE_BLANK=%04X\n", range_blank);
  }

  Serial::SerialPort serialPort(serialdev,
          Serial::BaudRate::B_19200,
          Serial::NumDataBits::EIGHT,
          Serial::Parity::NONE,
          Serial::NumStopBits::ONE,
          Serial::HardwareFlowControl::OFF,
          Serial::SoftwareFlowControl::OFF);
  serialPort.SetTimeout(100); // Block for up to 100ms to receive data

  SerialPort port(serialPort);

  K150::CHIPInfo chip;
  chip.setDebug(debug);
  K150::Programmer programmer;
  programmer.setDebug(debug);

  bool ok = true;

  switch (op)
  {
  case NONE:
    fputs("Use option -h or --help to print usage.\n", stderr);
    break;

  case PING:
  {
    ok &= programmer.connect(&port);
    if (ok)
      programmer.disconnect();
    break;
  }

  case DRYRUN:
  {
    K150::HexData hex;
    hex.setDebug(debug);
    ok &= hex.loadHEX(newhex);
    if (!ok)
      break;

    ok &= load_chip_info(chip, datpath, chipname);
    if (!ok)
      break;
    ok &= programmer.configure(chip);
    if (!ok)
      break;

    ok &= program_pic(programmer, hex, ID, icsp,
            false, program_rom, program_eeprom, program_config);
    break;
  }

  case DUMP:
  {
    K150::HexData hex;
    hex.setDebug(debug);

    if (!program_rom && !program_eeprom && !program_config)
    {
      ok &= hex.loadHEX(newhex);
      hex.dumpSegments();
      break;
    }

    // Others require configuration and connection

    ok &= load_chip_info(chip, datpath, chipname);
    if (!ok)
      break;
    ok &= programmer.configure(chip);
    if (!ok)
      break;

    fprintf(stderr, "Initializing programmer on port '%s'.\n",
            serialdev.c_str());
    ok &= programmer.connect(&port);
    if (!ok)
      break;

    ok &= read_pic(programmer, icsp, program_rom, program_eeprom, program_config, outhex);
      
    programmer.disconnect();
    break;
  }

  case ERASE:
  {
    ok &= load_chip_info(chip, datpath, chipname);
    if (!ok)
      break;
    ok &= programmer.configure(chip);
    if (!ok)
      break;

    fprintf(stderr, "Initializing programmer on port '%s'.\n",
            serialdev.c_str());
    ok &= programmer.connect(&port);
    if (!ok)
      break;

    ok &= erase_pic(programmer, icsp);

    programmer.disconnect();
    break;
  }

  case PROGRAM:
  {
    K150::HexData hex;
    hex.setDebug(debug);
    ok &= hex.loadHEX(newhex);
    if (!ok)
      break;

    ok &= load_chip_info(chip, datpath, chipname);
    if (!ok)
      break;
    ok &= programmer.configure(chip);
    if (!ok)
      break;

    fprintf(stderr, "Initializing programmer on port '%s'.\n",
            serialdev.c_str());
    ok &= programmer.connect(&port);
    if (!ok)
      break;

    ok &= program_pic(programmer, hex, ID, icsp,
            true, program_rom, program_eeprom, program_config);

    programmer.disconnect();
    break;
  }

  case VERIFY:
  {
    K150::HexData hex;
    hex.setDebug(debug);
    ok &= hex.loadHEX(newhex);
    if (!ok)
      break;

    ok &= load_chip_info(chip, datpath, chipname);
    if (!ok)
      break;
    ok &= programmer.configure(chip);
    if (!ok)
      break;

    fprintf(stderr, "Initializing programmer on port '%s'.\n",
            serialdev.c_str());
    ok &= programmer.connect(&port);
    if (!ok)
      break;

    ok &= verify_pic(programmer, hex, icsp,
            program_rom, program_eeprom);

    programmer.disconnect();
    break;
  }

  case ISBLANK:
  {
    ok &= load_chip_info(chip, datpath, chipname);
    if (!ok)
      break;
    ok &= programmer.configure(chip);
    if (!ok)
      break;

    fprintf(stderr, "Initializing programmer on port '%s'.\n",
            serialdev.c_str());
    ok &= programmer.connect(&port);
    if (!ok)
      break;

    ok &= isblank_pic(programmer, icsp,
            program_rom, program_eeprom);

    programmer.disconnect();
    break;
  }

  case CONVERT:
  {
    if (newhex.empty() || outhex.empty() || range_end == 0)
    {
      fprintf(stderr, "Missing arguments.\n");
      ok = false;
    }
    else if (convert_hex2raw)
    {
      K150::HexData hex;
      hex.setDebug(debug);
      ok &= hex.loadHEX(newhex);
      if (!ok)
        break;

      fprintf(stderr, "Converting HEX segment from address %X to raw data.\n",
              range_beg);

      // range ends are included, i.e 0000-0FFF counts 1000 bytes
      std::vector<uint8_t> data = hex.rangeOfData(
              range_beg,
              (range_end - range_beg + 1) / 2,
              range_blank, swab);
      FILE * fb = fopen(outhex.c_str(), "wb");
      if (fb == nullptr)
      {
        fprintf(stderr, "Failed to open out file (%s).\n", outhex.c_str());
        ok = false;
        break;
      }
      ok &= (fwrite(data.data(), data.size(), 1, fb) == 1);
      fclose(fb);
      if (!ok)
        fputs("Operation failed.\n", stderr);
      else
        fputs("Operation succeeded.\n", stderr);
    }
    else if (convert_raw2hex)
    {
      FILE * fb = fopen(newhex.c_str(), "rb");
      if (fb == nullptr)
      {
        fprintf(stderr, "Failed to open input file (%s).\n", newhex.c_str());
        ok = false;
        break;
      }

      fprintf(stderr, "Converting raw data to HEX at address %X.\n",
              range_beg);

      std::vector<uint8_t> data;
      // the segment size
      int sz = 2 * ((range_end - range_beg + 1) / 2);
      for (;;)
      {
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf), fb);
        for (int i = 0; i < n && data.size() < sz; ++i)
          data.push_back(buf[i]);
        if (n == 0 || data.size() >= sz)
          break;
      }
      fclose(fb);

      if ((data.size() % 2) != 0)
      {
        fprintf(stderr, "The bytes count must be even (%u).\n",
                (unsigned) data.size());
        break;
      }
      K150::HexData hex;
      hex.setDebug(debug);
      ok &= hex.loadRAW(range_beg, data, swab);
      ok &= hex.saveHEX(outhex);
      if (!ok)
        fputs("Operation failed.\n", stderr);
      else
        fputs("Operation succeeded.\n", stderr);
    }
    break;
  }

  case LIST:
  {
    chip.dumplist(datpath, list_filter);
    break;
  }

  }

  if (!ok)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

//
//
//

std::string dirname(const std::string& filepath)
{
  int p = filepath.find_last_of('/');
  if (p == std::string::npos)
    return "./";
  if (p < 1)
    return "/";
  return filepath.substr(0, p + 1);
}

void logdata(FILE * out, const std::vector<uint8_t>& data)
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
      fprintf(out, "%02x ", (unsigned char) data[idx]);
      str[i] = (data[idx] > 32 && data[idx] < 127 ? data[idx] : '.');
    }
    str[i] = '\0';
    while (i++ < 16) fputs("   ", out);
    fputc(' ', out);
    fputs(str, out);
    fputc('\n', out);
  }
}

bool load_chip_info(K150::CHIPInfo& info, const std::string& datpath,
        const std::string& chipname)
{
  if (!info.loaddata(datpath, chipname))
  {
    fprintf(stderr, "Chip type '%s' is unknown.\n", chipname.c_str());
    return false;
  }
  fprintf(stderr, "Chip type %s found in database with ID %s.\n",
          info.data().chip_name.c_str(), info.data().chip_id.c_str());
  return true;
}

bool read_pic(
        K150::Programmer& programmer,
        bool icsp_mode,
        bool program_rom,
        bool program_eeprom,
        bool program_config,
        const std::string& outhex)
{
  bool ok = true;
  const K150::Programmer::Properties& props = programmer.properties();
  K150::HexData hex;

  // Initialize programming variables
  ok &= programmer.initializeProgrammingVariables(icsp_mode);
  if (!ok)
    return false;

  // Instruct user to insert chip
  if (icsp_mode || props.socket_hint.empty())
    fprintf(stderr, "Accessing chip connected to ICSP port.\n");
  else
  {
    ok &= programmer.waitUntilChipInSocket();
    if (!ok)
      return false;
    ::sleep(1);
  }

  ok &= programmer.setProgrammingVoltages(true);
  if (!ok)
    return false;

  if (program_rom)
  {
    std::vector<uint8_t> data;
    ok &= programmer.readROM(data);
    if (!outhex.empty())
      // ROM word is LE for all cores, so swap bytes
      ok &= hex.loadRAW(props.rom_base, data, true);
    else
      logdata(stdout, data);
  }

  if (program_eeprom)
  {
    std::vector<uint8_t> data;
    ok &= programmer.readEEPROM(data);
    if (!outhex.empty())
    {
      switch (props.core_bits)
      {
      case 12:
      case 14:
        ok &= hex.loadRAW_LE8(props.eeprom_base, data);
        break;
      case 16:
        ok &= hex.loadRAW(props.eeprom_base, data, false);
        break;
      default:
        ok = false;
        fprintf(stderr, "Core bits not supported (%d).\n", props.core_bits);
      }
    }
    else
      logdata(stdout, data);
  }

  if (program_config)
  {
    std::vector<int> fuses;
    ok &= programmer.readCONFIG(fuses);
    if (ok && !outhex.empty())
    {
      std::vector<uint8_t> data;
      for (int f : fuses)
      {
        data.push_back((f >> 8) & 0xff);
        data.push_back((f & 0xff));
      }
      ok &= hex.loadRAW(props.config_base, data, true);
    }
  }

  if (ok && !outhex.empty())
  {
    ok &= hex.saveHEX(outhex);
  }
  if (ok)
    fprintf(stderr, "Operation succeeded.\n");
  else
    fprintf(stderr, "Operation aborted.\n");

  ok &= programmer.setProgrammingVoltages(false);

  return ok;
}

bool erase_pic(
        K150::Programmer& programmer,
        bool icsp_mode)
{
  bool ok = true;
  const K150::Programmer::Properties& props = programmer.properties();

  // Initialize programming variables
  ok &= programmer.initializeProgrammingVariables(icsp_mode);
  if (!ok)
    return false;

  // Instruct user to insert chip
  if (icsp_mode || props.socket_hint.empty())
    fprintf(stderr, "Accessing chip connected to ICSP port.\n");
  else
  {
    ok &= programmer.waitUntilChipInSocket();
    if (!ok)
      return false;
    ::sleep(1);
  }

  ok &= programmer.setProgrammingVoltages(true);
  if (!ok)
    return false;

  fprintf(stderr, "Erasing Chip\n");
  ok &= programmer.eraseChip();
  if (!ok)
    fprintf(stderr, "Erasure failed.\n");
  else
    fprintf(stderr, "Erasure succeeded.\n");

  ok &= programmer.setProgrammingVoltages(false);

  return ok;
}

bool program_pic(
        K150::Programmer& programmer,
        K150::HexData& hex,
        const std::vector<uint8_t>& ID,
        bool icsp_mode,
        bool program,
        bool program_rom,
        bool program_eeprom,
        bool program_config)
{
  const K150::Programmer::Properties& props = programmer.properties();

  // create byte-level data for ROM
  // ROM word is LE for all cores, so swap bytes
  std::vector<uint8_t> rom_data = hex.rangeOfData(props.rom_base, props.rom_size, props.rom_blank, true);

  // create byte-level data from EEPROM
  std::vector<uint8_t> eeprom_data;
  switch (props.core_bits)
  {
  case 12:
  case 14:
  {
    eeprom_data.reserve(props.eeprom_size);
    std::vector<uint8_t> tmp = hex.rangeOfData(props.eeprom_base, props.eeprom_size, 0xffff, false);
    for (int i = 0; i < tmp.size(); i += 2)
      eeprom_data.push_back(tmp[i]); // lsb
    break;
  }
  case 16:
    eeprom_data = hex.rangeOfData(props.eeprom_base, props.eeprom_size / 2, 0xffff, false);
    break;
  default:
    fprintf(stderr, "Core bits not supported (%d).\n", props.core_bits);
    return false;
  }

  std::vector<uint8_t> id_data = ID;

  // Pull fuse data from config records, and incorporate into default setting
  // it expects fuse word as LE, so swap bytes
  std::vector<int> fuse_values = props.fuse_blank;
  std::vector<uint8_t> fuse_data = hex.rangeOfData(props.config_base, props.fuse_blank.size(), props.rom_blank, true);
  fuse_values[0] = (fuse_data[0] << 8) | (fuse_data[1]);

  bool ok = true;

  // If write mode is active, program the ROM, EEPROM, ID and fuses
  if (program)
  {
    // Initialize programming variables
    ok &= programmer.initializeProgrammingVariables(icsp_mode);
    if (!ok)
      return false;

    // Instruct user to insert chip
    if (icsp_mode || props.socket_hint.empty())
      fprintf(stderr, "Accessing chip connected to ICSP port.\n");
    else
    {
      ok &= programmer.waitUntilChipInSocket();
      if (!ok)
        return false;
      ::sleep(1);
    }

    ok &= programmer.setProgrammingVoltages(true);
    if (!ok)
      return false;

    // Write ROM, EEPROM, ID and fuses
    if (props.flag_flash_chip &&
            program_rom && program_eeprom && program_config)
    {
      fprintf(stderr, "Erasing Chip\n");
      if (!programmer.eraseChip())
        fprintf(stderr, "Erasure failed.\n");
      if (!programmer.cycleProgrammingVoltages())
        return false;
    }

    if (program_rom)
    {
      fprintf(stderr, "Programming ROM\n");
      if (!programmer.programROM(rom_data))
        fprintf(stderr, "ROM programming failed.\n");
    }

    if (program_eeprom && props.eeprom_size > 0)
    {
      fprintf(stderr, "Programming EEPROM\n");
      if (!programmer.programEEPROM(eeprom_data))
        fprintf(stderr, "EEPROM programming failed.\n");
    }

    if (program_config)
    {
      fprintf(stderr, "Programming ID and fuses\n");
      if (!programmer.programCONFIG(id_data, fuse_values))
        fprintf(stderr, "Programming ID and fuses failed.\n");
    }

    // Verify programmed data

    if (program_rom)
    {
      fprintf(stderr, "Verifying ROM\n");
      std::vector<uint8_t> buf;
      if (programmer.readROM(buf) && buf == rom_data)
        fprintf(stderr, "ROM verified.\n");
      else
      {
        fprintf(stderr, "ROM verification failed.\n");
        ok = false;
      }
    }

    if (program_eeprom && props.eeprom_size > 0)
    {
      fprintf(stderr, "Verifying EEPROM\n");
      std::vector<uint8_t> buf;
      if (programmer.readEEPROM(buf) && buf == eeprom_data)
        fprintf(stderr, "EEPROM verified.\n");
      else
      {
        fprintf(stderr, "EEPROM verification failed.\n");
        ok = false;
      }
    }

    if (ok && props.core_bits == 16 && program_config)
    {
      fprintf(stderr, "Committing FUSE data.\n");
      programmer.programCOMMIT_18FXXXX_FUSE();
    }

    if (ok && program_config)
    {
      std::vector<int> buf;
      if (programmer.readCONFIG(buf) && buf == fuse_values)
        fprintf(stderr, "CONFIG verified.\n");
      else
      {
        fprintf(stderr, "CONFIG verification failed.\n");
        ok = false;
      }
    }

    ok &= programmer.setProgrammingVoltages(false);
  }
  else
  {
    // dryrun: only output data
    if (icsp_mode || props.socket_hint.empty())
      fprintf(stdout, "\nAccessing chip connected to ICSP port.\n");
    else
      fprintf(stdout, "\nInsert chip into socket with pin 1 at %s.\n", props.socket_hint.c_str());

    if (program_rom)
    {
      fprintf(stdout, "\nProgramming ROM (%06X : %uKB)\n", props.rom_base,  props.rom_size >> 9);
      logdata(stdout, rom_data);
    }
    if (program_eeprom && props.eeprom_size > 0)
    {
      fprintf(stdout, "\nProgramming EEPROM (%06X : %uB)\n", props.eeprom_base,  props.eeprom_size);
      logdata(stdout, eeprom_data);
    }
    if (program_config)
    {
      fprintf(stdout, "\nProgramming ID\n");
      logdata(stdout, id_data);
      fprintf(stdout, "\nProgramming fuses (%06X : %uB)\n", props.config_base, (unsigned) fuse_values.size() << 1);
      for (int i = 0; i < fuse_values.size(); ++i)
        fprintf(stdout, "%04X ", fuse_values[i]);
      fputc('\n', stdout);
    }
  }

  return ok;
}

bool verify_pic(
        K150::Programmer& programmer,
        K150::HexData& hex,
        bool icsp_mode,
        bool program_rom,
        bool program_eeprom)
{
  const K150::Programmer::Properties& props = programmer.properties();

  // create byte-level data for ROM
  // ROM word is LE for all cores, so swap bytes
  std::vector<uint8_t> rom_data = hex.rangeOfData(props.rom_base, props.rom_size, props.rom_blank, true);

  // create byte-level data from EEPROM
  std::vector<uint8_t> eeprom_data;
  switch (props.core_bits)
  {
  case 12:
  case 14:
  {
    eeprom_data.reserve(props.eeprom_size);
    std::vector<uint8_t> tmp = hex.rangeOfData(props.eeprom_base, props.eeprom_size, 0xffff, false);
    for (int i = 0; i < tmp.size(); i += 2)
      eeprom_data.push_back(tmp[i]); // lsb
    break;
  }
  case 16:
    eeprom_data = hex.rangeOfData(props.eeprom_base, props.eeprom_size / 2, 0xffff, false);
    break;
  default:
    fprintf(stderr, "Core bits not supported (%d).\n", props.core_bits);
    return false;
  }

  bool ok = true;

  // Initialize programming variables
  programmer.initializeProgrammingVariables(icsp_mode);

  // Instruct user to insert chip
  if (icsp_mode || props.socket_hint.empty())
    fprintf(stderr, "Accessing chip connected to ICSP port.\n");
  else
  {
    ok &= programmer.waitUntilChipInSocket();
    if (!ok)
      return false;
    ::sleep(1);
  }

  // Verify programmed data

  ok &= programmer.setProgrammingVoltages(true);
  if (!ok)
    return false;

  if (program_rom)
  {
    fprintf(stderr, "Verifying ROM\n");
    std::vector<uint8_t> buf;
    if (programmer.readROM(buf) && buf == rom_data)
      fprintf(stderr, "ROM verified.\n");
    else
    {
      fprintf(stderr, "ROM verification failed.\n");
      ok = false;
    }
  }

  if (program_eeprom && props.eeprom_size > 0)
  {
    fprintf(stderr, "Verifying EEPROM\n");
    std::vector<uint8_t> buf;
    if (programmer.readEEPROM(buf) && buf == eeprom_data)
      fprintf(stderr, "EEPROM verified.\n");
    else
    {
      fprintf(stderr, "EEPROM verification failed.\n");
      ok = false;
    }
  }

  ok &= programmer.setProgrammingVoltages(false);

  return ok;
}

bool isblank_pic(
        K150::Programmer& programmer,
        bool icsp_mode,
        bool program_rom,
        bool program_eeprom)
{
  //@FIXME:
  // command 15 & 16 always return N
  // to workaround, I read memory and compare it with blank segment

  const K150::Programmer::Properties& props = programmer.properties();

  K150::HexData hex;
  // create byte-level data for blank ROM
  // ROM word is LE for all cores, so swap bytes
  std::vector<uint8_t> rom_data = hex.rangeOfData(props.rom_base, props.rom_size, props.rom_blank, true);

  // create byte-level data from blank EEPROM
  std::vector<uint8_t> eeprom_data(props.eeprom_size, 0xff);

  bool ok = true;

  // Initialize programming variables
  ok &= programmer.initializeProgrammingVariables(icsp_mode);
  if (!ok)
    return false;

  // Instruct user to insert chip
  if (icsp_mode || props.socket_hint.empty())
    fprintf(stderr, "Accessing chip connected to ICSP port.\n");
  else
  {
    ok &= programmer.waitUntilChipInSocket();
    if (!ok)
      return false;
    ::sleep(1);
  }

  // Verify programmed data
  std::vector<uint8_t> buf;

  ok &= programmer.setProgrammingVoltages(true);
  if (!ok)
    return false;

  if (program_rom)
  {
    fprintf(stderr, "Checking ROM (%d B) is blank\n", 2 * props.rom_size);
    ok &= programmer.readROM(buf);
    if (!ok || buf.size() != 2 * props.rom_size)
    {
      fprintf(stderr, "Command failed.\n");
    }
    else if (buf == rom_data)
      fprintf(stdout, "TRUE\n");
    else
      fprintf(stdout, "FALSE\n");
  }

  if (program_eeprom && props.eeprom_size > 0)
  {
    fprintf(stderr, "Checking EEPROM (%d B) is blank\n", props.eeprom_size);
    ok &= programmer.readEEPROM(buf);
    if (!ok || buf.size() != props.eeprom_size)
    {
      fprintf(stderr, "Command failed.\n");
    }
    else if (buf == eeprom_data)
      fprintf(stdout, "TRUE\n");
    else
      fprintf(stdout, "FALSE\n");
  }

  ok &= programmer.setProgrammingVoltages(false);

  return ok;
}

#include "TMCCInterface.h"
#include "PDIInterface.h"
#ifdef WINDOWS
#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>

HANDLE serialPort;
static HANDLE s_serial = nullptr;

#endif // WINDOWS
#include <filesystem>

static std::vector<DeviceInfo> s_devices;
static DeviceInfo* s_currentDevice;


DeviceInfo::DeviceInfo()
  : m_portName{nullptr}
  , m_friendlyName{nullptr}
  , m_hardwareId{nullptr}
{
}

DeviceInfo::~DeviceInfo()
{
  if (m_portName)
    free(m_portName);
  if (m_friendlyName)
    free(m_friendlyName);
  if (m_hardwareId)
    free(m_hardwareId);
}

DeviceInfo::DeviceInfo(const DeviceInfo& other)
{
  m_portName = other.m_portName ? strdup(other.m_portName) : nullptr;
  m_friendlyName = other.m_friendlyName ? strdup(other.m_friendlyName) : nullptr;
  m_hardwareId = other.m_hardwareId ? strdup(other.m_hardwareId) : nullptr;
}

DeviceInfo::DeviceInfo(DeviceInfo&& other)
{
  m_portName = other.m_portName;
  other.m_portName = nullptr;
  m_friendlyName = other.m_friendlyName;
  other.m_friendlyName = nullptr;
  m_hardwareId = other.m_hardwareId;
  other.m_hardwareId = nullptr;
}


void DeviceInfo::SetPortName(const char* portName)
{
  if (m_portName)
    free(m_portName);
  m_portName = nullptr;

  if (!portName)
    return;

  m_portName = strdup(portName);
}

void DeviceInfo::SetFriendlyName(const char* friendlyName)
{
  if (m_friendlyName)
    free(m_friendlyName);
  m_friendlyName = nullptr;

  if (!friendlyName)
    return;

  m_friendlyName = strdup(friendlyName);
}

void DeviceInfo::SetHardwareID(const char* hardwareId)
{
  if (m_hardwareId)
    free(m_hardwareId);
  m_hardwareId = nullptr;

  if (!hardwareId)
    return;

  m_hardwareId = strdup(hardwareId);
}


static std::vector<DeviceInfo> GetDevices()
{
  std::vector<DeviceInfo> devices;

#ifdef WINDOWS

  // get the tree containing the info for the ports
  HDEVINFO hDeviceInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS,
    0,
    nullptr,
    DIGCF_PRESENT
  );
  if (hDeviceInfo == INVALID_HANDLE_VALUE)
  {
    return devices;
  }

  DeviceInfo device;

  SP_DEVINFO_DATA devInfoData = {};
  devInfoData.cbSize = sizeof(devInfoData);

  // iterate over all the devices in the tree
  int nDevice = 0;
  while (SetupDiEnumDeviceInfo(hDeviceInfo,            // Our device tree
    nDevice++,            // The member to look for
    &devInfoData))
  {
    DWORD regDataType;
    DWORD reqSize = 0;

    // SPDRP_SERVICE - Serial
    // SPDRP_CLASS - Ports

    // get com port
    HKEY key = SetupDiOpenDevRegKey(hDeviceInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
    if (key != INVALID_HANDLE_VALUE)
    {
      LPCSTR lpValueName = "PortName";

      DWORD dwBytes = 0;
      LSTATUS nStatus;
      nStatus = RegQueryValueEx(key, lpValueName, nullptr, nullptr, nullptr, &dwBytes);
      if (nStatus != ERROR_SUCCESS)
      {
        //SetLastError(nStatus);
        //return false;
        RegCloseKey(key);
        assert(false);
        continue;
      }

      char* portName = new char[dwBytes + 1];
      DWORD dwType{ 0 };
      nStatus = RegQueryValueEx(key, lpValueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(portName), &dwBytes);
      if (nStatus != ERROR_SUCCESS)
      {
        //SetLastError(nStatus);
        //return false;
        RegCloseKey(key);
        assert(false);
        continue;
      }
      if ((dwType != REG_SZ) && (dwType != REG_EXPAND_SZ))
      {
        //SetLastError(ERROR_INVALID_DATA);
        //return false;
        //assert(false);
        RegCloseKey(key);
        continue;
      }
      if ((dwBytes % sizeof(TCHAR)) != 0)
      {
        //SetLastError(ERROR_INVALID_DATA);
        //return false;
        //assert(false);
        RegCloseKey(key);
        continue;
      }

      RegCloseKey(key);

      //printf("port name %s\n", portName.c_str());
      device.SetPortName(portName);
      delete[] portName;
    }

    //SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData, SPDRP_UI_NUMBER_DESC_FORMAT, nullptr, nullptr, 0, &reqSize);
    //BYTE* testData = new BYTE[(reqSize > 1) ? reqSize : 1];
    //// now store it in a buffer
    //if (!SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData, SPDRP_UI_NUMBER_DESC_FORMAT, &regDataType, testData, sizeof(testData) * reqSize, nullptr))
    //{
    //  memset(testData, 0, reqSize > 1 ? reqSize : 1);
    //}

    // find the size required to hold the device info
    SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData, SPDRP_HARDWAREID, nullptr, nullptr, 0, &reqSize);
    BYTE* hardwareId = new BYTE[(reqSize > 1) ? reqSize : 1];
    // now store it in a buffer
    if (SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData, SPDRP_HARDWAREID, &regDataType, hardwareId, sizeof(hardwareId) * reqSize, nullptr))
    {
      // find the size required to hold the friendly name
      reqSize = 0;
      SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData, SPDRP_FRIENDLYNAME, nullptr, nullptr, 0, &reqSize);
      BYTE* friendlyName = new BYTE[(reqSize > 1) ? reqSize : 1];
      // now store it in a buffer
      if (!SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData, SPDRP_FRIENDLYNAME, nullptr, friendlyName, sizeof(friendlyName) * reqSize, nullptr))
      {
        // device does not have this property set
        memset(friendlyName, 0, reqSize > 1 ? reqSize : 1);
      }
      // use friendlyName here

      device.SetFriendlyName((char*)friendlyName);
      device.SetHardwareID((char*)hardwareId);

      devices.push_back(device);
      delete[] friendlyName;
    }
    delete[] hardwareId;
  }

  SetupDiDestroyDeviceInfoList(hDeviceInfo);

#else // !WINDOWS
  std::filesystem::path path("/sys/class/tty");
  for (const auto& e : std::filesystem::directory_iterator(path))
  {
    if (std::filesystem::is_symlink(e.symlink_status()))
    {
      std::filesystem::path points = std::filesystem::read_symlink(e);
      std::filesystem::path canonical_path = std::filesystem::canonical(points);

      DeviceInfo device;
      device.SetPortName(canonical_path.string().c_str());
      devices.push_back(device);
    }
  }
#endif // !WINDOWS

  return devices;
}

int TMCCInterface::EnumerateDevices(DeviceInfo** devices)
{
  s_devices = GetDevices();

  *devices = s_devices.data();
  return s_devices.size();
}

bool TMCCInterface::Init(int device, bool base3USB)
{
  if (device < 0 || device >= s_devices.size())
    return false;

  if (s_currentDevice)
    delete s_currentDevice;
  s_currentDevice = new DeviceInfo(s_devices[device]);
  printf("Initialized device %d [%s]\n", device, s_devices[device].GetFriendlyName());

#ifdef WINDOWS
  if (s_serial)
  {
    CloseHandle(s_serial);
    s_serial = nullptr;
  }

  DCB dcb;
  s_serial = CreateFile(
    s_currentDevice->GetPortName(),
    (GENERIC_WRITE),
    0,
    NULL,
    OPEN_EXISTING,
    0,
    NULL
  );

  if (!GetCommState(s_serial, &dcb))
  {
    delete s_currentDevice;
    CloseHandle(s_serial);
    printf("Failed to retrieve comm state!\n");
    return false;
  }

  if(base3USB)
    dcb.BaudRate = CBR_115200; // 11520 baud rate for Base3 USB port
  else
    dcb.BaudRate = CBR_9600; //9600 Baud
  dcb.ByteSize = 8; //8 data bits
  dcb.Parity = NOPARITY; //no parity
  dcb.StopBits = ONESTOPBIT; //1 stop
  if (!SetCommState(s_serial, &dcb))
  {
    delete s_currentDevice;
    CloseHandle(s_serial);
    printf("Failed to set comm state!\n");
    return false;
  }
#endif // WINDOWS

  return true;
}

void TMCCInterface::Shutdown()
{
#ifdef WINDOWS
  if (s_serial)
  {
    CloseHandle(s_serial);
    s_serial = nullptr;
  }
#endif // WINDOWS

  if (s_currentDevice)
    delete s_currentDevice;
  s_currentDevice = nullptr;
}

bool TMCCInterface::WriteData(ubyte* pData, int length)
{
  if (!s_currentDevice)
  {
    //assert(false);
    return false;
  }

  bool bSucceeded = false;

#ifdef WINDOWS
  if (!s_serial)
    return false;

  DWORD byteswritten;

  bSucceeded = WriteFile(s_serial, pData, length, &byteswritten, NULL);
#endif // WINDOWS

  return bSucceeded;
}

int PDIInterface::EnumerateDevicesPDI(DeviceInfo** devices)
{
  s_devices = GetDevices();

  *devices = s_devices.data();
  return s_devices.size();
}

bool PDIInterface::InitPDI(int device)
{
  if (device < 0 || device >= s_devices.size())
    return false;

  if (s_currentDevice)
    delete s_currentDevice;
  s_currentDevice = new DeviceInfo(s_devices[device]);
  printf("Initialized PDI Bus device %d [%s]\n", device, s_devices[device].GetFriendlyName());

  return true;
}

void PDIInterface::ShutdownPDI()
{
  if (s_currentDevice)
    delete s_currentDevice;
  s_currentDevice = nullptr;
}

bool PDIInterface::WriteData(ubyte* pData, int length)
{
  if (!s_currentDevice)
  {
    //assert(false);
    return false;
  }

  bool bSucceeded = false;

#ifdef WINDOWS
  DCB dcb;
  DWORD byteswritten;
  HANDLE hPort = CreateFile(
    s_currentDevice->GetPortName(),
    GENERIC_WRITE,
    0,
    NULL,
    OPEN_EXISTING,
    0,
    NULL
  );

  if (!GetCommState(hPort, &dcb))
  {
    CloseHandle(hPort);
    return false;
  }
  dcb.BaudRate = CBR_115200; //115200 Baud
  dcb.ByteSize = 8; //8 data bits
  dcb.Parity = NOPARITY; //no parity
  dcb.StopBits = ONESTOPBIT; //1 stop
  if (!SetCommState(hPort, &dcb))
  {
    CloseHandle(hPort);
    return false;
  }
  bSucceeded = WriteFile(hPort, pData, length, &byteswritten, NULL);
  CloseHandle(hPort); //close the handle

#endif // WINDOWS

  return bSucceeded;
}


enum CommandType : uint8
{
  CT_ACTION           = 0x0,
  CT_EXTENDED         = 0x1,
  CT_RELATIVE_SPEED   = 0x2,
  CT_ABSOLUTE_SPEED   = 0x3,
  CT_MASK             = 0x3,
};

enum SwitchCommandParams : uint8
{
  SC_THROW_THROUGH                  = 0b00000,
  SC_THROW_OUT                      = 0b11111,
  ESC_SET_ADDRESS                   = 0b01011,

  SC_ASSIGN_TO_ROUTE_THROUGH_FLAG   = 0b00000,
  SC_ASSIGN_TO_ROUTE_OUT_FLAG       = 0b00000,
};

struct SwitchCommand : public TMCCCommand
{
  SwitchCommand(SwitchHandle address, CommandType type, SwitchCommandParams params)
  {
    data |= 1 << 14;

    data |= (uint16)(address & 0b01111111) << 7;
    data |= (uint16)(type & CT_MASK) << 5;
    data |= params & 0b11111;
  }
};


enum RouteCommandParams : uint8
{
  RC_ROUTE_THROW = 0b11111,
  ERC_ROUTE_CLEAR = 0b00000,
};

struct RouteCommand : public TMCCCommand
{
  RouteCommand(RouteHandle address, CommandType type, RouteCommandParams params)
  {
    data |= 0b1101 << 12;

    data |= (uint16)(address & 0b11111) << 7;
    data |= (uint16)(type & CT_MASK) << 5;
    data |= params & 0b11111;
  }
};

enum EngineCommandParams : uint8
{
  EC_FORWARD_DIRECTION                    = 0b00000,
  EC_TOGGLE_DIRECTION                     = 0b00001,
  EC_REVERSE_DIRECTION                    = 0b00011,
  EC_BOOST_SPEED                          = 0b00100,
  EC_BRAKE_SPEED                          = 0b00111,
  EC_OPEN_FRONT_COUPLER                   = 0b00101,
  EC_OPEN_REAR_COUPLER                    = 0b00110,
  EC_BLOW_HORN_1                          = 0b11100,
  EC_RING_BELL                            = 0b11101,
  EC_LET_OFF_SOUND                        = 0b11110,
  EC_BLOW_HORN_2                          = 0b11111,
  EC_AUX_1_OFF                            = 0b01000,
  EC_AUX_1_OPTION_1                       = 0b01001,
  EC_AUX_1_OPTION_2                       = 0b01010,
  EC_AUX_1_ON                             = 0b01011,
  EC_AUX_2_OFF                            = 0b01100,
  EC_AUX_2_OPTION_1                       = 0b01101,
  EC_AUX_2_OPTION_2                       = 0b01110,
  EC_AUX_2_ON                             = 0b01111,
  EC_NUMERIC_FLAG                         = 0b10000,
  EC_NUMERIC_MASK                         = 0b01111,

  // extended commands
  EEC_ASSIGN_TO_TRAIN_FLAG                = 0b10000,
  EEC_ASSIGN_TO_TRAIN_MASK                = 0b01111,
  EEC_ASSIGN_AS_SINGLE_UNIT_FORWARD       = 0b00000,
  EEC_ASSIGN_AS_SINGLE_UNIT_REVERSE       = 0b00100,
  EEC_ASSIGN_AS_HEAD_END_UNIT_FORWARD     = 0b00001,
  EEC_ASSIGN_AS_HEAD_END_UNIT_REVERSE     = 0b00101,
  EEC_ASSIGN_AS_MIDDLE_UNIT_FORWARD       = 0b00010,
  EEC_ASSIGN_AS_MIDDLE_UNIT_REVERSE       = 0b00110,
  EEC_ASSIGN_AS_REAR_END_UNIT_FORWARD     = 0b00011,
  EEC_ASSIGN_AS_REAR_END_UNIT_REVERSE     = 0b00111,

  EEC_SET_MOMENTUM_LOW                    = 0b01000,
  EEC_SET_MOMENTUM_MEDIUM                 = 0b01001,
  EEC_SET_MOMENTUM_HIGH                   = 0b01010,
  EEC_SET_ENGINE_ADDRESS                  = 0b01011,

  // extended train commands
  ETC_SET_MOMENTUM_LOW                    = 0b01000,
  ETC_SET_MOMENTUM_MEDIUM                 = 0b01001,
  ETC_SET_MOMENTUM_HIGH                   = 0b01010,
  ETC_SET_TMCC_ADDRESS                    = 0b01011,
  ETC_CLEAR_CONSIST                       = 0b01100,
};

struct EngineCommand : public TMCCCommand
{
  EngineCommand(EngineHandle address, CommandType type, EngineCommandParams params)
  {
    //data |= 0 << 14;

    data |= (uint16)(address & 0b01111111) << 7;
    data |= (uint16)(type & CT_MASK) << 5;
    data |= params & 0b11111;
  }
};

struct TrainCommand : public TMCCCommand
{
  TrainCommand(TrainHandle address, CommandType type, EngineCommandParams params)
  {
    data |= 0b11001 << 11;

    data |= (uint16)(address & 0b1111) << 7;
    data |= (uint16)(type & CT_MASK) << 5;
    data |= params & 0b11111;
  }
};

enum AccessoryCommandParams : uint8
{
  AC_AUX_1_OFF                    = 0b01000,
  AC_AUX_1_OPTION_1               = 0b01001,
  AC_AUX_1_OPTION_2               = 0b01010,
  AC_AUX_1_ON                     = 0b01011,
  AC_AUX_2_OFF                    = 0b01100,
  AC_AUX_2_OPTION_1               = 0b01101,
  AC_AUX_2_OPTION_2               = 0b01110,
  AC_AUX_2_ON                     = 0b01111,
  AC_NUMERIC_FLAG                 = 0b10000,
  AC_NUMERIC_MASK                 = 0b01111,

  // extended commands
  EAC_ALL_OFF                     = 0b00000,
  EAC_ALL_ON                      = 0b01111,
  EAC_SET_ADDRESS                 = 0b01011,
  // ?
  EAC_ASSIGN_AUX_1_TO_GROUP_FLAG  = 0b00000,
  EAC_ASSIGN_AUX_2_TO_GROUP_FLAG  = 0b00000,
};

struct AccessoryCommand : public TMCCCommand
{
  AccessoryCommand(TMCCAddress address, CommandType type, AccessoryCommandParams params)
  {
    data |= 0b10 << 14;

    data |= (uint16)(address & 0b01111111) << 7;
    data |= (uint16)(type & CT_MASK) << 5;
    data |= params & 0b11111;
  }
};

enum GroupCommandParams : uint8
{
  GC_GROUP_OFF    = 0b01000,
  GC_OPTION_1     = 0b01001,
  GC_OPTION_2     = 0b01011,
  // ?
  GC_GROUP_ON     = 0b01011,
  // make sure to use extended type for this
  EGC_GROUP_CLEAR  = 0b01100,

  //Group	OFF 0 0 0 1 0 0 0
  //Option1 0 0 0 1 0 0 1
  //Option2 0 0 0 1 0 1 1
  //Group	ON 0 0 0 1 0 1 1
  //Group	Clear 0 1 0 1 1 0 0
};

struct GroupCommand : public TMCCCommand
{
  GroupCommand(TMCCAddress address, CommandType type, GroupCommandParams params)
  {
    data |= 0b11000 << 11;

    data |= (uint16)(address & 0b1111) << 7;
    data |= (uint16)(type & CT_MASK) << 5;
    data |= params & 0b11111;
  }
};

bool TMCCInterface::SendTMCC1Command(const TMCCCommand& cmd)
{
  ubyte data[3];
  data[0] = 0xFE;
  *((uint16*)&data[1]) = bswap_16(cmd.data);

  return WriteData(data, 3);
}

bool TMCCInterface::SendEngineCommand(const TMCC2Command& cmd)
{
  ubyte data[3];
  data[0] = 0xF8;
  *((uint16*)&data[1]) = bswap_16(cmd.data);

  return WriteData(data, 3);
}

bool TMCCInterface::SendTrainCommand(const TMCC2Command& cmd)
{
  ubyte data[3];
  data[0] = 0xF9;
  *((uint16*)&data[1]) = bswap_16(cmd.data);

  return WriteData(data, 3);
}

bool TMCCInterface::SendMultiWordCommand(const MultiWordCommand& cmd)
{
  uint8 address = cmd.address & 0b1111111;
  uint8 index = cmd.index & 0b1111;

  uint16 word0 = ((uint16)address) << 9;
  word0 |= 0b10111 << 4;
  word0 |= index;

  uint16 word1 = ((uint16)address) << 9;
  if (cmd.train) word1 |= 1 << 8;
  word1 |= cmd.command;

  uint16 word2 = ((uint16)address) << 9;
  if (cmd.train) word2 |= 1 << 8;

  uint8 checksum = 0;
  checksum += word0 >> 8;
  checksum += word0 & 0xFF;
  checksum += word1 >> 8;
  checksum += word1 & 0xFF;
  checksum += word2 >> 8;
  word2 |= ~checksum;
  word2 &= 0xFF;
  word2 |= ((uint16)address) << 9;
  
  
  
  //printf("Word2: %#04x\n", word2);

  ubyte data[9];
  data[0] = cmd.train ? 0xF9 : 0xF8;
  *((uint16*)&data[1]) = bswap_16(word0);
  data[3] = 0xFB;
  *((uint16*)&data[4]) = bswap_16(word1);
  data[6] = 0xFB;
  *((uint16*)&data[7]) = bswap_16(word2);

  //printf("address = %d , %#04x + %#04x + %#04x\n", address, word0, word1, word2);

  return WriteData(data, 9);
}

bool TMCCInterface::AccessoryAux1Off(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_1_OFF));
}
bool TMCCInterface::AccessoryAux1Option1(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_1_OPTION_1));
}
bool TMCCInterface::AccessoryAux1Option2(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_1_OPTION_2));
}
bool TMCCInterface::AccessoryAux1On(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_1_ON));
}
bool TMCCInterface::AccessoryAux2Off(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_2_OFF));
}
bool TMCCInterface::AccessoryAux2On(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_2_ON));
}
bool TMCCInterface::AccessoryAux2Option1(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_2_OPTION_1));
}
bool TMCCInterface::AccessoryAux2Option2(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, AC_AUX_2_OPTION_2));
}

bool TMCCInterface::AccessoryAllOn(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_EXTENDED, EAC_ALL_ON));
}

bool TMCCInterface::AccessoryAllOff(AccessoryHandle id)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_EXTENDED, EAC_ALL_OFF));
}

bool TMCCInterface::AccessorySetAddress(AccessoryHandle address)
{
  return SendTMCC1Command(AccessoryCommand(address, CT_EXTENDED, EAC_SET_ADDRESS));
}

bool TMCCInterface::AccessoryAssignAux1ToGroup(AccessoryHandle id, GroupHandle groupID)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_EXTENDED, (AccessoryCommandParams)(groupID | EAC_ASSIGN_AUX_1_TO_GROUP_FLAG)));
}

bool TMCCInterface::AccessoryAssignAux2ToGroup(AccessoryHandle id, GroupHandle groupID)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_EXTENDED, (AccessoryCommandParams)(groupID | EAC_ASSIGN_AUX_2_TO_GROUP_FLAG)));
}

bool TMCCInterface::AccessoryNumericCommand(AccessoryHandle id, uint8 cmd)
{
  return SendTMCC1Command(AccessoryCommand(id, CT_ACTION, (AccessoryCommandParams)(AC_NUMERIC_FLAG | (AC_NUMERIC_MASK & cmd))));
}

bool TMCCInterface::GroupOff(GroupHandle id)
{
  return SendTMCC1Command(GroupCommand(id, CT_ACTION, GC_GROUP_OFF));
}

bool TMCCInterface::GroupOn(GroupHandle id)
{
  return SendTMCC1Command(GroupCommand(id, CT_ACTION, GC_GROUP_ON));
}

bool TMCCInterface::GroupOption1(GroupHandle id)
{
  return SendTMCC1Command(GroupCommand(id, CT_ACTION, GC_OPTION_1));
}

bool TMCCInterface::GroupOption2(GroupHandle id)
{
  return SendTMCC1Command(GroupCommand(id, CT_ACTION, GC_OPTION_2));
}

bool TMCCInterface::GroupClear(GroupHandle id)
{
  return SendTMCC1Command(GroupCommand(id, CT_EXTENDED, EGC_GROUP_CLEAR));
}

bool TMCCInterface::SwitchThrowThrough(SwitchHandle id)
{
  return SendTMCC1Command(SwitchCommand(id, CT_ACTION, SC_THROW_THROUGH));
}

bool TMCCInterface::SwitchThrowOut(SwitchHandle id)
{
  return SendTMCC1Command(SwitchCommand(id, CT_ACTION, SC_THROW_OUT));
}

bool TMCCInterface::SwitchSetAddress(SwitchHandle address)
{
  return SendTMCC1Command(SwitchCommand(address, CT_EXTENDED, ESC_SET_ADDRESS));
}

bool TMCCInterface::SwitchAssignToRouteThrough(SwitchHandle id, RouteHandle routeID)
{
  return SendTMCC1Command(SwitchCommand(id, CT_RELATIVE_SPEED, (SwitchCommandParams)(routeID | SC_ASSIGN_TO_ROUTE_THROUGH_FLAG)));
}

bool TMCCInterface::SwitchAssignToRouteOut(SwitchHandle id, RouteHandle routeID)
{
  return SendTMCC1Command(SwitchCommand(id, CT_ABSOLUTE_SPEED, (SwitchCommandParams)(routeID | SC_ASSIGN_TO_ROUTE_OUT_FLAG)));
}

bool TMCCInterface::RouteThrow(RouteHandle id)
{
  return SendTMCC1Command(RouteCommand(id, CT_ACTION, RC_ROUTE_THROW));
}

bool TMCCInterface::RouteClear(RouteHandle id)
{
  return SendTMCC1Command(RouteCommand(id, CT_EXTENDED, ERC_ROUTE_CLEAR));
}

bool TMCCInterface::EngineSetDirection(EngineHandle id, TMCCDirection dir)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, dir == TMCC_FORWARD ? EC_FORWARD_DIRECTION : EC_REVERSE_DIRECTION));
}

bool TMCCInterface::EngineToggleDirection(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_TOGGLE_DIRECTION));
}

bool TMCCInterface::EngineBoostSpeed(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_BOOST_SPEED));
}

bool TMCCInterface::EngineBrakeSpeed(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_BRAKE_SPEED));
}

bool TMCCInterface::EngineOpenFrontCoupler(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_OPEN_FRONT_COUPLER));
}

bool TMCCInterface::EngineOpenRearCoupler(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_OPEN_REAR_COUPLER));
}

bool TMCCInterface::EngineBlowHorn1(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_BLOW_HORN_1));
}

bool TMCCInterface::EngineRingBell(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_RING_BELL));
}

bool TMCCInterface::EngineLetOffSound(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_LET_OFF_SOUND));
}

bool TMCCInterface::EngineBlowHorn2(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_BLOW_HORN_2));
}

//bool TMCCInterface::EngineAux1Off(EngineHandle id)
//{
//  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_1_OFF));
//}

bool TMCCInterface::EngineAux1Option1(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_1_OPTION_1));
}

bool TMCCInterface::EngineAux1Option2(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_1_OPTION_2));
}

//bool TMCCInterface::EngineAux1On(EngineHandle id)
//{
//  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_1_ON));
//}

bool TMCCInterface::EngineSetAux1(EngineHandle id, TMCCActiveState state)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, state == TMCC_ON ? EC_AUX_1_ON : EC_AUX_1_OFF));
}

//bool TMCCInterface::EngineAux2Off(EngineHandle id)
//{
//  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_2_OFF));
//}

bool TMCCInterface::EngineAux2Option1(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_2_OPTION_1));
}

bool TMCCInterface::EngineAux2Option2(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_2_OPTION_2));
}

//bool TMCCInterface::EngineAux2On(EngineHandle id)
//{
//  return SendTMCC1Command(EngineCommand(id, CT_ACTION, EC_AUX_2_ON));
//}

bool TMCCInterface::EngineSetAux2(EngineHandle id, TMCCActiveState state)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, state == TMCC_ON ? EC_AUX_2_ON : EC_AUX_2_OFF));
}

bool TMCCInterface::EngineNumericCommand(EngineHandle id, uint8 cmd)
{
  return SendTMCC1Command(EngineCommand(id, CT_ACTION, (EngineCommandParams)(EC_NUMERIC_FLAG | (EC_NUMERIC_MASK & cmd))));
}

bool TMCCInterface::TrainSetDirection(TrainHandle id, TMCCDirection dir)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, dir == TMCC_FORWARD ? EC_FORWARD_DIRECTION : EC_REVERSE_DIRECTION));
}

bool TMCCInterface::TrainToggleDirection(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_TOGGLE_DIRECTION));
}

bool TMCCInterface::TrainBoostSpeed(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_BOOST_SPEED));
}

bool TMCCInterface::TrainBrakeSpeed(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_BRAKE_SPEED));
}

bool TMCCInterface::TrainOpenFrontCoupler(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_OPEN_FRONT_COUPLER));
}

bool TMCCInterface::TrainOpenRearCoupler(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_OPEN_REAR_COUPLER));
}

bool TMCCInterface::TrainBlowHorn1(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_BLOW_HORN_1));
}

bool TMCCInterface::TrainRingBell(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_RING_BELL));
}

bool TMCCInterface::TrainLetOffSound(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_LET_OFF_SOUND));
}

bool TMCCInterface::TrainBlowHorn2(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_BLOW_HORN_2));
}

//bool TMCCInterface::TrainAux1Off(TrainHandle id)
//{
//  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_1_OFF));
//}

bool TMCCInterface::TrainAux1Option1(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_1_OPTION_1));
}

bool TMCCInterface::TrainAux1Option2(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_1_OPTION_2));
}

//bool TMCCInterface::TrainAux1On(TrainHandle id)
//{
//  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_1_ON));
//}

bool TMCCInterface::TrainSetAux1(TrainHandle id, TMCCActiveState state)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, state == TMCC_ON ? EC_AUX_1_ON : EC_AUX_1_OFF));
}

//bool TMCCInterface::TrainAux2Off(TrainHandle id)
//{
//  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_2_OFF));
//}

bool TMCCInterface::TrainAux2Option1(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_2_OPTION_1));
}

bool TMCCInterface::TrainAux2Option2(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_2_OPTION_2));
}

//bool TMCCInterface::TrainAux2On(TrainHandle id)
//{
//  return SendTMCC1Command(TrainCommand(id, CT_ACTION, EC_AUX_2_ON));
//}

bool TMCCInterface::TrainSetAux2(TrainHandle id, TMCCActiveState state)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, state == TMCC_ON ? EC_AUX_2_ON : EC_AUX_2_OFF));
}

bool TMCCInterface::TrainNumericCommand(TrainHandle id, uint8 cmd)
{
  return SendTMCC1Command(TrainCommand(id, CT_ACTION, (EngineCommandParams)(EC_NUMERIC_FLAG | (EC_NUMERIC_MASK & cmd))));
}

bool TMCCInterface::EngineSetMomentumLow(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, EEC_SET_MOMENTUM_LOW));
}

bool TMCCInterface::EngineSetMomentumMedium(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, EEC_SET_MOMENTUM_MEDIUM));
}

bool TMCCInterface::EngineSetMomentumHigh(EngineHandle id)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, EEC_SET_MOMENTUM_HIGH));
}

bool TMCCInterface::EngineSetAbsoluteSpeed(EngineHandle id, int speed)
{
  if (speed < 0) speed = 0;
  if (speed > 31) speed = 31;

  return SendTMCC1Command(EngineCommand(id, CT_ABSOLUTE_SPEED, (EngineCommandParams)(speed & 0b11111)));
}

bool TMCCInterface::EngineSetRelativeSpeed(EngineHandle id, int speed)
{
  if (speed > 5) speed = 5;
  if (speed < -5) speed = -5;

  return SendTMCC1Command(EngineCommand(id, CT_RELATIVE_SPEED, (EngineCommandParams)((speed + 5) & 0b01111)));
}

bool TMCCInterface::TrainSetMomentumLow(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_EXTENDED, ETC_SET_MOMENTUM_LOW));
}

bool TMCCInterface::TrainSetMomentumMedium(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_EXTENDED, ETC_SET_MOMENTUM_MEDIUM));
}

bool TMCCInterface::TrainSetMomentumHigh(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_EXTENDED, ETC_SET_MOMENTUM_HIGH));
}

bool TMCCInterface::TrainSetAbsoluteSpeed(TrainHandle id, int speed)
{
  if (speed < 0) speed = 0;
  if (speed > 31) speed = 31;

  return SendTMCC1Command(TrainCommand(id, CT_ABSOLUTE_SPEED, (EngineCommandParams)(speed & 0b11111)));
}

bool TMCCInterface::TrainSetRelativeSpeed(TrainHandle id, int speed)
{
  if (speed > 5) speed = 5;
  if (speed < -5) speed = -5;

  return SendTMCC1Command(TrainCommand(id, CT_RELATIVE_SPEED, (EngineCommandParams)((speed + 5) & 0b01111)));
}

bool TMCCInterface::EngineAssignToTrain(EngineHandle id, TrainHandle trainAddress)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, (EngineCommandParams)(EEC_ASSIGN_TO_TRAIN_FLAG | (trainAddress & EEC_ASSIGN_TO_TRAIN_MASK))));
}

bool TMCCInterface::EngineAssignAsSingleUnit(EngineHandle id, TMCCDirection dir)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, dir == TMCC_FORWARD ? EEC_ASSIGN_AS_SINGLE_UNIT_FORWARD : EEC_ASSIGN_AS_SINGLE_UNIT_REVERSE));
}

bool TMCCInterface::EngineAssignAsHeadEndUnit(EngineHandle id, TMCCDirection dir)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, dir == TMCC_FORWARD ? EEC_ASSIGN_AS_HEAD_END_UNIT_FORWARD : EEC_ASSIGN_AS_HEAD_END_UNIT_REVERSE));
}

bool TMCCInterface::EngineAssignAsMiddleUnit(EngineHandle id, TMCCDirection dir)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, dir == TMCC_FORWARD ? EEC_ASSIGN_AS_MIDDLE_UNIT_FORWARD : EEC_ASSIGN_AS_MIDDLE_UNIT_REVERSE));
}

bool TMCCInterface::EngineAssignAsRearEndUnit(EngineHandle id, TMCCDirection dir)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, dir == TMCC_FORWARD ? EEC_ASSIGN_AS_REAR_END_UNIT_FORWARD : EEC_ASSIGN_AS_REAR_END_UNIT_REVERSE));
}

bool TMCCInterface::EngineSetEngineAddress(TMCCAddress id)
{
  return SendTMCC1Command(EngineCommand(id, CT_EXTENDED, EEC_SET_ENGINE_ADDRESS));
}

bool TMCCInterface::TrainSetTMCCAddress(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_EXTENDED, ETC_SET_TMCC_ADDRESS));
}

bool TMCCInterface::TrainClearConsist(TrainHandle id)
{
  return SendTMCC1Command(TrainCommand(id, CT_EXTENDED, ETC_CLEAR_CONSIST));
}



struct EngineCommand2 : public TMCC2Command
{
  EngineCommand2(EngineHandle address, EngineCommand2Params params)
  {
    //data |= 0 << 14;

    data |= (uint16)(address & 0b01111111) << 9;
    data |= (params & 0b111111111);
  }

  EngineCommand2(TrainHandle address, EngineCommand2Params params)
  {
    //data |= 0 << 14;

    data |= (uint16)(address & 0b01111111) << 9;
    data |= (params & 0b111111111);
  }
};


bool TMCCInterface::SendEngineCommand2(EngineHandle id, EngineCommand2Params command)
{
  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)command));

}


bool TMCCInterface::EngineBlowHorn1TMCC2(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_BLOW_HORN_1));
}

bool TMCCInterface::EngineBlowHorn2TMCC2(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_BLOW_HORN_2));
}

bool TMCCInterface::EngineSetAbsoluteSpeed2(EngineHandle id, int speed)
{
  if (speed < 0) speed = 0;
  if (speed > 199) speed = 199;
  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(speed & EC2_SET_ABSOLUTE_SPEED_STEP_MASK)));
}

//bool TMCCInterface::EngineAux1Off_2(EngineHandle id)
//{
//  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX1_OFF));
//}

bool TMCCInterface::EngineAux1Option1_2(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX_1_OPTION_1));
}

bool TMCCInterface::EngineAux1Option2_2(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX_1_OPTION_2));
}

//bool TMCCInterface::EngineAux1On_2(EngineHandle id)
//{
//  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX1_ON));
//}

bool TMCCInterface::EngineSetAux1_2(EngineHandle id, TMCCActiveState state)
{
    return SendEngineCommand(EngineCommand2(id, state == TMCC_ON ? (EngineCommand2Params)EC2_AUX_1_ON : (EngineCommand2Params)EC2_AUX_1_OFF));
}

//bool TMCCInterface::EngineAux2Off_2(EngineHandle id)
//{
//  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX1_OFF));
//}

bool TMCCInterface::EngineAux2Option1_2(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX_2_OPTION_1));
}

bool TMCCInterface::EngineAux2Option2_2(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX_2_OPTION_2));
}

bool TMCCInterface::EngineAux3Trigger(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX_3));
}

//bool TMCCInterface::EngineAux2On_2(EngineHandle id)
//{
//  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_AUX2_ON));
//}

bool TMCCInterface::EngineSetAux2_2(EngineHandle id, TMCCActiveState state)
{
    return SendEngineCommand(EngineCommand2(id, state == TMCC_ON ? (EngineCommand2Params)EC2_AUX_2_ON : (EngineCommand2Params)EC2_AUX_2_OFF));
}

bool TMCCInterface::SendGenericCommand(EngineHandle id, uint16 cmd)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)cmd));
}

bool TMCCInterface::EngineNumericCommand2(EngineHandle id, uint8 cmd)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_NUMERIC_FLAG | (EC2_NUMERIC_MASK & cmd))));
}

bool TMCCInterface::EngineCrewTalkSpeed(EngineHandle id)
{
    return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)EC2_CREWTALK_SPEED_VAR));
}

bool TMCCInterface::EngineSetMomentum2(EngineHandle id, int momentum)
{
  if (momentum < 0) momentum = 0;
  if (momentum > 7) momentum = 7;
  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_SET_MOMENTUM_FLAG | (momentum & EC2_SET_MOMENTUM_MASK))));
}

bool TMCCInterface::EngineSetBrakeLevel2(EngineHandle id, int brake)
{
  if (brake < 0) brake = 0;
  if (brake > 7) brake = 7;
  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_BRAKE_LEVEL_FLAG | (brake & EC2_BRAKE_LEVEL_MASK))));
}

bool TMCCInterface::EngineSetBoostLevel2(EngineHandle id, int boost)
{
  if (boost < 0) boost = 0;
  if (boost > 7) boost = 7;
  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_BOOST_LEVEL_FLAG | (boost & EC2_BOOST_LEVEL_MASK))));
}

bool TMCCInterface::EngineSetTrainBrake2(EngineHandle id, int brake)
{
  if (brake < 0) brake = 0;
  if (brake > 7) brake = 7;
  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_TRAIN_BRAKE_FLAG | (brake & EC2_TRAIN_BRAKE_MASK))));
}

bool TMCCInterface::EngineSetStall2(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_SET_STALL));
}

bool TMCCInterface::EngineStopImmediate2(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_STOP_IMMEDIATE));
}

bool TMCCInterface::TrainSetAbsoluteSpeed2(TrainHandle id, int speed)
{
  if (speed < 0) speed = 0;
  if (speed > 199) speed = 199;
  return SendTrainCommand(EngineCommand2(id, (EngineCommand2Params)(speed & EC2_SET_ABSOLUTE_SPEED_STEP_MASK)));
}

bool TMCCInterface::TrainSetMomentum2(TrainHandle id, int momentum)
{
  if (momentum < 0) momentum = 0;
  if (momentum > 7) momentum = 7;
  return SendTrainCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_SET_MOMENTUM_FLAG | (momentum & EC2_SET_MOMENTUM_MASK))));
}

bool TMCCInterface::TrainSetBrakeLevel2(TrainHandle id, int brake)
{
  if (brake < 0) brake = 0;
  if (brake > 7) brake = 7;
  return SendTrainCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_BRAKE_LEVEL_FLAG | (brake & EC2_BRAKE_LEVEL_MASK))));
}

bool TMCCInterface::TrainSetBoostLevel2(TrainHandle id, int boost)
{
  if (boost < 0) boost = 0;
  if (boost > 7) boost = 7;
  return SendTrainCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_BOOST_LEVEL_FLAG | (boost & EC2_BOOST_LEVEL_MASK))));
}

bool TMCCInterface::TrainSetTrainBrake2(TrainHandle id, int brake)
{
  if (brake < 0) brake = 0;
  if (brake > 7) brake = 7;
  return SendTrainCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_TRAIN_BRAKE_FLAG | (brake & EC2_TRAIN_BRAKE_MASK))));
}

bool TMCCInterface::TrainSetStall2(TrainHandle id)
{
  return SendTrainCommand(EngineCommand2(id, EC2_SET_STALL));
}

bool TMCCInterface::TrainStopImmediate2(TrainHandle id)
{
  return SendTrainCommand(EngineCommand2(id, EC2_STOP_IMMEDIATE));
}

bool TMCCInterface::EngineSetDieselRunLevel(EngineHandle id, int level)
{
  if (level < 0) level = 0;
  if (level > 7) level = 7;

  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_DIESEL_RUN_LEVEL_FLAG | (level & EC2_DIESEL_RUN_LEVEL_MASK))));
}

bool TMCCInterface::EngineRSTriggerWaterInjector(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_RS_TRIGGER_WATER_INJECTOR));
}

bool TMCCInterface::EngineSetBellSliderPosition(EngineHandle id, int position)
{
  if (position < 2) position = 2;
  if (position > 5) position = 5;

  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_BELL_SLIDER_POSITION_FLAG | (position & EC2_BELL_SLIDER_POSITION_MASK))));
}

bool TMCCInterface::EngineSetLabor(EngineHandle id, int labor)
{
  if (labor < 0) labor = 0;
  if (labor > 31) labor = 31;

  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_ENGINE_LABOR_FLAG | (labor & EC2_ENGINE_LABOR_MASK))));
}

bool TMCCInterface::EngineSetQuillingHornIntensity(EngineHandle id, int intensity)
{
  if (intensity < 0) intensity = 0;
  if (intensity > 16) intensity = 16;

  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_QUILLING_HORN_INTENSITY_FLAG | (intensity & EC2_QUILLING_HORN_INTENSITY_MASK))));
}

bool TMCCInterface::EngineBellOneShotDing(EngineHandle id, int x)
{
  if (x < 0) x = 0;
  if (x > 3) x = 3;

  return SendEngineCommand(EngineCommand2(id, (EngineCommand2Params)(EC2_BELL_ONESHOT_DING_FLAG | (x & EC2_BELL_ONESHOT_DING_MASK))));
}

bool TMCCInterface::EngineSetBell(EngineHandle id, TMCCActiveState state)
{
  return SendEngineCommand(EngineCommand2(id, state == TMCC_ON ? EC2_BELL_ON : EC2_BELL_OFF));
}

bool TMCCInterface::EngineBrakeSquealSound(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_BRAKE_SQUEAL_SOUND));
}

bool TMCCInterface::EngineAugerSound(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_AUGER_SOUND));
}

bool TMCCInterface::EngineRSTriggerBrakeAirRelease(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_RS_TRIGGER_BRAKE_AIR_RELEASE));
}

bool TMCCInterface::EngineRSTriggerShortLetOff(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_RS_TRIGGER_SHORT_LETOFF));
}

bool TMCCInterface::EngineRSTriggerLongLetOff(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_RS_TRIGGER_LONG_LETOFF));
}

bool TMCCInterface::EngineStartupSequence1(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_STARTUP_SEQUENCE_1));
}

bool TMCCInterface::EngineStartupSequence2(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_STARTUP_SEQUENCE_2));
}

bool TMCCInterface::EngineShutdownSequence1(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_SHUTDOWN_SEQUENCE_1));
}

bool TMCCInterface::EngineShutdownSequence2(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_SHUTDOWN_SEQUENCE_2));
}

bool TMCCInterface::EngineSetDinerLight(EngineHandle id, TMCCActiveState state)
{
  return SendEngineCommand(EngineCommand2(id, state == TMCC_ON ? EC2_STATION_DINER_LIGHTS_ON : EC2_STATION_DINER_LIGHTS_OFF));
}

bool TMCCInterface::EngineBlowAirHorn(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_RS_TRIGGER_AUX_AIR_HORN_UNUSED));
}

bool TMCCInterface::EngineLocoRefuelSound(EngineHandle id)
{
  return SendEngineCommand(EngineCommand2(id, EC2_LOCOMOTIVE_REFUELING_SOUND));
}

bool TMCCInterface::EngineDialogCommand(EngineHandle id, DialogCommandParams x)
{
    return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_DIALOG_TRIGGERS, x));
}

bool TMCCInterface::EngineEffectCommand(EngineHandle id, EffectCommandParams x)
{
    return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_EFFECT_TRIGGERS, x));
}

bool TMCCInterface::EngineSetSmokeSystem(EngineHandle id, int level)
{
  if (level < 0)
    level = 0;
  if (level > 3)
    level = 3;
  EffectCommandParams smokeLevel = level == 0 ? FX_SMOKE_SYSTEM_OFF : (level == 1 ? FX_SMOKE_SYSTEM_LOW : (level == 2 ? FX_SMOKE_SYSTEM_MEDIUM : FX_SMOKE_SYSTEM_HIGH));
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, smokeLevel));
}


bool TMCCInterface::EngineSetMasterVolume(EngineHandle id, TMCCActiveState state)
{
    return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_EFFECT_TRIGGERS, state == TMCC_ON ? ET_MASTER_VOLUME_UP : ET_MASTER_VOLUME_DOWN));
}

bool TMCCInterface::EngineSetFrontPantograph(EngineHandle id, TMCCPantographState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, state == TMCC_UP ? FX_PANTOGRAPH_FRONT_UP : FX_PANTOGRAPH_FRONT_DOWN));
}



bool TMCCInterface::EngineSetRearPantograph(EngineHandle id, TMCCPantographState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, state == TMCC_UP ? FX_PANTOGRAPH_REAR_UP : FX_PANTOGRAPH_REAR_DOWN));
}

bool TMCCInterface::EngineSetSubwayDoorState(EngineHandle id, TMCCSide side, TMCCDoorState state)
{
  EffectCommandParams params = side == TMCC_LEFT ? (state == TMCC_OPEN ? FX_SUBWAY_LEFT_DOOR_OPEN : FX_SUBWAY_LEFT_DOOR_CLOSE) : (state == TMCC_OPEN ? FX_SUBWAY_RIGHT_DOOR_OPEN : FX_SUBWAY_RIGHT_DOOR_CLOSE);
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, params));
}

bool TMCCInterface::EngineSetStockCarOption1(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, state == TMCC_ON ? FX_STOCK_CAR_OPTION_1_ON : FX_STOCK_CAR_OPTION_1_OFF));
}

bool TMCCInterface::EngineSetStockCarOption2(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, state == TMCC_ON ? FX_STOCK_CAR_OPTION_2_ON : FX_STOCK_CAR_OPTION_2_OFF));
}

bool TMCCInterface::EngineStockCarLoad(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, FX_STOCK_CAR_LOAD));
}

bool TMCCInterface::EngineStockCarUnload(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, FX_STOCK_CAR_UNLOAD));
}

bool TMCCInterface::EngineStockCarSetFRED(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, state == TMCC_ON ? FX_STOCK_CAR_FRED_ON : FX_STOCK_CAR_FRED_OFF));
}

bool TMCCInterface::EngineStockCarSetFlatWheel(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, state == TMCC_ON ? FX_STOCK_CAR_FLAT_WHEEL_ON : FX_STOCK_CAR_FLAT_WHEEL_OFF));
}

bool TMCCInterface::EngineStockCarSetGame(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, state == TMCC_ON ? FX_STOCK_CAR_GAME_ON : FX_STOCK_CAR_GAME_OFF));
}

bool TMCCInterface::EngineSetDoghouseLight(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_DOGHOUSE_LIGHT_ON : LT_DOGHOUSE_LIGHT_OFF));
}

bool TMCCInterface::EngineSetHazardLight(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_HAZARD_LIGHT_ON : LT_HAZARD_LIGHT_OFF));
}

bool TMCCInterface::EngineSetHazardLightAuto(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, LT_HAZARD_LIGHT_AUTO));
}

bool TMCCInterface::EngineSetDitchLights(EngineHandle id, TMCCActiveState state, TMCCActiveState pulse)
{
  LightingCommandParams params = state == TMCC_ON ? (pulse == TMCC_ON ? LT_DITCH_LIGHTS_ON : LT_DITCH_LIGHTS_ON_PULSE_OFF) : (pulse == TMCC_ON ? LT_DITCH_LIGHTS_OFF_PULSE_ON : LT_DITCH_LIGHTS_OFF);
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, params));
}

bool TMCCInterface::EngineSetLocoMarkerLight(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_LOCO_MARKER_LIGHT_ON : LT_LOCO_MARKER_LIGHT_OFF));
}

bool TMCCInterface::EngineSetTenderMarkerLight(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_TENDER_MARKER_LIGHT_ON : LT_TENDER_MARKER_LIGHT_OFF));
}

bool TMCCInterface::EngineSetGroundLights(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_GROUND_LIGHTS_ON : LT_GROUND_LIGHTS_OFF));
}

bool TMCCInterface::EngineSetGroundLightsAuto(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, LT_GROUND_LIGHTS_AUTO));
}

bool TMCCInterface::EngineSetStrobeLight(EngineHandle id, int strobe)
{
  LightingCommandParams params = strobe == 0 ? LT_STROBE_LIGHT_OFF : (strobe == 1 ? LT_STROBE_LIGHT_ON_SINGLE_FLASH : LT_STROBE_LIGHT_ON_DOUBLE_FLASH);
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, params));
}

bool TMCCInterface::EngineSetMarsLight(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_MARS_LIGHT_ON : LT_MARS_LIGHT_OFF));
}

bool TMCCInterface::EngineSetCabLight(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_CAB_LIGHT_ON : LT_CAB_LIGHT_OFF));
}

bool TMCCInterface::EngineSetCabLightAuto(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, LT_CAB_LIGHT_AUTO));
}

bool TMCCInterface::EngineSetRule17(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_RULE_17_ON : LT_RULE_17_OFF));
}

bool TMCCInterface::EngineSetRule17Auto(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, LT_RULE_17_AUTO));
}

bool TMCCInterface::EngineSetCarCabinLight(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, state == TMCC_ON ? LT_CAR_CABIN_LIGHT_ON : LT_CAR_CABIN_LIGHT_OFF));
}

bool TMCCInterface::EngineSetCarCabinLightAuto(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_LIGHTING_CONTROLS, LT_CAR_CABIN_LIGHT_AUTO));
}

bool TMCCInterface::SoundMaskControl(SoundMaskingControl dialog, SoundMaskingControl signal)
{
  uint16 params = ((signal & 0x3) << 2) | (dialog & 0x3);
  //printf("dialog: %d signal: %d params: %d\n", dialog, signal, params);
  return params;
}

bool TMCCInterface::EngineToggleSequenceCtrl(EngineHandle id, TMCCActiveState state)
{
  // First need to send the Enable Sequence control command, followed by the Railsounds dialogue command for it otherwise the crewtalk/whistle will not enable. 
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_EFFECT_TRIGGERS, state == TMCC_ON ? ET_SEQUENCE_CONTROL_ON : ET_SEQUENCE_CONTROL_OFF));
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_DIALOG_TRIGGERS, state == TMCC_ON ? DC_SEQ_CONTROL_ENABLE : DC_SEQ_CONTROL_DISABLE));

}

bool TMCCInterface::EngineToggleCylinderCock(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_EFFECT_TRIGGERS, state == TMCC_ON ? ET_CYL_COCK_CLEAR_ON : ET_CYL_COCK_CLEAR_OFF));
}

bool TMCCInterface::EngineTogglePrimeMover(EngineHandle id, TMCCActiveState state)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_EFFECT_TRIGGERS, state == TMCC_ON ? ET_PRIME_MOVER_ON : ET_PRIME_MOVER_OFF));
}

bool TMCCInterface::EngineSoundMaskControl(EngineHandle id, SoundMaskingControl dialog, SoundMaskingControl signal)
{
  uint16 params = ((signal & 0x3) << 2) | (dialog & 0x3);
  //printf("dialog: %d signal: %d params: %d\n", dialog, signal, params);
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_MASKING_CONTROL, params));
}

bool TMCCInterface::EngineSoundMaskPlayNever(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_MASKING_CONTROL, 0x0A));
}

bool TMCCInterface::EngineSoundMaskPlayAlways(EngineHandle id)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_MASKING_CONTROL, 0x05));
}

bool TMCCInterface::EnginePlayRailsoundsEffectTrigger(EngineHandle id, RailSoundsEffectTriggerParams command)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RAILSOUNDS_EFFECT_TRIGGERS, (RailSoundsEffectTriggerParams)command));
}

bool TMCCInterface::EnginePlayEffectCommand(EngineHandle id, EffectCommandParams command)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_EFFECTS_CONTROLS, (EffectCommandParams)command));
}

// Unknown reserved areas. Must do more testing. 

bool TMCCInterface::EngineReservedArea3(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_3, x));
}
bool TMCCInterface::EngineReservedArea5(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_5, x));
}
bool TMCCInterface::EngineReservedArea7(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_7, x));
}
bool TMCCInterface::EngineReservedArea8(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_8, x));
}
bool TMCCInterface::EngineReservedArea9(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_9, x));
}
bool TMCCInterface::EngineReservedArea10(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_10, x));
}
bool TMCCInterface::EngineReservedArea11(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_11, x));
}
bool TMCCInterface::EngineReservedArea12(EngineHandle id, int x)
{
  return SendMultiWordCommand(MultiWordCommand(id, PI_RESERVED_12, x));
}



bool PDIInterface::EngineBlowHorn1(EngineHandle id)
{
  return SendTMCC1PDICommand(0x27, EngineCommand(id, CT_ACTION, EC_BLOW_HORN_1));
}
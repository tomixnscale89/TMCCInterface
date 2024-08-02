#include "PDIInterface.h"
#include "TMCCInterface.h"

static std::vector<DeviceInfo> s_devices;
static DeviceInfo* s_currentDevice;







bool PDIInterface::SendTMCC1PDICommand(uint8 command, const TMCCCommand& cmd)
{
  uint8 checksum = 0;
  uint8 byte1 = (cmd.data) >> 8;
  uint8 byte2 = (cmd.data) & 0b11111111;

  if (byte1 == 0xD1 || byte1 == 0xDF && byte2 == 0xD1 || byte2 == 0xDF) // if both first and second bits have the SOP or EOP
  {
    ubyte data[9]; // max could be 9 bytes if byte 1 and byte 2 both need to be stuffed. 
    data[0] = 0xD1; // START OF PACKET
    data[1] = command;
    data[2] = TMCC1_PREFIX;
    data[3] = 0xDE;
    *((uint16*)&data[4]) = bswap_16(byte1);
    data[5] = 0xDE;
    *((uint16*)&data[6]) = bswap_16(byte2);
    checksum = data[1] + data[2] + data[3] + data[3] + data[4] + data[5] + data[6];
    checksum = 0 - checksum;

    *((uint16*)&data[7]) = bswap_16(checksum);
    data[8] = 0xDF; // END OF PACKET

    printf("%04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x\n",data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
    return WriteData(data, 9);
  }

  else // otherwise check if first or second
  {
    if (byte1 == 0xD1 || byte1 == 0xDF) // only first byte needs to be stuffed
    {
      ubyte data[8]; // max could be 9 bytes if byte 1 and byte 2 both need to be stuffed. 
      data[0] = 0xD1; // START OF PACKET
      data[1] = command;
      data[2] = TMCC1_PREFIX;
      data[3] = 0xDE;
      *((uint16*)&data[4]) = bswap_16(byte1);
      *((uint16*)&data[5]) = bswap_16(byte2);
      checksum = data[1] + data[2] + data[3] + data[3] + data[4] + data[5];
      checksum = 0 - checksum;

      *((uint16*)&data[6]) = bswap_16(checksum);
      data[7] = 0xDF; // END OF PACKET
      printf("%04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
      return PDIInterface::WriteData(data, 8);
    }
    else if (byte2 == 0xD1 || byte2 == 0xDF) // only second byte needs to be stuffed
    {
      ubyte data[8]; // max could be 9 bytes if byte 1 and byte 2 both need to be stuffed. 
      data[0] = 0xD1; // START OF PACKET
      data[1] = command;
      data[2] = TMCC1_PREFIX;
      *((uint16*)&data[3]) = bswap_16(byte1);
      data[4] = 0xDE;
      *((uint16*)&data[5]) = bswap_16(byte2);
      checksum = data[1] + data[2] + data[3] + data[3] + data[4] + data[5];
      checksum = 0 - checksum;

      *((uint16*)&data[6]) = bswap_16(checksum);
      data[7] = 0xDF; // END OF PACKET
      printf("%04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
      return PDIInterface::WriteData(data, 8);
    }
    else // no bytes are stuffed
    {
      ubyte data[7]; // no bytes stuffed
      data[0] = 0xD1;
      data[1] = command;
      data[2] = TMCC1_PREFIX;
      //*((uint16*)&data[3]) = bswap_16(byte1);
      //*((uint16*)&data[4]) = bswap_16(byte2);
      data[3] = byte1;
      data[4] = byte2;
      checksum = data[1] + data[2] + data[3] + data[3] + data[4];
      checksum = 0 - checksum;

      //*((uint16*)&data[5]) = bswap_16(checksum);
      data[5] = checksum;
      data[6] = 0xDF; // END OF PACKET
      printf("%04x, %04x, %04x, %04x, %04x, %04x, %04x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
      return PDIInterface::WriteData(data, 7);
    }
  }
}


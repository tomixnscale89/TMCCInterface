#ifndef _PDI_INTERFACE_H_
#define _PDI_INTERFACE_H_
#pragma once

#include "types.h"
#include "TMCCInterface.h"


typedef uint8 PDIAddress;

// handles
struct PDIHandle
{
private:
  PDIAddress m_addr;

public:
  PDIHandle()
    : m_addr{}
  {
  }

  PDIHandle(PDIAddress addr)
    : m_addr(addr)
  {
  }

  operator PDIAddress& () { return m_addr; }
  //operator TMCCAddress() { return m_addr; }
};

struct SourceIDHandle : public PDIHandle { using PDIHandle::PDIHandle; };
struct DestinationIDHandle : public PDIHandle { using PDIHandle::PDIHandle; };

//typedef TMCCAddress EngineHandle;
//typedef TMCCAddress TrainHandle;
//typedef TMCCAddress SwitchHandle;
//typedef TMCCAddress RouteHandle;
//typedef TMCCAddress AccessoryHandle;
//typedef TMCCAddress GroupHandle;

struct PDICommand
{
  uint8 destination_id;
  uint8 source_id;
  uint8 command;
  uint8 address;
  uint8 data_action;
  uint64 data;

  PDICommand()
    : source_id(0),
      destination_id(0),
      command(0),
      address(0),
      data_action(0),
      data(0)
  {
  }

  PDICommand(SourceIDHandle _src_identifier, DestinationIDHandle _dst_identifier, uint8 _command, uint8 _address, uint8 _data_action, uint64 _data)
    : source_id(_src_identifier)
    , destination_id(_dst_identifier)
    , command(_command)
    , address(_address)
    , data_action(_data_action)
    , data(_data)
  {
  }

  PDICommand(uint8 _command, uint16 _data) // TMCC PDI COMMAND
    : command(_command)
    , data(_data)
  {
  }

};

enum TMCC_PREFIX :uint8
{
  TMCC1_PREFIX = 0xFE,
  TMCC2_ENGINE_PREFIX = 0xF8,
  TMCC2_TRAIN_PREFIX = 0xF9,
};

enum PDI_COMMAND : uint8
{

  PDI_CMD_ALL_GET = 0x01,
  PDI_CMD_ALL_SET = 0x02,
  PDI_CMD_BASEDATA_ENGINE = 0x20, //Engine data to update in database
  PDI_CMD_BASEDATA_TRAIN = 0x21, //Train data to update in database
  PDI_CMD_BASEDATA_ACC = 0x22, //Accessory data to update in database
  PDI_CMD_BASEDATA_BASE = 0x23, //Base data to update in database
  PDI_CMD_BASEDATA_ROUTE = 0x24, //Route data to update in database
  PDI_CMD_BASEDATA_SWITCH = 0x25, //Switch data to update in database
  PDI_CMD_BASEDATA_MEMORY = 0x26, //Read / write base data memory
  PDI_CMD_TX_TMCC_DATA = 0x27, //TMCC data to send to base
  PDI_CMD_RX_TMCC_DATA = 0x28, //TMCC data received from base
  PDI_CMD_PING = 0x29, //Ping to verify system is connected to base
  PDI_CMD_UPDATESPEED_ENGINE = 0x2A, //Update base / cab2 with new engine speed
  PDI_CMD_UPDATESPEED_TRAIN = 0x2B, //Update base / cab2 with new train speed

};

enum PDI_ACTION_CODE : uint8
{

  PDI_ACTION_FIRMWARE = 0x01, // RETURN FIRMWARE REVISION
  PDI_ACTION_STATUS = 0x02, // RETURN MODULE STATUS
  PDI_ACTION_CONFIG = 0x03, // GET OR SET NON-VOLATILE CONFIGURATION
  PDI_ACTION_INFO = 0x04, // RETURN MODULE INFORMATION
  PDI_ACTION_CLEAR_ERRORS = 0x05, // CLEAR ERROR FLAG
  PDI_ACTION_RESET = 0x06, // RESET MODULE
  PDI_ACTION_IDENTITY = 0x07, // TURN ON LED


};


class PDIInterface
{
public:

  TMCC_API static int EnumerateDevicesPDI(DeviceInfo** devices);

  TMCC_API static bool InitPDI(int device);
  TMCC_API static void ShutdownPDI();
   

  TMCC_API static bool SendTMCC1PDICommand(uint8 command, const TMCCCommand& cmd);
  TMCC_API static bool EngineBlowHorn1(EngineHandle id);
  


protected:
  static bool WriteData(ubyte* pData, int length);
  //static bool SendTMCC1PDICommand(uint8 command, const TMCCCommand& cmd)

};

#endif // _PDI_INTERFACE_H_
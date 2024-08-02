
#include <iostream>
#include <boost/asio.hpp>
//#include <websocketpp/config/asio_no_tls.hpp>
//#include <websocketpp/server.hpp>
#include "TMCCInterface.h"

using namespace boost::asio;
using ip::tcp;

//#ifdef WINDOWS
//#include <WinSock2.h>
//#else // !WINDOWS
//#include <netinet/in.h>
//#include <sys/socket.h>
//#endif // !WINDOWS

//typedef websocketpp::server<websocketpp::config::asio> ws_server;
//typedef ws_server::message_ptr message_ptr;
//using websocketpp::lib::placeholders::_1;
//using websocketpp::lib::placeholders::_2;
//
//static void on_message(ws_server* s, websocketpp::connection_hdl hdl, message_ptr msg);
#define PORT 8080
#define DEBUG_DEVICE 9999
static bool s_debug = false;
//static int s_engine = 0;
static bool s_legacy = false;

static void ProcessTMCCCommand(const char* command, const char* data);

int main(int argc, char* argv[])
{

  int deviceID;
  DeviceInfo* devices;
  int numDevices;
  do
  {
    printf("Please select a device:\n");
    numDevices = TMCCInterface::EnumerateDevices(&devices);
    for (int i = 0; i < numDevices; i++)
    {
      printf("%d: %s (%s)\n", i, devices[i].GetFriendlyName(), devices[i].GetPortName());
    }

    if (!scanf("%d", &deviceID))
      deviceID = -1;
  } while ((deviceID < 0 || deviceID >= numDevices)
#ifdef _DEBUG
    && deviceID != DEBUG_DEVICE
#endif // _DEBUG
    );

#ifdef _DEBUG
  s_debug = deviceID == DEBUG_DEVICE;
#endif // DEBUG

  if (!s_debug && !TMCCInterface::Init(deviceID))
  {
    printf("Failed to initialize TMCC.\n");
    return 1;
  }

  boost::asio::io_service io_service;
  //socket creation
  tcp::socket socket(io_service);
  //connection
  socket.connect(tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
  printf("Connected to port %d\n", PORT);

  boost::system::error_code error;

  //const std::string msg = "Hello from Client!\n";
  //boost::asio::write(socket, boost::asio::buffer(msg), error);
  //if (error)
  //{
  //  printf("send failed: %s\n", error.message().c_str());
  //}
  //printf("sent client message\n");

  while (true)
  {
    boost::asio::streambuf receive_buffer;
    //boost::asio::read(socket, receive_buffer, boost::asio::transfer_all(), error);
    boost::asio::read_until(socket, receive_buffer, "\n", error);
   // boost::asio::read(socket, receive_buffer, boost::asio::read_until('\n'), error);
    if (error && error != boost::asio::error::eof)
    {
      printf("Receive failed: %s\n", error.message().c_str());
      break;
    }
    else
    {
      const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());
      //printf("received command '%s'\n", data);
      char buf[512];
      strncpy(buf, data, sizeof(buf));

      //char* cmd = strtok(buf, " ");
      //char* cmd_data = strtok(NULL, " ");
      char* cmd = buf;
      char* cmd_data = NULL;

      int c = 0;
      while (c < sizeof(buf) && buf[c])
      {
        if (buf[c] == ' ')
        {
          buf[c] = '\0';
          cmd_data = &buf[c + 1];
          break;
        }
        c++;
      }

      //printf("Command: '%s', Data: '%s'\n", cmd, cmd_data);
      ProcessTMCCCommand(cmd, cmd_data);
    }
  }

  TMCCInterface::Shutdown();
}

static void ProcessTMCCCommand(const char* command, const char* data)
{
  printf("received command '%s' data '%s'\n", command, data);
  /*if (!strcmp(command, "setEngine"))
  {
    int engine;
    if (sscanf(data, "%d", &engine))
    {
      s_engine = engine;
      printf("Set engine to %d\n", s_engine);
    }
  }
  else */if (!strcmp(command, "setLegacy"))
  {
    int legacy;
    if (sscanf(data, "%d", &legacy) == 1)
    {
      s_legacy = legacy;
      printf("Set legacy to %d\n", legacy);
    }
  }
  else if (!strcmp(command, "numericCommand"))
  {
    int engine;
    int input;
    if (sscanf(data, "%d %d", &engine, &input) == 2)
    {
      if (!s_debug)
        TMCCInterface::EngineNumericCommand(engine, input);
      printf("Numeric command %d for engine %d\n", input, engine);
    }
  }
  else if (!strcmp(command, "setThrottle"))
  {
    int engine;
    float throttle;
    if (sscanf(data, "%d %f", &engine, &throttle) == 2)
    {
      if (!s_debug)
      {
        if (s_legacy)
          TMCCInterface::EngineSetAbsoluteSpeed(engine, (int)(throttle * 32.0f));
        else
          TMCCInterface::EngineSetAbsoluteSpeed2(engine, (int)(throttle * 200.0f));
      }
      printf("Set throttle to %f for engine %d\n", throttle, engine);
    }
  }
  else if (!strcmp(command, "setBrake"))
  {
    int engine;
    float brake;
    if (sscanf(data, "%d %f", &engine, &brake) == 2)
    {
      if (!s_debug && !s_legacy)
        TMCCInterface::EngineSetBrakeLevel2(engine, (int)(brake * 8.0f));
      printf("Set brake to %f for engine %d\n", brake, engine);
    }
  }
  else if (!strcmp(command, "setDirection"))
  {
    int engine;
    int dir;
    if (sscanf(data, "%d %d", &engine, &dir) == 2)
    {
      if (!s_debug)
        TMCCInterface::EngineSetDirection(engine, dir != 0 ? TMCC_FORWARD : TMCC_REVERSE);

      printf("Set direction to %d for engine %d\n", dir, engine);
    }
  }
  else if (!strcmp(command, "setBell"))
  {
    int engine;
    int state;
    if (sscanf(data, "%d %d", &engine, &state) == 2)
    {
      if (!s_debug)
        TMCCInterface::EngineSetBell(engine, state != 0 ? TMCC_ON : TMCC_OFF);

      printf("Setting bell to %d for engine %d\n", state, engine);
    }
  }
  else if (!strcmp(command, "blowHorn"))
  {
    int engine;
    if (sscanf(data, "%d", &engine) == 1)
    {
      if (!s_debug)
      {
        if (!s_legacy)
          TMCCInterface::EngineBlowHorn1(engine);
        else
          TMCCInterface::EngineBlowHorn1TMCC2(engine);
      }
      printf("Blowing horn for engine %d\n", engine);
    }
  }
  else if (!strcmp(command, "setJunctionOut"))
  {
    int junc;
    if (sscanf(data, "%d", &junc) == 1)
    {
      if (!s_debug)
        TMCCInterface::SwitchThrowOut(junc);

      printf("Set switch %d to out\n", junc);
    }
  }
  else if (!strcmp(command, "setJunctionThrough"))
  {
    int junc;
    if (sscanf(data, "%d", &junc) == 1)
    {
      if (!s_debug)
        TMCCInterface::SwitchThrowThrough(junc);

      printf("Set switch %d to through\n", junc);
    }
  }
  else if (!strcmp(command, "dialog"))
  {
    int engine;
    int dialog;
    if (sscanf(data, "%d %d", &engine, &dialog) == 2)
    {
      if (!s_debug)
        TMCCInterface::EngineDialogCommand(engine, (DialogCommandParams)dialog);

      printf("Play dialog %d for engine %d\n", dialog, engine);
    }
  }
  else
  {
    printf("Unrecognized command '%s'\n", command);
  }
}
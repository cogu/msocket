/*****************************************************************************
* \file      MsocketCpp.cpp
* \author    Pierre Svärd
* \date      2017-03-05
* \brief     msocket C++ wrapper
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2014-2017 Pierre Svärd
*
******************************************************************************/

#ifndef MSOCKETPP_H
#define MSOCKETPP_H

extern "C" {
#include "msocket.h"
}

#include <functional>
#include <memory>
#include <string>

class Msocket;

typedef std::shared_ptr<Msocket> MsocketPtr;

class Msocket {
   friend class MsocketServer;
public:

   typedef int8_t ErrorCodeT;
   typedef uint16_t PortNumberT;
   typedef uint8_t BufferDataT;
   typedef uint32_t BufferSizeT;

   enum StateT {
      StateNone = MSOCKET_STATE_NONE,
      StateListening = MSOCKET_STATE_LISTENING,
      StateAccepting = MSOCKET_STATE_ACCEPTING,
      StatePending = MSOCKET_STATE_PENDING,
      StateEstablished = MSOCKET_STATE_ESTABLISHED,
      StateClosing = MSOCKET_STATE_CLOSING,
      StateClosed = MSOCKET_STATE_CLOSED
   };

   enum ModeT {
      ModeNone = MSOCKET_MODE_NONE,
      ModeUDP = MSOCKET_MODE_UDP,
      ModeTCP = MSOCKET_MODE_TCP
   };

   typedef std::function<void(MsocketPtr)> TcpAcceptHandler;
   typedef std::function<void(const std::string&, PortNumberT)> ConnectHandler;
   typedef std::function<uint8_t(const BufferDataT*, BufferSizeT, BufferSizeT&)> TcpDataHandler;
   typedef std::function<void()> DisconnectedHandler;
   typedef std::function<void(uint32_t elapsed)> TcpInactivityHandler;
   typedef std::function<void(const char*, PortNumberT, const BufferDataT*, BufferSizeT)> UdpMsgHandler;

   struct HandlerD {
      TcpAcceptHandler tcpAcceptHandler;
      ConnectHandler connectHandler;
      TcpDataHandler tcpDataHandler;
      DisconnectedHandler disconnectedHandler;
      TcpInactivityHandler tcpInactivityHandler;
      UdpMsgHandler udpMsgHandler;
   };

   Msocket(uint8_t addressFamily);
   virtual ~Msocket();
   virtual void setHandler(HandlerD& handler);
   virtual void close();
   virtual bool connect(const std::string& socketPath, PortNumberT port);
   virtual bool unixConnect(const std::string& socketPath);
   virtual bool send(BufferDataT* msgData, BufferSizeT msgLen);
   virtual bool sendto(const std::string& addr, PortNumberT port, const BufferDataT *msgData, BufferSizeT msgLen);
   virtual bool listen(ModeT mode, const PortNumberT port, const std::string& addr);
   #ifndef _WIN32
   virtual bool unixListen(const std::string& socketPath);
   #endif
   MsocketPtr accept(MsocketPtr childPtr);
   virtual void startIO();
   virtual StateT state();
   virtual ErrorCodeT error() { return m_error; }

protected:

   Msocket(msocket_t* msocket);

   static void onConnectedWrapper(void *arg, const char *addr, uint16_t port);
   static ErrorCodeT onTcpDataWrapper(void *arg, const BufferDataT *dataBuf, BufferSizeT dataLen, BufferSizeT *parseLen);
   static void onDisconnectedWrapper(void *arg);
   static void onTcpInactivityWrapper(uint32_t elapsed);
   static void onUdpMsgWrapper(void *arg, const char *addr, PortNumberT port, const BufferDataT *dataBuf, BufferSizeT dataLen);

   msocket_t* m_msocket;
   HandlerD m_handler;
   ErrorCodeT m_error = 0;
};

#endif /* MSOCKETPP_H */

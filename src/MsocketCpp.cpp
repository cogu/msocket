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

#include <MsocketCpp.h>

Msocket::Msocket(uint8_t addressFamily) {
   m_msocket = msocket_new(addressFamily);
}

Msocket::Msocket(msocket_t* msocket) {
   m_msocket = msocket;
}

Msocket::~Msocket() {
   msocket_delete(m_msocket);
   m_msocket = NULL;
}

void Msocket::setHandler(HandlerD& handler) {

   m_handler = handler;
   msocket_handler_t msocket_handler;
   memset(&msocket_handler, 0, sizeof(msocket_handler));
   msocket_handler.udp_msg = Msocket::onUdpMsgWrapper;
   msocket_handler.tcp_connected = Msocket::onConnectedWrapper;
   msocket_handler.tcp_disconnected = Msocket::onDisconnectedWrapper;
   msocket_handler.tcp_data = Msocket::onTcpDataWrapper;
   msocket_handler.tcp_inactivity = Msocket::onTcpInactivityWrapper;

   msocket_sethandler(m_msocket, &msocket_handler, (void*)(&m_handler));
}

void Msocket::close() {
   msocket_close(m_msocket);
}

bool Msocket::connect(const std::string& socketPath, PortNumberT port) {
   m_error = msocket_connect(m_msocket, socketPath.c_str(), port);
   return m_error == 0;
}

bool Msocket::unixConnect(const std::string& socketPath) {
   m_error = msocket_unix_connect(m_msocket, socketPath.c_str());
   return m_error == 0;
}

bool Msocket::send(BufferDataT* msgData, BufferSizeT msgLen) {
   m_error = msocket_send(m_msocket, msgData, msgLen);
   return m_error == 0;
}

bool Msocket::sendto(const std::string& addr, PortNumberT port, const BufferDataT *msgData, BufferSizeT msgLen) {
   m_error = msocket_sendto(m_msocket, addr.c_str(), port, msgData, msgLen);
   return m_error == 0;
}

bool Msocket::listen(ModeT mode, PortNumberT port, const std::string& addr) {
   m_error = msocket_listen(m_msocket, mode, port, addr.c_str());
   return m_error == 0;
}

#ifndef _WIN32
bool Msocket::unixListen(const std::string& socketPath) {
   m_error = msocket_unix_listen(m_msocket, socketPath.c_str());
   return m_error == 0;
}
#endif

MsocketPtr Msocket::accept(MsocketPtr childPtr) {

   msocket_t* msocket = NULL;
   if(childPtr.get()) {
      msocket = childPtr->m_msocket;
   }
   msocket_t* child_msocket = msocket_accept(m_msocket, msocket);

   return MsocketPtr(new Msocket(child_msocket));
}

void Msocket::startIO() {
   msocket_start_io(m_msocket);
}

Msocket::StateT Msocket::state() {
   return static_cast<Msocket::StateT>(msocket_state(m_msocket));
}

void Msocket::onConnectedWrapper(void *arg, const char *addr, PortNumberT port) {
   HandlerD* handler = static_cast<HandlerD*>(arg);
   std::string addrString(addr);
   if(handler->connectHandler) {
      handler->connectHandler(addrString, port);
   }
}

int8_t Msocket::onTcpDataWrapper(void *arg, const BufferDataT *dataBuf, BufferSizeT dataLen, BufferSizeT *parseLen) {
   HandlerD* handler = static_cast<HandlerD*>(arg);
   if(handler->tcpDataHandler) {
      if( handler->tcpDataHandler(dataBuf, dataLen, *parseLen) ) {
         return 0;
      }
   }
   return -1;
}

void Msocket::onDisconnectedWrapper(void *arg) {
   HandlerD* handler = static_cast<HandlerD*>(arg);
   if(handler->disconnectedHandler) {
      handler->disconnectedHandler();
   }
}

void Msocket::onTcpInactivityWrapper(uint32_t elapsed) {
   // Oops no handler
}

void Msocket::onUdpMsgWrapper(void *arg, const char *addr, PortNumberT port, const BufferDataT *dataBuf, BufferSizeT dataLen) {
   HandlerD* handler = static_cast<HandlerD*>(arg);
   if(handler->udpMsgHandler) {
      handler->udpMsgHandler(addr, port, dataBuf, dataLen);
   }
}

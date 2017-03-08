/*****************************************************************************
* \file      MSocket.cpp
* \author    Pierre Svärd
* \date      2017-03-05
* \brief     msocket C++ wrapper
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2017 Pierre Svärd
*
******************************************************************************/

#include <MSocket.hpp>

MSocket::MSocket(AddressFamilyT addressFamily) {
   m_msocket = msocket_new(addressFamily);
}

MSocket::MSocket(msocket_t* msocket) {
   m_msocket = msocket;
}

MSocket::~MSocket() {
   msocket_delete(m_msocket);
   m_msocket = NULL;
}

void MSocket::setHandler(HandlerT& handler) {

   m_handler = handler;
   msocket_handler_t msocket_handler;
   memset(&msocket_handler, 0, sizeof(msocket_handler));
   msocket_handler.udp_msg = MSocket::onUdpMsgWrapper;
   msocket_handler.tcp_connected = MSocket::onConnectedWrapper;
   msocket_handler.tcp_disconnected = MSocket::onDisconnectedWrapper;
   msocket_handler.tcp_data = MSocket::onTcpDataWrapper;
   msocket_handler.tcp_inactivity = MSocket::onTcpInactivityWrapper;

   msocket_sethandler(m_msocket, &msocket_handler, (void*)(&m_handler));
}

void MSocket::close() {
   msocket_close(m_msocket);
}

bool MSocket::connect(const std::string& socketPath, PortNumberT port) {
   m_error = msocket_connect(m_msocket, socketPath.c_str(), port);
   return m_error == 0;
}

bool MSocket::unixConnect(const std::string& socketPath) {
   m_error = msocket_unix_connect(m_msocket, socketPath.c_str());
   return m_error == 0;
}

bool MSocket::send(BufferDataT* msgData, BufferSizeT msgLen) {
   m_error = msocket_send(m_msocket, msgData, msgLen);
   return m_error == 0;
}

bool MSocket::sendto(const std::string& addr, PortNumberT port, const BufferDataT *msgData, BufferSizeT msgLen) {
   m_error = msocket_sendto(m_msocket, addr.c_str(), port, msgData, msgLen);
   return m_error == 0;
}

bool MSocket::listen(ModeT mode, PortNumberT port, const std::string& addr) {
   m_error = msocket_listen(m_msocket, mode, port, addr.c_str());
   return m_error == 0;
}

#ifndef _WIN32
bool MSocket::unixListen(const std::string& socketPath) {
   m_error = msocket_unix_listen(m_msocket, socketPath.c_str());
   return m_error == 0;
}
#endif

MSocketPtr MSocket::accept(MSocketPtr childPtr) {

   msocket_t* msocket = NULL;
   if(childPtr.get()) {
      msocket = childPtr->m_msocket;
   }
   msocket_t* child_msocket = msocket_accept(m_msocket, msocket);

   return MSocketPtr(new MSocket(child_msocket));
}

void MSocket::startIO() {
   msocket_start_io(m_msocket);
}

MSocket::StateT MSocket::state() {
   return static_cast<MSocket::StateT>(msocket_state(m_msocket));
}

void MSocket::onConnectedWrapper(void *arg, const char *addr, PortNumberT port) {

   if(arg == NULL) {
      return;
   }

   HandlerT* handler = static_cast<HandlerT*>(arg);
   std::string addrString(addr);
   if(handler->connectHandler) {
      handler->connectHandler(addrString, port);
   }
}

int8_t MSocket::onTcpDataWrapper(void *arg, const BufferDataT *dataBuf, BufferSizeT dataLen, BufferSizeT *parseLen) {

   if(arg == NULL) {
      return -1;
   }

   HandlerT* handler = static_cast<HandlerT*>(arg);
   if(handler->tcpDataHandler) {
      if( handler->tcpDataHandler(dataBuf, dataLen, *parseLen) ) {
         return 0;
      }
   }
   return -1;
}

void MSocket::onDisconnectedWrapper(void *arg) {

   if(arg == NULL) {
      return;
   }

   HandlerT* handler = static_cast<HandlerT*>(arg);
   if(handler->disconnectedHandler) {
      handler->disconnectedHandler();
   }
}

void MSocket::onTcpInactivityWrapper(uint32_t elapsed) {
   // Oops no handler
}

void MSocket::onUdpMsgWrapper(void *arg, const char *addr, PortNumberT port, const BufferDataT *dataBuf, BufferSizeT dataLen) {

   if(arg == NULL) {
      return;
   }

   HandlerT* handler = static_cast<HandlerT*>(arg);
   if(handler->udpMsgHandler) {
      std::string addrString(addr);
      handler->udpMsgHandler(addrString, port, dataBuf, dataLen);
   }
}

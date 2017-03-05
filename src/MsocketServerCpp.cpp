/*****************************************************************************
* \file      MsocketServerCpp.cpp
* \author    Pierre Svärd
* \date      2017-03-05
* \brief     msocket_server C++ wrapper
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2014-2017 Pierre Svärd
*
******************************************************************************/

#include <MsocketServerCpp.h>

MsocketServer::MsocketServer(uint8_t addressFamily) {
	m_msocket_server = msocket_server_new(addressFamily, MsocketServer::onCleanupConnection);
}

MsocketServer::~MsocketServer() {
   msocket_server_delete(m_msocket_server);
}

void MsocketServer::setHandler(Msocket::HandlerD& handler) {
   m_handler = handler;
   msocket_handler_t msocket_handler;
   msocket_handler.tcp_accept = MsocketServer::onTcpAcceptWrapper;
   msocket_server_sethandler(m_msocket_server, &msocket_handler, (void*)(&m_handler));
}

void MsocketServer::start(const std::string& udpAddr, uint16_t udpPort, uint16_t tcpPort) {
   msocket_server_start(m_msocket_server, udpAddr.c_str(), udpPort, tcpPort);
}

void MsocketServer::startUnix(const std::string& socketPath) {
   msocket_server_unix_start(m_msocket_server, socketPath.c_str());
}

void MsocketServer::cleanupConnection(MsocketServer::CleanupCallback callback) {
   m_cleanupCallback = callback;
   msocket_server_cleanup_connection(m_msocket_server, (void*)this);
}

void MsocketServer::onTcpAcceptWrapper(void *arg, struct msocket_server_t *srv, struct msocket_t *msocket) {
   Msocket::HandlerD* handler = static_cast<Msocket::HandlerD*>(arg);

   MsocketPtr msocketPtr(new Msocket(msocket));

   if(handler->tcpAcceptHandler) {
      handler->tcpAcceptHandler(msocketPtr);
   }
}

void MsocketServer::onCleanupConnection(void* arg) {

   MsocketServer* msocketServer = static_cast<MsocketServer*>(arg);

   if(msocketServer->m_cleanupCallback) {
      msocketServer->m_cleanupCallback();
   }
}

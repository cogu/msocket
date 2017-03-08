/*****************************************************************************
* \file      MSocketServer.cpp
* \author    Pierre Svärd
* \date      2017-03-05
* \brief     msocket server C++ wrapper
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2017 Pierre Svärd
*
******************************************************************************/

#include <MSocketServer.hpp>

MSocketServer::MSocketServer(MSocket::AddressFamilyT addressFamily) {
	m_msocket_server = msocket_server_new(addressFamily, MSocketServer::onCleanupConnection);
}

MSocketServer::~MSocketServer() {
   msocket_server_delete(m_msocket_server);
}

void MSocketServer::setHandler(MSocket::HandlerT& handler) {
   m_handler = handler;
   msocket_handler_t msocket_handler;
   msocket_handler.tcp_accept = MSocketServer::onTcpAcceptWrapper;
   msocket_server_sethandler(m_msocket_server, &msocket_handler, (void*)(&m_handler));
}

void MSocketServer::start(const std::string& udpAddr, uint16_t udpPort, uint16_t tcpPort) {
   msocket_server_start(m_msocket_server, udpAddr.c_str(), udpPort, tcpPort);
}

void MSocketServer::startUnix(const std::string& socketPath) {
   msocket_server_unix_start(m_msocket_server, socketPath.c_str());
}

void MSocketServer::cleanupConnection(MSocketServer::CleanupCallback callback) {
   m_cleanupCallback = callback;
   msocket_server_cleanup_connection(m_msocket_server, (void*)this);
}

void MSocketServer::onTcpAcceptWrapper(void *arg, struct msocket_server_t *srv, struct msocket_t *msocket) {
   MSocket::HandlerT* handler = static_cast<MSocket::HandlerT*>(arg);

   MSocketPtr msocketPtr(new MSocket(msocket));

   if(handler->tcpAcceptHandler) {
      handler->tcpAcceptHandler(msocketPtr);
   }
}

void MSocketServer::onCleanupConnection(void* arg) {

   MSocketServer* msocketServer = static_cast<MSocketServer*>(arg);

   if(msocketServer->m_cleanupCallback) {
      msocketServer->m_cleanupCallback();
   }
}

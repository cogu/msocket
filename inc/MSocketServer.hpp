/*****************************************************************************
* \file      MSocketServer.hpp
* \author    Pierre Svärd
* \date      2017-03-05
* \brief     msocket server C++ wrapper
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2017 Pierre Svärd
*
******************************************************************************/

#ifndef MSOCKETSERVER_HPP
#define MSOCKETSERVER_HPP

#include <MSocketWrapper.hpp>

extern "C" {
#include "msocket_server.h"
}

#include <memory>
#include <functional>

class MSocketServer {
public:

	MSocketServer(MSocket::AddressFamilyT addressFamily);
	virtual ~MSocketServer();

	virtual void setHandler(MSocket::HandlerT& handler);
	virtual void start(const std::string& udpAddr, uint16_t udpPort, uint16_t tcpPort);
	virtual void startUnix(const std::string& socketPath);

	typedef std::function<void()> CleanupCallback;
	virtual void cleanupConnection(CleanupCallback callback);

protected:

   static void onTcpAcceptWrapper(void *arg, struct msocket_server_t *srv, struct msocket_t *msocket);
   static void onCleanupConnection(void *arg);

	msocket_server_t* m_msocket_server;
	MSocket::HandlerT m_handler;
	CleanupCallback m_cleanupCallback;

};

#endif /* MSOCKETSERVER_HPP */

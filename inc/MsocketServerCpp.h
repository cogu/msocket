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

#ifndef MSOCKETSERVERCPP_H_
#define MSOCKETSERVERCPP_H_

#include <MsocketCpp.h>

extern "C" {
#include "msocket_server.h"
}

#include <memory>
#include <functional>

class MsocketServer {
public:

	MsocketServer(uint8_t addressFamily);
	virtual ~MsocketServer();

	virtual void setHandler(Msocket::HandlerD& handler);
	virtual void start(const std::string& udpAddr, uint16_t udpPort, uint16_t tcpPort);
	virtual void startUnix(const std::string& socketPath);

	typedef std::function<void()> CleanupCallback;
	virtual void cleanupConnection(CleanupCallback callback);

protected:

   static void onTcpAcceptWrapper(void *arg, struct msocket_server_t *srv, struct msocket_t *msocket);
   static void onCleanupConnection(void *arg);

	msocket_server_t* m_msocket_server;
	Msocket::HandlerD m_handler;
	CleanupCallback m_cleanupCallback;

};

#endif /* MSOCKETSERVERCPP_H_ */

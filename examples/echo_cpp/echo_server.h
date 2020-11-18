#pragma once
#include "server_connection.h"

class EchoServer : public msocket::Handler
{
public:
   EchoServer();
   ~EchoServer();
   void tcp_accept(msocket_server_t* server, msocket_t* accepted_socket) override;
   void start(std::uint16_t tcp_port);
protected:
   msocket_server_t m_server;
};
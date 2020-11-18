#include "echo_server.h"

EchoServer::EchoServer()
{
   msocket_server_create(&m_server, AF_INET, server_connection_vdelete);
   msocket::set_server_handler(&m_server, this);
}

EchoServer::~EchoServer()
{
   msocket_server_destroy(&m_server);
}

void EchoServer::tcp_accept(msocket_server_t* server, msocket_t* accepted_socket)
{
   auto server_connection = new ServerConnection(accepted_socket, server);
}

void EchoServer::start(std::uint16_t tcp_port)
{
   msocket_server_start(&m_server, nullptr, 0u, tcp_port);
}

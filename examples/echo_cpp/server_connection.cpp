#include <iostream>
#include "server_connection.h"

ServerConnection::ServerConnection(msocket_t* socket, msocket_server_t* server) : m_socket{ socket }, m_parent{ server }
{
   assert(m_socket != nullptr);
   std::cout << "[SERVER] New Client Connected" << std::endl;
   msocket::set_handler(m_socket, this);
   msocket_start_io(m_socket);
}

ServerConnection::~ServerConnection()
{
   msocket_delete(m_socket);
}

void ServerConnection::socket_disconnected()
{
   std::cout << "[SERVER] Client disconnected" << std::endl;
   msocket_server_cleanup_connection(m_parent, reinterpret_cast<void*>(this));
}

int ServerConnection::socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len)
{
   std::cout << "[SERVER] Received " << data_size << " bytes of data" << std::endl;
   parse_len = data_size;
   msocket_send(m_socket, data, static_cast<std::uint32_t>(data_size));
   return 0;
}

void server_connection_vdelete(void* arg)
{
   auto connection = reinterpret_cast<ServerConnection*>(arg);
   if (connection != nullptr)
   {
      std::cout << "[SERVER] Cleaning up client connection" << std::endl;
      delete connection;
   }
}

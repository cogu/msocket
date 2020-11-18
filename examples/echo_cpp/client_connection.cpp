#include <iostream>
#include "client_connection.h"

ClientConnection::ClientConnection()
{
   msocket_create(&m_socket, AF_INET);
   msocket::set_handler(&m_socket, this);
}

ClientConnection::~ClientConnection()
{
   msocket_destroy(&m_socket);
}

void ClientConnection::socket_connected(const std::string& address, std::uint16_t port)
{
   std::cout << "[CLIENT] Connected to server" << std::endl;
}

void ClientConnection::socket_disconnected()
{
   std::cout << "[CLIENT] Disconnected from server" << std::endl;
}

int ClientConnection::socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len)
{
   parse_len = data_size; //consume all data
   std::string message{ reinterpret_cast<const char*>(data), data_size };
   std::cout << "> " << message << std::endl;
   return 0;
}

bool ClientConnection::connect(const std::string& address, std::uint16_t port)
{
   auto result = msocket_connect(&m_socket, address.data(), port);
   return (result < 0) ? false : true;
}

bool ClientConnection::connect(const char* address, std::uint16_t port)
{
   auto result = msocket_connect(&m_socket, address, port);
   return (result < 0) ? false : true;
}

bool ClientConnection::send(const std::string& message)
{
   auto result = msocket_send(&m_socket, reinterpret_cast<const std::uint8_t*>(message.data()), static_cast<std::uint32_t>(message.size()));
   return (result < 0) ? false : true;
}

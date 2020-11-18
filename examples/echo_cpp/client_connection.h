#pragma once

#include "msocket_adapter.h"

class ClientConnection : public msocket::Handler
{
public:
   ClientConnection();
   ~ClientConnection();
   void socket_connected(const std::string& address, std::uint16_t port) override;
   void socket_disconnected() override;
   int socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len) override;

   bool connect(const std::string& address, std::uint16_t port);
   bool connect(const char* address, std::uint16_t port);
   bool send(const std::string& message);
protected:
   msocket_t m_socket;
};
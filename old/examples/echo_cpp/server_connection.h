#pragma once
#include <cassert>
#include "msocket_adapter.h"

class ServerConnection : public msocket::Handler
{
public:
   ServerConnection(msocket_t* socket, msocket_server_t* server);
   ~ServerConnection();
   void socket_disconnected() override;
   int socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len) override;
protected:
   msocket_t* m_socket;
   msocket_server_t* m_parent;
};

extern "C"
{
   void server_connection_vdelete(void* arg);
}

/*****************************************************************************
* \file:    msocket_adapter.h
* \author:  Conny Gustafsson
* \date:    2020-11-16
* \brief:   A C++ helper interface for msocket library
*
* Copyright (c) 2020 Conny Gustafsson
*
******************************************************************************/

#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include "msocket_server.h" //Also includes msocket.h
#ifdef UNIT_TEST
#include "testsocket.h"
#endif

namespace msocket
{
   class Handler
   {
   public:
      virtual void socket_accepted(msocket_server_t* server, msocket_t* accepted_socket);
      virtual void udp_message_received(const std::string& address, std::uint16_t port, const std::uint8_t* data, std::size_t data_size);
      virtual void socket_connected(const std::string& address, std::uint16_t port);
      virtual void socket_disconnected();
      virtual int socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len);
   };

#ifdef UNIT_TEST
   void set_client_handler(testsocket_t* msocket, Handler* handler);
   void set_server_handler(testsocket_t* msocket, Handler* handler);
#endif
   void set_handler(msocket_t* msocket, Handler* handler);
   void set_server_handler(msocket_server_t* server, Handler* handler);
}
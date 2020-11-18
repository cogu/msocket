/*****************************************************************************
* \file:    msocket_adapter.cpp
* \author:  Conny Gustafsson
* \date:    2020-11-16
* \brief:   A C++ helper interface for msocket library
*
* Copyright (c) 2020 Conny Gustafsson
*
******************************************************************************/
#include "msocket_adapter.h"
#include <cstring>

extern "C"
{
   static void msocket_adapter_tcp_accept(void* arg, struct msocket_server_tag* server, struct msocket_t* accepted_socket);
   static void msocket_adapter_udp_msg(void* arg, const char* addr, uint16_t port, const uint8_t* dataBuf, uint32_t dataLen);
   static void msocket_adapter_tcp_connected(void* arg, const char* addr, uint16_t port);
   static void msocket_adapter_tcp_disconnected(void* arg);
   static int8_t msocket_adapter_tcp_data(void* arg, const uint8_t* dataBuf, uint32_t dataLen, uint32_t* parseLen);
}

namespace msocket
{
   void Handler::tcp_accept(msocket_server_t* server, msocket_t* accepted_socket)
   {
      (void)server;
      (void)accepted_socket;
   }

   void Handler::udp_msg_received(const std::string& address, std::uint16_t port, const std::uint8_t* data, std::size_t data_size)
   {
      (void)address;
      (void)port;
      (void)data;
      (void)data_size;
   }

   void Handler::socket_connected(const std::string& address, std::uint16_t port)
   {
      (void)address;
      (void)port;
   }

   void Handler::socket_disconnected()
   {

   }

   int Handler::socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len)
   {
      (void)data;
      (void)data_size;
      (void)parse_len;
      return -1;
   }

   void set_handler(msocket_t* msocket, Handler* handler)
   {
      if ((msocket != nullptr) && (handler != nullptr))
      {
         msocket_handler_t handler_struct;
         std::memset(&handler_struct, 0, sizeof(handler_struct));
         handler_struct.tcp_connected = msocket_adapter_tcp_connected;
         handler_struct.tcp_disconnected = msocket_adapter_tcp_disconnected;
         handler_struct.tcp_data = msocket_adapter_tcp_data;
         handler_struct.udp_msg = msocket_adapter_udp_msg; //TODO: Check if this line is actually needed
         msocket_set_handler(msocket, &handler_struct, reinterpret_cast<void*>(handler));
      }
   }

   void set_server_handler(msocket_server_t* server, Handler* handler)
   {
      if ((server != nullptr) && (handler != nullptr))
      {
         msocket_handler_t handler_struct;
         std::memset(&handler_struct, 0, sizeof(handler_struct));
         //The only callbacks that makes sense in a server are socket accept and new datagram messages
         handler_struct.tcp_accept = msocket_adapter_tcp_accept;
         handler_struct.udp_msg = msocket_adapter_udp_msg;
         msocket_server_set_handler(server, &handler_struct, reinterpret_cast<void*>(handler));
      }
   }
}

static void msocket_adapter_tcp_accept(void* arg, struct msocket_server_tag* server, struct msocket_t* accepted_socket)
{
   auto handler = reinterpret_cast<msocket::Handler*>(arg);
   if (handler != nullptr)
   {
      handler->tcp_accept(server, accepted_socket);
   }
}

static void msocket_adapter_udp_msg(void* arg, const char* addr, uint16_t port, const uint8_t* dataBuf, uint32_t dataLen)
{
   auto handler = reinterpret_cast<msocket::Handler*>(arg);
   if (handler != nullptr)
   {
      std::string address{ addr };
      std::size_t size = static_cast<std::size_t>(dataLen);
      handler->udp_msg_received(address, port, dataBuf, size);
   }
}

static void msocket_adapter_tcp_connected(void* arg, const char* addr, uint16_t port)
{
   auto handler = reinterpret_cast<msocket::Handler*>(arg);
   if (handler != nullptr)
   {
      std::string address{ addr };
      handler->socket_connected(address, port);
   }
}

static void msocket_adapter_tcp_disconnected(void* arg)
{
   auto handler = reinterpret_cast<msocket::Handler*>(arg);
   if (handler != nullptr)
   {
      handler->socket_disconnected();
   }
}

static int8_t msocket_adapter_tcp_data(void* arg, const uint8_t* dataBuf, uint32_t dataLen, uint32_t* parseLen)
{
   auto handler = reinterpret_cast<msocket::Handler*>(arg);
   if (handler != nullptr)
   {
      std::size_t data_size = static_cast<std::size_t>(dataLen);
      std::size_t parse_len = 0u;
      int result = handler->socket_data_received(dataBuf, data_size, parse_len);
      if (parseLen != nullptr)
      {
         *parseLen = static_cast<std::uint32_t>(parse_len);
      }
      return static_cast<std::int8_t>(result);
   }
   return -1;
}

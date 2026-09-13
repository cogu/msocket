/*****************************************************************************
* \file      msocket_adapter.cpp
* \author    Conny Gustafsson
* \date      2020-11-16
* \brief     C++ RAII socket and server wrapper for msocket
*
* Copyright (c) 2020-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#include "msocket_adapter.h"
#include <cstring>
#include <utility>

namespace
{
   extern "C"
   {
      void c_stream_connected(void* arg, void* socket, const char* addr, uint16_t port)
      {
         (void)socket;
         auto listener = reinterpret_cast<msocket::SocketListener*>(arg);
         if (listener != nullptr)
         {
            std::string_view address = (addr != nullptr) ? addr : "";
            listener->on_connected(address, port);
         }
      }

      void c_stream_disconnected(void* arg, void* socket)
      {
         (void)socket;
         auto listener = reinterpret_cast<msocket::SocketListener*>(arg);
         if (listener != nullptr)
         {
            listener->on_disconnected();
         }
      }

      msocket_error_t c_stream_data(void* arg, void* socket, const uint8_t* data, const uint32_t num_bytes, uint32_t* consumed_bytes, uint32_t* msg_size_hint)
      {
         (void)socket;
         auto listener = reinterpret_cast<msocket::SocketListener*>(arg);
         if (listener != nullptr)
         {
            std::size_t parsed = 0;
            std::size_t hint = 0;
            int result = listener->on_data_received(data, static_cast<std::size_t>(num_bytes), parsed, hint);
            if (consumed_bytes != nullptr)
            {
               *consumed_bytes = static_cast<uint32_t>(parsed);
            }
            if (msg_size_hint != nullptr)
            {
               *msg_size_hint = static_cast<uint32_t>(hint);
            }
            return static_cast<msocket_error_t>(result);
         }
         return MSOCKET_INVALID_ARGUMENT_ERROR;
      }

      void c_stream_accept(void* arg, msocket_server_t* server, void* socket)
      {
         (void)server;
         auto listener = reinterpret_cast<msocket::ServerListener*>(arg);
         auto child_socket = reinterpret_cast<msocket_t*>(socket);
         if ((listener != nullptr) && (child_socket != nullptr))
         {
            auto accepted = std::make_unique<msocket::TcpSocket>(child_socket, true);
            listener->on_connection_accepted(std::move(accepted));
         }
      }

      void legacy_c_stream_accept(void* arg, msocket_server_t* server, void* socket)
      {
         auto handler = reinterpret_cast<msocket::Handler*>(arg);
         auto child_socket = reinterpret_cast<msocket_t*>(socket);
         if (handler != nullptr)
         {
            handler->socket_accepted(server, child_socket);
         }
      }

      void legacy_c_datagram_msg(void* arg, void* socket, const char* addr, uint16_t port, const uint8_t* data, const uint32_t num_bytes)
      {
         (void)socket;
         auto handler = reinterpret_cast<msocket::Handler*>(arg);
         if (handler != nullptr)
         {
            std::string address = (addr != nullptr) ? addr : "";
            handler->udp_message_received(address, port, data, static_cast<std::size_t>(num_bytes));
         }
      }
   }
} // anonymous namespace

namespace msocket
{
   // =========================================================================
   // TcpSocket
   // =========================================================================

   TcpSocket::TcpSocket(int address_family)
      : m_socket{ msocket_new(static_cast<uint8_t>(address_family)) }, m_owns_socket{ true }
   {
   }

   TcpSocket::TcpSocket(msocket_t* raw_socket, bool take_ownership)
      : m_socket{ raw_socket }, m_owns_socket{ take_ownership }
   {
   }

   TcpSocket::~TcpSocket()
   {
      TcpSocket::close();
   }

   TcpSocket::TcpSocket(TcpSocket&& other) noexcept
      : m_socket{ other.m_socket }, m_owns_socket{ other.m_owns_socket }, m_listener{ other.m_listener }
   {
      other.m_socket = nullptr;
      other.m_owns_socket = false;
      other.m_listener = nullptr;
   }

   TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept
   {
      if (this != &other)
      {
         close();
         m_socket = other.m_socket;
         m_owns_socket = other.m_owns_socket;
         m_listener = other.m_listener;

         other.m_socket = nullptr;
         other.m_owns_socket = false;
         other.m_listener = nullptr;
      }
      return *this;
   }

   int TcpSocket::connect(std::string_view address, std::uint16_t port)
   {
      if (m_socket == nullptr)
      {
         return -1;
      }
      std::string addr_str{ address };
      return static_cast<int>(msocket_connect(m_socket, addr_str.c_str(), port));
   }

   int TcpSocket::connect_unix(std::string_view path)
   {
      if (m_socket == nullptr)
      {
         return -1;
      }
      std::string path_str{ path };
      return static_cast<int>(msocket_unix_connect(m_socket, path_str.c_str()));
   }

   int TcpSocket::send(const std::uint8_t* data, std::size_t size)
   {
      if (m_socket == nullptr)
      {
         return -1;
      }
      return static_cast<int>(msocket_send(m_socket, data, static_cast<uint32_t>(size)));
   }

   void TcpSocket::start_io()
   {
      if (m_socket != nullptr)
      {
         msocket_start_io(m_socket);
      }
   }

   void TcpSocket::close()
   {
      if (m_socket != nullptr)
      {
         TcpSocket::set_listener(nullptr);
         if (m_owns_socket)
         {
            msocket_delete(m_socket);
         }
         else
         {
            msocket_close(m_socket);
         }
         m_socket = nullptr;
      }
      m_listener = nullptr;
   }

   void TcpSocket::set_listener(SocketListener* listener)
   {
      m_listener = listener;
      if (m_socket != nullptr)
      {
         if (m_listener != nullptr)
         {
            msocket_handler_t handlers;
            std::memset(&handlers, 0, sizeof(handlers));
            handlers.stream_connected = c_stream_connected;
            handlers.stream_disconnected = c_stream_disconnected;
            handlers.stream_data = c_stream_data;
            msocket_set_handler(m_socket, &handlers, reinterpret_cast<void*>(m_listener));
         }
         else
         {
            msocket_set_handler(m_socket, nullptr, nullptr);
         }
      }
   }

   msocket_t* TcpSocket::release() noexcept
   {
      msocket_t* sock = m_socket;
      m_socket = nullptr;
      m_owns_socket = false;
      m_listener = nullptr;
      return sock;
   }

   // =========================================================================
   // TestSocket
   // =========================================================================

   TestSocket::TestSocket()
      : m_socket{ testsocket_new() }, m_owns_socket{ true }
   {
   }

   TestSocket::TestSocket(testsocket_t* raw_socket, bool take_ownership)
      : m_socket{ raw_socket }, m_owns_socket{ take_ownership }
   {
   }

   TestSocket::~TestSocket()
   {
      TestSocket::close();
   }

   TestSocket::TestSocket(TestSocket&& other) noexcept
      : m_socket{ other.m_socket }, m_owns_socket{ other.m_owns_socket }, m_listener{ other.m_listener }
   {
      other.m_socket = nullptr;
      other.m_owns_socket = false;
      other.m_listener = nullptr;
   }

   TestSocket& TestSocket::operator=(TestSocket&& other) noexcept
   {
      if (this != &other)
      {
         close();
         m_socket = other.m_socket;
         m_owns_socket = other.m_owns_socket;
         m_listener = other.m_listener;

         other.m_socket = nullptr;
         other.m_owns_socket = false;
         other.m_listener = nullptr;
      }
      return *this;
   }

   int TestSocket::connect(std::string_view address, std::uint16_t port)
   {
      (void)address;
      (void)port;
      if (m_socket == nullptr)
      {
         return -1;
      }
      testsocket_on_connect(m_socket);
      return 0;
   }

   int TestSocket::connect_unix(std::string_view path)
   {
      (void)path;
      if (m_socket == nullptr)
      {
         return -1;
      }
      testsocket_on_connect(m_socket);
      return 0;
   }

   int TestSocket::send(const std::uint8_t* data, std::size_t size)
   {
      if (m_socket == nullptr)
      {
         return -1;
      }
      return static_cast<int>(testsocket_client_send(m_socket, data, static_cast<uint32_t>(size)));
   }

   void TestSocket::start_io()
   {
      // No background thread needed for mock socket
   }

   void TestSocket::close()
   {
      if (m_socket != nullptr)
      {
         TestSocket::set_listener(nullptr);
         if (m_owns_socket)
         {
            testsocket_delete(m_socket);
         }
         m_socket = nullptr;
      }
      m_listener = nullptr;
   }

   void TestSocket::set_listener(SocketListener* listener)
   {
      m_listener = listener;
      if (m_socket != nullptr)
      {
         if (m_listener != nullptr)
         {
            msocket_handler_t handlers;
            std::memset(&handlers, 0, sizeof(handlers));
            handlers.stream_connected = c_stream_connected;
            handlers.stream_disconnected = c_stream_disconnected;
            handlers.stream_data = c_stream_data;
            testsocket_set_client_handler(m_socket, &handlers, reinterpret_cast<void*>(m_listener));
         }
         else
         {
            testsocket_set_client_handler(m_socket, nullptr, nullptr);
         }
      }
   }

   void TestSocket::run()
   {
      if (m_socket != nullptr)
      {
         testsocket_run(m_socket);
      }
   }

   void TestSocket::server_send(const std::uint8_t* data, std::size_t size)
   {
      if (m_socket != nullptr)
      {
         testsocket_server_send(m_socket, data, static_cast<uint32_t>(size));
      }
   }

   void TestSocket::simulate_disconnect()
   {
      if (m_socket != nullptr)
      {
         testsocket_on_disconnect(m_socket);
      }
   }

   testsocket_t* TestSocket::release() noexcept
   {
      testsocket_t* sock = m_socket;
      m_socket = nullptr;
      m_owns_socket = false;
      m_listener = nullptr;
      return sock;
   }

   // =========================================================================
   // TcpServer
   // =========================================================================

   TcpServer::TcpServer(int address_family)
      : m_server{ msocket_server_new(static_cast<uint8_t>(address_family), nullptr) }
   {
   }

   TcpServer::~TcpServer()
   {
      stop();
   }

   TcpServer::TcpServer(TcpServer&& other) noexcept
      : m_server{ other.m_server }, m_listener{ other.m_listener }
   {
      other.m_server = nullptr;
      other.m_listener = nullptr;
   }

   TcpServer& TcpServer::operator=(TcpServer&& other) noexcept
   {
      if (this != &other)
      {
         stop();
         m_server = other.m_server;
         m_listener = other.m_listener;
         other.m_server = nullptr;
         other.m_listener = nullptr;
      }
      return *this;
   }

   int TcpServer::start(std::uint16_t tcp_port, const char* address)
   {
      if (m_server == nullptr)
      {
         return -1;
      }
      msocket_server_start(m_server, address, 0u, tcp_port);
      return 0;
   }

   int TcpServer::start_unix(const char* socket_path)
   {
      if (m_server == nullptr)
      {
         return -1;
      }
      msocket_server_unix_start(m_server, socket_path);
      return 0;
   }

   void TcpServer::stop()
   {
      if (m_server != nullptr)
      {
         msocket_server_delete(m_server);
         m_server = nullptr;
      }
      m_listener = nullptr;
   }

   void TcpServer::set_listener(ServerListener* listener)
   {
      m_listener = listener;
      if (m_server != nullptr)
      {
         if (m_listener != nullptr)
         {
            msocket_handler_t handlers;
            std::memset(&handlers, 0, sizeof(handlers));
            handlers.stream_accept = c_stream_accept;
            msocket_server_set_handler(m_server, &handlers, reinterpret_cast<void*>(m_listener));
         }
         else
         {
            msocket_server_set_handler(m_server, nullptr, nullptr);
         }
      }
   }

   // =========================================================================
   // Backward Compatibility Functions
   // =========================================================================

   void set_handler(msocket_t* msocket, Handler* handler)
   {
      if ((msocket != nullptr) && (handler != nullptr))
      {
         msocket_handler_t handlers;
         std::memset(&handlers, 0, sizeof(handlers));
         handlers.stream_connected = c_stream_connected;
         handlers.stream_disconnected = c_stream_disconnected;
         handlers.stream_data = c_stream_data;
         handlers.datagram_msg = legacy_c_datagram_msg;
         msocket_set_handler(msocket, &handlers, reinterpret_cast<void*>(handler));
      }
   }

   void set_server_handler(msocket_server_t* server, Handler* handler)
   {
      if ((server != nullptr) && (handler != nullptr))
      {
         msocket_handler_t handlers;
         std::memset(&handlers, 0, sizeof(handlers));
         handlers.stream_accept = legacy_c_stream_accept;
         handlers.datagram_msg = legacy_c_datagram_msg;
         msocket_server_set_handler(server, &handlers, reinterpret_cast<void*>(handler));
      }
   }

   void set_client_handler(testsocket_t* msocket, Handler* handler)
   {
      if ((msocket != nullptr) && (handler != nullptr))
      {
         msocket_handler_t handlers;
         std::memset(&handlers, 0, sizeof(handlers));
         handlers.stream_connected = c_stream_connected;
         handlers.stream_disconnected = c_stream_disconnected;
         handlers.stream_data = c_stream_data;
         testsocket_set_client_handler(msocket, &handlers, reinterpret_cast<void*>(handler));
      }
   }

   void set_server_handler(testsocket_t* msocket, Handler* handler)
   {
      if ((msocket != nullptr) && (handler != nullptr))
      {
         msocket_handler_t handlers;
         std::memset(&handlers, 0, sizeof(handlers));
         handlers.stream_connected = c_stream_connected;
         handlers.stream_disconnected = c_stream_disconnected;
         handlers.stream_data = c_stream_data;
         handlers.datagram_msg = legacy_c_datagram_msg;
         testsocket_set_server_handler(msocket, &handlers, reinterpret_cast<void*>(handler));
      }
   }

} // namespace msocket

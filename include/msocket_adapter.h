/*****************************************************************************
* \file      msocket_adapter.h
* \author    Conny Gustafsson
* \date      2020-11-16
* \brief     C++ RAII socket and server wrapper for msocket
*
* Copyright (c) 2020-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include "msocket.h"
#include "msocket_server.h"
#include "testsocket.h"

namespace msocket
{
   class Socket;

   /**
    * \brief Listener interface for stream socket events (connect, disconnect, data arrival).
    */
   class SocketListener
   {
   public:
      virtual ~SocketListener() = default;

      virtual void on_connected(std::string_view address, std::uint16_t port)
      {
         (void)address;
         (void)port;
      }

      virtual void on_disconnected()
      {
      }

      virtual int on_data_received(const std::uint8_t* data, std::size_t size, std::size_t& parse_len)
      {
         (void)data;
         parse_len = size;
         return 0;
      }

      virtual int on_data_received(const std::uint8_t* data, std::size_t size, std::size_t& parse_len, std::size_t& msg_size_hint)
      {
         msg_size_hint = 0;
         return on_data_received(data, size, parse_len);
      }
   };

   /**
    * \brief Listener interface for server socket events (accepted incoming connections).
    */
   class ServerListener
   {
   public:
      virtual ~ServerListener() = default;

      virtual void on_connection_accepted(std::unique_ptr<Socket> accepted_socket)
      {
         (void)accepted_socket;
      }
   };

   /**
    * \brief Abstract base class representing a connected stream socket.
    *
    * Provides a uniform interface for real network sockets (TcpSocket) and mock sockets (TestSocket).
    */
   class Socket
   {
   public:
      virtual ~Socket() = default;

      Socket(const Socket&) = delete;
      Socket& operator=(const Socket&) = delete;

      Socket(Socket&&) noexcept = default;
      Socket& operator=(Socket&&) noexcept = default;

      virtual int connect(std::string_view address, std::uint16_t port) = 0;
      virtual int connect_unix(std::string_view path)
      {
         (void)path;
         return -1;
      }

      virtual int send(const std::uint8_t* data, std::size_t size) = 0;

      int send(std::string_view data)
      {
         return send(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
      }

      virtual void start_io() = 0;
      virtual void close() = 0;
      virtual void set_listener(SocketListener* listener) = 0;

   protected:
      Socket() = default;
   };

   /**
    * \brief Concrete RAII wrapper around an OS-level msocket_t stream socket.
    */
   class TcpSocket : public Socket
   {
   public:
      explicit TcpSocket(int address_family = MSOCKET_ADDR_INET);
      explicit TcpSocket(msocket_t* raw_socket, bool take_ownership);
      ~TcpSocket() override;

      TcpSocket(TcpSocket&& other) noexcept;
      TcpSocket& operator=(TcpSocket&& other) noexcept;

      int connect(std::string_view address, std::uint16_t port) override;
      int connect_unix(std::string_view path) override;
      int send(const std::uint8_t* data, std::size_t size) override;
      void start_io() override;
      void close() override;
      void set_listener(SocketListener* listener) override;

      msocket_t* raw_handle() const noexcept { return m_socket; }
      msocket_t* release() noexcept;
      bool is_valid() const noexcept { return m_socket != nullptr; }

   private:
      msocket_t* m_socket{ nullptr };
      bool m_owns_socket{ true };
      SocketListener* m_listener{ nullptr };
   };

   /**
    * \brief Concrete RAII wrapper around an in-memory mock testsocket_t.
    */
   class TestSocket : public Socket
   {
   public:
      TestSocket();
      explicit TestSocket(testsocket_t* raw_socket, bool take_ownership);
      ~TestSocket() override;

      TestSocket(TestSocket&& other) noexcept;
      TestSocket& operator=(TestSocket&& other) noexcept;

      int connect(std::string_view address, std::uint16_t port) override;
      int connect_unix(std::string_view path) override;
      int send(const std::uint8_t* data, std::size_t size) override;
      void start_io() override;
      void close() override;
      void set_listener(SocketListener* listener) override;

      // Mock control helpers
      void run();
      void server_send(const std::uint8_t* data, std::size_t size);
      void server_send(std::string_view data)
      {
         server_send(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
      }
      void simulate_disconnect();

      testsocket_t* raw_handle() const noexcept { return m_socket; }
      testsocket_t* release() noexcept;
      bool is_valid() const noexcept { return m_socket != nullptr; }

   private:
      testsocket_t* m_socket{ nullptr };
      bool m_owns_socket{ true };
      SocketListener* m_listener{ nullptr };
   };

   /**
    * \brief Concrete RAII wrapper around an msocket_server_t connection listener.
    */
   class TcpServer
   {
   public:
      explicit TcpServer(int address_family = MSOCKET_ADDR_INET);
      ~TcpServer();

      TcpServer(const TcpServer&) = delete;
      TcpServer& operator=(const TcpServer&) = delete;

      TcpServer(TcpServer&& other) noexcept;
      TcpServer& operator=(TcpServer&& other) noexcept;

      int start(std::uint16_t tcp_port, const char* address = nullptr);
      int start_unix(const char* socket_path);
      void stop();
      void set_listener(ServerListener* listener);

      msocket_server_t* raw_handle() const noexcept { return m_server; }

   private:
      msocket_server_t* m_server{ nullptr };
      ServerListener* m_listener{ nullptr };
   };

   /**
    * \brief Legacy compound callback handler for backward compatibility with cpp-apx.
    */
   class Handler : public SocketListener
   {
   public:
      virtual ~Handler() = default;

      virtual void socket_accepted(msocket_server_t* server, msocket_t* accepted_socket)
      {
         (void)server;
         (void)accepted_socket;
      }

      virtual void udp_message_received(const std::string& address, std::uint16_t port, const std::uint8_t* data, std::size_t data_size)
      {
         (void)address;
         (void)port;
         (void)data;
         (void)data_size;
      }

      virtual void socket_connected(const std::string& address, std::uint16_t port)
      {
         (void)address;
         (void)port;
      }

      virtual void socket_disconnected()
      {
      }

      virtual int socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len)
      {
         (void)data;
         parse_len = data_size;
         return 0;
      }

      // Adapt SocketListener to legacy Handler methods
      void on_connected(std::string_view address, std::uint16_t port) override
      {
         socket_connected(std::string(address), port);
      }

      void on_disconnected() override
      {
         socket_disconnected();
      }

      int on_data_received(const std::uint8_t* data, std::size_t size, std::size_t& parse_len) override
      {
         return socket_data_received(data, size, parse_len);
      }
   };

   // Legacy C pointer registration functions (backward compatibility)
   void set_handler(msocket_t* msocket, Handler* handler);
   void set_server_handler(msocket_server_t* server, Handler* handler);
   void set_client_handler(testsocket_t* msocket, Handler* handler);
   void set_server_handler(testsocket_t* msocket, Handler* handler);

} // namespace msocket

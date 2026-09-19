/*****************************************************************************
* \file      testsuite_cpp_socket.cpp
* \author    Conny Gustafsson
* \date      2026-09-12
* \brief     Unit tests for modern C++ msocket wrapper
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

extern "C" {
#include "CuTest.h"
#include "test_common.h"
}

#include "msocket_adapter.h"
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace
{
   class MockSocketListener : public msocket::SocketListener
   {
   public:
      int connected_calls{ 0 };
      int disconnected_calls{ 0 };
      int data_calls{ 0 };
      std::string last_connected_addr;
      std::uint16_t last_connected_port{ 0 };
      std::vector<std::uint8_t> received_data;

      void on_connected(std::string_view address, std::uint16_t port) override
      {
         connected_calls++;
         last_connected_addr = address;
         last_connected_port = port;
      }

      void on_disconnected() override
      {
         disconnected_calls++;
      }

      int on_data_received(const std::uint8_t* data, std::size_t size, std::size_t& parse_len) override
      {
         data_calls++;
         received_data.assign(data, data + size);
         parse_len = size;
         return 0;
      }
   };

   class MockLegacyHandler : public msocket::Handler
   {
   public:
      int connected_calls{ 0 };
      int disconnected_calls{ 0 };
      int data_calls{ 0 };
      std::string received_msg;

      void socket_connected(const std::string& address, std::uint16_t port) override
      {
         (void)address;
         (void)port;
         connected_calls++;
      }

      void socket_disconnected() override
      {
         disconnected_calls++;
      }

      int socket_data_received(const std::uint8_t* data, std::size_t data_size, std::size_t& parse_len) override
      {
         data_calls++;
         received_msg.assign(reinterpret_cast<const char*>(data), data_size);
         parse_len = data_size;
         return 0;
      }
   };

   class MockServerListener : public msocket::ServerListener
   {
   public:
      std::mutex m_mutex;
      std::condition_variable m_cv;
      int accept_calls{ 0 };
      std::unique_ptr<msocket::Socket> accepted_socket;

      void on_connection_accepted(std::unique_ptr<msocket::Socket> socket) override
      {
         std::lock_guard<std::mutex> lock(m_mutex);
         accept_calls++;
         accepted_socket = std::move(socket);
         m_cv.notify_one();
      }
   };

   void helper_send_via_base(msocket::Socket& sock, std::string_view message)
   {
      sock.send(message);
   }
} // namespace

static void test_cpp_test_socket_connect_and_send(CuTest* tc)
{
   MockSocketListener listener;
   msocket::TestSocket sock;
   CuAssertTrue(tc, sock.is_valid());

   sock.set_listener(&listener);

   int res = sock.connect("127.0.0.1", 8080);
   CuAssertIntEquals(tc, 0, res);
   CuAssertIntEquals(tc, 1, listener.connected_calls);

   std::string msg = "Hello from TestSocket";
   res = sock.send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
   CuAssertIntEquals(tc, 0, res);

   // Inject data into mock from the "server"
   std::string srv_msg = "Server Reply";
   sock.server_send(srv_msg);
   sock.run();

   CuAssertIntEquals(tc, 1, listener.data_calls);
   CuAssertIntEquals(tc, static_cast<int>(srv_msg.size()), static_cast<int>(listener.received_data.size()));
   std::string recvd(listener.received_data.begin(), listener.received_data.end());
   CuAssertStrEquals(tc, srv_msg.c_str(), recvd.c_str());

   sock.simulate_disconnect();
   CuAssertIntEquals(tc, 1, listener.disconnected_calls);

   sock.close();
   CuAssertTrue(tc, !sock.is_valid());
}

static void test_cpp_socket_polymorphism(CuTest* tc)
{
   MockSocketListener listener;
   msocket::TestSocket test_sock;
   msocket::Socket& sock_ref = test_sock;

   sock_ref.set_listener(&listener);
   sock_ref.connect("localhost", 1234);

   helper_send_via_base(sock_ref, "Polymorphic Send");

   std::string reply = "Pong";
   test_sock.server_send(reply);
   test_sock.run();

   CuAssertIntEquals(tc, 1, listener.data_calls);
   std::string recvd(listener.received_data.begin(), listener.received_data.end());
   CuAssertStrEquals(tc, "Pong", recvd.c_str());

   test_sock.simulate_disconnect();
   CuAssertIntEquals(tc, 1, listener.disconnected_calls);
}

static void test_cpp_test_socket_move(CuTest* tc)
{
   MockSocketListener listener;
   msocket::TestSocket sock1;
   CuAssertTrue(tc, sock1.is_valid());

   msocket::TestSocket sock2 = std::move(sock1);
   // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
   CuAssertTrue(tc, !sock1.is_valid());
   CuAssertTrue(tc, sock2.is_valid());

   sock2.set_listener(&listener);
   sock2.connect("127.0.0.1", 9000);
   CuAssertIntEquals(tc, 1, listener.connected_calls);

   msocket::TestSocket sock3;
   sock3 = std::move(sock2);
   // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
   CuAssertTrue(tc, !sock2.is_valid());
   CuAssertTrue(tc, sock3.is_valid());

   sock3.simulate_disconnect();
   CuAssertIntEquals(tc, 1, listener.disconnected_calls);
   sock3.close();
}

static void test_cpp_legacy_handler_compat(CuTest* tc)
{
   testsocket_t* raw = testsocket_new();
   CuAssertPtrNotNull(tc, raw);

   MockLegacyHandler handler;
   msocket::set_client_handler(raw, &handler);

   testsocket_onConnect(raw);
   CuAssertIntEquals(tc, 1, handler.connected_calls);

   std::string msg = "legacy packet";
   testsocket_serverSend(raw, msg.data(), static_cast<uint32_t>(msg.size()));
   testsocket_run(raw);

   CuAssertIntEquals(tc, 1, handler.data_calls);
   CuAssertStrEquals(tc, "legacy packet", handler.received_msg.c_str());

   testsocket_onDisconnect(raw);
   CuAssertIntEquals(tc, 1, handler.disconnected_calls);

   testsocket_delete(raw);
}

static void test_cpp_tcp_socket_lifecycle(CuTest* tc)
{
   msocket::TcpSocket sock(MSOCKET_ADDR_INET);
   CuAssertTrue(tc, sock.is_valid());
   CuAssertPtrNotNull(tc, sock.raw_handle());

   msocket::TcpSocket sock_moved = std::move(sock);
   // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
   CuAssertTrue(tc, !sock.is_valid());
   CuAssertTrue(tc, sock_moved.is_valid());

   // Destructor of sock_moved will safely call msocket_delete via RAII
}

static void test_cpp_tcp_server_lifecycle(CuTest* tc)
{
   MockServerListener srv_listener;
   msocket::TcpServer server(MSOCKET_ADDR_INET);
   server.set_listener(&srv_listener);
   int res = server.start(19890);
   CuAssertIntEquals(tc, 0, res);

   MockSocketListener cli_listener;
   msocket::TcpSocket client(MSOCKET_ADDR_INET);
   client.set_listener(&cli_listener);
   res = client.connect("127.0.0.1", 19890);
   CuAssertIntEquals(tc, 0, res);

   std::unique_lock<std::mutex> lock(srv_listener.m_mutex);
   bool accepted = srv_listener.m_cv.wait_for(lock, std::chrono::milliseconds(1000), [&]() {
      return srv_listener.accept_calls > 0;
   });
   CuAssertTrue(tc, accepted);
   CuAssertIntEquals(tc, 1, srv_listener.accept_calls);
   CuAssertPtrNotNull(tc, srv_listener.accepted_socket.get());
   auto accepted_sock = std::move(srv_listener.accepted_socket);
   lock.unlock();

   client.close();
   accepted_sock->close();
   accepted_sock.reset();

   server.stop();
}

extern "C" CuSuite* testsuite_cpp_socket(void)
{
   CuSuite* suite = CuSuiteNew();
   SUITE_ADD_TEST(suite, test_cpp_test_socket_connect_and_send);
   SUITE_ADD_TEST(suite, test_cpp_socket_polymorphism);
   SUITE_ADD_TEST(suite, test_cpp_test_socket_move);
   SUITE_ADD_TEST(suite, test_cpp_legacy_handler_compat);
   SUITE_ADD_TEST(suite, test_cpp_tcp_socket_lifecycle);
   SUITE_ADD_TEST(suite, test_cpp_tcp_server_lifecycle);
   return suite;
}

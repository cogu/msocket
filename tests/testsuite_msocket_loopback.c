/*****************************************************************************
* \file      testsuite_msocket_loopback.c
* \author    Conny Gustafsson
* \date      2026-09-10
* \brief     Integration loopback tests for TCP, UDP, and UNIX domain sockets
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#include "test_common.h"
#include "msocket.h"
#include "msocket_internal.h"
#include <string.h>

#define TCP_PORT 19876u
#define UDP_PORT 19877u

static msocket_sem_t *g_sem_client_connected = NULL;
static msocket_sem_t *g_sem_server_received = NULL;
static msocket_sem_t *g_sem_client_received = NULL;
static msocket_sem_t *g_sem_udp_received = NULL;

static msocket_t *g_accepted_peer = NULL;
static char g_client_recv_buf[64];
static char g_server_recv_buf[64];
static char g_udp_recv_buf[64];

static void client_on_connected(void *arg, void *socket, const char *addr, uint16_t port)
{
   (void)arg;
   (void)socket;
   (void)addr;
   (void)port;
   msocket_sem_post(g_sem_client_connected);
}

static msocket_error_t client_on_data(void *arg, void *socket, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)socket;
   (void)msg_size_hint;
   uint32_t copy_len = (num_bytes < sizeof(g_client_recv_buf) - 1u) ? num_bytes : sizeof(g_client_recv_buf) - 1u;
   memcpy(g_client_recv_buf, data, copy_len);
   g_client_recv_buf[copy_len] = '\0';
   *consumed_bytes = num_bytes;
   msocket_sem_post(g_sem_client_received);
   return MSOCKET_NO_ERROR;
}

static msocket_error_t server_on_data(void *arg, void *socket, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)socket;
   (void)msg_size_hint;
   uint32_t copy_len = (num_bytes < sizeof(g_server_recv_buf) - 1u) ? num_bytes : sizeof(g_server_recv_buf) - 1u;
   memcpy(g_server_recv_buf, data, copy_len);
   g_server_recv_buf[copy_len] = '\0';
   *consumed_bytes = num_bytes;

   /* Echo Pong back to client */
   if (g_accepted_peer != NULL) {
      msocket_send(g_accepted_peer, "Pong!", 5u);
   }

   msocket_sem_post(g_sem_server_received);
   return MSOCKET_NO_ERROR;
}

static void on_udp_msg(void *arg, void *socket, const char *addr, uint16_t port, const uint8_t *data, const uint32_t num_bytes)
{
   (void)arg;
   (void)socket;
   (void)addr;
   (void)port;
   uint32_t copy_len = (num_bytes < sizeof(g_udp_recv_buf) - 1u) ? num_bytes : sizeof(g_udp_recv_buf) - 1u;
   memcpy(g_udp_recv_buf, data, copy_len);
   g_udp_recv_buf[copy_len] = '\0';
   msocket_sem_post(g_sem_udp_received);
}

typedef struct {
   msocket_t *listener;
   msocket_handler_t *handler;
} accept_thread_arg_t;

static void accept_worker(void *arg)
{
   accept_thread_arg_t *ctx = (accept_thread_arg_t *)arg;
   msocket_t *peer = msocket_accept(ctx->listener, NULL);
   if (peer != NULL) {
      g_accepted_peer = peer;
      msocket_set_handler(peer, ctx->handler, NULL);
      msocket_start_io(peer);
   }
}

static void test_tcp_loopback(CuTest *tc)
{
   g_sem_client_connected = msocket_sem_new(0u);
   g_sem_server_received = msocket_sem_new(0u);
   g_sem_client_received = msocket_sem_new(0u);
   g_accepted_peer = NULL;
   g_client_recv_buf[0] = '\0';
   g_server_recv_buf[0] = '\0';

   msocket_t *srv = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, srv);
   msocket_error_t rc = msocket_listen(srv, MSOCKET_MODE_STREAM, TCP_PORT, "127.0.0.1");
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   msocket_handler_t srv_handler;
   memset(&srv_handler, 0, sizeof(srv_handler));
   srv_handler.stream_data = server_on_data;

   accept_thread_arg_t ctx = { srv, &srv_handler };
   msocket_thread_t *accept_thread = msocket_thread_create(accept_worker, &ctx);
   CuAssertPtrNotNull(tc, accept_thread);

   /* Start client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   cli_handler.stream_connected = client_on_connected;
   cli_handler.stream_data = client_on_data;
   msocket_set_handler(cli, &cli_handler, NULL);

   rc = msocket_connect(cli, "127.0.0.1", TCP_PORT);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   /* Wait for client connected */
   int attempts = 0;
   while (msocket_sem_test(g_sem_client_connected) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Send Ping! */
   rc = msocket_send(cli, "Ping!", 5u);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   /* Wait for server receive */
   attempts = 0;
   while (msocket_sem_test(g_sem_server_received) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertStrEquals(tc, "Ping!", g_server_recv_buf);

   /* Wait for client echo receive */
   attempts = 0;
   while (msocket_sem_test(g_sem_client_received) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertStrEquals(tc, "Pong!", g_client_recv_buf);

   /* Cleanup */
   msocket_thread_join(accept_thread);
   msocket_thread_delete(accept_thread);

   msocket_close(cli);
   msocket_delete(cli);

   if (g_accepted_peer != NULL) {
      msocket_close(g_accepted_peer);
      msocket_delete(g_accepted_peer);
      g_accepted_peer = NULL;
   }

   msocket_close(srv);
   msocket_delete(srv);

   msocket_sem_delete(g_sem_client_connected);
   msocket_sem_delete(g_sem_server_received);
   msocket_sem_delete(g_sem_client_received);
}

static void test_udp_loopback(CuTest *tc)
{
   g_sem_udp_received = msocket_sem_new(0u);
   g_udp_recv_buf[0] = '\0';

   msocket_t *receiver = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, receiver);

   msocket_handler_t rx_handler;
   memset(&rx_handler, 0, sizeof(rx_handler));
   rx_handler.datagram_msg = on_udp_msg;
   msocket_set_handler(receiver, &rx_handler, NULL);

   msocket_error_t rc = msocket_listen(receiver, MSOCKET_MODE_DGRAM, UDP_PORT, "127.0.0.1");
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   msocket_t *sender = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, sender);
   rc = msocket_listen(sender, MSOCKET_MODE_DGRAM, 0u, "127.0.0.1");
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   rc = msocket_send_to(sender, "127.0.0.1", UDP_PORT, "HelloUDP", 8u);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   int attempts = 0;
   while (msocket_sem_test(g_sem_udp_received) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertStrEquals(tc, "HelloUDP", g_udp_recv_buf);

   msocket_close(sender);
   msocket_delete(sender);

   msocket_close(receiver);
   msocket_delete(receiver);

   msocket_sem_delete(g_sem_udp_received);
}

#define TCP_PORT_SIGPIPE 19878u

static void test_tcp_send_to_closed_peer_returns_error_without_sigpipe(CuTest *tc)
{
   g_sem_client_connected = msocket_sem_new(0u);
   g_accepted_peer = NULL;

   msocket_t *srv = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, srv);
   msocket_error_t rc = msocket_listen(srv, MSOCKET_MODE_STREAM, TCP_PORT_SIGPIPE, "127.0.0.1");
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   msocket_handler_t srv_handler;
   memset(&srv_handler, 0, sizeof(srv_handler));

   accept_thread_arg_t ctx = { srv, &srv_handler };
   msocket_thread_t *accept_thread = msocket_thread_create(accept_worker, &ctx);
   CuAssertPtrNotNull(tc, accept_thread);

   /* Start client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   cli_handler.stream_connected = client_on_connected;
   msocket_set_handler(cli, &cli_handler, NULL);

   rc = msocket_connect(cli, "127.0.0.1", TCP_PORT_SIGPIPE);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   /* Wait for client connected */
   int attempts = 0;
   while (msocket_sem_test(g_sem_client_connected) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Wait for accept thread to finish accepting peer */
   msocket_thread_join(accept_thread);
   msocket_thread_delete(accept_thread);
   CuAssertPtrNotNull(tc, g_accepted_peer);

   /* Close the client abruptly */
   msocket_close(cli);
   msocket_delete(cli);
   msocket_sleep_ms(50u);

   /* Sending to the closed peer from server side should not trigger SIGPIPE or crash */
   (void)msocket_send(g_accepted_peer, "Test1", 5u);
   msocket_sleep_ms(50u);
   (void)msocket_send(g_accepted_peer, "Test2", 5u);

   /* Cleanup */
   if (g_accepted_peer != NULL) {
      msocket_close(g_accepted_peer);
      msocket_delete(g_accepted_peer);
      g_accepted_peer = NULL;
   }

   msocket_close(srv);
   msocket_delete(srv);

   msocket_sem_delete(g_sem_client_connected);
}

CuSuite *testsuite_msocket_loopback(void)
{
   CuSuite *suite = CuSuiteNew();
   SUITE_ADD_TEST(suite, test_tcp_loopback);
   SUITE_ADD_TEST(suite, test_udp_loopback);
   SUITE_ADD_TEST(suite, test_tcp_send_to_closed_peer_returns_error_without_sigpipe);
   return suite;
}


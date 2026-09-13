/*****************************************************************************
* \file      testsuite_testsocket.c
* \author    Conny Gustafsson
* \date      2026-09-10
* \brief     Unit tests for testsocket and testsocket_spy
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#include "test_common.h"
#include "testsocket.h"
#include "testsocket_spy.h"
#include <string.h>

static int g_client_data_calls = 0;
static int g_server_data_calls = 0;
static uint32_t g_last_client_len = 0;
static uint32_t g_last_server_len = 0;

static msocket_error_t client_on_data(void *arg, void *socket, const uint8_t *buf, const uint32_t len, uint32_t *parse_len, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)socket;
   (void)buf;
   (void)msg_size_hint;
   g_client_data_calls++;
   g_last_client_len = len;
   *parse_len = len;
   return MSOCKET_NO_ERROR;
}

static msocket_error_t server_on_data(void *arg, void *socket, const uint8_t *buf, const uint32_t len, uint32_t *parse_len, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)socket;
   (void)buf;
   (void)msg_size_hint;
   g_server_data_calls++;
   g_last_server_len = len;
   *parse_len = len;
   return MSOCKET_NO_ERROR;
}

static void test_testsocket_create_destroy(CuTest *tc)
{
   testsocket_t *sock = testsocket_new();
   CuAssertPtrNotNull(tc, sock);
   testsocket_delete(sock);
}

static void test_testsocket_transfer(CuTest *tc)
{
   testsocket_t sock;
   testsocket_create(&sock);

   msocket_handler_t client_handler;
   memset(&client_handler, 0, sizeof(client_handler));
   client_handler.stream_data = client_on_data;
   testsocket_set_client_handler(&sock, &client_handler, NULL);

   msocket_handler_t server_handler;
   memset(&server_handler, 0, sizeof(server_handler));
   server_handler.stream_data = server_on_data;
   testsocket_set_server_handler(&sock, &server_handler, NULL);

   g_client_data_calls = 0;
   g_server_data_calls = 0;

   /* Server sends to client */
   const char msg1[] = "Hello from server";
   msocket_error_t rc = testsocket_server_send(&sock, msg1, (uint32_t)strlen(msg1));
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 0, g_client_data_calls);

   testsocket_run(&sock);
   CuAssertIntEquals(tc, 1, g_client_data_calls);
   CuAssertIntEquals(tc, (int)strlen(msg1), (int)g_last_client_len);

   /* Client sends to server */
   const char msg2[] = "Hello from client";
   rc = testsocket_client_send(&sock, msg2, (uint32_t)strlen(msg2));
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 0, g_server_data_calls);

   testsocket_run(&sock);
   CuAssertIntEquals(tc, 1, g_server_data_calls);
   CuAssertIntEquals(tc, (int)strlen(msg2), (int)g_last_server_len);

   testsocket_destroy(&sock);
}

static void test_testsocket_spy(CuTest *tc)
{
   testsocket_spy_create();

   testsocket_t *client = testsocket_spy_client();
   testsocket_t *server = testsocket_spy_server();
   CuAssertPtrNotNull(tc, client);
   CuAssertPtrNotNull(tc, server);

   testsocket_on_connect(client);
   CuAssertIntEquals(tc, 1, testsocket_spy_get_client_connected_count());

   testsocket_on_connect(server);
   CuAssertIntEquals(tc, 1, testsocket_spy_get_server_connected_count());

   const char data[] = "12345";
   testsocket_client_send(server, data, 5u);
   testsocket_run(server);

   CuAssertIntEquals(tc, 5, (int)testsocket_spy_get_server_bytes_received());

   uint32_t recv_len = 0u;
   const uint8_t *recv_data = testsocket_spy_get_received_data(&recv_len);
   CuAssertIntEquals(tc, 5, (int)recv_len);
   CuAssertTrue(tc, memcmp(recv_data, "12345", 5) == 0);

   testsocket_on_disconnect(client);
   CuAssertIntEquals(tc, 1, testsocket_spy_get_client_disconnect_count());

   testsocket_delete(client);
   testsocket_delete(server);
   testsocket_spy_destroy();
}

CuSuite *testsuite_testsocket(void)
{
   CuSuite *suite = CuSuiteNew();
   SUITE_ADD_TEST(suite, test_testsocket_create_destroy);
   SUITE_ADD_TEST(suite, test_testsocket_transfer);
   SUITE_ADD_TEST(suite, test_testsocket_spy);
   return suite;
}

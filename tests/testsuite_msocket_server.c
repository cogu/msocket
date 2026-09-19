/*****************************************************************************
* \file      testsuite_msocket_server.c
* \author    Conny Gustafsson
* \date      2026-09-10
* \brief     Unit tests for msocket_server accept loop and cleanup queue
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#include "test_common.h"
#include "msocket_server.h"
#include "msocket_internal.h"
#include <string.h>

#define SERVER_TEST_PORT 19878u

static msocket_sem_t *m_sem_accepted = NULL;
static msocket_sem_t *m_sem_cleaned_up = NULL;
static int m_cleanup_count = 0;
static msocket_t *m_accepted_socket = NULL;

typedef struct custom_conn_tag {
   msocket_t *sock;
   msocket_server_t *srv;
   int custom_id;
} custom_conn_t;

static void on_custom_destructor(void *arg)
{
   msocket_t *sock = (msocket_t *)arg;
   if (sock != NULL) {
      msocket_delete(sock);
      m_cleanup_count++;
      msocket_sem_post(m_sem_cleaned_up);
   }
}

static void on_server_accept_cleanup(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   (void)srv;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      msocket_sem_post(m_sem_accepted);
      /* Queue for asynchronous destruction */
      msocket_server_cleanup_connection(srv, child);
   }
}

static void test_msocket_server_cleanup(CuTest *tc)
{
   m_sem_accepted = msocket_sem_new(0u);
   m_sem_cleaned_up = msocket_sem_new(0u);
   m_cleanup_count = 0;

   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, on_custom_destructor);
   CuAssertPtrNotNull(tc, srv);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = on_server_accept_cleanup;
   msocket_server_set_handler(srv, &handler, NULL);

   msocket_server_start(srv, NULL, 0u, SERVER_TEST_PORT);

   /* Connect client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   msocket_set_handler(cli, &cli_handler, NULL);

   msocket_error_t rc = msocket_connect(cli, "127.0.0.1", SERVER_TEST_PORT);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);

   /* Wait for server accept */
   int attempts = 0;
   while (msocket_sem_test(m_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Wait for asynchronous cleanup queue to run destructor */
   attempts = 0;
   while (msocket_sem_test(m_sem_cleaned_up) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertIntEquals(tc, 1, m_cleanup_count);

   msocket_close(cli);
   msocket_delete(cli);
   msocket_server_delete(srv);

   msocket_sem_delete(m_sem_accepted);
   msocket_sem_delete(m_sem_cleaned_up);
}

static void on_server_accept_auto(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      msocket_set_server(child, srv);
      msocket_handler_t child_handler;
      memset(&child_handler, 0, sizeof(child_handler));
      msocket_set_handler(child, &child_handler, NULL);
      msocket_start_io(child);
      msocket_sem_post(m_sem_accepted);
   }
}

static void test_msocket_server_auto_reap(CuTest *tc)
{
   m_sem_accepted = msocket_sem_new(0u);
   m_sem_cleaned_up = msocket_sem_new(0u);
   m_cleanup_count = 0;

   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, on_custom_destructor);
   CuAssertPtrNotNull(tc, srv);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = on_server_accept_auto;
   msocket_server_set_handler(srv, &handler, NULL);

   msocket_server_start(srv, NULL, 0u, SERVER_TEST_PORT + 1u);

   /* Connect client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   msocket_set_handler(cli, &cli_handler, NULL);

   msocket_error_t rc = msocket_connect(cli, "127.0.0.1", SERVER_TEST_PORT + 1u);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   msocket_start_io(cli);

   /* Wait for server accept */
   int attempts = 0;
   while (msocket_sem_test(m_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Disconnect client */
   msocket_close(cli);
   msocket_delete(cli);

   /* Wait for automatic cleanup queue to run destructor */
   attempts = 0;
   while (msocket_sem_test(m_sem_cleaned_up) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertIntEquals(tc, 1, m_cleanup_count);

   msocket_server_delete(srv);

   msocket_sem_delete(m_sem_accepted);
   msocket_sem_delete(m_sem_cleaned_up);
}

static void on_server_accept_default(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   (void)srv;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      msocket_handler_t child_handler;
      memset(&child_handler, 0, sizeof(child_handler));
      msocket_set_handler(child, &child_handler, NULL);
      msocket_start_io(child);
      msocket_sem_post(m_sem_accepted);
   }
}

static void test_msocket_server_default_auto_reap(CuTest *tc)
{
   m_sem_accepted = msocket_sem_new(0u);

   /* Server with default destructor (msocket_vdelete) automatically attaches accepted sockets */
   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, NULL);
   CuAssertPtrNotNull(tc, srv);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = on_server_accept_default;
   msocket_server_set_handler(srv, &handler, NULL);

   msocket_server_start(srv, NULL, 0u, SERVER_TEST_PORT + 2u);

   /* Connect client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   msocket_set_handler(cli, &cli_handler, NULL);

   msocket_error_t rc = msocket_connect(cli, "127.0.0.1", SERVER_TEST_PORT + 2u);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   msocket_start_io(cli);

   /* Wait for server accept */
   int attempts = 0;
   while (msocket_sem_test(m_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Disconnect client - auto-reap should handle child socket without error */
   msocket_close(cli);
   msocket_delete(cli);

   msocket_sleep_ms(50u);
   msocket_server_delete(srv);
   msocket_sem_delete(m_sem_accepted);
}

static void on_custom_conn_delete(void *arg)
{
   custom_conn_t *conn = (custom_conn_t *)arg;
   if (conn != NULL) {
      if (conn->sock != NULL) {
         msocket_delete(conn->sock);
         conn->sock = NULL;
      }
      free(conn);
      m_cleanup_count++;
      msocket_sem_post(m_sem_cleaned_up);
   }
}

static void on_wrapper_client_disconnected(void *arg, void *socket)
{
   (void)socket;
   custom_conn_t *conn = (custom_conn_t *)arg;
   if (conn != NULL && conn->srv != NULL) {
      msocket_server_cleanup_connection(conn->srv, conn);
   }
}

static void on_server_accept_custom_wrapper(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      custom_conn_t *conn = (custom_conn_t *)malloc(sizeof(custom_conn_t));
      if (conn != NULL) {
         conn->sock = child;
         conn->srv = srv;
         conn->custom_id = 42;
         msocket_handler_t child_handler;
         memset(&child_handler, 0, sizeof(child_handler));
         child_handler.stream_disconnected = on_wrapper_client_disconnected;
         msocket_set_handler(child, &child_handler, (void *)conn);
         msocket_start_io(child);
         msocket_sem_post(m_sem_accepted);
      }
   }
}

static void test_msocket_server_custom_wrapper(CuTest *tc)
{
   m_sem_accepted = msocket_sem_new(0u);
   m_sem_cleaned_up = msocket_sem_new(0u);
   m_cleanup_count = 0;

   /* Server with custom wrapper destructor (e.g. c-apx json_server_connection_vdelete pattern) */
   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, on_custom_conn_delete);
   CuAssertPtrNotNull(tc, srv);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = on_server_accept_custom_wrapper;
   msocket_server_set_handler(srv, &handler, NULL);

   msocket_server_start(srv, NULL, 0u, SERVER_TEST_PORT + 3u);

   /* Connect client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   msocket_set_handler(cli, &cli_handler, NULL);

   msocket_error_t rc = msocket_connect(cli, "127.0.0.1", SERVER_TEST_PORT + 3u);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   msocket_start_io(cli);

   /* Wait for server accept */
   int attempts = 0;
   while (msocket_sem_test(m_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Disconnect client - wrapper disconnect callback invokes msocket_server_cleanup_connection */
   msocket_close(cli);
   msocket_delete(cli);

   /* Wait for custom destructor to execute */
   attempts = 0;
   while (msocket_sem_test(m_sem_cleaned_up) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertIntEquals(tc, 1, m_cleanup_count);

   msocket_server_delete(srv);
   msocket_sem_delete(m_sem_accepted);
   msocket_sem_delete(m_sem_cleaned_up);
}

static void on_child_disconnected_cleanup(void *arg, void *socket)
{
   msocket_server_t *srv = (msocket_server_t *)arg;
   msocket_t *child = (msocket_t *)socket;
   if (srv != NULL && child != NULL) {
      msocket_server_cleanup_connection(srv, child);
   }
}

static void on_server_accept_manual_default(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      msocket_handler_t child_handler;
      memset(&child_handler, 0, sizeof(child_handler));
      child_handler.stream_disconnected = on_child_disconnected_cleanup;
      msocket_set_handler(child, &child_handler, (void *)srv);
      msocket_start_io(child);
      msocket_sem_post(m_sem_accepted);
   }
}

static void test_msocket_server_manual_cleanup_default(CuTest *tc)
{
   m_sem_accepted = msocket_sem_new(0u);

   /* Server with default destructor */
   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, NULL);
   CuAssertPtrNotNull(tc, srv);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = on_server_accept_manual_default;
   msocket_server_set_handler(srv, &handler, NULL);

   msocket_server_start(srv, NULL, 0u, SERVER_TEST_PORT + 4u);

   /* Connect client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   msocket_set_handler(cli, &cli_handler, NULL);

   msocket_error_t rc = msocket_connect(cli, "127.0.0.1", SERVER_TEST_PORT + 4u);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   msocket_start_io(cli);

   /* Wait for server accept */
   int attempts = 0;
   while (msocket_sem_test(m_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Disconnect client - on_child_disconnected_cleanup calls msocket_server_cleanup_connection.
    * It unsets child->server, so when io_task finishes it will not double reap. */
   msocket_close(cli);
   msocket_delete(cli);

   msocket_sleep_ms(50u);
   msocket_server_delete(srv);
   msocket_sem_delete(m_sem_accepted);
}

static void on_server_accept_disabled(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   (void)srv;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      m_accepted_socket = child;
      msocket_handler_t child_handler;
      memset(&child_handler, 0, sizeof(child_handler));
      msocket_set_handler(child, &child_handler, NULL);
      msocket_start_io(child);
      msocket_sem_post(m_sem_accepted);
   }
}

static void test_msocket_server_disable_cleanup(CuTest *tc)
{
   m_sem_accepted = msocket_sem_new(0u);
   m_accepted_socket = NULL;

   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, NULL);
   CuAssertPtrNotNull(tc, srv);

   /* Disable automatic cleanup (as in apx_socket_extension) */
   msocket_server_disable_cleanup(srv);
   CuAssertPtrEquals(tc, NULL, srv->destructor);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = on_server_accept_disabled;
   msocket_server_set_handler(srv, &handler, NULL);

   msocket_server_start(srv, NULL, 0u, SERVER_TEST_PORT + 5u);

   /* Connect client */
   msocket_t *cli = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, cli);

   msocket_handler_t cli_handler;
   memset(&cli_handler, 0, sizeof(cli_handler));
   msocket_set_handler(cli, &cli_handler, NULL);

   msocket_error_t rc = msocket_connect(cli, "127.0.0.1", SERVER_TEST_PORT + 5u);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   msocket_start_io(cli);

   /* Wait for server accept */
   int attempts = 0;
   while (msocket_sem_test(m_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertPtrNotNull(tc, m_accepted_socket);

   /* Disconnect client */
   msocket_close(cli);
   msocket_delete(cli);

   msocket_sleep_ms(30u);

   /* Application manually manages child socket lifecycle */
   msocket_delete(m_accepted_socket);

   msocket_server_delete(srv);
   msocket_sem_delete(m_sem_accepted);
}

CuSuite *testsuite_msocket_server(void)
{
   CuSuite *suite = CuSuiteNew();
   SUITE_ADD_TEST(suite, test_msocket_server_cleanup);
   SUITE_ADD_TEST(suite, test_msocket_server_auto_reap);
   SUITE_ADD_TEST(suite, test_msocket_server_default_auto_reap);
   SUITE_ADD_TEST(suite, test_msocket_server_custom_wrapper);
   SUITE_ADD_TEST(suite, test_msocket_server_manual_cleanup_default);
   SUITE_ADD_TEST(suite, test_msocket_server_disable_cleanup);
   return suite;
}

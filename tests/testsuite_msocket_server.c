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

static msocket_sem_t *g_sem_accepted = NULL;
static msocket_sem_t *g_sem_cleaned_up = NULL;
static int g_cleanup_count = 0;

static void custom_destructor(void *arg)
{
   msocket_t *sock = (msocket_t *)arg;
   if (sock != NULL) {
      msocket_delete(sock);
      g_cleanup_count++;
      msocket_sem_post(g_sem_cleaned_up);
   }
}

static void server_on_accept(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   (void)srv;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      msocket_sem_post(g_sem_accepted);
      /* Queue for asynchronous destruction */
      msocket_server_cleanup_connection(srv, child);
   }
}

static void test_msocket_server_cleanup(CuTest *tc)
{
   g_sem_accepted = msocket_sem_new(0u);
   g_sem_cleaned_up = msocket_sem_new(0u);
   g_cleanup_count = 0;

   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, custom_destructor);
   CuAssertPtrNotNull(tc, srv);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = server_on_accept;
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
   while (msocket_sem_test(g_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Wait for asynchronous cleanup queue to run destructor */
   attempts = 0;
   while (msocket_sem_test(g_sem_cleaned_up) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertIntEquals(tc, 1, g_cleanup_count);

   msocket_close(cli);
   msocket_delete(cli);
   msocket_server_delete(srv);

   msocket_sem_delete(g_sem_accepted);
   msocket_sem_delete(g_sem_cleaned_up);
}

static void server_on_accept_auto(void *arg, msocket_server_t *srv, void *socket)
{
   (void)arg;
   (void)srv;
   msocket_t *child = (msocket_t *)socket;
   if (child != NULL) {
      msocket_handler_t child_handler;
      memset(&child_handler, 0, sizeof(child_handler));
      msocket_set_handler(child, &child_handler, NULL);
      msocket_start_io(child);
      msocket_sem_post(g_sem_accepted);
   }
}

static void test_msocket_server_auto_reap(CuTest *tc)
{
   g_sem_accepted = msocket_sem_new(0u);
   g_sem_cleaned_up = msocket_sem_new(0u);
   g_cleanup_count = 0;

   msocket_server_t *srv = msocket_server_new(MSOCKET_ADDR_INET, custom_destructor);
   CuAssertPtrNotNull(tc, srv);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_accept = server_on_accept_auto;
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
   while (msocket_sem_test(g_sem_accepted) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);

   /* Disconnect client */
   msocket_close(cli);
   msocket_delete(cli);

   /* Wait for automatic cleanup queue to run destructor */
   attempts = 0;
   while (msocket_sem_test(g_sem_cleaned_up) <= 0 && attempts++ < 50) {
      msocket_sleep_ms(20u);
   }
   CuAssertTrue(tc, attempts < 50);
   CuAssertIntEquals(tc, 1, g_cleanup_count);

   msocket_server_delete(srv);

   msocket_sem_delete(g_sem_accepted);
   msocket_sem_delete(g_sem_cleaned_up);
}

CuSuite *testsuite_msocket_server(void)
{
   CuSuite *suite = CuSuiteNew();
   SUITE_ADD_TEST(suite, test_msocket_server_cleanup);
   SUITE_ADD_TEST(suite, test_msocket_server_auto_reap);
   return suite;
}

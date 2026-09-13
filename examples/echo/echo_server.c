/*****************************************************************************
* \file      echo_server.c
* \author    Conny Gustafsson
* \date      2026-09-12
* \brief     Example TCP Echo Server using msocket2
*
* Copyright (c) 2014-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "msocket.h"
#include "msocket_server.h"

//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND TYPES
//////////////////////////////////////////////////////////////////////////////
#define DEFAULT_PORT 5000u

static volatile int g_running = 1;

static void sigint_handler(int signum)
{
   (void)signum;
   g_running = 0;
}

//////////////////////////////////////////////////////////////////////////////
// PER-CLIENT EVENT HANDLERS
//////////////////////////////////////////////////////////////////////////////

static msocket_error_t client_on_data(void *arg, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)msg_size_hint;
   msocket_t *client_socket = (msocket_t *)arg;

   /* Mark all received data as consumed */
   *consumed_bytes = num_bytes;

   printf("[SERVER] Received %u bytes: \"%.*s\"\n", num_bytes, (int)num_bytes, (const char *)data);

   /* Echo received data back to client */
   msocket_error_t rc = msocket_send(client_socket, data, num_bytes);
   if (rc != MSOCKET_NO_ERROR) {
      fprintf(stderr, "[SERVER] Failed to echo data back to client\n");
      return rc;
   }
   return MSOCKET_NO_ERROR;
}

static msocket_server_t *g_server = NULL;

static void client_on_disconnected(void *arg)
{
   msocket_t *client_socket = (msocket_t *)arg;
   printf("[SERVER] Client disconnected\n");

   /* Queue disconnected socket for asynchronous destruction by cleanup thread */
   if (g_server != NULL) {
      msocket_server_cleanup_connection(g_server, (void *)client_socket);
   }
}

//////////////////////////////////////////////////////////////////////////////
// SERVER CONNECTION ACCEPT CALLBACK
//////////////////////////////////////////////////////////////////////////////

static void server_on_accept(void *arg, msocket_server_t *srv, msocket_t *child_socket)
{
   (void)srv;
   (void)arg;

   printf("[SERVER] Accepted new connection from %s:%u\n", child_socket->stream_info.addr, child_socket->stream_info.port);

   /* Configure handlers for the new child socket */
   msocket_handler_t client_handler;
   memset(&client_handler, 0, sizeof(client_handler));
   client_handler.stream_data = client_on_data;
   client_handler.stream_disconnected = client_on_disconnected;
   msocket_set_handler(child_socket, &client_handler, (void *)child_socket);

   /*
    * IMPORTANT: For accepted server sockets, the I/O event thread must be
    * started explicitly once per-connection handlers and context are attached!
    */
   if (msocket_start_io(child_socket) != MSOCKET_NO_ERROR) {
      fprintf(stderr, "[SERVER] Failed to start I/O for accepted socket\n");
      msocket_delete(child_socket);
      return;
   }
}

//////////////////////////////////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
   uint16_t port = DEFAULT_PORT;

   if (argc > 1) {
      int p = atoi(argv[1]);
      if (p > 0 && p <= 65535) {
         port = (uint16_t)p;
      } else {
         fprintf(stderr, "Usage: %s [port]\n", argv[0]);
         return 1;
      }
   }

   signal(SIGINT, sigint_handler);
   signal(SIGTERM, sigint_handler);

   printf("[SERVER] Starting Echo Server on port %u (press Ctrl+C to exit)...\n", port);

   msocket_server_t *server = msocket_server_new(MSOCKET_ADDR_INET, msocket_vdelete);
   if (server == NULL) {
      fprintf(stderr, "[SERVER] Failed to allocate msocket_server\n");
      return 1;
   }
   g_server = server;

   msocket_handler_t server_handler;
   memset(&server_handler, 0, sizeof(server_handler));
   server_handler.stream_accept = server_on_accept;
   msocket_server_set_handler(server, &server_handler, (void *)server);

   /* Bind and start listening */
   msocket_server_start(server, NULL, 0u, port);

   while (g_running) {
#ifdef _WIN32
      Sleep(200);
#else
      usleep(200000);
#endif
   }

   printf("\n[SERVER] Shutting down...\n");
   msocket_server_delete(server);
   printf("[SERVER] Done.\n");
   return 0;
}

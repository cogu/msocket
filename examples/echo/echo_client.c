/*****************************************************************************
* \file      echo_client.c
* \author    Conny Gustafsson
* \date      2026-09-12
* \brief     Example TCP Echo Client using msocket2
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
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "msocket.h"

//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND TYPES
//////////////////////////////////////////////////////////////////////////////
#define DEFAULT_PORT 5000u
#define DEFAULT_ADDR "127.0.0.1"

static volatile int g_connected = 0;
static volatile int g_received = 0;
static volatile int g_disconnected = 0;

//////////////////////////////////////////////////////////////////////////////
// CLIENT EVENT HANDLERS
//////////////////////////////////////////////////////////////////////////////

static void on_connected(void *arg, void *socket, const char *addr, uint16_t port)
{
   (void)arg;
   (void)socket;
   printf("[CLIENT] Connected successfully to %s:%u\n", addr, port);
   g_connected = 1;
}

static msocket_error_t on_data(void *arg, void *socket, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)socket;
   (void)msg_size_hint;
   *consumed_bytes = num_bytes;
   printf("[CLIENT] Server echoed %u bytes: \"%.*s\"\n", num_bytes, (int)num_bytes, (const char *)data);
   g_received = 1;
   return MSOCKET_NO_ERROR;
}

static void on_disconnected(void *arg, void *socket)
{
   (void)arg;
   (void)socket;
   printf("[CLIENT] Connection closed by server\n");
   g_disconnected = 1;
}

//////////////////////////////////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
   const char *addr = DEFAULT_ADDR;
   uint16_t port = DEFAULT_PORT;

   if (argc > 1) {
      addr = argv[1];
   }
   if (argc > 2) {
      int parsed_port = atoi(argv[2]);
      if (parsed_port > 0 && parsed_port <= 65535) {
         port = (uint16_t)parsed_port;
      }
   }

   printf("[CLIENT] Connecting to %s:%u...\n", addr, port);

   msocket_t *client = msocket_new(MSOCKET_ADDR_INET);
   if (client == NULL) {
      fprintf(stderr, "[CLIENT] Failed to allocate msocket\n");
      return 1;
   }

   /*
    * Configure handlers before connecting.
    */
   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_connected = on_connected;
   handler.stream_data = on_data;
   handler.stream_disconnected = on_disconnected;
   msocket_set_handler(client, &handler, (void *)client);

   /*
    * NOTE: msocket_connect() initiates the connection AND starts the
    * background I/O event thread automatically!
    * There is NO need to call msocket_start_io() for outgoing client sockets.
    */
   if (msocket_connect(client, addr, port) != MSOCKET_NO_ERROR) {
      fprintf(stderr, "[CLIENT] Failed to initiate connection to %s:%u\n", addr, port);
      msocket_delete(client);
      return 1;
   }

   /* Wait for connection to be confirmed */
   int wait_count = 0;
   while (!g_connected && !g_disconnected && wait_count++ < 50) {
#ifdef _WIN32
      Sleep(20);
#else
      usleep(20000);
#endif
   }

   if (!g_connected) {
      fprintf(stderr, "[CLIENT] Connection timed out\n");
      msocket_delete(client);
      return 1;
   }

   /* Send test messages */
   const char *messages[] = {
      "Hello, msocket2!",
      "Cross-platform networking in C99",
      "Echo test completed successfully."
   };
   size_t num_messages = sizeof(messages) / sizeof(messages[0]);

   for (size_t i = 0; i < num_messages; i++) {
      g_received = 0;
      printf("[CLIENT] Sending: \"%s\"\n", messages[i]);
      if (msocket_send(client, messages[i], (uint32_t)strlen(messages[i])) != MSOCKET_NO_ERROR) {
         fprintf(stderr, "[CLIENT] Failed to send message\n");
         break;
      }

      /* Wait for echo response */
      int resp_wait = 0;
      while (!g_received && !g_disconnected && resp_wait++ < 50) {
#ifdef _WIN32
         Sleep(20);
#else
         usleep(20000);
#endif
      }
   }

   printf("[CLIENT] Closing connection...\n");
   msocket_close(client);
   msocket_delete(client);
   printf("[CLIENT] Finished.\n");

   return 0;
}

/*****************************************************************************
* \file      testsocket_spy.c
* \author    Conny Gustafsson
* \date      2018-08-09
* \brief     A testsocket factory and spy
*
* Copyright (c) 2018-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include "testsocket_spy.h"
#include "adt_bytearray.h"

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void client_socket_connected(void *arg, const char *addr, uint16_t port);
static void client_socket_disconnected(void *arg);
static msocket_error_t client_socket_data(void *arg, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint);
static void server_socket_connected(void *arg, const char *addr, uint16_t port);
static void server_socket_disconnected(void *arg);
static msocket_error_t server_socket_data(void *arg, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint);

//////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
//////////////////////////////////////////////////////////////////////////////
static int32_t m_client_connected_count;
static int32_t m_client_disconnected_count;
static uint32_t m_client_bytes_received_total;
static int32_t m_server_connected_count;
static int32_t m_server_disconnected_count;
static uint32_t m_server_bytes_received_total;
static adt_bytearray_t m_data_received;

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

void testsocket_spy_create(void)
{
   m_client_bytes_received_total = 0u;
   m_client_connected_count = 0;
   m_client_disconnected_count = 0;
   m_server_bytes_received_total = 0u;
   m_server_connected_count = 0;
   m_server_disconnected_count = 0;
   adt_bytearray_create(&m_data_received);
}

void testsocket_spy_destroy(void)
{
   adt_bytearray_destroy(&m_data_received);
}

testsocket_t *testsocket_spy_client(void)
{
   testsocket_t *socket_object = testsocket_new();
   if (socket_object != NULL) {
      msocket_handler_t handler_table;
      memset(&handler_table, 0, sizeof(handler_table));
      handler_table.stream_connected = client_socket_connected;
      handler_table.stream_disconnected = client_socket_disconnected;
      handler_table.stream_data = client_socket_data;
      testsocket_set_client_handler(socket_object, &handler_table, NULL);
   }
   return socket_object;
}

testsocket_t *testsocket_spy_server(void)
{
   testsocket_t *socket_object = testsocket_new();
   if (socket_object != NULL) {
      msocket_handler_t handler_table;
      memset(&handler_table, 0, sizeof(handler_table));
      handler_table.stream_connected = server_socket_connected;
      handler_table.stream_disconnected = server_socket_disconnected;
      handler_table.stream_data = server_socket_data;
      testsocket_set_server_handler(socket_object, &handler_table, NULL);
   }
   return socket_object;
}

const uint8_t *testsocket_spy_get_received_data(uint32_t *data_len)
{
   uint32_t cur_len = adt_bytearray_length(&m_data_received);
   if (data_len != NULL) {
      *data_len = cur_len;
   }
   if (cur_len > 0u) {
      return adt_bytearray_data(&m_data_received);
   }
   return NULL;
}

void testsocket_spy_clear_received_data(void)
{
   adt_bytearray_clear(&m_data_received);
}

int32_t testsocket_spy_get_client_connected_count(void)
{
   return m_client_connected_count;
}

int32_t testsocket_spy_get_client_disconnect_count(void)
{
   return m_client_disconnected_count;
}

int32_t testsocket_spy_get_server_connected_count(void)
{
   return m_server_connected_count;
}

int32_t testsocket_spy_get_server_disconnect_count(void)
{
   return m_server_disconnected_count;
}

uint32_t testsocket_spy_get_client_bytes_received(void)
{
   return m_client_bytes_received_total;
}

uint32_t testsocket_spy_get_server_bytes_received(void)
{
   return m_server_bytes_received_total;
}

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

static void client_socket_connected(void *arg, const char *addr, uint16_t port)
{
   (void)arg;
   (void)addr;
   (void)port;
   m_client_connected_count++;
}

static void client_socket_disconnected(void *arg)
{
   (void)arg;
   m_client_disconnected_count++;
}

static msocket_error_t client_socket_data(void *arg, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)msg_size_hint;
   *consumed_bytes = num_bytes;
   m_client_bytes_received_total += num_bytes;
   adt_bytearray_append(&m_data_received, data, num_bytes);
   return MSOCKET_NO_ERROR;
}

static void server_socket_connected(void *arg, const char *addr, uint16_t port)
{
   (void)arg;
   (void)addr;
   (void)port;
   m_server_connected_count++;
}

static void server_socket_disconnected(void *arg)
{
   (void)arg;
   m_server_disconnected_count++;
}

static msocket_error_t server_socket_data(void *arg, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)msg_size_hint;
   *consumed_bytes = num_bytes;
   m_server_bytes_received_total += num_bytes;
   adt_bytearray_append(&m_data_received, data, num_bytes);
   return MSOCKET_NO_ERROR;
}

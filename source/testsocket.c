/*****************************************************************************
* \file:    testsocket.c
* \author:  Conny Gustafsson
* \date:    2017-08-29
* \brief:   A drop-in mock socket implementation for unit testing purposes
*
* Copyright (c) 2017-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "testsocket.h"

void testsocket_create(testsocket_t *self)
{
   if (self != NULL) {
      adt_bytearray_create(&self->pending_client);
      adt_bytearray_create(&self->pending_server);
      memset(&self->client_handler_table, 0, sizeof(msocket_handler_t));
      memset(&self->server_handler_table, 0, sizeof(msocket_handler_t));
      self->server_handler_arg = NULL;
      self->client_handler_arg = NULL;
   }
}

void testsocket_destroy(testsocket_t *self)
{
   if (self != NULL) {
      adt_bytearray_destroy(&self->pending_client);
      adt_bytearray_destroy(&self->pending_server);
   }
}

testsocket_t *testsocket_new(void)
{
   testsocket_t *self = (testsocket_t *)malloc(sizeof(testsocket_t));
   if (self != NULL) {
      testsocket_create(self);
   } else {
      errno = ENOMEM;
   }
   return self;
}

void testsocket_delete(testsocket_t *self)
{
   if (self != NULL) {
      testsocket_destroy(self);
      free(self);
   }
}

void testsocket_vdelete(void *arg)
{
   testsocket_delete((testsocket_t *)arg);
}

msocket_error_t testsocket_server_send(testsocket_t *self, const void *msg_data, uint32_t msg_len)
{
   if (self == NULL || msg_data == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   return (adt_bytearray_append(&self->pending_client, (const uint8_t *)msg_data, msg_len) == ADT_NO_ERROR) ? MSOCKET_NO_ERROR : MSOCKET_MEM_ERROR;
}

msocket_error_t testsocket_client_send(testsocket_t *self, const void *msg_data, uint32_t msg_len)
{
   if (self == NULL || msg_data == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   return (adt_bytearray_append(&self->pending_server, (const uint8_t *)msg_data, msg_len) == ADT_NO_ERROR) ? MSOCKET_NO_ERROR : MSOCKET_MEM_ERROR;
}

void testsocket_set_server_handler(testsocket_t *self, const msocket_handler_t *handler_table, void *handler_arg)
{
   if (self != NULL) {
      if (handler_table == NULL) {
         memset(&self->server_handler_table, 0, sizeof(msocket_handler_t));
         self->server_handler_arg = NULL;
      } else {
         memcpy(&self->server_handler_table, handler_table, sizeof(msocket_handler_t));
         self->server_handler_arg = handler_arg;
      }
   }
}

void testsocket_set_client_handler(testsocket_t *self, const msocket_handler_t *handler_table, void *handler_arg)
{
   if (self != NULL) {
      if (handler_table == NULL) {
         memset(&self->client_handler_table, 0, sizeof(msocket_handler_t));
         self->client_handler_arg = NULL;
      } else {
         memcpy(&self->client_handler_table, handler_table, sizeof(msocket_handler_t));
         self->client_handler_arg = handler_arg;
      }
   }
}

void testsocket_on_connect(testsocket_t *self)
{
   if (self != NULL) {
      if (self->server_handler_table.stream_connected != NULL) {
         self->server_handler_table.stream_connected(self->server_handler_arg, "testsocket", 0);
      }
      if (self->client_handler_table.stream_connected != NULL) {
         self->client_handler_table.stream_connected(self->client_handler_arg, "testsocket", 0);
      }
   }
}

void testsocket_on_disconnect(testsocket_t *self)
{
   if (self != NULL) {
      if (self->server_handler_table.stream_disconnected != NULL) {
         self->server_handler_table.stream_disconnected(self->server_handler_arg);
      }
      if (self->client_handler_table.stream_disconnected != NULL) {
         self->client_handler_table.stream_disconnected(self->client_handler_arg);
      }
   }
}

void testsocket_run(testsocket_t *self)
{
   if (self != NULL) {
      uint32_t server_pending = adt_bytearray_length(&self->pending_server);
      uint32_t client_pending = adt_bytearray_length(&self->pending_client);

      if (server_pending > 0u && self->server_handler_table.stream_data != NULL) {
         uint32_t consumed_bytes = 0u;
         uint32_t msg_size_hint = 0u;
         const uint8_t *data = adt_bytearray_data(&self->pending_server);
         msocket_error_t result = self->server_handler_table.stream_data(self->server_handler_arg, data, server_pending, &consumed_bytes, &msg_size_hint);
         if (result == MSOCKET_NO_ERROR && consumed_bytes > 0u) {
            adt_bytearray_trim_left(&self->pending_server, data + consumed_bytes);
         }
      }

      if (client_pending > 0u && self->client_handler_table.stream_data != NULL) {
         uint32_t consumed_bytes = 0u;
         uint32_t msg_size_hint = 0u;
         const uint8_t *data = adt_bytearray_data(&self->pending_client);
         msocket_error_t result = self->client_handler_table.stream_data(self->client_handler_arg, data, client_pending, &consumed_bytes, &msg_size_hint);
         if (result == MSOCKET_NO_ERROR && consumed_bytes > 0u) {
            adt_bytearray_trim_left(&self->pending_client, data + consumed_bytes);
         }
      }
   }
}

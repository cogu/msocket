/*****************************************************************************
* \file:    testsocket.h
* \author:  Conny Gustafsson
* \date:    2017-08-29
* \brief:   A drop-in mock socket implementation for unit testing purposes
*
* Copyright (c) 2017-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#ifndef TEST_SOCKET_H
#define TEST_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "msocket.h"
#include "adt_bytearray.h"

//////////////////////////////////////////////////////////////////////////////
// PUBLIC CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
typedef struct testsocket_tag {
   adt_bytearray_t pending_client;
   adt_bytearray_t pending_server;
   msocket_handler_t server_handler_table;
   msocket_handler_t client_handler_table;
   void *server_handler_arg;
   void *client_handler_arg;
} testsocket_t;

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
void testsocket_create(testsocket_t *self);
void testsocket_destroy(testsocket_t *self);
testsocket_t *testsocket_new(void);
void testsocket_delete(testsocket_t *self);
void testsocket_vdelete(void *arg);

msocket_error_t testsocket_server_send(testsocket_t *self, const void *msg_data, uint32_t msg_len);
msocket_error_t testsocket_client_send(testsocket_t *self, const void *msg_data, uint32_t msg_len);
void testsocket_set_server_handler(testsocket_t *self, const msocket_handler_t *handler_table, void *handler_arg);
void testsocket_set_client_handler(testsocket_t *self, const msocket_handler_t *handler_table, void *handler_arg);
void testsocket_on_connect(testsocket_t *self);
void testsocket_on_disconnect(testsocket_t *self);
void testsocket_run(testsocket_t *self);

/* Backwards compatibility */
#define testsocket_serverSend(s, d, l) testsocket_server_send(s, d, l)
#define testsocket_clientSend(s, d, l) testsocket_client_send(s, d, l)
#define testsocket_setServerHandler(s, t, a) testsocket_set_server_handler(s, t, a)
#define testsocket_setClientHandler(s, t, a) testsocket_set_client_handler(s, t, a)
#define testsocket_onConnect(s) testsocket_on_connect(s)
#define testsocket_onDisconnect(s) testsocket_on_disconnect(s)

#ifdef __cplusplus
}
#endif

#endif /* TEST_SOCKET_H */

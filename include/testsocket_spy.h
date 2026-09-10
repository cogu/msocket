/*****************************************************************************
* \file      testsocket_spy.h
* \author    Conny Gustafsson
* \date      2018-08-09
* \brief     Test socket spy / call tracker
*
* Copyright (c) 2018-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#ifndef TEST_SOCKET_SPY_H
#define TEST_SOCKET_SPY_H

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "testsocket.h"

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
void testsocket_spy_create(void);
void testsocket_spy_destroy(void);
testsocket_t *testsocket_spy_client(void);
testsocket_t *testsocket_spy_server(void);
const uint8_t *testsocket_spy_get_received_data(uint32_t *data_len);
void testsocket_spy_clear_received_data(void);
int32_t testsocket_spy_get_client_connected_count(void);
int32_t testsocket_spy_get_client_disconnect_count(void);
int32_t testsocket_spy_get_server_connected_count(void);
int32_t testsocket_spy_get_server_disconnect_count(void);
uint32_t testsocket_spy_get_client_bytes_received(void);
uint32_t testsocket_spy_get_server_bytes_received(void);

#define testsocket_client_spy() testsocket_spy_server()
#define testsocket_server_spy() testsocket_spy_client()

/* Backwards compatibility */
#define testsocket_spy_getReceivedData(l) testsocket_spy_get_received_data(l)
#define testsocket_spy_clearReceivedData() testsocket_spy_clear_received_data()
#define testsocket_spy_getClientConnectedCount() testsocket_spy_get_client_connected_count()
#define testsocket_spy_getClientDisconnectCount() testsocket_spy_get_client_disconnect_count()
#define testsocket_spy_getServerConnectedCount() testsocket_spy_get_server_connected_count()
#define testsocket_spy_getServerDisconnectCount() testsocket_spy_get_server_disconnect_count()
#define testsocket_spy_getClientBytesReceived() testsocket_spy_get_client_bytes_received()
#define testsocket_spy_getServerBytesReceived() testsocket_spy_get_server_bytes_received()

#ifdef __cplusplus
}
#endif

#endif /* TEST_SOCKET_SPY_H */

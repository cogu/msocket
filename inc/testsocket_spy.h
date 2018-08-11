/*****************************************************************************
* \file      testsocketspy.h
* \author    Conny Gustafsson
* \date      2018-08-09
* \brief     Description
*
* Copyright (c) 2018 Conny Gustafsson
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
* the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:

* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
******************************************************************************/
#ifndef TEST_SOCKET_SPY_H
#define TEST_SOCKET_SPY_H

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "testsocket.h"
//////////////////////////////////////////////////////////////////////////////
// PUBLIC CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PUBLIC VARIABLES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
void testsocket_spy_create(void);
void testsocket_spy_destroy(void);
testsocket_t * testsocket_spy_client(void);
testsocket_t * testsocket_spy_server(void);
const uint8_t *testsocket_spy_getReceivedData(uint32_t *dataLen);
void testsocket_spy_clearReceivedData(void);
int32_t testsocket_spy_getClientConnectedCount(void);
int32_t testsocket_spy_getClientDisconnectCount(void);
int32_t testsocket_spy_getServerConnectedCount(void);
int32_t testsocket_spy_getServerDisconnectCount(void);
uint32_t testsocket_spy_getClientBytesReceived(void);
uint32_t testsocket_spy_getServerBytesReceived(void);


#endif //TEST_SOCKET_SPY_H

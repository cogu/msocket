/*****************************************************************************
* \file      testsocketspy.c
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
//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include "testsocket_spy.h"
#include "adt_bytearray.h"

//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void client_socket_connected(void *arg,const char *addr, uint16_t port);
static void client_socket_disconnected(void *arg);
static int8_t client_socket_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen);
static void server_socket_connected(void *arg,const char *addr, uint16_t port);
static void server_socket_disconnected(void *arg);
static int8_t server_socket_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen);
//////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
//////////////////////////////////////////////////////////////////////////////
static int32_t m_clientConnectedCount;
static int32_t m_clientDisconnectedCount;
static uint32_t m_clientBytesReceivedTotal;
static int32_t m_serverConnectedCount;
static int32_t m_serverDisconnectedCount;
static uint32_t m_serverBytesReceivedTotal;
static adt_bytearray_t m_dataReceived;

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
void testsocket_spy_create(void)
{
   m_clientBytesReceivedTotal = 0;
   m_clientConnectedCount = 0;
   m_clientDisconnectedCount = 0;
   m_serverBytesReceivedTotal = 0;
   m_serverConnectedCount = 0;
   m_serverDisconnectedCount = 0;
   adt_bytearray_create(&m_dataReceived, ADT_BYTE_ARRAY_DEFAULT_GROW_SIZE);
}

void testsocket_spy_destroy(void)
{
   adt_bytearray_destroy(&m_dataReceived);
}

testsocket_t * testsocket_spy_client(void)
{
   testsocket_t *socketObject;
   msocket_handler_t handlerTable;
   socketObject = testsocket_new();
   memset(&handlerTable, 0, sizeof(handlerTable));
   handlerTable.tcp_connected = client_socket_connected;
   handlerTable.tcp_disconnected = client_socket_disconnected;
   handlerTable.tcp_data = client_socket_data;
   testsocket_setClientHandler(socketObject, &handlerTable, (void*) 0);
   return socketObject;
}

testsocket_t * testsocket_spy_server(void)
{
   testsocket_t *socketObject;
   msocket_handler_t handlerTable;
   socketObject = testsocket_new();
   memset(&handlerTable, 0, sizeof(handlerTable));
   handlerTable.tcp_connected = server_socket_connected;
   handlerTable.tcp_disconnected = server_socket_disconnected;
   handlerTable.tcp_data = server_socket_data;
   testsocket_setServerHandler(socketObject, &handlerTable, (void*) 0);
   return socketObject;
}

const uint8_t *testsocket_spy_getReceivedData(uint32_t *dataLen)
{
   uint32_t curLen = adt_bytearray_length(&m_dataReceived);
   if (dataLen != (uint32_t*) 0)
   {
      *dataLen = curLen;
   }
   if (curLen > 0)
   {
      return adt_bytearray_data(&m_dataReceived);
   }
   return (const uint8_t*) 0;
}

void testsocket_spy_clearReceivedData(void)
{
   adt_bytearray_clear(&m_dataReceived);
}

int32_t testsocket_spy_getClientConnectedCount(void)
{
   return m_clientConnectedCount;
}

int32_t testsocket_spy_getClientDisconnectCount(void)
{
   return m_clientDisconnectedCount;
}

int32_t testsocket_spy_getServerConnectedCount(void)
{
   return m_serverConnectedCount;
}

int32_t testsocket_spy_getServerDisconnectCount(void)
{
   return m_serverDisconnectedCount;
}

uint32_t testsocket_spy_getClientBytesReceived(void)
{
   return m_clientBytesReceivedTotal;
}

uint32_t testsocket_spy_getServerBytesReceived(void)
{
   return m_serverBytesReceivedTotal;
}


//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
static void client_socket_connected(void *arg,const char *addr, uint16_t port)
{
   m_clientConnectedCount++;
}

static void client_socket_disconnected(void *arg)
{
   m_clientDisconnectedCount++;
}

static int8_t client_socket_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
{
   *parseLen = dataLen;
   m_clientBytesReceivedTotal+=dataLen;
   adt_bytearray_append(&m_dataReceived, dataBuf, dataLen);
   return 0;
}

static void server_socket_connected(void *arg,const char *addr, uint16_t port)
{
   m_serverConnectedCount++;
}

static void server_socket_disconnected(void *arg)
{
   m_serverDisconnectedCount++;
}

static int8_t server_socket_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
{
   *parseLen = dataLen;
   m_serverBytesReceivedTotal+=dataLen;
   adt_bytearray_append(&m_dataReceived, dataBuf, dataLen);
   return 0;
}

/*****************************************************************************
* \file:    testsocket.h
* \author:  Conny Gustafsson
* \date:    2017-08-29
* \brief:   A drop-in replacement of msocket that can be used for unit testing purposes
*
* Copyright (c) 2017-2020 Conny Gustafsson
*
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



//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
typedef struct testsocket_tag
{
   msocket_bytearray_t pendingClient;
   msocket_bytearray_t pendingServer;
   msocket_handler_t serverHandlerTable;
   msocket_handler_t clientHandlerTable;
   void *serverHandlerArg;
   void *clientHandlerArg;
}testsocket_t;

//////////////////////////////////////////////////////////////////////////////
// FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
void testsocket_create(testsocket_t *self);
void testsocket_destroy(testsocket_t *self);
testsocket_t *testsocket_new(void);
void testsocket_delete(testsocket_t *self);
void testsocket_vdelete(void *arg);
int8_t testsocket_serverSend(testsocket_t *self,const void *msgData,uint32_t msgLen);
int8_t testsocket_clientSend(testsocket_t *self,const void *msgData,uint32_t msgLen);
void testsocket_setServerHandler(testsocket_t *self, const msocket_handler_t *handlerTable, void *handlerArg);
void testsocket_setClientHandler(testsocket_t *self, const msocket_handler_t *handlerTable, void *handlerArg);
void testsocket_onConnect(testsocket_t *self);
void testsocket_onDisconnect(testsocket_t *self);
void testsocket_run(testsocket_t *self);

#ifdef __cplusplus
}
#endif

#endif //TEST_SOCKET_H

/*****************************************************************************
* \file:    testsocket.c
* \author:  Conny Gustafsson
* \date:    2017-08-29
* \brief:   A drop-in replacement of msocket that can be used for unit testing purposes
*
* Copyright (c) 2017 Conny Gustafsson
*
******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "testsocket.h"
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#ifdef MEM_LEAK_CHECK
#include "CMemLeak.h"
#endif


//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// LOCAL VARIABLES
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
void testsocket_create(testsocket_t *self)
{
   if (self != 0)
   {
      adt_bytearray_create(&self->pendingClient, 0);
      adt_bytearray_create(&self->pendingServer, 0);
   }
}

void testsocket_destroy(testsocket_t *self)
{
   if (self != 0)
   {
      adt_bytearray_destroy(&self->pendingClient);
      adt_bytearray_destroy(&self->pendingServer);
   }
}

testsocket_t *testsocket_new(void)
{
   testsocket_t *self = (testsocket_t*) malloc(sizeof(testsocket_t));
   if(self != 0)
   {
      testsocket_create(self);
   }
   else
   {
      errno = ENOMEM;
   }
   return self;

}

void testsocket_delete(testsocket_t *self)
{
   if (self != 0)
   {
      testsocket_destroy(self);
      free(self);
   }
}

void testsocket_vdelete(void *arg)
{
   testsocket_delete((testsocket_t*) arg);
}

/**
 * Sends data from the server to the client
 */
int8_t testsocket_serverSend(testsocket_t *self,void *msgData,uint32_t msgLen)
{
   if (self != 0)
   {
      return adt_bytearray_append(&self->pendingClient, msgData, msgLen);
   }
   return -1;
}

/**
 * Sends data fromt the client to the server
 */
int8_t testsocket_clientSend(testsocket_t *self,void *msgData,uint32_t msgLen)
{
   if (self != 0)
   {
      return adt_bytearray_append(&self->pendingServer, msgData, msgLen);
   }
   return -1;
}

void testsocket_setServerHandler(testsocket_t *self, const msocket_handler_t *handlerTable, void *handlerArg)
{
   if(self != 0)
   {

      if(handlerTable == 0)
      {
         memset(&self->serverHandlerTable, 0, sizeof(msocket_handler_t));
         self->serverHandlerArg = (void*) 0;
      }
      else
      {
        memcpy(&self->serverHandlerTable, handlerTable, sizeof(msocket_handler_t));
        self->serverHandlerArg = handlerArg;
      }
   }
}

void testsocket_setClientHandler(testsocket_t *self, const msocket_handler_t *handlerTable, void *handlerArg)
{
   if(self != 0)
   {

      if(handlerTable == 0)
      {
         memset(&self->clientHandlerTable, 0, sizeof(msocket_handler_t));
         self->clientHandlerArg = (void*) 0;
      }
      else
      {
        memcpy(&self->clientHandlerTable, handlerTable, sizeof(msocket_handler_t));
        self->clientHandlerArg = handlerArg;
      }
   }
}

void testSocket_run(testsocket_t *self)
{
   if (self != 0)
   {
      uint32_t serverPending;
      uint32_t clientPending;
      serverPending = (uint32_t) adt_bytearray_length(&self->pendingServer);
      clientPending = (uint32_t) adt_bytearray_length(&self->pendingClient);
      if ( (serverPending > 0) && (self->serverHandlerTable.tcp_data != 0) )
      {
         uint32_t parseLen=0;
         const uint8_t *data;
         int8_t result;
         data = (const uint8_t*) adt_bytearray_data(&self->pendingServer);
         result = self->serverHandlerTable.tcp_data(self->serverHandlerArg, data, serverPending, &parseLen);
         if (result == 0)
         {
            adt_bytearray_trimLeft(&self->pendingServer, data + parseLen);
         }
      }
      if ( (clientPending > 0) && (self->clientHandlerTable.tcp_data != 0) )
      {
         uint32_t parseLen=0;
         const uint8_t *data;
         int8_t result;
         data = (const uint8_t*) adt_bytearray_data(&self->pendingClient);
         result = self->clientHandlerTable.tcp_data(self->clientHandlerArg, data, clientPending, &parseLen);
         if (result == 0)
         {
            adt_bytearray_trimLeft(&self->pendingClient, data + parseLen);
         }
      }
   }
}

//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////


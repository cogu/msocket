/*****************************************************************************
* \file      msocket_server.h
* \author    Conny Gustafsson
* \date      2014-12-18
* \brief     msocket (TCP/UDP) server helper
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2014-2016 Conny Gustafsson
*
******************************************************************************/
#ifndef MSOCKET_SERVER_H
#define MSOCKET_SERVER_H
#include "msocket.h"
#include "adt_ary.h"
#include "osmacro.h"
#ifndef _WIN32
#include <semaphore.h>
#include <pthread.h>
#endif



typedef struct msocket_server_tag{
   msocket_t *acceptSocket;
   uint16_t tcpPort;
   uint16_t udpPort;
   char *udpAddr;
   char *socketPath;
   adt_ary_t cleanupItems;
   THREAD_T acceptThread;
   THREAD_T cleanupThread;
   SEMAPHORE_T sem;
   MUTEX_T mutex;
   uint8_t cleanupStop;
   uint8_t addressFamily;
   void *handlerArg;
   msocket_handler_t handlerTable;
   void (*pDestructor)(void *arg);
#ifdef _WIN32
   unsigned int acceptThreadId;
   unsigned int cleanupThreadId;
#endif
}msocket_server_t;



/***************** Public Function Declarations *******************/
void msocket_server_create(msocket_server_t *self, uint8_t addressFamily, void (*pDestructor)(void*)); //for addressFamily: use AF_INET or AF_INET6
void msocket_server_destroy(msocket_server_t *self);
msocket_server_t *msocket_server_new(uint8_t addressFamily,void (*pDestructor)(void*));
void msocket_server_delete(msocket_server_t *self);
void msocket_server_sethandler(msocket_server_t *self,const msocket_handler_t *handler, void *handlerArg);
void msocket_server_start(msocket_server_t *self,const char *udpAddr,uint16_t udpPort,uint16_t tcpPort);
void msocket_server_unix_start(msocket_server_t *self,const char *socketPath);
void msocket_server_cleanup_connection(msocket_server_t *self, void *arg);


#endif //APX_NETSOCKET_SERVER_H

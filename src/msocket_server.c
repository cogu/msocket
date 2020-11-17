/*****************************************************************************
* \file      msocket_server.c
* \author    Conny Gustafsson
* \date      2014-12-18
* \brief     msocket (TCP/UDP) server helper
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2014-2016 Conny Gustafsson
*
******************************************************************************/
#include "msocket_server.h"
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "osmacro.h"
#include "osutil.h"

#ifdef _WIN32
#include <process.h>
#define strdup _strdup
#else
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#endif


#define CLEANUP_INTERVAL_MS 200


/**************** Private Function Declarations *******************/
static void msocket_server_start_threads(msocket_server_t *self);
static THREAD_PROTO(acceptTask,arg);
static THREAD_PROTO(cleanupTask,arg);
/**************** Private Variable Declarations *******************/

/****************** Public Function Definitions *******************/

void msocket_server_create(msocket_server_t *self, uint8_t addressFamily, void (*pDestructor)(void*)){
   if(self != 0){
      self->tcpPort = 0;
      self->udpPort = 0;
      self->udpAddr=0;
      self->socketPath=0;
      self->acceptSocket = 0;
      self->cleanupStop = 0;
      memset(&self->handlerTable,0,sizeof(self->handlerTable));
      self->handlerArg = 0;
      self->addressFamily = addressFamily;
      if (pDestructor != 0)
      {
         self->pDestructor = pDestructor;
      }
      else
      {
         self->pDestructor = msocket_vdelete;
      }
      msocket_ary_create(&self->cleanupItems,0);
      MUTEX_INIT(self->mutex);
      SEMAPHORE_CREATE(self->sem);
   }
}

void msocket_server_destroy(msocket_server_t *self){
#ifndef _WIN32
   void *result;
#endif

   if(self != 0){
      msocket_t *acceptSocket;
      MUTEX_LOCK(self->mutex);
      self->cleanupStop = 1;
      acceptSocket = self->acceptSocket;
      if(acceptSocket != 0){
         msocket_close(acceptSocket);
         MUTEX_UNLOCK(self->mutex);
#ifdef _WIN32
         WaitForSingleObject( self->acceptThread, INFINITE );
         CloseHandle( self->acceptThread );
#else
         pthread_join(self->acceptThread,&result);
#endif
      }
      else {
         MUTEX_UNLOCK(self->mutex);
      }
      if (self->pDestructor != 0) {
#ifdef _WIN32
         WaitForSingleObject( self->cleanupThread, INFINITE );
         CloseHandle( self->cleanupThread );
#else
         pthread_join(self->cleanupThread,&result);
#endif
      }
      msocket_ary_destroy(&self->cleanupItems);
      SEMAPHORE_DESTROY(self->sem);
      MUTEX_DESTROY(self->mutex);
      if(self->udpAddr != 0){
         free(self->udpAddr);
      }
#ifndef _WIN32
      if(self->socketPath != 0){
         if (self->socketPath[0] != '\0') { //files that belong to the abstract namespace (starts with null-byte) does not need to be explicitly deleted
            unlink(self->socketPath);
         }
         free(self->socketPath);
      }
#endif
   }
}

msocket_server_t *msocket_server_new(uint8_t addressFamily, void (*pDestructor)(void*)){
   msocket_server_t *self = (msocket_server_t*) malloc(sizeof(msocket_server_t));
   if(self != 0){
      msocket_server_create(self,addressFamily, pDestructor);
   }
   return self;
}

void msocket_server_delete(msocket_server_t *self){
   if(self != 0){
      msocket_server_destroy(self);
      free(self);
   }
}

void msocket_server_sethandler(msocket_server_t *self, const msocket_handler_t *handler, void *handlerArg){
   if(self != 0){
      memcpy(&self->handlerTable,handler,sizeof(msocket_handler_t));
      self->handlerArg = handlerArg;
   }
}

void msocket_server_start(msocket_server_t *self,const char *udpAddr,uint16_t udpPort,uint16_t tcpPort){
   if(self != 0){
      self->tcpPort = tcpPort;
      self->udpPort = udpPort;
      if(udpAddr != 0){
         self->udpAddr = strdup(udpAddr);
      }
      msocket_server_start_threads(self);
   }
}

void msocket_server_unix_start(msocket_server_t *self,const char *socketPath) {
#ifndef _WIN32
   if(self != 0){
      self->tcpPort = 0;
      self->udpPort = 0;
      if (socketPath != 0)
      {
         /* Linux supports an abstract namespace where the path starts with null-byte
          * Socket files created in this namespace does not show up in the normal file system and will also be deleted automatically when closed.
          */
         if (*socketPath=='\0')
         {
            //abstract namespace
            int len=strlen(socketPath+1)+2; //reserve space for 1 null-byte at beginning and another null-byte at the end
            self->socketPath=(char*) malloc(len);
            if (self->socketPath != 0)
            {
               memcpy(self->socketPath, socketPath, len);
            }
         }
         else
         {
            //filesystem namespace
            self->socketPath=strdup(socketPath);
            unlink(socketPath);
         }
      }
      msocket_server_start_threads(self);
   }
#else
   (void)self;
   (void)socketPath;
#endif
}

void msocket_server_disable_cleanup(msocket_server_t *self)
{
   if ( self != 0)
   {
      self->pDestructor = (void (*)(void*)) 0;
   }
}

void msocket_server_cleanup_connection(msocket_server_t *self, void *arg){
   MUTEX_LOCK(self->mutex);
   assert(self->cleanupStop == 0);
   msocket_ary_push(&self->cleanupItems,arg);
   MUTEX_UNLOCK(self->mutex);
   SEMAPHORE_POST(self->sem);
}



/***************** Private Function Definitions *******************/

static void msocket_server_start_threads(msocket_server_t *self) {
#ifdef _WIN32
   THREAD_CREATE(self->acceptThread,acceptTask,(void*) self,self->acceptThreadId);
#else
   THREAD_CREATE(self->acceptThread,acceptTask,(void*) self);
#endif
   //User can disable cleanup by explicitly calling msocket_server_disable_cleanup() before calling start. User then has to do cleanup manually.
   if (self->pDestructor != 0) {
#ifdef _WIN32
      THREAD_CREATE(self->cleanupThread, cleanupTask,(void*) self,self->cleanupThreadId);
#else
      THREAD_CREATE(self->cleanupThread, cleanupTask,(void*) self);
#endif
   }
}

THREAD_PROTO(acceptTask,arg){
   msocket_server_t *self = (msocket_server_t *) arg;
   if(self != 0){

      msocket_handler_t handler;
      memset(&handler,0,sizeof(handler));
      self->acceptSocket = msocket_new(self->addressFamily);

      if( (self->acceptSocket != 0) ){
         int rc;
         if(self->udpPort != 0){
            rc = msocket_listen(self->acceptSocket,MSOCKET_MODE_UDP,self->udpPort,self->udpAddr);
            if(rc<0){
               printf("*** WARNING: failed to bind to UDP port %d ***\n",self->udpPort);
               THREAD_RETURN(rc);
            }

         }
         if(self->tcpPort != 0){
            rc = msocket_listen(self->acceptSocket,MSOCKET_MODE_TCP,self->tcpPort,0);
            if(rc<0){
               printf("*** WARNING: failed to bind to TCP port %d ***\n",self->tcpPort);
               THREAD_RETURN(rc);
            }
         }
#ifndef _WIN32
         if (self->socketPath != 0)
         {
            rc = msocket_unix_listen(self->acceptSocket,self->socketPath);
            if(rc<0){
               printf("*** WARNING: failed to bind to path %s ***\n",self->socketPath);
               THREAD_RETURN(rc);
            }
         }
#endif
         while(1){
            msocket_t *child;
#ifdef MSOCKET_DEBUG
            printf("[MSOCKET] accept wait\n");
#endif
            child = msocket_accept(self->acceptSocket, 0);
#ifdef MSOCKET_DEBUG
            printf("[MSOCKET] accept return\n");
#endif
            if( child == 0 ){
               break;
            }
            else{
               if(self->handlerTable.tcp_accept != 0){
                  self->handlerTable.tcp_accept(self->handlerArg, self, child);
               }
            }
         }
         MUTEX_LOCK(self->mutex);
         msocket_delete(self->acceptSocket);
         self->acceptSocket = (msocket_t*) 0;
         MUTEX_UNLOCK(self->mutex);
      }
   }
#ifdef MSOCKET_DEBUG
   printf("acceptThread exit\n");
#endif
   THREAD_RETURN(0);
}

THREAD_PROTO(cleanupTask,arg)
{
   msocket_server_t *self = (msocket_server_t *) arg;
   if(self != 0)
   {
      if (self->pDestructor == 0)
      {
         THREAD_RETURN(0);
      }
      while(1)
      {
         const int8_t rc = _sem_test(&self->sem);
         if(rc < 0)
         {
            //failure
            printf("Error in cleanupTask, errno=%d\n",errno);
            break; //break while-loop
         }
         else if(rc > 0)
         {
            //successfully decreased semaphore, this means that there must be something in the array
            void *item;
            MUTEX_LOCK(self->mutex);
            assert(msocket_ary_length(&self->cleanupItems)>0);
            item = msocket_ary_shift(&self->cleanupItems);
            self->pDestructor(item);
            MUTEX_UNLOCK(self->mutex);
         }
         else
         {
            //failed to decrease semaphore, check if time to close
            if(self->cleanupStop != 0)
            {
               break; //break while-loop
            }
            SLEEP(CLEANUP_INTERVAL_MS);
         }
      }
   }
   THREAD_RETURN(0);
}

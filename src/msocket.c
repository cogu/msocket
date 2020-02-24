/*****************************************************************************
* \file:    msocket.c
* \author:  Conny Gustafsson
* \date:    2014-10-01
* \brief:   event-driven socket library for Linux and Windows
*
* Copyright (c) 2014-2018 Conny Gustafsson
*
******************************************************************************/


#ifdef _WIN32
#pragma comment(lib,"ws2_32.lib")
#include <process.h>

#else
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#endif
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#ifndef MSOCKET_DEBUG
#define MSOCKET_DEBUG 0
#endif
#include "msocket.h"

#if MSOCKET_DEBUG
#include <stdio.h>
#endif

#ifndef _WIN32
#define INVALID_SOCKET -1
#endif

/****************** Constants and Types ***************************/
#define MSG_BUF_SIZE 8192
#define TIMEOUT_MS 50 //ms for select-function to wait for activity
#define TIMEOUT_US (TIMEOUT_MS*1000)
#define TIMEOUT_CALL_INTERVAL_MS 1000 //interval for timeout callback handler

/**************** Private Function Declarations *******************/
static THREAD_PROTO(ioTask,arg);
static int8_t msocket_startIoThread(msocket_t *self);
static int msocket_udpRxHandler(msocket_t *self,uint8_t *recvBuf, int len);
static int msocket_tcpRxHandler(msocket_t *self,uint8_t *recvBuf, int len);
static void msocket_timeoutReset(msocket_t *self);
static uint8_t msocket_timeoutIncrease(msocket_t *self);
static void msocket_joinIoThread(msocket_t *self);
static void msocket_closeInternalSocket(msocket_t *self, uint8_t socketMode);
static void msocket_reset(msocket_t *self);
static void msocket_shutdownPrepare(msocket_t *self);


/**************** Private Variable Declarations *******************/


/****************** Public Function Definitions *******************/
int8_t msocket_create(msocket_t *self, uint8_t addressFamily){
   if( (self != 0) ){
      switch(addressFamily){
      case 0:
         self->addressFamily = AF_INET;
         break;
      case AF_INET:
         self->addressFamily = AF_INET;
         break;
      case AF_INET6:
         self->addressFamily = AF_INET6;
         break;
#ifndef _WIN32
      case AF_LOCAL:
         self->addressFamily = AF_UNIX;
         break;
#endif
      default:
         return -1;//invalid/unsupported address family
      }
      memset(&self->tcpInfo,0,sizeof(msocketAddrInfo_t));
      memset(&self->udpInfo,0,sizeof(msocketAddrInfo_t));
      self->handlerTable = 0;
      self->handlerArg = 0;
      self->threadRunning = 0; //ioThreadId is UNDEFINED, ioThread is UNDEFINED
      self->socketMode = MSOCKET_MODE_NONE; //tcpsockfd is UNDEFINED, udpsockfd is UNDEFINED
      self->state = MSOCKET_STATE_NONE;
      self->newConnection = 0; //used to differentiate between UDP and TCP on ioTask startup
      self->tcpsockfd = INVALID_SOCKET; //very unclear if socket is an integer on all linux/unix systems
      self->udpsockfd = INVALID_SOCKET;
#ifdef _WIN32
      self->ioThread = INVALID_HANDLE_VALUE;
#endif
      msocket_timeoutReset(self);
      adt_bytearray_create(&self->tcpRxBuf, (uint32_t) MSOCKET_RCV_BUF_GROW_SIZE);
      adt_bytearray_reserve(&self->tcpRxBuf, MSOCKET_MIN_RCV_BUF_SIZE);
      MUTEX_INIT(self->mutex);
      return 0;
   }
   return -1;
}

void msocket_destroy(msocket_t *self){
	if( self != 0 ){
      msocket_close(self);
      MUTEX_DESTROY(self->mutex);
      adt_bytearray_destroy(&self->tcpRxBuf);
      if(self->handlerTable != 0){
         free(self->handlerTable);
      }
   }
}

msocket_t *msocket_new(uint8_t addressFamily){
   msocket_t *self;
   self=(msocket_t *) malloc(sizeof(msocket_t));
   if(self !=  0){
      int8_t rc = msocket_create(self,addressFamily);
      if(rc != 0){ //constructor failure
         free(self);
         self = (msocket_t *) 0;
      }
   }
   return self;
}

void msocket_delete(msocket_t *self){
   if(self != 0){
      msocket_destroy(self);
      free(self);
   }
}

void msocket_vdelete(void *arg)
{
   msocket_delete( (msocket_t*) arg);
}

/**
 * This function closes the internal socket handle and stops the internal ioThread.
 * When called from the ioThread itself (during connect/disconnect callouts) it will
 * have no effect since a thread cannot be joined with itself.
 */
void msocket_close(msocket_t *self){
   if (self !=0 ){
#ifdef _WIN32
      unsigned int currentThreadId = (unsigned int) GetCurrentThreadId();
      if (currentThreadId == self->ioThreadId)
#else
      if(pthread_equal(pthread_self(),self->ioThread) != 0)
#endif
      {
#if MSOCKET_DEBUG
         printf("[MSOCKET] Not possible to call msocket_close from ioTask thread\n");
#endif
         return;
      }

      for(;;){
         uint8_t threadRunning;
         uint8_t socketMode;
         MUTEX_LOCK(self->mutex);
         msocket_shutdownPrepare(self);
         threadRunning = self->threadRunning;
         socketMode = self->socketMode;
         MUTEX_UNLOCK(self->mutex);
         if ( (threadRunning == 0) && (socketMode == MSOCKET_MODE_NONE) ){
            break;
         }
         if (threadRunning != 0){
            msocket_joinIoThread(self);
         }
         if (socketMode != MSOCKET_MODE_NONE){
            msocket_closeInternalSocket(self, socketMode);
         }
      }
      msocket_reset(self);
   }
}



int8_t msocket_listen(msocket_t *self,uint8_t mode, const uint16_t port, const char *addr){
   if(self != 0){
        int rc;
        struct sockaddr_in saddr;
        struct sockaddr_in6 saddr6;

        if( (mode != MSOCKET_MODE_UDP) && (mode != MSOCKET_MODE_TCP) ){
           errno = EINVAL;
           return -1;
        }

        //initialize the address struct based on selected address family
        if(self->addressFamily == AF_INET6){
           memset(&saddr6, 0,sizeof(saddr6));
           saddr6.sin6_family = AF_INET6;
           saddr6.sin6_addr = in6addr_any; //addr parameter not supported (TCP)
           saddr6.sin6_port = htons(port);
        }
        else{
           memset(&saddr, 0,sizeof(saddr));
           saddr.sin_family = AF_INET;
           if(addr == 0){
                 saddr.sin_addr.s_addr = INADDR_ANY;
           }
           else{
              inet_pton(AF_INET, addr, &saddr);
           }
           saddr.sin_port = htons(port);
        }

        if(mode == MSOCKET_MODE_UDP){
           /*** this is for creating a UDP socket ****/
           struct ipv6_mreq mreq;
           SOCKET_T sockudp;
           int one = 1;
           self->udpInfo.port = port;
           if(self->addressFamily == AF_INET6){
              sockudp = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
           }
           else{
              sockudp = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
           }
           if(IS_INVALID_SOCKET(sockudp)){
              return -1;
           }
           setsockopt(sockudp, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
           if(self->addressFamily == AF_INET6){
              inet_pton(self->addressFamily, addr, &(mreq.ipv6mr_multiaddr));
              mreq.ipv6mr_interface = 0;
              setsockopt(sockudp, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char*) &mreq, sizeof(mreq)); //join multicast group
              rc=bind(sockudp, (struct sockaddr *) &saddr6,sizeof(saddr6));
           }
           else{
              //on IPV4 DOIP uses broadcast address 255.255.255.255
              setsockopt(sockudp,SOL_SOCKET,SO_BROADCAST,(const char*)&one, sizeof(one));
              rc=bind(sockudp, (struct sockaddr *) &saddr,sizeof(saddr));
           }

           if (rc < 0){
              SOCKET_CLOSE(sockudp);
              return rc;
           }
           self->udpsockfd = sockudp;
           self->socketMode |= mode;
           msocket_startIoThread(self);
        }
        else {
           /*** this is for creating a TCP socket ****/
           SOCKET_T socktcp;
           int sockoptval;
           socklen_t sockoptlen = sizeof(sockoptval);
           self->tcpInfo.port = port;
           if(self->addressFamily == AF_INET6){
              socktcp = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
           }
           else{
              socktcp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
           }
           if(IS_INVALID_SOCKET(socktcp)){
              return -1;
           }
           sockoptval = 1;
           setsockopt(socktcp, SOL_SOCKET, SO_REUSEADDR, (const char*)&sockoptval, sockoptlen);
           setsockopt(socktcp, IPPROTO_TCP, TCP_NODELAY, (const char*)&sockoptval, sockoptlen);
           if(self->addressFamily == AF_INET6){
              rc=bind(socktcp, (struct sockaddr *) &saddr6,sizeof(saddr6));
           }
           else{
              rc=bind(socktcp, (struct sockaddr *) &saddr,sizeof(saddr));
           }
           if (rc < 0){
              SOCKET_CLOSE(socktcp);
              return rc;
           }
           rc = listen(socktcp,5);
           if(rc<0){
              SOCKET_CLOSE(socktcp);
              return rc;
           }
           self->tcpsockfd = socktcp;
           self->state = MSOCKET_STATE_LISTENING;
           self->socketMode |= mode;
        }
        return 0;
     }
     errno = EINVAL;
     return -1;
}

#ifndef _WIN32
int8_t msocket_unix_listen(msocket_t *self, const char *socket_path)
{
   if(self != 0){

      struct sockaddr_un saddr;
      uint8_t mode = MSOCKET_MODE_TCP; //internally we can act the same way as if this is a TCP socket

      memset(&saddr, 0,sizeof(saddr));
      saddr.sun_family = AF_UNIX;
      if (*socket_path == '\0') {
         *saddr.sun_path = '\0';
         strncpy(saddr.sun_path+1, socket_path+1, sizeof(saddr.sun_path)-2);
      } else {
         strncpy(saddr.sun_path, socket_path, sizeof(saddr.sun_path)-1);
      }
      if (mode == MSOCKET_MODE_TCP){
         int rc;
         SOCKET_T sockunix;
         int sockoptval;
         socklen_t sockoptlen = sizeof(sockoptval);
         sockunix = socket(PF_LOCAL, SOCK_STREAM, 0);
         if(IS_INVALID_SOCKET(sockunix)){
            return -1;
         }
         sockoptval = 1;
         setsockopt(sockunix, SOL_SOCKET, SO_REUSEADDR, (const char*)&sockoptval, sockoptlen);
         rc=bind(sockunix, (struct sockaddr *) &saddr,sizeof(saddr));
         if (rc < 0){
            SOCKET_CLOSE(sockunix);
            return rc;
         }
         rc = listen(sockunix,5);
         if(rc<0){
            SOCKET_CLOSE(sockunix);
            return rc;
         }
         self->tcpsockfd = sockunix;
         self->state = MSOCKET_STATE_LISTENING;
         self->socketMode |= mode;
      }
      return 0;
   }
   errno = EINVAL;
   return -1;
}
#endif

msocket_t *msocket_accept(msocket_t *self,msocket_t *child){
   if((self != 0) && (self->state == MSOCKET_STATE_LISTENING) ){
      struct sockaddr_in cli_addr;
      struct sockaddr_in6 cli_addr6;
      int sockoptval;
      socklen_t sockoptlen = sizeof(sockoptval);
      SOCKET_T sockfd;
      uint8_t placementNew = 0;
#ifdef _WIN32
      int cli_len;
#else
      socklen_t cli_len;
#endif
      if(child == 0){
         child = msocket_new(self->addressFamily);
      }
      else{
         placementNew = 1;
         msocket_create(child,self->addressFamily);
      }

      if(self->addressFamily == AF_INET6){
         cli_len = sizeof(cli_addr6);
         memset(&cli_addr6,0,cli_len);
      }
      else{
         cli_len = sizeof(cli_addr);
         memset(&cli_addr,0,cli_len);
      }

      MUTEX_LOCK(self->mutex);
      self->state = MSOCKET_STATE_ACCEPTING;
      MUTEX_UNLOCK(self->mutex);
      if(self->addressFamily == AF_INET6){
         sockfd=accept(self->tcpsockfd,(struct sockaddr *) &cli_addr6, &cli_len); //blocking call (close tcpsockfd from another thread to unblock)
      }
      else if (self->addressFamily == AF_INET){
         sockfd=accept(self->tcpsockfd,(struct sockaddr *) &cli_addr, &cli_len); //blocking call (close tcpsockfd from another thread to unblock)
      }
#ifndef _WIN32
      else if (self->addressFamily == AF_LOCAL){
         sockfd=accept(self->tcpsockfd, NULL, NULL);
      }
#endif
      else
      {
         assert(0);
      }
      MUTEX_LOCK(self->mutex);
      self->state = MSOCKET_STATE_LISTENING;
      MUTEX_UNLOCK(self->mutex);
      if(IS_INVALID_SOCKET(sockfd)){
         if(placementNew == 0){
            msocket_delete(child);
         }
         else{
            msocket_destroy(child);
         }
         return (msocket_t *) 0;
      }
      MUTEX_LOCK(child->mutex);
      if(self->addressFamily == AF_INET6){
         child->tcpInfo.port=ntohs(cli_addr6.sin6_port);
         inet_ntop(AF_INET, &(cli_addr6.sin6_addr), child->tcpInfo.addr, MSOCKET_ADDRSTRLEN);
      }
      else{
         child->tcpInfo.port=ntohs(cli_addr.sin_port);
         inet_ntop(AF_INET, &(cli_addr.sin_addr), child->tcpInfo.addr, MSOCKET_ADDRSTRLEN);
      }
      /*set socket options */
      sockoptval = 1;
      setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&sockoptval, sockoptlen);
      child->state = MSOCKET_STATE_ESTABLISHED;
      child->tcpsockfd = sockfd;
      child->socketMode = MSOCKET_MODE_TCP; //child socket is always TCP
      child->newConnection = 1;
      MUTEX_UNLOCK(child->mutex);
      return child;
   }
   return (msocket_t *) 0;
}

void msocket_sethandler(msocket_t *self, const msocket_handler_t *handlerTable, void *handlerArg){
   if(self != 0){
      if(self->handlerTable != 0){
         free(self->handlerTable);
      }
      self->handlerTable = (msocket_handler_t*) malloc(sizeof(msocket_handler_t));
      if(self->handlerTable != 0){
        memcpy(self->handlerTable,handlerTable,sizeof(msocket_handler_t));
        self->handlerArg = handlerArg;
      }
   }
}

int8_t msocket_start_io(msocket_t *self){
   if(self != 0){
      return (int8_t) msocket_startIoThread(self);
   }
   errno=EINVAL;
   return -1;
}



int8_t msocket_connect(msocket_t *self,const char *addr,uint16_t port){
   if( (self != 0) && ( (self->socketMode & MSOCKET_MODE_TCP) == 0) && (self->handlerTable != 0) ){
      int rc;
      int sockoptval;
      socklen_t sockoptlen = sizeof(sockoptval);

      struct sockaddr_in saddr;
      struct sockaddr_in6 saddr6;
      if(self->addressFamily == AF_INET6) {
         memset(&saddr6, 0, sizeof(saddr6));
         rc = inet_pton(AF_INET6, addr, &(saddr6.sin6_addr));
      }
      else{
         memset(&saddr, 0, sizeof(saddr));
         rc = inet_pton(AF_INET, addr, &(saddr.sin_addr));
      }

      if(rc != 0){
         SOCKET_T sockfd;
         self->tcpInfo.port = port;
#ifdef _WIN32
         strcpy_s(self->tcpInfo.addr, MSOCKET_ADDRSTRLEN, addr);
#else
         strcpy(self->tcpInfo.addr, addr);
#endif
         if(self->addressFamily == AF_INET6){            
            saddr6.sin6_family = AF_INET6;
            saddr6.sin6_port = htons((uint16_t) self->tcpInfo.port);
            sockfd = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
         }
         else{            
            saddr.sin_family = AF_INET;
            saddr.sin_port = htons((uint16_t) self->tcpInfo.port);
            sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
         }
         if(IS_INVALID_SOCKET(sockfd)){
            return -1;
         }
         if(self->addressFamily == AF_INET6){
            rc=connect(sockfd,(struct sockaddr*) &saddr6,sizeof(saddr6));
         }
         else{
            rc=connect(sockfd,(struct sockaddr*) &saddr,sizeof(saddr));
         }
         if (rc < 0){
            SOCKET_CLOSE(sockfd);
            return rc;
         }
         /*set socket options */
         sockoptval = 1;
         setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&sockoptval, sockoptlen);
         MUTEX_LOCK(self->mutex);
         self->tcpsockfd = sockfd;
         self->socketMode |= MSOCKET_MODE_TCP;
         self->state = MSOCKET_STATE_ESTABLISHED;
         self->newConnection = 1;
         MUTEX_UNLOCK(self->mutex);
         rc = msocket_startIoThread(self);
         if(rc != 0){
            return rc;
         }
         return 0;
      }
      //inet_pton failed to parse addr argument, set errno to EINVAL and return error
   }
   errno = EINVAL;
   return -1;
}

#ifndef _WIN32
int8_t msocket_unix_connect(msocket_t *self,const char *socketPath)
{
   if( (self != 0) && ( (self->socketMode & MSOCKET_MODE_TCP) == 0) && (self->handlerTable != 0) ){
      int rc;
      SOCKET_T sockfd;
      struct sockaddr_un saddr;

      memset(&saddr, 0,sizeof(saddr));
      saddr.sun_family = AF_LOCAL;
      if (*socketPath == '\0') {
        *saddr.sun_path = '\0';
        strncpy(saddr.sun_path+1, socketPath+1, sizeof(saddr.sun_path)-2);
      } else {
        strncpy(saddr.sun_path, socketPath, sizeof(saddr.sun_path)-1);
      }

      sockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
      if(IS_INVALID_SOCKET(sockfd)){
         return -1;
      }
      rc=connect(sockfd,(struct sockaddr*) &saddr,sizeof(saddr));
      if (rc < 0){
         SOCKET_CLOSE(sockfd);
         return rc;
      }
      MUTEX_LOCK(self->mutex);
      self->tcpsockfd = sockfd;
      self->socketMode |= MSOCKET_MODE_TCP;
      self->state = MSOCKET_STATE_ESTABLISHED;
      self->newConnection = 1;
      MUTEX_UNLOCK(self->mutex);
      rc = msocket_startIoThread(self);
      if(rc != 0){
         return rc;
      }
      return 0;

      //inet_pton failed to parse addr argument, set errno to EINVAL and return error
   }
   errno = EINVAL;
   return -1;
}
#endif

/**
 * send UDP message
 */
int8_t msocket_sendto(msocket_t *self,const char *addr,uint16_t port,const void *msgData,uint32_t msgLen){
   if( (self != 0) && ( (self->socketMode & MSOCKET_MODE_UDP) != 0)){
      struct sockaddr_in saddr;
      struct sockaddr_in6 saddr6;
      int rc;
      if(self->addressFamily == AF_INET6){
         memset(&saddr6, 0,sizeof(saddr6));
         saddr6.sin6_family = AF_INET6;
         saddr6.sin6_port = htons((uint16_t)port);
         inet_pton(AF_INET6, addr, &(saddr6.sin6_addr));
         rc = sendto(self->udpsockfd,msgData,msgLen,0,(struct sockaddr *)&saddr6,sizeof(saddr6));
      }
      else{
         memset(&saddr, 0,sizeof(saddr));
         saddr.sin_family = AF_INET;
         saddr.sin_port = htons((uint16_t)port);
         inet_pton(AF_INET, addr, &(saddr.sin_addr));
         rc = sendto(self->udpsockfd,msgData,msgLen,0,(struct sockaddr *)&saddr,sizeof(saddr));
      }
      if(rc < 0){
         return -1;
      }
      MUTEX_LOCK(self->mutex);
      msocket_timeoutReset(self);
      MUTEX_UNLOCK(self->mutex);
      return 0;
   }
   errno = EINVAL;
   return -1;
}

/**
 * Returns 0 on success, -1 on failure
 */
int8_t msocket_send(msocket_t *self,const void *msgData,uint32_t msgLen){
   if( (self != 0) && ( (self->socketMode & MSOCKET_MODE_TCP) != 0) ){
      assert(msgData != 0);
      const uint8_t *p = (const uint8_t*) msgData;
      uint32_t remain = msgLen;
      while(remain>0){
         int n = send(self->tcpsockfd,p,remain,0);
         if(n <= 0){
#if(MSOCKET_DEBUG)
            perror("msocket: send failed\n");
#endif
            return -1;
         }
         remain -= n;
         p += n;
      }
      MUTEX_LOCK(self->mutex);
      msocket_timeoutReset(self);
      MUTEX_UNLOCK(self->mutex);
      return 0;
   }
   errno = EINVAL;
   return -1;
}

int8_t msocket_state(msocket_t *self){
   if(self != 0){
      uint8_t state;
      MUTEX_LOCK(self->mutex);
      state = self->state;
      MUTEX_UNLOCK(self->mutex);
      return state;
   }
   return 0;
}

/***************** Private Function Definitions *******************/


THREAD_PROTO(ioTask,arg){
   if(arg!=0){
      msocket_t *self = (msocket_t*)arg;
      int rc;
      fd_set readfds;
      uint8_t *recvBuf;
      struct timeval timeout;
# if(MSOCKET_DEBUG)
   printf("[MSOCKET](0x%p)  ioTask starting\n",arg);
#endif


      timeout.tv_sec=0;
      recvBuf = (uint8_t*) malloc(MSG_BUF_SIZE);
      if(recvBuf == 0){
         THREAD_RETURN(1);
      }
      //wait for parent thread to release lock before executing this thread
      MUTEX_LOCK(self->mutex);
      MUTEX_UNLOCK(self->mutex);

      while(1){
#ifdef _WIN32
         unsigned int max_sd;
#else
         int max_sd;
#endif
         int activity;
         uint8_t newConnection;
         MUTEX_LOCK(self->mutex);
         newConnection = self->newConnection;
         MUTEX_UNLOCK(self->mutex);
         if( newConnection != 0){
            MUTEX_LOCK(self->mutex);
            self->newConnection = 0;
            MUTEX_UNLOCK(self->mutex);
            if(self->handlerTable->tcp_connected != 0 ){
               self->handlerTable->tcp_connected(self->handlerArg,&self->tcpInfo.addr[0],self->tcpInfo.port);
            }
         }
         max_sd=0;
         FD_ZERO(&readfds);
         if(self->socketMode & MSOCKET_MODE_UDP){
            FD_SET(self->udpsockfd, &readfds);
            if(self->udpsockfd > max_sd){
               max_sd=self->udpsockfd;
            }
         }
         else if(self->socketMode & MSOCKET_MODE_TCP){ //the thread listening on UDP uses a different thread for TCP accept, use else-if here to prevent deadlock
            FD_SET(self->tcpsockfd, &readfds);
            if(self->tcpsockfd > max_sd){
               max_sd=self->tcpsockfd;
            }
         }
         timeout.tv_usec=TIMEOUT_US;
         activity = select( max_sd + 1 , &readfds , NULL , NULL , &timeout);
         if(activity>0){
            if( (self->socketMode & MSOCKET_MODE_UDP) && (FD_ISSET(self->udpsockfd,&readfds) != 0) ){
               SOCK_LEN_T len;
               //UDP activity
               if(self->addressFamily == AF_INET6){
                   struct sockaddr_in6 sock_addr6;
                   len = (SOCK_LEN_T) sizeof(sock_addr6);
                   memset(&sock_addr6, 0,sizeof(sock_addr6));
                   rc = recvfrom(self->udpsockfd, (void*) recvBuf, MSG_BUF_SIZE, 0,(struct sockaddr *)&sock_addr6,&len);
                   if(rc >= 0){
                       self->udpInfo.port=ntohs(sock_addr6.sin6_port);
                       inet_ntop(AF_INET6, &(sock_addr6.sin6_addr), &self->udpInfo.addr[0], INET6_ADDRSTRLEN);
                   }
               }
               else{
                   struct sockaddr_in sock_addr4;
                   len = (SOCK_LEN_T) sizeof(sock_addr4);
                   memset(&sock_addr4, 0,sizeof(sock_addr4));
                   rc = recvfrom(self->udpsockfd, (void*) recvBuf, MSG_BUF_SIZE, 0,(struct sockaddr *)&sock_addr4,&len);
                   if(rc >= 0){
                       self->udpInfo.port=ntohs(sock_addr4.sin_port);
                       inet_ntop(AF_INET, &(sock_addr4.sin_addr), &self->udpInfo.addr[0], INET6_ADDRSTRLEN);
                   }
               }
               if(rc>=0){
                   rc = msocket_udpRxHandler(self,recvBuf,rc);
                   if(rc < 0){
                      break;
                   }
               }
            }
            if( (self->socketMode & MSOCKET_MODE_TCP) && (FD_ISSET(self->tcpsockfd,&readfds) != 0) ){
               rc=recv(self->tcpsockfd,(uint8_t*) &recvBuf[0],MSG_BUF_SIZE,0);
               rc = msocket_tcpRxHandler(self,recvBuf,rc);
               if(rc < 0){
                  break;
               }
            }
         }
         else{
            uint8_t state;
            MUTEX_LOCK(self->mutex);
            state = self->state;
            MUTEX_UNLOCK(self->mutex);
            if(state == MSOCKET_STATE_CLOSING){
               break;
            }
            else if(state == MSOCKET_STATE_ESTABLISHED){
               uint8_t inactivity_timeout = 0;
               MUTEX_LOCK(self->mutex);
               inactivity_timeout = msocket_timeoutIncrease(self);
               MUTEX_UNLOCK(self->mutex);
               if( (inactivity_timeout != 0) && (self->handlerTable->tcp_inactivity != 0)){
                  self->handlerTable->tcp_inactivity(self->inactivityMs);
               }
            }
         }
      }
      free(recvBuf);
   }
# if(MSOCKET_DEBUG)
   printf("[MSOCKET](0x%p) ioTask exiting\n",arg);
#endif
   THREAD_RETURN(0);
}

static int8_t msocket_startIoThread(msocket_t *self){
   if( (self != 0) && (self->handlerTable != 0) && (self->threadRunning == 0) ){
#ifdef _WIN32
      THREAD_CREATE(self->ioThread,ioTask,self,self->ioThreadId);
      if(self->ioThread == INVALID_HANDLE_VALUE){
         return -1;
      }
#else
      int rc = THREAD_CREATE(self->ioThread,ioTask,self);
      if(rc != 0){
         return -1;
      }
#endif
      self->threadRunning = 1;
      return 0;
   }
   errno = EINVAL;
   return -1;
}

static int msocket_udpRxHandler(msocket_t *self,uint8_t *recvBuf, int len){
   if((self != 0) && (len > 0) && (self->handlerTable->udp_msg != 0)){
      self->handlerTable->udp_msg(self->handlerArg,&self->udpInfo.addr[0],self->udpInfo.port,recvBuf,(uint32_t) len);
      return 0;
   }
   return -1;
}

static int msocket_tcpRxHandler(msocket_t *self,uint8_t *recvBuf, int len){
   if( len < 0 ){
#ifdef _WIN32      
      int lastError = WSAGetLastError();
      if (lastError == WSAECONNRESET)
      {
         len = 0; //change to socket closed event
      }
      else
      {
#if(MSOCKET_DEBUG)
         fprintf(stderr, "[MSOCKET] recv error %d\n", lastError);
#endif
         return -1;
      }
#else
      if(errno == ECONNRESET){
         len = 0; //change to socket closed event
      }
      else{
#if(MSOCKET_DEBUG)
         perror("msocket: recv error");
#endif
         return -1;
      }
#endif
   }
   if( self != 0 ){
      if( len == 0 ){
         int8_t triggerCallback = 1u;
         MUTEX_LOCK(self->mutex);
         if (self->state == MSOCKET_STATE_CLOSING) {
            triggerCallback = 0;
         }
         self->state = MSOCKET_STATE_CLOSING;
         MUTEX_UNLOCK(self->mutex);
         if( (triggerCallback != 0) && (self->handlerTable->tcp_disconnected != 0) ){
            self->handlerTable->tcp_disconnected(self->handlerArg);
         }
         return -1;
      }
      else if(self->handlerTable->tcp_data != 0){
         if(adt_bytearray_append(&self->tcpRxBuf,recvBuf,(uint32_t) len) != 0){
            return -1;
         }
         while(1){
            //message parse loop
            int8_t rc;
            uint32_t parseLen = 0;
            uint32_t u32Len;
            const uint8_t *pBegin = (const uint8_t*) self->tcpRxBuf.pData;
            u32Len = self->tcpRxBuf.u32CurLen;
            if(u32Len == 0){
               break; //no more data
            }
            rc = self->handlerTable->tcp_data(self->handlerArg, pBegin, u32Len, &parseLen);
            if( rc != 0 ){
               MUTEX_LOCK(self->mutex);
               self->state = MSOCKET_STATE_CLOSING;
               MUTEX_UNLOCK(self->mutex);
               break;
            }
            if(parseLen == 0){
               break;
            }
            else{
               assert(parseLen<=u32Len);
               adt_bytearray_trimLeft(&self->tcpRxBuf,pBegin+parseLen);
            }
         }
      }
   }
   return 0;
}

void msocket_timeoutReset(msocket_t *self){
   if(self != 0){
      self->inactivityMs=0;
      self->inactivityCallMs=TIMEOUT_CALL_INTERVAL_MS;
   }
}

uint8_t msocket_timeoutIncrease(msocket_t *self){
   if(self != 0){
      self->inactivityMs+=TIMEOUT_MS;
      if(self->inactivityMs==self->inactivityCallMs){
         self->inactivityCallMs+=TIMEOUT_CALL_INTERVAL_MS;
         return 1;
      }
   }
   return 0;
}

static void msocket_joinIoThread(msocket_t *self){
#ifdef _WIN32
   DWORD result = WaitForSingleObject(self->ioThread, 1000);
   if (result == WAIT_OBJECT_0){
      CloseHandle(self->ioThread);
      self->ioThread=INVALID_HANDLE_VALUE;
      MUTEX_LOCK(self->mutex);
      self->threadRunning = 0;
      MUTEX_UNLOCK(self->mutex);
   }
#else
   void *status;

   int s = pthread_join(self->ioThread, &status);
   if (s == 0){
      MUTEX_LOCK(self->mutex);
      self->threadRunning = 0;
      MUTEX_UNLOCK(self->mutex);
   }
# if(MSOCKET_DEBUG)
   else{
      printf("[MSOCKET] pthread_join error %d\n", s);
   }
# endif
#endif
}

static void msocket_closeInternalSocket(msocket_t *self, uint8_t socketMode){
   if (socketMode & MSOCKET_MODE_TCP){
      SOCKET_CLOSE(self->tcpsockfd);
      MUTEX_LOCK(self->mutex);
      self->socketMode &=(uint8_t) (~MSOCKET_MODE_TCP);
      self->tcpsockfd = INVALID_SOCKET;
      MUTEX_UNLOCK(self->mutex);
   }
   if (socketMode & MSOCKET_MODE_UDP){
      SOCKET_CLOSE(self->udpsockfd);
      MUTEX_LOCK(self->mutex);
      self->socketMode &=(uint8_t) (~MSOCKET_MODE_UDP);
      self->udpsockfd = INVALID_SOCKET;
      MUTEX_UNLOCK(self->mutex);
   }
}

static void msocket_reset(msocket_t *self){
   self->state = MSOCKET_STATE_NONE;
   self->newConnection = 0;
   msocket_timeoutReset(self);
   adt_bytearray_clear(&self->tcpRxBuf);
}

static void msocket_shutdownPrepare(msocket_t *self){
    if( (self->state == MSOCKET_STATE_PENDING) || (self->state == MSOCKET_STATE_ESTABLISHED) || (self->state == MSOCKET_STATE_ACCEPTING) ){
       self->state = MSOCKET_STATE_CLOSING;
       if (self->socketMode & MSOCKET_MODE_TCP){
          SOCKET_SHUTDOWN(self->tcpsockfd);
       }
   }
}

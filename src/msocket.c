/*****************************************************************************
* \file:    msocket.c
* \author:  Conny Gustafsson
* \date:    2014-10-01
* \brief:   event-driven socket library for Linux and Windows
*
* Copyright (c) 2014-2020 Conny Gustafsson
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
#define MAX_CLOSE_ATTEMPTS 20

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
static int msocket_accept_inet(msocket_t* self, msocket_t* child);
static int msocket_accept_inet6(msocket_t* self, msocket_t* child);
#ifndef _WIN32
static int msocket_accept_local(msocket_t* self, msocket_t* child);
#endif
static int msocket_connect_inet(msocket_t* self, const char* address, uint16_t port);
static int msocket_connect_inet6(msocket_t* self, const char* address, uint16_t port);
#ifndef _WIN32
static int msocket_connect_unix_internal(msocket_t* self, const char* socketPath);
#endif

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
      msocket_bytearray_create(&self->tcpRxBuf, (uint32_t) MSOCKET_RCV_BUF_GROW_SIZE);
      msocket_bytearray_reserve(&self->tcpRxBuf, MSOCKET_MIN_RCV_BUF_SIZE);
      MUTEX_INIT(self->mutex);
      return 0;
   }
   return -1;
}

void msocket_destroy(msocket_t *self){
	if( self != 0 ){
      msocket_close(self);
      msocket_bytearray_destroy(&self->tcpRxBuf);
      MUTEX_DESTROY(self->mutex);
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
      uint8_t attempts;
      for(attempts=0; attempts < MAX_CLOSE_ATTEMPTS; attempts++){
         uint8_t threadRunning;
         uint8_t socketMode;
         MUTEX_LOCK(self->mutex);
         msocket_shutdownPrepare(self);
         threadRunning = self->threadRunning;
         socketMode = self->socketMode;
         MUTEX_UNLOCK(self->mutex);
         if ( (threadRunning == 0) && (socketMode == MSOCKET_MODE_NONE) ){
            break; //already closed
         }
         if (threadRunning != 0) {
#ifdef _WIN32
            unsigned int currentThreadId = (unsigned int) GetCurrentThreadId();
            if (currentThreadId == self->ioThreadId)
#else
            if(pthread_equal(pthread_self(),self->ioThread) != 0)
#endif
            {
#if MSOCKET_DEBUG
                  printf("[MSOCKET] Not allowed to call msocket_close from msocket ioTask thread\n");
#endif
               return;
            }
            msocket_joinIoThread(self);
         }
         if (socketMode != MSOCKET_MODE_NONE){
            MUTEX_LOCK(self->mutex);
            msocket_closeInternalSocket(self, socketMode);
            msocket_reset(self);
            MUTEX_UNLOCK(self->mutex);
         }
         else
         {
            MUTEX_LOCK(self->mutex);
            msocket_reset(self);
            MUTEX_UNLOCK(self->mutex);
         }
      }
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
           saddr6.sin6_addr = in6addr_any; //addr parameter not yet supported
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
              return (int8_t) rc;
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
              return (int8_t) rc;
           }
           rc = listen(socktcp,5);
           if(rc<0){
              SOCKET_CLOSE(socktcp);
              return (int8_t) rc;
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

msocket_t *msocket_accept(msocket_t *self, msocket_t *child){
   if ( (self != 0) && (self->state == MSOCKET_STATE_LISTENING) ) {

      uint8_t placementNew = 0u;
      int result = 0u;

      if(child == 0) {
         child = msocket_new(self->addressFamily);
      }
      else {
         placementNew = 1;
         msocket_create(child,self->addressFamily);
      }
      MUTEX_LOCK(self->mutex);
      self->state = MSOCKET_STATE_ACCEPTING;
      MUTEX_UNLOCK(self->mutex);

      switch (self->addressFamily) {
      case AF_INET:
         result = msocket_accept_inet(self, child);
         break;
      case AF_INET6:
         result = msocket_accept_inet6(self, child);
         break;
#ifndef _WIN32
      case AF_LOCAL:
         result = msocket_accept_local(self, child);
         break;
      default:
         result = -1;
#endif
      }

      MUTEX_LOCK(self->mutex);
      self->state = MSOCKET_STATE_LISTENING;
      MUTEX_UNLOCK(self->mutex);

      if (result < 0) {
         if (placementNew == 0) {
            msocket_delete(child);
         }
         else {
            msocket_destroy(child);
         }
         return (msocket_t*)0;
      }

      if ( (child->addressFamily == AF_INET) || (child->addressFamily == AF_INET6) ) {
         int sockoptval = 1;
         socklen_t sockoptlen = sizeof(sockoptval);
         setsockopt(child->tcpsockfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&sockoptval, sockoptlen);
      }

      MUTEX_LOCK(child->mutex);
      child->state = MSOCKET_STATE_ESTABLISHED;
      child->socketMode = MSOCKET_MODE_TCP;
      child->newConnection = 1;
      MUTEX_UNLOCK(child->mutex);
      return child;
   }
   return (msocket_t *) 0;
}

void msocket_set_handler(msocket_t *self, const msocket_handler_t *handlerTable, void *handlerArg){
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
   if(self != 0) {
      if (self->handlerTable == 0) {
         //Cannot start I/O thread without first setting up handler table
         errno = EFAULT;
         return -1;
      }
      return (int8_t) msocket_startIoThread(self);
   }
   errno=EINVAL;
   return -1;
}



int8_t msocket_connect(msocket_t *self, const char *addr, uint16_t port){
   if( (self != 0) && (addr != 0) && ( (self->socketMode & MSOCKET_MODE_TCP) == 0) ) {
      int sockoptval = 1;
      socklen_t sockoptlen = sizeof(sockoptval);
      int result;
      if (self->handlerTable == 0) {
         errno = EFAULT;
         return -1;
      }
      result = (self->addressFamily == AF_INET6) ? msocket_connect_inet6(self, addr, port) : msocket_connect_inet(self, addr, port);
      if (result < 0) {
         return -1;
      }
      setsockopt(self->tcpsockfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&sockoptval, sockoptlen);
      self->socketMode |= MSOCKET_MODE_TCP;
      self->state = MSOCKET_STATE_ESTABLISHED;
      self->newConnection = 1;
      result = msocket_startIoThread(self);
      if (result < 0) {
         SOCKET_CLOSE(self->tcpsockfd);
         self->state = MSOCKET_STATE_CLOSED;
         return -1;
      }
      return 0;
   }
   errno = EINVAL;
   return -1;
}


int8_t msocket_unix_connect(msocket_t *self, const char *socketPath)
{
   if( (self != 0) && ( (self->socketMode & MSOCKET_MODE_TCP) == 0) ){
      int result;
      if (self->handlerTable == 0) {
         errno = EFAULT;
         return -1;
      }
#ifndef _WIN32
      result = msocket_connect_unix_internal(self, socketPath);
      if (result < 0) {
         //errno alredy set by system call
         return -1;
      }

      self->socketMode |= MSOCKET_MODE_TCP; //Treat connection the same way as if it was set to TCP
      self->state = MSOCKET_STATE_ESTABLISHED;
      self->newConnection = 1;
      result = msocket_startIoThread(self);
      if (result < 0) {
         SOCKET_CLOSE(self->tcpsockfd);
         self->state = MSOCKET_STATE_CLOSED;
         return -1;
      }
      return 0;
#else
      (void)socketPath;
      errno = EAFNOSUPPORT; //UNIX domain socket not supported in Windows
      result = -1;
#endif // !_WIN32
   }
   errno = EINVAL;
   return -1;
}


/**
 * send UDP message
 */
int8_t msocket_send_to(msocket_t *self, const char *addr,uint16_t port,const void *msgData,uint32_t msgLen){
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
         int n = send(self->tcpsockfd, (const char*) p, remain,0);
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
      uint8_t newConnection;
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
      newConnection = self->newConnection;
      self->newConnection = 0;
      MUTEX_UNLOCK(self->mutex);

      if (newConnection != 0) {
         if (self->handlerTable->tcp_connected != 0) {
            self->handlerTable->tcp_connected(self->handlerArg, &self->tcpInfo.addr[0], self->tcpInfo.port);
         }
      }

      while(1){
         int max_sd = 0;
         int activity;

         FD_ZERO(&readfds);
         if(self->socketMode & MSOCKET_MODE_UDP){
            FD_SET(self->udpsockfd, &readfds);
#ifndef _WIN32 //max_sd param is not used in WinSock2
            if(self->udpsockfd > max_sd){
               max_sd=self->udpsockfd;
            }
#endif
         }
         else if(self->socketMode & MSOCKET_MODE_TCP){ //the thread listening on UDP uses a different thread for TCP accept, use else-if here to prevent deadlock
            FD_SET(self->tcpsockfd, &readfds);
#ifndef _WIN32 //max_sd param is not used in WinSock2
            if(self->tcpsockfd > max_sd){
               max_sd=self->tcpsockfd;
            }
#endif
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
                   rc = msocket_udpRxHandler(self, recvBuf, rc);
                   if(rc < 0){
                      break;
                   }
               }
            }
            if( (self->socketMode & MSOCKET_MODE_TCP) && (FD_ISSET(self->tcpsockfd,&readfds) != 0) ){
               rc=recv(self->tcpsockfd, (char*) &recvBuf[0],MSG_BUF_SIZE,0);
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
         if(msocket_bytearray_append(&self->tcpRxBuf,recvBuf,(uint32_t) len) != 0){
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
               msocket_bytearray_trimLeft(&self->tcpRxBuf,pBegin+parseLen);
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
      self->socketMode &=(uint8_t) (~MSOCKET_MODE_TCP);
      self->tcpsockfd = INVALID_SOCKET;
   }
   if (socketMode & MSOCKET_MODE_UDP){
      SOCKET_CLOSE(self->udpsockfd);
      self->socketMode &=(uint8_t) (~MSOCKET_MODE_UDP);
      self->udpsockfd = INVALID_SOCKET;
   }
}

static void msocket_reset(msocket_t *self){
   self->state = MSOCKET_STATE_NONE;
   self->newConnection = 0u;
   self->socketMode = 0u;
   msocket_timeoutReset(self);
   msocket_bytearray_clear(&self->tcpRxBuf);
}

static void msocket_shutdownPrepare(msocket_t *self){
    if( (self->state == MSOCKET_STATE_PENDING) || (self->state == MSOCKET_STATE_ESTABLISHED) || (self->state == MSOCKET_STATE_ACCEPTING) ){
       self->state = MSOCKET_STATE_CLOSING;
       if (self->socketMode & MSOCKET_MODE_TCP){
          SOCKET_SHUTDOWN(self->tcpsockfd);
       }
   }
}

#ifndef _WIN32
static int msocket_accept_local(msocket_t* self, msocket_t* child) {
   SOCKET_T sockfd = accept(self->tcpsockfd, NULL, NULL);
   if (IS_INVALID_SOCKET(sockfd)) {
      return -1;
   }
   child->tcpsockfd = sockfd;
   return 0;
}
#endif

static int msocket_accept_inet(msocket_t* self, msocket_t* child) {
   struct sockaddr_in cli_addr;
   SOCKET_T sockfd;
#ifdef _WIN32
   int cli_len;
#else
   socklen_t cli_len;
#endif

   cli_len = sizeof(cli_addr);
   memset(&cli_addr, 0, cli_len);
   sockfd = accept(self->tcpsockfd, (struct sockaddr*)&cli_addr, &cli_len); //blocking call (close tcpsockfd from another thread to unblock)
   if (IS_INVALID_SOCKET(sockfd)) {
      return -1;
   }
   if (inet_ntop(AF_INET, &(cli_addr.sin_addr), child->tcpInfo.addr, MSOCKET_ADDRSTRLEN) == NULL) {
      SOCKET_CLOSE(sockfd);
      return -1;
   }
   child->tcpInfo.port = ntohs(cli_addr.sin_port);
   child->tcpsockfd = sockfd;
   return 0;
}

static int msocket_accept_inet6(msocket_t* self, msocket_t* child) {
   struct sockaddr_in6 cli_addr6;
   SOCKET_T sockfd;
#ifdef _WIN32
   int cli_len;
#else
   socklen_t cli_len;
#endif

   cli_len = sizeof(cli_addr6);
   memset(&cli_addr6, 0, cli_len);
   sockfd = accept(self->tcpsockfd, (struct sockaddr*) &cli_addr6, &cli_len);
   if (IS_INVALID_SOCKET(sockfd)) {
      return -1;
   }
   if (inet_ntop(AF_INET, &(cli_addr6.sin6_addr), child->tcpInfo.addr, MSOCKET_ADDRSTRLEN) == NULL) {
      SOCKET_CLOSE(sockfd);
      return -1;
   }
   child->tcpInfo.port = ntohs(cli_addr6.sin6_port);
   child->tcpsockfd = sockfd;
   return 0;
}


static int msocket_connect_inet(msocket_t* self, const char* address, uint16_t port) {
   struct sockaddr_in saddr;
   int result;
   SOCKET_T sockfd;

   memset(&saddr, 0, sizeof(saddr));
   result = inet_pton(AF_INET, address, &(saddr.sin_addr));
   if (result <= 0) {
      //inet_pton retuns 1 on success, -1 on unsupported address family and 0 when string wasn't successfully parsed.
      return -1;
   }

#ifdef _WIN32
   strcpy_s(self->tcpInfo.addr, MSOCKET_ADDRSTRLEN, address);
#else
   strcpy(self->tcpInfo.addr, address);
#endif
   saddr.sin_family = AF_INET;
   saddr.sin_port = htons(port);
   sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

   if (IS_INVALID_SOCKET(sockfd)) {
      return -1;
   }
   result = connect(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));
   if (result < 0) {
      SOCKET_CLOSE(sockfd);
      return -1;
   }
   self->tcpInfo.port = port;
   self->tcpsockfd = sockfd;
   return 0;
}

static int msocket_connect_inet6(msocket_t* self, const char* address, uint16_t port)
{
   struct sockaddr_in6 saddr6;
   int result;
   SOCKET_T sockfd;

   memset(&saddr6, 0, sizeof(saddr6));
   result = inet_pton(AF_INET6, address, &(saddr6.sin6_addr));
   if (result <= 0) {
      //inet_pton retuns 1 on success, -1 on unsupported address family and 0 when string wasn't successfully parsed.
      return -1;
   }

   self->tcpInfo.port = port;
#ifdef _WIN32
   strcpy_s(self->tcpInfo.addr, MSOCKET_ADDRSTRLEN, address);
#else
   strcpy(self->tcpInfo.addr, address);
#endif
   saddr6.sin6_family = AF_INET6;
   saddr6.sin6_port = htons((uint16_t)self->tcpInfo.port);
   sockfd = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);

   if (IS_INVALID_SOCKET(sockfd)) {
      return -1;
   }
   result = connect(sockfd, (struct sockaddr*)&saddr6, sizeof(saddr6));
   if (result < 0) {
      SOCKET_CLOSE(sockfd);
      return -1;
   }
   return 0;
}

#ifndef _WIN32
static int msocket_connect_unix_internal(msocket_t* self, const char* socketPath) {
   SOCKET_T sockfd;
   int result;
   struct sockaddr_un saddr;

   memset(&saddr, 0, sizeof(saddr));
   saddr.sun_family = AF_LOCAL;
   if (*socketPath == '\0') {
      *saddr.sun_path = '\0';
      strncpy(saddr.sun_path + 1, socketPath + 1, sizeof(saddr.sun_path) - 2);
   }
   else {
      strncpy(saddr.sun_path, socketPath, sizeof(saddr.sun_path) - 1);
   }

   sockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
   if (IS_INVALID_SOCKET(sockfd)) {
      return -1;
   }
   result = connect(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));
   if (result < 0) {
      SOCKET_CLOSE(sockfd);
      return result;
   }
   return 0;
}
#endif

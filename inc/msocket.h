/*****************************************************************************
* \file:    msocket.h
* \author:  Conny Gustafsson
* \date:    2014-10-01
* \brief:   event-driven socket library for Linux and Windows
*
* Copyright (c) 2014-2016 Conny Gustafsson
*
******************************************************************************/

#ifndef MSOCKET_H
#define MSOCKET_H

/********************************* Includes **********************************/
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Winsock2.h>
#include <ws2tcpip.h>
#else
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#endif
#include "osmacro.h"
#include "adt_bytearray.h"

/**************************** Constants and Types ****************************/

#define MSOCKET_STATE_NONE          0
#define MSOCKET_STATE_LISTENING     1
#define MSOCKET_STATE_ACCEPTING     2
#define MSOCKET_STATE_PENDING       3
#define MSOCKET_STATE_ESTABLISHED   4
#define MSOCKET_STATE_CLOSING       5
#define MSOCKET_STATE_CLOSED        6

#define MSOCKET_MODE_NONE           0
#define MSOCKET_MODE_UDP            1
#define MSOCKET_MODE_TCP            2

#define MSOCKET_ADDRSTRLEN INET6_ADDRSTRLEN

#define MSOCKET_RCV_BUF_GROW_SIZE (8*1024)
#define MSOCKET_MIN_RCV_BUF_SIZE (MSOCKET_RCV_BUF_GROW_SIZE)

struct msocket_t;
struct msocket_server_t;


typedef struct msocket_handler_t{
   void (*tcp_accept)(void *arg, struct msocket_server_t *srv,struct msocket_t *msocket);
   void (*udp_msg)(void *arg, const char *addr, uint16_t port, const uint8_t *dataBuf, uint32_t dataLen);
   void (*tcp_connected)(void *arg,const char *addr, uint16_t port);
   void (*tcp_disconnected)(void *arg);
   int8_t (*tcp_data)(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen); //return 0 on success, -1 on failure (this will force the socket to close)
   void (*tcp_inactivity)(uint32_t elapsed);
} msocket_handler_t;

typedef struct msocketAddrInfo_t{
   uint16_t port;
   char addr[MSOCKET_ADDRSTRLEN];
}msocketAddrInfo_t;

typedef struct msocket_t{
   SOCKET_T tcpsockfd;
   SOCKET_T udpsockfd;
   THREAD_T ioThread;
   MUTEX_T mutex;
#ifdef _WIN32
   unsigned int ioThreadId;
#endif
   msocketAddrInfo_t tcpInfo;
   msocketAddrInfo_t udpInfo;
   adt_bytearray_t tcpRxBuf;
   msocket_handler_t *handlerTable;
   void *handlerArg;
   uint8_t state; //TCP socket state
   uint8_t threadRunning;
   uint8_t socketMode;
   uint8_t newConnection;
   uint32_t inactivityMs;
   uint32_t inactivityCallMs;
   uint8_t addressFamily;
}msocket_t;
/********************************* Functions *********************************/
int8_t msocket_create(msocket_t *self,uint8_t addressFamily);
void msocket_destroy(msocket_t *self);
msocket_t *msocket_new(uint8_t addressFamily);
void msocket_delete(msocket_t *self);
void msocket_vdelete(void *arg);
void msocket_close(msocket_t *self);
int8_t msocket_listen(msocket_t *self,uint8_t mode, const uint16_t port, const char *addr);
#ifndef _WIN32
int8_t msocket_unix_listen(msocket_t *self, const char *socket_path);
#endif
msocket_t *msocket_accept(msocket_t *self,msocket_t *child);
void msocket_sethandler(msocket_t *self, const msocket_handler_t *handlerTable, void *handlerArg);
int8_t msocket_start_io(msocket_t *self);

int8_t msocket_connect(msocket_t *self,const char *addr,uint16_t port);
#ifndef _WIN32
int8_t msocket_unix_connect(msocket_t *self,const char *socketPath);
#endif
int8_t msocket_sendto(msocket_t *self,const char *addr,uint16_t port,const void *msgData,uint32_t msgLen);
int8_t msocket_send(msocket_t *self,void *msgData,uint32_t msgLen);
int8_t msocket_state(msocket_t *self);


#endif //MSOCKET_H

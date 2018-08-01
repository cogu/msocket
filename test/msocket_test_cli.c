#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <semaphore.h>
#include <unistd.h>
#endif
#include "msocket.h"
#include "osmacro.h"
#include "msocket.h"

/************************** VARIABLES ***********************************/
static msocket_t *m_socket = 0;
static int m_port = -1;
static SEMAPHORE_T m_sem;

/************************** STATIC FUNCTION DECLARATIONS ***********************************/
static int8_t tcp_client_connect(uint16_t port);
static void tcp_client_disconnect(void);
static int8_t tcp_client_reconnect(uint16_t port);
static void tcp_client_connected(void *arg, const char *addr, uint16_t port);
static int8_t tcp_client_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen);
static void tcp_client_disconnected(void *arg);

/******************************* TCP SERVER **************************************/
int main(int argc, char **argv)
{
   int8_t result;
   int32_t attempt = 0;
#ifdef _WIN32
   WORD wVersionRequested;
   WSADATA wsaData;
   int err;
   wVersionRequested = MAKEWORD(2, 2);
   err = WSAStartup(wVersionRequested, &wsaData);
   if (err != 0) {
      /* Tell the user that we could not find a usable Winsock DLL*/
      printf("WSAStartup failed with error: %d\n", err);
      return 1;
   }
#endif
   m_port = 8400;
   SEMAPHORE_CREATE(m_sem);
   ++attempt;
   result = tcp_client_connect(m_port);
   if (result == 0)
   {
#ifdef _WIN32
      (void) WaitForSingleObject(m_sem, INFINITE);
#else
      sem_wait(&m_sem);
#endif
      printf("[CLIENT] Attempt %d successful\n", attempt);
      SLEEP(1000);
      tcp_client_disconnect();
      SLEEP(1000);
      ++attempt;
      result = tcp_client_reconnect(m_port);
      if (result == 0)
      {
#ifdef _WIN32
         (void) WaitForSingleObject(m_sem, INFINITE);
#else
         sem_wait(&m_sem);
#endif
         printf("[CLIENT] Attempt %d successful\n", attempt);
         SLEEP(1000);
         tcp_client_disconnect();
      }
      else
      {
         printf("[CLIENT] Attempt %d, failed to connect\n", attempt);
      }
   }
   else
   {
      printf("[CLIENT] Attempt %d, failed to connect\n", attempt);
   }
   SEMAPHORE_DESTROY(m_sem);

#ifdef _WIN32
   WSACleanup();
#endif
   return 0;
}

/************************** STATIC FUNCTIONS ***********************************/
static int8_t tcp_client_connect(uint16_t port)
{
   //attempt TCP connection
   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.tcp_connected = tcp_client_connected;
   handler.tcp_data = tcp_client_data;
   handler.tcp_disconnected = tcp_client_disconnected;
   printf("[CLIENT] Connecting on port %d\n", port);
   m_socket = msocket_new(AF_INET);
   msocket_sethandler(m_socket, &handler, 0);
   return msocket_connect(m_socket, "127.0.0.1", (uint16_t)port);
}

static void tcp_client_disconnect(void)
{
   if (m_socket != 0)
   {
      msocket_close(m_socket);
   }
}

static int8_t tcp_client_reconnect(uint16_t port)
{
   if (m_socket != 0)
   {
      return msocket_connect(m_socket, "127.0.0.1", (uint16_t)port);
   }
   return -1;
}

static void tcp_client_connected(void *arg, const char *addr, uint16_t port)
{
   SEMAPHORE_POST(m_sem);
}

static int8_t tcp_client_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
{
   *parseLen = dataLen;
   return 0;
}

static void tcp_client_disconnected(void *arg)
{
   printf("[CLIENT] server connection lost\n");
   msocket_delete(m_socket);
   m_socket = 0;
}



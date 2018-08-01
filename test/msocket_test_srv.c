#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "msocket.h"
#include "osmacro.h"
#include "msocket_server.h"

#define BUF_LEN 1024

/************************** VARIABLES ***********************************/
static msocket_t *m_socket = 0;
static msocket_server_t *m_srv;
static int m_port = -1;

/************************** STATIC FUNCTION DECLARATIONS ***********************************/
static void process_data(const uint8_t *dataBuf, uint32_t dataLen);
static int8_t tcp_server_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen);
static void tcp_server_disconnected(void *arg);
static void tcp_new_connection(void *arg, msocket_server_t *srv, msocket_t *msocket);
static void tcp_cleanup_connection(void* arg);

/******************************* TCP SERVER **************************************/
int main(int argc, char **argv)
{
#ifdef _WIN32
   WORD wVersionRequested;
   WSADATA wsaData;
   int err;
#endif
   msocket_handler_t handler;
   m_port = 8400;
#ifdef _WIN32
   wVersionRequested = MAKEWORD(2, 2);
   err = WSAStartup(wVersionRequested, &wsaData);
   if (err != 0) {
      /* Tell the user that we could not find a usable Winsock DLL*/
      printf("WSAStartup failed with error: %d\n", err);
      return 1;
   }
#endif
   //start server
   memset(&handler, 0, sizeof(handler));
   printf("[SERVER] starting server on port %d\n", (int)m_port);
   handler.tcp_accept = tcp_new_connection;
   handler.tcp_data = tcp_server_data;
   m_srv = msocket_server_new(AF_INET, tcp_cleanup_connection);
   msocket_server_sethandler(m_srv, &handler, 0);
   msocket_server_start(m_srv, 0, 0, (uint16_t)m_port);

   for (;;)
   {
      SLEEP(1000);
   }

#ifdef _WIN32
   WSACleanup();
#endif
   return 0;
}

/************************** STATIC FUNCTIONS ***********************************/
static void process_data(const uint8_t *dataBuf, uint32_t dataLen)
{
   if (m_socket != 0)
   {
      msocket_send(m_socket, (void*) dataBuf, dataLen);
   }
}

static int8_t tcp_server_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
{
   *parseLen = dataLen; //consume all data
   process_data(dataBuf, dataLen);
   return 0;
}

static void tcp_server_disconnected(void *arg)
{
   printf("[SERVER] client connection lost\n");
   msocket_server_cleanup_connection(m_srv, (void*) m_socket);
   m_socket = 0;
}

static void tcp_new_connection(void *arg, msocket_server_t *srv, msocket_t *msocket)
{
   msocket_handler_t handler;
   printf("[SERVER] connection accepted\n");
   m_socket = msocket;
   memset(&handler, 0, sizeof(handler));
   handler.tcp_data = tcp_server_data;
   handler.tcp_disconnected = tcp_server_disconnected;
   msocket_sethandler(m_socket, &handler, (void*)0);
   msocket_start_io(m_socket);
}

static void tcp_cleanup_connection(void* arg) {
   msocket_t *sock = (msocket_t*)arg;
   if (sock != 0)
   {
      msocket_delete(sock);
   }
}

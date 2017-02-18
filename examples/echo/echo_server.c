#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "msocket.h"
#include "osmacro.h"
#include "msocket_server.h"

#define BUF_LEN 1024

/************************** VARIABLES ***********************************/
static msocket_t *m_socket = 0;
static msocket_server_t *m_srv;
static int m_port = -1;


/******************************* TCP SERVER **************************************/
int8_t tcp_server_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
{
   *parseLen = dataLen; //consume all data
   printf("[SERVER] got %d bytes of data\n", (int)dataLen);
   return 0;
}


void tcp_server_disconnected(void *arg)
{
   printf("[SERVER] client connection lost\n");
   msocket_delete(m_socket);
   m_socket = 0;
}

void tcp_new_connection(void *arg, msocket_server_t *srv, msocket_t *msocket) {
   msocket_handler_t handler;
   printf("[SERVER] connection accepted\n");
   m_socket = msocket;
   memset(&handler, 0, sizeof(handler));
   handler.tcp_data = tcp_server_data;
   handler.tcp_disconnected = tcp_server_disconnected;
   msocket_sethandler(m_socket, &handler, (void*)0);
   msocket_start_io(m_socket);
}

/****************************** SHARED *******************************************/

void printUsage(void)
{
   printf("echo_server -p<port>\n");
}

static void parse_args(int argc, char **argv)
{
   int i;
   for (i = 1; i < argc; i++)
   {
      if (strncmp(argv[i], "-p=", 3) == 0)
      {
         char *endptr = 0;
         long num = strtol(&argv[i][3], &endptr, 10);
         if (endptr > &argv[i][3])
         {
            m_port = (int)num;
         }
      }
      else if (strncmp(argv[i], "-p", 2) == 0)
      {
         char *endptr = 0;
         long num = strtol(&argv[i][2], &endptr, 10);
         if (endptr > &argv[i][2])
         {
            m_port = (int)num;
         }
      }
   }
}

int main(int argc, char **argv)
{
   char *buf;
   int len;
#ifdef _WIN32
   WORD wVersionRequested;
   WSADATA wsaData;
   int err;
#endif
   msocket_handler_t handler;
   if (argc < 2)
   {
      printUsage();
      return 0;
   }
   parse_args(argc, argv);
   if ((m_port < 0))
   {
      printUsage();
      return 0;
   }
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
   m_srv = msocket_server_new(AF_INET, NULL);
   msocket_server_sethandler(m_srv, &handler, 0);
   msocket_server_start(m_srv, 0, 0, (uint16_t)m_port);

   buf = (char*)malloc(BUF_LEN);

   for (;;)
   {
      char * result;
      len = BUF_LEN;
      result = fgets(buf, len, stdin);
      if (result == 0)
      {
         break;
      }
      else
      {
         if (m_socket != 0)
         {
            msocket_send(m_socket, buf, (uint32_t)strlen(result));
         }
      }
   }
   free(buf);
#ifdef _WIN32
   WSACleanup();
#endif
   return 0;
}

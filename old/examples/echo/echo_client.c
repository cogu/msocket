#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "msocket.h"
#include "osmacro.h"

/************************** SHARED ***********************************/
static msocket_t *m_socket = 0;
static int m_port = -1;

/*************************** ECHO CLIENT ****************************/
void tcp_client_connected(void *arg, const char *addr, uint16_t port)
{
	printf("[CLIENT] connected on port %d\n", (int)port);
}

int8_t tcp_client_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
{
   char tmp[1024];
   int copyLen;
   *parseLen = dataLen; //tell msocket layer that you consumed all the data	
	copyLen = dataLen;
	if (dataLen >= sizeof(tmp))
	{
		copyLen = sizeof(tmp) - 1; //reserve 1 byte for the NULL-character
	}
	memcpy(tmp, dataBuf, copyLen);
	tmp[copyLen] = '\0';
	printf("[CLIENT] %s\n", tmp);
	return 0; //return OK
}

void tcp_client_disconnected(void *arg)
{
	printf("[CLIENT] server connection lost\n");
	msocket_delete(m_socket);
	m_socket = 0;
}

int8_t tcp_client_connect(uint16_t port)
{
	//attempt TCP connection
	msocket_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.tcp_connected = tcp_client_connected;
	handler.tcp_data = tcp_client_data;
	handler.tcp_disconnected = tcp_client_disconnected;
	printf("starting client using port %d\n", port);
	m_socket = msocket_new(AF_INET);
	msocket_sethandler(m_socket, &handler, 0);
	return msocket_connect(m_socket, "127.0.0.1", (uint16_t)port);
}

/****************************** CLIENT *******************************************/

void printUsage(void)
{
	printf("echo_client -p<port>\n");
}

static void parse_args(int argc, char **argv)
{
	int i;
	for (i = 1; i<argc; i++)
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
   int8_t result;
   int retval = 0;
#ifdef _WIN32
   WORD wVersionRequested;
   WSADATA wsaData;
   int err;
#endif
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
   result = tcp_client_connect(m_port);
   if (result == 0)
   {
      buf = (char*)malloc(1024);
      for (;;)
      {
         SLEEP(1000);
      }
      free(buf);
   }
   else
   {
      printf("connect failed\n");
      retval=-1;
   }
#ifdef _WIN32
   WSACleanup();
#endif
	return retval;
}

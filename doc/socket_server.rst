socket_server
=============

socket_server is an optional extension module for msocket. It provides a ready to use tcp/udp server without the hassle of setting it up from scratch using msocket directly.

It requires some extra .h/.c files:
   * adt_ary.h
   * ary_ary.c
   
Both of these files you can copy and compile into your existing project from here: https://github.com/cogu/adt

Creating a TCP server
----------------------------

Simple TCP server using port 3000

.. code:: C
   
   #include <string.h>
   #include <stdio.h>
   #include <unistd.h>
   #include "msocket_server.h"
   
   void tcp_server_disconnected(void *arg)
   {
      printf("client connection lost\n");
   }
   
   uint8_t tcp_server_data(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
   {
      printf("server received %d bytes\n",(int) dataLen);
      *parseLen=dataLen; /*signal to msocket that you consumed all the data*/
      return 0; /*must return 0 here*/
   }
   
   void tcp_new_connection(void *arg,msocket_server_t *srv,msocket_t *childSocket)
   {
      msocket_handler_t handler;
      printf("connection accepted\n");
      memset(&handler,0,sizeof(handler));
      handler.tcp_data=tcp_server_data;
      handler.tcp_disconnected = tcp_server_disconnected;
      msocket_sethandler(childSocket,&handler,(void*) 0);
      msocket_start_io(childSocket);
   }
   
   static msocket_server_t *m_srv;
   
   int main(int argc, char **argv)
   {
      uint16_t port=3000;
      msocket_handler_t handler;
      memset(&handler,0,sizeof(handler));
   
      handler.tcp_accept = tcp_new_connection;
      m_srv = msocket_server_new(AF_INET);
      msocket_server_sethandler(m_srv,&handler,0);
      msocket_server_start(m_srv,0,0,port);
      printf("Waiting for connection on port %d\n",(int) port);
      for(;;)
      {
         sleep(1); /*main thread is sleeping while child threads do all the work*/
      }
   }

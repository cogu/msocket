msocket
=======

Usage:

.. code:: C
   
   #include <stdio.h>
   #include <stdio.h>
   #include <string.h>
   #include <stdlib.h>
   #include <unistd.h> //only used for sleep function
   #include "msocket.h"
   
   static void onConnected(void *arg,const char *addr, uint16_t port)
   {
      printf("connected on port %d\n",port);
   }
   
   static void onDisconnected(void *arg)
   {
      msocket_t *msocket = (msocket_t*) arg;
      printf("disconnected on port %d\n",msocket->tcpInfo.port);
   }
   
   static uint8_t onData(void *arg, const uint8_t *dataBuf, uint32_t dataLen, uint32_t *parseLen)
   {
      printf("Received %d bytes of data\n",dataLen);
      *parseLen = dataLen;
      return 0;
   }
   
   int main(int argc, char** argv)
   {
      msocket_handler_t handler;
      msocket_t *msocket = msocket_new(AF_INET);
      memset(&handler,0,sizeof(handler));
   
      handler.tcp_connected = onConnected;
      handler.tcp_disconnected = onDisconnected;
      handler.tcp_data = onData;
      msocket_sethandler(msocket,&handler,msocket);
      msocket_connect(msocket,"127.0.0.1",3000);
      for(;;)
      {
         sleep(2);
      }
      return 0;
   }



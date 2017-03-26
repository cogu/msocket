msocket client
==============

Usage
-----

Linux client 
~~~~~~~~~~~~

.. code:: C
   
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

Client C API
-------------

msocket_create
~~~~~~~~~~~~~~
   
.. c:function:: int8_t msocket_create(msocket_t *self,uint8_t addressFamily)

Initializes the msocket object pointed to by self. 
   
addressFamily:
   
+-------------------------+--------------------------------+
| addressFamily : uint8_t | Description                    |
+=========================+================================+
| 0                       | Defaults to AF_INET            |
+-------------------------+--------------------------------+
| AF_INET                 | Internet protocol              |
+-------------------------+--------------------------------+
| AF_INET6                | IPv6                           |
+-------------------------+--------------------------------+
| AF_UNIX                 | Local Socket (Unix/Linux only) |
+-------------------------+--------------------------------+

msocket_destroy
~~~~~~~~~~~~~~~

.. c:function:: void msocket_destroy(msocket_t *self)

Destroys the msocket object (but does not free its memory). Use on objects created by **msocket_create**.

msocket_new
~~~~~~~~~~~~~~

.. c:function:: msocket_t *msocket_new(uint8_t addressFamily)

Allocates memory for new msocket object then calls msocket_create on it. 
Returns the new object or NULL on failure.

msocket_delete
~~~~~~~~~~~~~~

.. c:function:: msocket_delete(msocket_t *self)

Destroys the msocket object and frees its memory. Use on objects created with **msocket_new**.

msocket_vdelete
~~~~~~~~~~~~~~~

.. c:function:: msocket_vdelete(void *arg)

Virtual destructor that internally calls msocket_delete by casting the void* argument into an msocket_t*.
Use with data structures that support virtual destructors such as *adt_list*, *adt_ary* etc. found in `adt <https://github.com/cogu/adt>`_.

msocket_close
~~~~~~~~~~~~~

.. c:function:: msocket_close(msocket_t *self)

Closes an open socket connection.


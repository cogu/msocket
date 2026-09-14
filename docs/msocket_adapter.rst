C++ RAII Adapters (msocket_adapter)
=====================================

The ``msocket_adapter`` module provides a modern C++17 RAII wrapper and listener interface around the C ``msocket`` and ``msocket_server`` libraries.

Architecture Overview
---------------------

- **Socket**: Pure virtual interface representing a bidirectional stream connection.
- **TcpSocket**: RAII wrapper around a real OS-backed stream socket (``msocket_t``). Automatically joins I/O threads and cleans up handles on destruction or move.
- **TestSocket**: RAII wrapper around an in-memory mock socket (``testsocket_t``) for deterministic unit testing.
- **TcpServer**: RAII wrapper around ``msocket_server_t`` managing listening sockets and accepted incoming connections.
- **SocketListener & ServerListener**: Interface classes for handling connection lifecycle events and incoming data callbacks.

Usage Example
-------------

Connecting and streaming in C++:

.. code-block:: cpp

   #include "msocket_adapter.h"
   #include <iostream>

   class MyListener : public msocket::SocketListener
   {
   public:
      void on_connected(std::string_view address, std::uint16_t port) override
      {
         std::cout << "Connected to " << address << ":" << port << "\n";
      }

      int on_data_received(const std::uint8_t* data, std::size_t size, std::size_t& parse_len) override
      {
         std::cout << "Received " << size << " bytes\n";
         parse_len = size;
         return 0;
      }
   };

   void run()
   {
      MyListener listener;
      msocket::TcpSocket socket;
      socket.set_listener(&listener);
      socket.connect("127.0.0.1", 5000);
      socket.send("Hello Server!\n");
   }

API Reference
-------------

.. default-domain:: cpp

Listener Interfaces
~~~~~~~~~~~~~~~~~~~

.. doxygenclass:: msocket::SocketListener
   :members:

.. doxygenclass:: msocket::ServerListener
   :members:

Stream Sockets
~~~~~~~~~~~~~~

.. doxygenclass:: msocket::Socket
   :members:

.. doxygenclass:: msocket::TcpSocket
   :members:

.. doxygenclass:: msocket::TestSocket
   :members:

Server
~~~~~~

.. doxygenclass:: msocket::TcpServer
   :members:

Legacy Adapter
~~~~~~~~~~~~~~

.. doxygenclass:: msocket::Handler
   :members:

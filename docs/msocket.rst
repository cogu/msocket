Socket Client and Peer (msocket)
================================

The ``msocket`` module represents the core socket abstraction in the library. It wraps low-level OS socket handles and runs an event-driven background worker thread for asynchronous read and write operations.

Features
--------

- **TCP / Stream**: Connects to remote TCP servers (IPv4 and IPv6) or acts as an accepted client peer.
- **UDP / Datagram**: Sends and receives UDP datagrams with optional broadcast/multicast.
- **UNIX Domain Sockets**: Local IPC connections on POSIX systems.
- **Callback Table**: Dispatches connect, disconnect, and incoming data events via :doc:`msocket_handler_t <socket_handler>`.

Handler Callbacks
-----------------

Applications register event callbacks using the :doc:`Socket Event Handler <socket_handler>` table to handle connection lifecycle events, data streaming, inactivity timeouts, and datagram messages.

See :doc:`Socket Event Handler <socket_handler>` for complete callback function signatures, parameter types, framing semantics, and implementation contracts.

API Reference
-------------


Lifecycle Functions
~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_create
.. doxygenfunction:: msocket_destroy
.. doxygenfunction:: msocket_new
.. doxygenfunction:: msocket_delete
.. doxygenfunction:: msocket_vdelete
.. doxygenfunction:: msocket_close

Configuration and Handlers
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_set_handler
.. doxygenfunction:: msocket_set_server

Connection and I/O Operations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_connect
.. doxygenfunction:: msocket_unix_connect
.. doxygenfunction:: msocket_listen
.. doxygenfunction:: msocket_unix_listen
.. doxygenfunction:: msocket_accept
.. doxygenfunction:: msocket_start_io
.. doxygenfunction:: msocket_send
.. doxygenfunction:: msocket_send_to

Utilities and State
~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_state
.. doxygenfunction:: msocket_parse_endpoint

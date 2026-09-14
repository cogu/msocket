Server Listener and Manager (msocket_server)
==============================================

The ``msocket_server`` module manages listening sockets and handles accepted client connections. It runs a dedicated accept thread to receive incoming connection requests, dispatches connection events to registered callbacks, and operates an asynchronous cleanup thread to safely reap disconnected client sockets.

Architecture & Acceptance Workflow
-----------------------------------

1. Initialize the server using :cpp:func:`msocket_server_create` or :cpp:func:`msocket_server_new`.
2. Register an accept handler callback table with :cpp:func:`msocket_server_set_handler`.
   Inside the ``stream_accept`` callback (see :doc:`Socket Event Handler <socket_handler>`):

   - Configure per-connection event handlers on the child socket using :cpp:func:`msocket_set_handler`.
   - Attach the child socket to the server using :cpp:func:`msocket_set_server(child_socket, srv) <msocket_set_server>` for automatic cleanup upon disconnect.
   - Start background I/O on the child connection by calling :cpp:func:`msocket_start_io(child_socket) <msocket_start_io>`.
3. Start the server on the desired TCP/UDP port using :cpp:func:`msocket_server_start` or on a UNIX domain socket using :cpp:func:`msocket_server_unix_start`.
4. When stopping, destroy the server with :cpp:func:`msocket_server_destroy` or :cpp:func:`msocket_server_delete`, which stops all listening threads and cleans up reaped client resources.

API Reference
-------------

Lifecycle Functions
~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_server_create
.. doxygenfunction:: msocket_server_destroy
.. doxygenfunction:: msocket_server_new
.. doxygenfunction:: msocket_server_delete

Configuration and Startup
~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_server_set_handler
.. doxygenfunction:: msocket_server_start
.. doxygenfunction:: msocket_server_unix_start

Connection Reaping & Cleanup
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_server_disable_cleanup
.. doxygenfunction:: msocket_server_reap_connection
.. doxygenfunction:: msocket_server_cleanup_connection

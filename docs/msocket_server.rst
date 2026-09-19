Server Listener and Manager (msocket_server)
==============================================

The ``msocket_server`` module manages listening sockets and handles accepted client connections. It runs a dedicated accept thread to receive incoming connection requests, dispatches connection events to registered callbacks, and operates an asynchronous cleanup thread to safely reap disconnected client sockets.

Architecture & Acceptance Workflow
-----------------------------------

The server operates in one of two modes depending on the destructor passed to :cpp:func:`msocket_server_create` or :cpp:func:`msocket_server_new`:

Default Auto-Reap Mode
~~~~~~~~~~~~~~~~~~~~~~

When initialized with ``NULL`` or :cpp:func:`msocket_vdelete` as the destructor, accepted sockets are managed automatically:

1. Initialize the server using :cpp:func:`msocket_server_create` (e.g. ``msocket_server_create(srv, family, NULL)``) or :cpp:func:`msocket_server_new` (e.g. ``msocket_server_new(family, NULL)``).
2. Register an accept handler callback table with :cpp:func:`msocket_server_set_handler`.
   Inside the ``stream_accept`` callback (see :doc:`Socket Event Handler <socket_handler>`):

   - Attach per-connection event handlers to the child socket using :cpp:func:`msocket_set_handler`.
   - Start background I/O on the child connection by calling :cpp:func:`msocket_start_io(child_socket) <msocket_start_io>`.
   - *Note:* Accepted child sockets are automatically cleaned up by the server upon disconnect. When the remote client disconnects, the socket's background thread automatically enqueues the raw socket to the cleanup thread for asynchronous deletion.

3. Start listening with :cpp:func:`msocket_server_start` (TCP/UDP) or :cpp:func:`msocket_server_unix_start` (UNIX domain).
4. When stopping, destroy the server with :cpp:func:`msocket_server_destroy` or :cpp:func:`msocket_server_delete`.

Custom Wrapper Mode
~~~~~~~~~~~~~~~~~~~

When wrapping client sockets inside application-level connection or session structures (as in APX):

1. Initialize the server with a custom destructor function: :cpp:func:`msocket_server_new` (e.g. ``msocket_server_new(family, on_custom_conn_delete)``).
2. Inside ``stream_accept``:

   - Allocate your wrapper structure and wrap the accepted ``child_socket``.
   - Configure per-connection event handlers on ``child_socket``, passing the wrapper as callback context.
   - Start background I/O on ``child_socket`` using :cpp:func:`msocket_start_io`.
   - *Note:* Accepted child sockets are not automatically cleaned up by the server in this mode, preventing raw socket pointers from being passed to the custom destructor.

3. Inside the ``stream_disconnected`` handler of your wrapper:

   - Enqueue the wrapper structure for deletion by calling :cpp:func:`msocket_server_cleanup_connection` (e.g. ``msocket_server_cleanup_connection(srv, wrapper)``).
   - The server cleanup thread invokes your custom destructor, safely deleting both the wrapper and the inner socket.

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

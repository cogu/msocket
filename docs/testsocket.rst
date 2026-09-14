Mock Socket Engine (testsocket)
=================================

The ``testsocket`` module provides an in-memory mock socket implementation for unit testing. It allows client and server application logic to be tested deterministically without creating operating system socket handles, network interfaces, or background threads.

How It Works
------------

A ``testsocket_t`` instance pairs client-side and server-side byte queues:

1. Messages sent via ``testsocket_client_send()`` are appended to the pending buffer for the server.
2. Messages sent via ``testsocket_server_send()`` are appended to the pending buffer for the client.
3. Calling ``testsocket_run()`` executes message delivery: buffered bytes are passed to the registered handler callbacks (``stream_data``) on each side until queues are drained.
4. Calling ``testsocket_on_connect()`` and ``testsocket_on_disconnect()`` triggers the corresponding connection lifecycle callbacks.

API Reference
-------------

Types and Structures
~~~~~~~~~~~~~~~~~~~~

.. doxygenstruct:: testsocket_tag
   :members:

Lifecycle Functions
~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: testsocket_create
.. doxygenfunction:: testsocket_destroy
.. doxygenfunction:: testsocket_new
.. doxygenfunction:: testsocket_delete
.. doxygenfunction:: testsocket_vdelete

Handler Configuration
~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: testsocket_set_server_handler
.. doxygenfunction:: testsocket_set_client_handler

Simulation and Execution
~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: testsocket_on_connect
.. doxygenfunction:: testsocket_on_disconnect
.. doxygenfunction:: testsocket_server_send
.. doxygenfunction:: testsocket_client_send
.. doxygenfunction:: testsocket_run

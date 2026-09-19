Socket Event Handler (msocket_handler_t)
=========================================

The ``msocket_handler_t`` structure is the central callback table for handling asynchronous socket lifecycle and I/O events in ``msocket``.

It is used across:

* :doc:`msocket` (client and peer stream/datagram sockets)
* :doc:`msocket_server` (connection acceptance)
* :doc:`testsocket` (in-memory mock sockets for unit testing)

Conventions
-----------

Callback Signatures
   All callbacks in ``msocket_handler_t`` pass the user context pointer first and the socket handle second:

   .. code-block:: c

      void on_event(void *arg, void *socket, ...);

   * ``arg``: The user-defined context pointer registered via ``msocket_set_handler()`` or ``msocket_server_set_handler()``.
   * ``socket``: Pointer to the originating socket instance (typically cast to ``msocket_t*`` or ``testsocket_t*``).

Naming Pattern
   Recommended callback function names follow the pattern ``on_<subject>_<event>`` (e.g., ``on_client_connected``, ``on_client_data``, ``on_client_disconnected``).

Struct Definition
-----------------

.. code-block:: c

   typedef struct msocket_handler_tag {
      void (*stream_accept)(void *arg, struct msocket_server_tag *srv, void *socket);
      void (*stream_connected)(void *arg, void *socket, const char *addr, uint16_t port);
      void (*stream_disconnected)(void *arg, void *socket);
      msocket_error_t (*stream_data)(void *arg,
                                     void *socket,
                                     const uint8_t *data,
                                     const uint32_t num_bytes,
                                     uint32_t *consumed_bytes,
                                     uint32_t *msg_size_hint);
      void (*stream_inactivity)(void *arg, void *socket, const uint32_t elapsed_ms);
      void (*datagram_msg)(void *arg,
                           void *socket,
                           const char *addr,
                           uint16_t port,
                           const uint8_t *data,
                           const uint32_t num_bytes);
   } msocket_handler_t;

Callback Reference
------------------

stream_accept
~~~~~~~~~~~~~

.. code-block:: c

   void on_client_accept(void *arg, msocket_server_t *srv, void *socket);

Invoked on the server when a remote client initiates a new connection.

* **``arg``**: User context registered via ``msocket_server_set_handler()``.
* **``srv``**: Pointer to the listening ``msocket_server_t`` instance.
* **``socket``**: Pointer to the newly accepted child socket (``msocket_t*``).

**Implementation Contract:** Configure the child socket before starting I/O:

1. Attach per-client callbacks and context: ``msocket_set_handler(child, &client_handler, client_ctx);``
2. Start background I/O: ``msocket_start_io(child);``

*Connection Lifecycle Cleanup:*

* Under **Default Auto-Reap Mode** (server created with ``NULL`` or :cpp:func:`msocket_vdelete`), the server accept task automatically associates the child socket for background cleanup upon disconnect. Manual binding is not required.
* Under **Custom Wrapper Mode** (server created with a custom destructor), wrap the socket in your application connection object, listen for ``stream_disconnected``, and call :cpp:func:`msocket_server_cleanup_connection` (e.g. ``msocket_server_cleanup_connection(srv, wrapper)``).

stream_connected
~~~~~~~~~~~~~~~~

.. code-block:: c

   void on_client_connected(void *arg, void *socket, const char *addr, uint16_t port);

Invoked when an outgoing stream connection (TCP or UNIX domain) successfully connects to the remote host.

* **``arg``**: User context pointer.
* **``socket``**: Originating socket pointer.
* **``addr``**: Remote host IP address string or UNIX domain socket path.
* **``port``**: Remote TCP port (0 for UNIX sockets).

stream_disconnected
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void on_client_disconnected(void *arg, void *socket);

Invoked when a stream connection is severed, either closed by the remote peer or closed locally.

* **``arg``**: User context pointer.
* **``socket``**: Disconnected socket pointer.

stream_data
~~~~~~~~~~~

.. code-block:: c

   msocket_error_t on_client_data(void *arg,
                                  void *socket,
                                  const uint8_t *data,
                                  const uint32_t num_bytes,
                                  uint32_t *consumed_bytes,
                                  uint32_t *msg_size_hint);

Invoked when new stream bytes arrive in the socket's internal receive buffer.

* **``arg``**: User context pointer.
* **``socket``**: Receiving socket pointer.
* **``data``**: Pointer to the continuous buffer of received bytes.
* **``num_bytes``**: Total number of bytes available in ``data``.
* **``consumed_bytes``**: **[Out]** The callback must set this value to the number of bytes parsed and consumed. Any remaining unconsumed bytes are retained in the internal buffer for the next invocation.
* **``msg_size_hint``**: **[Out]** Optional hint indicating the expected total length of an incoming message. Allows the library to optimize internal buffer allocation. Set to 0 if unknown.
* **Return Value**: Return ``MSOCKET_NO_ERROR`` (0) on success. Returning an error code will cause the library to close the connection and transition the socket to the closed state.

stream_inactivity
~~~~~~~~~~~~~~~~~

.. code-block:: c

   void on_client_inactivity(void *arg, void *socket, const uint32_t elapsed_ms);

Periodically invoked when a stream connection remains idle with no incoming traffic.

* **``arg``**: User context pointer.
* **``socket``**: Idle socket pointer.
* **``elapsed_ms``**: Milliseconds elapsed since last activity.

datagram_msg
~~~~~~~~~~~~

.. code-block:: c

   void on_datagram_msg(void *arg,
                        void *socket,
                        const char *addr,
                        uint16_t port,
                        const uint8_t *data,
                        const uint32_t num_bytes);

Invoked when an incoming UDP datagram packet is received.

* **``arg``**: User context pointer.
* **``socket``**: Receiving socket pointer.
* **``addr``**: Sender IP address string.
* **``port``**: Sender UDP port.
* **``data``**: Pointer to received datagram payload.
* **``num_bytes``**: Number of bytes in the payload.

API Reference
-------------

.. doxygenstruct:: msocket_handler_tag
   :members:
   :undoc-members:

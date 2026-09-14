Test Socket Spy (testsocket_spy)
=================================

The ``testsocket_spy`` module provides a singleton call tracker and traffic inspector built on top of ``testsocket``. It records connections, disconnections, and received byte sequences so that test suites can verify communication behavior without writing custom test harnesses.

Usage Example
-------------

In unit tests:

.. code-block:: c

   #include "testsocket_spy.h"

   void test_handshake(void)
   {
      testsocket_spy_create();

      // Get mock socket handle to inject into system-under-test
      testsocket_t *sock = testsocket_spy_client();

      // Trigger simulated connection
      testsocket_on_connect(sock);

      // Verify connection callback counter
      assert(testsocket_spy_get_client_connected_count() == 1);

      testsocket_spy_destroy();
   }

API Reference
-------------

Lifecycle Functions
~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: testsocket_spy_create
.. doxygenfunction:: testsocket_spy_destroy

Socket Access
~~~~~~~~~~~~~

.. doxygenfunction:: testsocket_spy_client
.. doxygenfunction:: testsocket_spy_server

Inspection and Metrics
~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: testsocket_spy_get_received_data
.. doxygenfunction:: testsocket_spy_clear_received_data
.. doxygenfunction:: testsocket_spy_get_client_connected_count
.. doxygenfunction:: testsocket_spy_get_client_disconnect_count
.. doxygenfunction:: testsocket_spy_get_server_connected_count
.. doxygenfunction:: testsocket_spy_get_server_disconnect_count
.. doxygenfunction:: testsocket_spy_get_client_bytes_received
.. doxygenfunction:: testsocket_spy_get_server_bytes_received

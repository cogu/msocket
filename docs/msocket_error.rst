Error Codes (msocket_error)
===========================

The ``msocket_error`` module defines standard error constants and helper functions used throughout the ``msocket`` library.

Error Constants
---------------

.. list-table::
   :header-rows: 1
   :widths: 35 15 50

   * - Constant
     - Value
     - Description
   * - ``MSOCKET_GENERIC_ERROR``
     - -1
     - Generic socket error.
   * - ``MSOCKET_NO_ERROR``
     - 0
     - Operation succeeded without error.
   * - ``MSOCKET_INVALID_ARGUMENT_ERROR``
     - 1
     - Invalid argument passed to a function.
   * - ``MSOCKET_MEM_ERROR``
     - 2
     - Memory allocation failure.
   * - ``MSOCKET_SOCKET_ERROR``
     - 3
     - Underlying OS socket operation failed.
   * - ``MSOCKET_NOT_IMPLEMENTED_ERROR``
     - 4
     - Requested operation is not implemented on the platform.
   * - ``MSOCKET_TIMEOUT_ERROR``
     - 5
     - Socket operation timed out.
   * - ``MSOCKET_NOT_CONNECTED_ERROR``
     - 6
     - Socket is not connected.

API Reference
-------------

.. doxygenfunction:: msocket_error_str

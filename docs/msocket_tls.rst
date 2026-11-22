TLS Configuration (msocket_tls)
===============================

The ``msocket_tls`` module provides TLS and mutual TLS (mTLS) configuration objects used by :cpp:func:`msocket_connect_tls` and :cpp:func:`msocket_server_start_tls` when ``msocket`` is built with ``MSOCKET_ENABLE_TLS``.

Data Types
----------

.. doxygenstruct:: msocket_tls_config_tag
   :members:

API Reference
-------------

Lifecycle Functions
~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_tls_config_create
.. doxygenfunction:: msocket_tls_config_destroy
.. doxygenfunction:: msocket_tls_config_new
.. doxygenfunction:: msocket_tls_config_delete

Certificate and Verification Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: msocket_tls_config_set_ca_cert
.. doxygenfunction:: msocket_tls_config_set_server_cert
.. doxygenfunction:: msocket_tls_config_set_client_cert
.. doxygenfunction:: msocket_tls_config_set_require_client_cert

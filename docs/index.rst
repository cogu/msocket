msocket
=======

**msocket** is a platform-independent, event-driven socket library written in C (C99 and later). It abstracts the details of setting up socket structures directly using the WinSock2 or BSD socket APIs into a clean, callback-driven interface.

It allows applications to register callbacks when lifecycle events occur on a socket (such as connection established, disconnected, or new data received). The ``msocket`` library manages the low-level details of the OS-level socket objects and background worker threads, functioning identically across Linux and Windows:

* **TCP Client and Server**: Stream communication supporting IPv4 and IPv6.
* **UDP Client and Server**: Datagram communication with support for unicast and multicast.
* **UNIX Domain Sockets**: High-performance local inter-process communication on Linux and POSIX platforms.
* **Event-Driven Asynchronous I/O**: Integrated worker threads monitor socket activity and dispatch connect, disconnect, and data callbacks.
* **In-Memory Testing Framework**: Built-in mock socket implementation (``testsocket`` and ``testsocket_spy``) enabling unit testing of network protocols and message parsing without OS network sockets.
* **C++ RAII Wrapper**: Type-safe RAII classes and listener interfaces for modern C++.
* **Cross-Platform**: Native Linux (POSIX) and native Windows (WinSock2) support with clean C99 public headers.

.. toctree::
   :maxdepth: 2
   :hidden:
   :caption: API Reference

   msocket
   msocket_server
   socket_handler
   msocket_error
   testsocket
   testsocket_spy
   msocket_adapter


Components Catalog
==================

Below is a summary of all modules provided by the msocket library:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Module
     - Header
     - Description
   * - :doc:`msocket`
     - ``msocket.h``
     - Client/peer event-driven socket handling TCP, UDP, and UNIX domains
   * - :doc:`msocket_server`
     - ``msocket_server.h``
     - Server connection listener and client socket lifecycle manager
   * - :doc:`socket_handler`
     - ``msocket.h``
     - Unified event callback table (``msocket_handler_t``) and handler contracts
   * - :doc:`msocket_error`
     - ``msocket_error.h``
     - Standardized error codes and error string translation
   * - :doc:`testsocket`
     - ``testsocket.h``
     - In-memory mock socket engine for protocol testing without network access
   * - :doc:`testsocket_spy`
     - ``testsocket_spy.h``
     - Mock socket spy for recording sent data and verifying interactions in unit tests
   * - :doc:`msocket_adapter`
     - ``msocket_adapter.h``
     - Modern C++ RAII socket and server wrappers and listener interfaces

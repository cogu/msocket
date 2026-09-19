![unit tests](https://github.com/cogu/msocket/workflows/unit%20tests/badge.svg)
[![Documentation Status](https://readthedocs.org/projects/msocket/badge/?version=latest)](https://msocket.readthedocs.io/en/latest/?badge=latest)

# msocket

Event-driven socket wrapper for Linux and Windows.

Online documentation and API reference: **[msocket.readthedocs.io](https://msocket.readthedocs.io/)**

## What is it?

**msocket** is a platform-independent, event-driven socket library written in C (C99 and later). It abstracts the tedious work of setting up socket structures directly using the WinSock2 or BSD socket APIs into a clean, callback-driven interface.

It allows applications to register callbacks when lifecycle events occur on a socket (such as connection established, disconnected, or new data received). The `msocket` library manages the low-level details of the OS-level socket objects and background worker threads, functioning identically across Linux and Windows:

* **TCP Client and Server**: Stream communication supporting IPv4 and IPv6.
* **UDP Client and Server**: Datagram communication with support for unicast and multicast.
* **UNIX Domain Sockets**: High-performance local inter-process communication on Linux and POSIX platforms.
* **Event-Driven Asynchronous I/O**: Integrated worker threads monitor socket activity and dispatch connect, disconnect, and data callbacks.
* **In-Memory Testing Framework**: Built-in mock socket implementation (`testsocket` and `testsocket_spy`) enabling unit testing of network protocols and message parsing without OS network sockets.
* **Cross-Platform**: Native Linux (POSIX) and native Windows (WinSock2) support with clean C99 public headers that do not leak OS header baggage.

### Available Components

| Component | Header | Category | Description |
|-----------|--------|----------|-------------|
| `msocket` | `msocket.h` | Core | Client/peer event-driven socket handling TCP, UDP, and UNIX domains |
| `msocket_server` | `msocket_server.h` | Core | Server connection listener and client socket lifecycle manager |
| `msocket_adapter` | `msocket_adapter.h` | C++ | RAII C++ wrapper classes (`Socket`, `TcpSocket`, `TestSocket`, `TcpServer`) and legacy adapter |
| `testsocket` | `testsocket.h` | Testing | In-memory mock socket engine for protocol testing without network access |
| `testsocket_spy` | `testsocket_spy.h` | Testing | Mock socket spy for recording sent data and verifying interactions in unit tests |

## Where is it used?

* [cogu/c-apx](https://github.com/cogu/c-apx)
* [cogu/cpp-apx](https://github.com/cogu/cpp-apx)


## Dependencies

* [cogu/adt](https://github.com/cogu/adt) (v0.3.7 or later)

When building standalone unit tests, clone `adt` and `msocket` side by side:

```bash
cd ~/repo
git clone https://github.com/cogu/adt.git
git clone https://github.com/cogu/msocket.git
cd msocket
```

## Building with CMake

### Using CMake Presets (Clang 18 + Ninja)

```bash
# Run unit tests
cmake --preset clang-test
cmake --build --preset clang-test
ctest --preset clang-test

# Address and Undefined Behavior Sanitizers (ASan + UBSan)
cmake --preset clang-asan
cmake --build --preset clang-asan
ctest --preset clang-asan

# ThreadSanitizer (TSan)
cmake --preset clang-tsan
cmake --build --preset clang-tsan
ctest --preset clang-tsan -V

# Static Analysis
cmake --preset clang-tidy
cmake --build --preset clang-tidy
```

## Examples

To build the example applications, configure CMake with `-DBUILD_EXAMPLES=ON`:

```bash
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build

# Terminal 1: Run Echo Server
./build/examples/echo_server 5000

# Terminal 2: Run Echo Client
./build/examples/echo_client 127.0.0.1 5000
```

### Manual CMake Workflows (Linux and Windows)

For Windows, use a "Native tools command prompt" from your Visual Studio installation. It comes with a cmake binary that by default chooses the appropriate compiler version.

#### Running unit tests

Configure:

```sh
cmake -S . -B build-test -GNinja -DUNIT_TEST=ON
```

Build:

```sh
cmake --build build-test
```

Run test cases:

```sh
ctest --test-dir build-test --output-on-failure
```

#### Building examples

Configure:

```sh
cmake -S . -B build -GNinja -DBUILD_EXAMPLES=ON
```

Build:

```sh
cmake --build build
```

### CMake Options

| CMake Option | Usage | Default | Description |
|---|---|---|---|
| `UNIT_TEST` | `-DUNIT_TEST=ON` | `OFF` | Enables building unit test executable (`msocket_unit`) |
| `BUILD_EXAMPLES` | `-DBUILD_EXAMPLES=ON` | `OFF` | Enables building example executables (`echo_server`, `echo_client`) |
| `MSOCKET_SANITIZERS` | `-DMSOCKET_SANITIZERS="address,undefined"` | `""` | Enables compiler sanitizers (GCC / Clang) |
| `ENABLE_MSVC_ANALYZE` | `-DENABLE_MSVC_ANALYZE=ON` | `OFF` | Enables MSVC static code analysis (`/analyze`) |
| `ADT_DIR` | `-DADT_DIR="/path/to/adt"` | `../adt` | Path to `adt` repository |

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

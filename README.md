# msocket

Event-driven socket wrapper for Linux and Windows.

## What is it?

MSocket is a system library written in C that wraps the tedious work of setting up socket structs directly using the WinSock2 or the BSD socket APIs.

It allows applications to register callbacks when certain events happen on the socket (such as connect, disconnect or new data received).
The msocket library takes care of the low-level details of managing the actual OS-level socket object and works the same way regardless whether
you are running on Linux or Windows (or even Cygwin).

MSocket is primarily used in applications which uses event-driven, message based communication.

### Features

- TCP client and server (TCP/IP v4 and v6).
- UDP client and server.
- UNIX domain socket client and server.
- Native Windows support using WinSock2 (Visual Studio 2012 and newer)
- Native Linux support
- Cygwin support.
- Easy to use adapter for C++.
- Special *testsocket* API used for unit testing,

## Where is it used?

- [cogu/c-apx](https://github.com/cogu/c-apx)
- [cogu/cpp-apx](https://github.com/cogu/cpp-apx)

## Dependencies

None, except for a standard C compiler and OS networking/thread APIs.

## Building with CMake

### Using CMake Presets (Clang 18 + Ninja)

```bash
# Debug build
cmake --preset clang-debug
cmake --build --preset clang-debug

# Release build
cmake --preset clang-release
cmake --build --preset clang-release

# Test build (includes mock testsocket)
cmake --preset clang-test
cmake --build --preset clang-test

# Address and Undefined Behavior Sanitizers (ASan + UBSan)
cmake --preset clang-asan
cmake --build --preset clang-asan

# Thread Sanitizer (TSan)
cmake --preset clang-tsan
cmake --build --preset clang-tsan

# Static Analysis
cmake --preset clang-tidy
cmake --build --preset clang-tidy
```

### Manual CMake Workflows (Linux and Windows)

```bash
cmake -S . -B build
cmake --build build
```

### CMake Options

| CMake Option        | Usage                                     | Description                                           |
|---------------------|-------------------------------------------|-------------------------------------------------------|
| UNIT_TEST           | -DUNIT_TEST=ON                            | Builds mock testsocket library for unit testing       |
| MSOCKET_SANITIZERS  | -DMSOCKET_SANITIZERS="address,undefined"  | Enables sanitizers (e.g. address,undefined or thread) |

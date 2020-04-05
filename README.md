# msocket

Event-driven socket wrapper for Linux and Windows.

## What is it?

Msocket is a systems library written in C that wraps the tedious work of setting up socket structs directly using the WinSock2 or the BSD socket APIs.

It allows applications to register callbacks when certain events happen on the socket (such as connect, disconnect or new data received).
The msocket library takes care of the low-level details of managing the actual OS-level socket object and works the same way regardless on whether
you are running on Linux or Windows (or even Cygwin).

Msocket is primarily used in applications which uses event-driven, message based communication.

### Features

- TCP client and server (TCP/IP v4 and v6).
- UDP client and server.
- UNIX domain socket client and server.
- Native Windows support using WinSock2 (Visual Studio 2012 and newer)
- Native Linux support
- Cygwin support.
- Language bindings for C++.
- Special *testsocket* API used for unit testing,

## Where is it used?

- [cogu/c-apx](https://github.com/cogu/c-apx)

This repo is a submodule of the [cogu/c-apx](https://github.com/cogu/c-apx) (top-level) project.

## Dependencies

- [cogu/adt](https://github.com/cogu/adt)

The unit test project assumes that repos have been cloned (separately) into a common directory as seen below.

- adt
- msocket (this repo)

### Git Example

```bash
cd ~
mkdir repo && cd repo
git clone https://github.com/cogu/adt.git
git clone https://github.com/cogu/msocket.git
cd msocket
```

## Building with CMake

First clone this repo and its dependencies into a common directory (such as ~/repo) as seen above. Alternatively the repos can be submodules of a top-level repo (as seen in [cogu/c-apx](https://github.com/cogu/c-apx)).

### Building on Linux

```bash
mkdir Release && cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Building on Windows

Use a Visual Studio command prompt from the start menu, such as "x64 Native Tools Command Prompt for VS2019".
It conveniently comes pre-installed with a version of CMake that generates Visual Studio projects by default.

```cmd
mkdir VisualStudio && cd VisualStudio
cmake ..
cmake --build . --config Release
```

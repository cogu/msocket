#include "echo_server.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#ifdef _WIN32
static int init_wsa(void);
#endif

int main()
{	
	const std::uint16_t port{ 6800u };
#ifdef _WIN32
	init_wsa();
#endif
	EchoServer server;
	server.start(port);
	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return 0;
}

#ifdef _WIN32
static int init_wsa(void)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	return err;
}
#endif

#include "client_connection.h"

#include <iostream>
#include <string>

#ifdef _WIN32
static int init_wsa(void);
#endif


int main()
{
	const std::string localhost{ "127.0.0.1" };
	const std::uint16_t port{ 6800u };
#ifdef _WIN32
	init_wsa();
#endif
	ClientConnection connection;
	auto success = connection.connect(localhost, port);
	if (success)
	{
		for(;;)
		{
			std::string message;
			std::getline(std::cin, message);
			if (message.size() == 0u)
			{
				break;
			}
			else
			{
				if (!connection.send(message))
				{
					break;
				}
			}
		}
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
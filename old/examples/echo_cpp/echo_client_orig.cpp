/*****************************************************************************
* \file      echo_client.cpp
* \author    Pierre Svärd
* \date      2017-03-05
* \brief     msocket C++ client example
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2017 Pierre Svärd
*
******************************************************************************/


#include "MSocketWrapper.hpp"

#include <functional>
#include <iostream>
#include <atomic>

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std::placeholders;

class EchoClient {

public:

   typedef MSocket::PortNumberT PortNumberT;
   typedef MSocket::BufferDataT BufferDataT;
   typedef MSocket::BufferSizeT BufferSizeT;

   EchoClient() : m_msocket(AF_INET), m_connected(false) {
      MSocket::HandlerT handler;
      handler.connectHandler = std::bind(&EchoClient::connectHandler, this, _1, _2);
      handler.disconnectedHandler = std::bind(&EchoClient::disconnectHandler, this);
      handler.tcpDataHandler = std::bind(&EchoClient::dataHandler, this, _1, _2, _3);
      m_msocket.setHandler(handler);
   }

   bool connect(const std::string& addr, PortNumberT port) {
      return m_msocket.connect(addr, port);
   }

   bool connected() {
      return m_connected;
   }

   bool send(const std::string& message) {
      return m_msocket.send((MSocket::BufferDataT*)message.c_str(), message.size());
   }

protected:

   void connectHandler(const std::string& addr, PortNumberT port) {
      std::cout << "[CLIENT] connected on port " << port << std::endl;
      m_connected = true;
   }

   void disconnectHandler() {
      std::cout << "[CLIENT] disconnected " << std::endl;
      m_connected = false;
   }

   bool dataHandler(const BufferDataT* dataBuf, BufferSizeT dataLen, BufferSizeT& parseLen) {

      parseLen = dataLen; //consume all data
      std::cout << "[CLIENT] got " << dataLen << " bytes of data" << std::endl;

      std::string message((const char*)dataBuf);
      std::cout << "Message: " << message << std::endl;

      return true;
   }

   MSocket m_msocket;
   std::atomic<bool> m_connected;
};


void printUsage(const char* name) {

   fprintf(stderr, "Usage: %s [-p port_number]\n", name);
   fprintf(stderr, ""
         "\t-p\tPort Number\n" \
         "\t-h\tShow this help message\n");
}


int main(int argc, char **argv) {

   char* port_str = NULL;
   int c;
   while ((c = getopt(argc, argv, "p:")) != -1) {
      switch (c) {
      case 'p':
         port_str = optarg;
         break;
      case 'h':
      default:
         printUsage(argv[0]);
         exit(EXIT_FAILURE);
      }
   }

   if(port_str == NULL) {
      printUsage(argv[0]);
      exit(EXIT_FAILURE);
   }

   EchoClient::PortNumberT port = -1;
   char* endptr = NULL;
   long num = strtol(port_str, &endptr, 10);
   if (endptr > port_str) {
      port = (EchoClient::PortNumberT)num;
   } else {
      printUsage(argv[0]);
      exit(EXIT_FAILURE);
   }

   std::string localhost = "127.0.0.1";

   std::cout << "Connecting to " << localhost << " on port " << port << std::endl;

   EchoClient client;
   client.connect(localhost, port);

   int retries = 5;
   while(retries-- && !client.connected()) {
      sleep(1);
   }

   while(client.connected()) {
      sleep(1);
   }

	return 0;
}


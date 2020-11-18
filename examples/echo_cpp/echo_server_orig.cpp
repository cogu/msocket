/*****************************************************************************
* \file      echo_server.cpp
* \author    Pierre Svärd
* \date      2017-03-05
* \brief     msocket C++ server example
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2017 Pierre Svärd
*
******************************************************************************/

#include "MSocketServer.hpp"

#include <functional>
#include <iostream>

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std::placeholders;

class EchoServer {

public:

   typedef MSocket::PortNumberT PortNumberT;
   typedef MSocket::BufferDataT BufferDataT;
   typedef MSocket::BufferSizeT BufferSizeT;

   EchoServer(PortNumberT port) : m_server(AF_INET), m_port(port) {
      MSocket::HandlerT handler;
      handler.tcpAcceptHandler = std::bind(&EchoServer::acceptHandler, this, _1);
      m_server.setHandler(handler);
   }

   void acceptHandler(MSocketPtr msocketPtr) {
      std::cout << "[SERVER] connection accepted" << std::endl;
      m_msocketPtr = msocketPtr;
      MSocket::HandlerT handler;
      handler.tcpDataHandler = std::bind(&EchoServer::dataHandler, this,  _1, _2, _3);
      handler.disconnectedHandler = std::bind(&EchoServer::disconnectedHandler, this);
      m_msocketPtr->setHandler(handler);
      m_msocketPtr->startIO();
   }

   bool dataHandler(const BufferDataT* dataBuf, BufferSizeT dataLen, BufferSizeT& parseLen) {

      parseLen = dataLen; //consume all data
      std::cout << "[SERVER] got " << dataLen << " bytes of data" << std::endl;
      return true;
   }

   void disconnectedHandler() {
      std::cout << "[SERVER] client connection lost" << std::endl;
      m_msocketPtr = MSocketPtr(NULL);
   }

   void start() {
      m_server.start("", 0, m_port);
   }

   void stop() {
      std::cout << "[SERVER] stopping server" << std::endl;
      m_server.cleanupConnection(std::bind(&EchoServer::onCleanup, this));
   }

   bool send(const std::string& message) {
      if(m_msocketPtr.get()) {
         return m_msocketPtr->send((BufferDataT*)message.c_str(), message.size());
      }
      return false;
   }

   void onCleanup() {
      std::cout << "[SERVER] closing open connections" << std::endl;

      m_msocketPtr->close();
   }

protected:
   MSocketServer m_server;
   uint16_t m_port;
   MSocketPtr m_msocketPtr;
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

   EchoServer::PortNumberT port = -1;
   char* endptr = NULL;
   long num = strtol(port_str, &endptr, 10);
   if (endptr > port_str) {
      port = (EchoServer::PortNumberT)num;
   } else {
      printUsage(argv[0]);
      exit(EXIT_FAILURE);
   }

   EchoServer server(port);
   server.start();

   std::cout << "Server started, type \"exit\" to stop" << std::endl;

   std::string inputStr;
   while(true) {

      getline(std::cin, inputStr);

      if(inputStr == "exit") {
         break;
      }

      if(!server.send(inputStr)) {
         std::cout << "NO client connected" << std::endl;
      }
   }

   server.stop();

	return 0;
}


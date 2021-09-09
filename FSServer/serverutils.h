#ifndef __SERVER_UTILS__
#define __SERVER_UTILS__
#pragma once

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 255
#define DEFAULT_PORT "27015"

/* socket relate utility functions */

int socketInit();

addrinfo* resolveAddress(PCSTR address, PCSTR portNumber);

void createSocket(addrinfo* addrSettings, SOCKET& freeSocket);

int bindAndListen(SOCKET& listenSocket, addrinfo* addrSpecs);

int acceptClient(SOCKET& listenSocket, SOCKET& freeClientSocket);

int TCPsend(SOCKET& connectionSocket, const char* data, size_t dataSize);
int TCPreceive(SOCKET& connectionSocket, char** dump);



/* general ones */
std::string readConfigFile(const char* filepath);


#endif

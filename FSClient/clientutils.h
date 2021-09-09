#ifndef __CLIENT_UTILS__
#define __CLIENT_UTILS__
#pragma once

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <stdio.h>


// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 1024
#define DEFAULT_HEADER_SIZE 9
#define SLIDE_WINDOW_SIZE 3
#define DEFAULT_PORT "27015"

/* socket relate utility functions */

struct ConfigInfo {

	std::string configString;
	std::string shareDirectory;
	std::string receiveDirectory;
	PCSTR serverPort;
	PCSTR serverHostname;
	PCSTR clientPort;
	PCSTR clientHostname;


};



int socketInit();

addrinfo* resolveAddress(PCSTR address, PCSTR portNumber, bool isTCP);

void createSocket(addrinfo* addrSettings, SOCKET& freeSocket);

int bindUDP(SOCKET& listenSocket, addrinfo* addrSpecs);

int acceptClient(SOCKET& listenSocket, SOCKET& freeClientSocket);



// client specific 

int connectSocket(addrinfo* addrSettings, SOCKET& freeSocket, bool exhaustive);


/* sending and receiving */

int TCPsend(SOCKET& connectionSocket, const char* data, size_t dataSize);

int TCPreceive(SOCKET& connectionSocket, char** dump);

int UDPsend(SOCKET& sendSocket, SOCKET& recvSocket, addrinfo* targetAddrInfo, std::ifstream& file, unsigned packetIDStart);

long UDPreceive(SOCKET& sendSocket, SOCKET& recvSocket, addrinfo* targetAddrInfo, const std::string& saveFilePath, unsigned packetIDStart);

int executeTransfer(SOCKET& sendSocket, SOCKET& recvSocket, const char* transferInfo, const std::string& folderPath);

int executeDownload(SOCKET& sendSocket, SOCKET& recvSocket, const char* downloadInfo, const std::string& folderPath);

void flushSocketBacklog(SOCKET& recvSocket, unsigned numSeconds);

/* general ones */

std::string readConfigFile(const char* filepath);

void loadConfig(ConfigInfo* configInfo, std::string& configStr);

std::string scanFolder(const char* folderName);

#endif





// finishing receiving
// figure out what command is this 
// forward info to correct command
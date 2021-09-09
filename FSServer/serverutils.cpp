#include "serverutils.h"
#include <fstream>
#include <sstream>
#include <iostream>

int socketInit() {

	WSADATA wsaData;
	int startResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	return startResult;
}

addrinfo* resolveAddress(PCSTR address, PCSTR portNumber) {

	struct addrinfo* resolution = NULL;
	struct addrinfo guidelines;

	ZeroMemory(&guidelines, sizeof(guidelines));
	guidelines.ai_family = AF_INET;
	guidelines.ai_socktype = SOCK_STREAM;
	guidelines.ai_protocol = IPPROTO_TCP;
	guidelines.ai_flags = AI_PASSIVE;

	int iResult = getaddrinfo(address, portNumber, &guidelines, &resolution);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return nullptr;
	}

	return resolution;
}

void createSocket(addrinfo* addrSettings, SOCKET& freeSocket) {

	freeSocket = socket(addrSettings->ai_family, addrSettings->ai_socktype, addrSettings->ai_protocol);
}

int bindAndListen(SOCKET& listenSocket, addrinfo* addrSpecs) {

	int iError = bind(listenSocket, addrSpecs->ai_addr, (int)addrSpecs->ai_addrlen);
	if (iError == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(addrSpecs);
		closesocket(listenSocket);
		WSACleanup();
		return iError;
	}

	iError = listen(listenSocket, SOMAXCONN);
	if (iError == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return iError;
	}
	return 0;
}

int acceptClient(SOCKET& listenSocket, SOCKET& freeClientSocket) {

	freeClientSocket = accept(listenSocket, NULL, NULL);
	if (freeClientSocket == INVALID_SOCKET) {
		//printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}
	return 0;
}



std::string readConfigFile(const char* filepath) {

	std::ostringstream buffer;
	std::ifstream configFile(filepath);
	buffer << configFile.rdbuf();
	return buffer.str();
}

int TCPsend(SOCKET& connectionSocket, const char* data, size_t dataSize) {


	int iResult = send(connectionSocket, data, (int)dataSize, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(connectionSocket);
		WSACleanup();
		return SOCKET_ERROR;
	}
	return 0;
}

/**/
int TCPreceive(SOCKET& connectionSocket, char** dump) {

	char buffer[DEFAULT_BUFLEN];
	int totalMsgLength = 0;
	int packetSize = 0;
	char* prevData;
	char* currData = (char*)malloc(10);

	do {
		prevData = currData;
		packetSize = recv(connectionSocket, buffer, DEFAULT_BUFLEN, 0);

		if (packetSize > 0) {
			currData = (char*)malloc((long)(totalMsgLength + packetSize));
			memcpy(currData, prevData, totalMsgLength);
			memcpy((currData + totalMsgLength), buffer, packetSize);
			totalMsgLength += packetSize;
			free(prevData);
			prevData = nullptr;
		}
		else if (packetSize == 0) {
			printf("Connection closed\n");
			free(currData);
			return -1;
		}
		else {
			free(currData);
			return -1;
		}

		if (currData[totalMsgLength - 1] == '\0') {
			break;
		}
		ZeroMemory(buffer, DEFAULT_BUFLEN);

	} while (true);

	*dump = currData;

	return totalMsgLength;
}





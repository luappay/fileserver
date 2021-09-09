
#ifndef __SERVER__
#define __SERVER__
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <list>
#include <mutex>
#include <string>
#include <iostream>




struct ClientInfo {

	std::string fileList; // add mutex lock
	std::thread* threadPointer;
	SOCKET* socketPointer;
	PCSTR clientAddress;
	PCSTR clientPort;
	bool isDisconnected;
	bool isBusy;

	ClientInfo(SOCKET* clientSocket)
		: socketPointer(clientSocket), threadPointer(nullptr),
		clientAddress(NULL), clientPort(NULL),
		isDisconnected(false), isBusy(false) {}

	~ClientInfo() {
		delete socketPointer;
		socketPointer = nullptr;
		delete threadPointer;
		threadPointer = nullptr;
		free((void*)clientAddress);
		free((void*)clientPort);
	}
};



class Server {

	//struct ClientInfo;

private:


	std::list<ClientInfo*> clientList;
	SOCKET listenSocket;
	addrinfo* addrSpecs;
	PCSTR serverAddrs;
	PCSTR serverPort;

	int cleanupCounter;
	bool isQuit;

public:

	Server(PCSTR address, PCSTR portNumber);
	~Server();

	int serverStart();
	void deployManagers();

	void adminManager();
	void serverList();
	int serverQuit();


	void clientManager();
	void manageSingleClient(SOCKET* clientSocket, ClientInfo* clientInfo);
	int serveClient(SOCKET& clientSocket, ClientInfo* clientInfo);

	void clientCleanupManager();
	void closeAllClients();
	void joinAllThreads();

	int sendFileList(SOCKET& clientSocket);
	void loadClientAddrs(ClientInfo* clientInfo, char* data);


	void liaiseTransfer(ClientInfo* fileRequester, ClientInfo* fileOwner, char* filename, int payloadSize);
	ClientInfo* searchFileOwner(const char* filename, int payloadSize);


};
#endif

#ifndef __CLIENT__
#define __CLIENT__
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <list>

#include "clientutils.h"


class Client {

private:

	ConfigInfo config;
	std::string fileList;
	//std::mutex serverCommsLock;
	addrinfo* serverAddrSpecs;
	addrinfo* clientAddrSpecs;
	SOCKET recvSocket;
	SOCKET sendSocket;
	SOCKET serverSocket;
	bool isQuit;
	bool serverConnected;

public:

	Client(ConfigInfo configInput, std::string _fileList);
	~Client();

	int clientStart();
	int connectToServer();
	void deployManagers();


	void adminManager();
	void clientList();
	void clientGetFile(const char* filename);
	void clientResume(const char* filename);
	int clientQuit();


	void requestManager(); // receive request from server and performs it

};




#endif
#pragma once
#include "client.h"
#include "clientutils.h"
#include <chrono>
#include <thread>


Client::Client(ConfigInfo configInput, std::string _fileList)
	: config(configInput), fileList(_fileList), serverAddrSpecs(nullptr), clientAddrSpecs(nullptr),
	recvSocket(INVALID_SOCKET), sendSocket(INVALID_SOCKET), serverSocket(INVALID_SOCKET)
	, isQuit(false), serverConnected(false)
{}

Client::~Client() {
	freeaddrinfo(clientAddrSpecs);
	freeaddrinfo(serverAddrSpecs);
	closesocket(recvSocket);
	closesocket(serverSocket);
	WSACleanup();

}

int Client::clientStart() {

	int iError = socketInit();
	if (iError) {
		printf("WSAStartup failed with error: %d\n", iError);
		return 1;
	}

	// setup UDP listening port for client
	clientAddrSpecs = resolveAddress(config.clientHostname, config.clientPort, false);
	if (clientAddrSpecs == nullptr) {
		return 1;
	}

	createSocket(clientAddrSpecs, recvSocket);
	if (recvSocket == INVALID_SOCKET) {
		printf("socket (creation) failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(clientAddrSpecs);
		WSACleanup();
		return 1;
	}

	createSocket(clientAddrSpecs, sendSocket);
	if (sendSocket == INVALID_SOCKET) {
		printf("socket (creation) failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(clientAddrSpecs);
		WSACleanup();
		return 1;
	}

	iError = bindUDP(recvSocket, clientAddrSpecs);
	if (iError) {
		return 1;
	}

	unsigned long iMode = 1;
	iError = ioctlsocket(recvSocket, FIONBIO, &iMode);
	if (iError != NO_ERROR) {
		printf("ioctlsocket failed with error: %ld\n", iError);
	}

	return 0;
}

int Client::connectToServer() {


	// resolve address to server
	serverAddrSpecs = resolveAddress(config.serverHostname, config.serverPort, true);
	if (serverAddrSpecs == nullptr) {
		return 1;
	}

	int iError = connectSocket(serverAddrSpecs, serverSocket, true);
	if (iError) {
		printf("Unable to connect to server\n");
		return 1;
	}

	std::string fileListData = "\x02";
	fileListData += fileList;
	fileListData += "\0";

	iError = TCPsend(serverSocket, fileListData.c_str(), fileListData.size() + 1);
	if (iError == SOCKET_ERROR) {
		printf("Failed to send filelist to server\n");
		return 1;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));


	std::string connectionInfo = "\x03";
	connectionInfo += config.clientHostname;
	connectionInfo += "\n";
	connectionInfo += config.clientPort;
	connectionInfo += "\n";
	connectionInfo += "\0";

	iError = TCPsend(serverSocket, connectionInfo.c_str(), connectionInfo.size() + 1);
	if (iError == SOCKET_ERROR) {
		printf("Failed to send connection to server\n");
		return 1;
	}
	serverConnected = true;
	std::cout << "Connected to Server!" << std::endl;

	return 0;
}


void Client::deployManagers() {

	std::thread adminThread(&Client::adminManager, this);
	std::thread requestThread(&Client::requestManager, this);

	adminThread.join();
	requestThread.join();
}

void Client::adminManager() {

	std::string command;

	while (isQuit == false) {
		std::getline(std::cin, command);
		//std::cin >> command;

		if (command == "list") {
			this->clientList();
		}
		else if (command == "quit") {
			this->clientQuit();
			closesocket(recvSocket);
			closesocket(serverSocket);
			break;
		}
		else if (command.find("resume") != -1) {
			int filenameStart = command.find(" ") + 1;
			std::string filename = command.substr(filenameStart, command.size() - 1);
			clientResume(filename.c_str());
		}
		else if (command.find("get") != -1) {
			int filenameStart = command.find(" ") + 1;
			std::string filename = command.substr(filenameStart, command.size() - 1);
			clientGetFile(filename.c_str());
		}
		else {
			continue;
		}
	}
}


void Client::clientList() {

	if (serverConnected == false) {
		printf("Not connected to server\n");
		return;
	}

	std::string listRequest = "\x01\0";
	//serverCommsLock.lock();
	int sendResult = TCPsend(serverSocket, listRequest.c_str(), 2);
	//serverCommsLock.unlock();
}

void Client::clientGetFile(const char* filename) {

	if (serverConnected == false) {
		printf("Not connected to server\n");
		return;
	}
	std::string getRequest = "\x04";													// set command byte
	getRequest += filename;																// append filename in

	char* fullRequest = (char*)malloc(getRequest.size() + 5);							// malloc space for fullRequest, which includes <command byte>,<filename>,<packetStartID>
	ZeroMemory(fullRequest, getRequest.size() + 5);										// set fullRequest buffer to 0
	memcpy(fullRequest, getRequest.c_str(), getRequest.size());							// write getRequest into fullRequest buffer

	//serverCommsLock.lock();
	int sendResult = TCPsend(serverSocket, fullRequest, getRequest.size() + 5);			// send request to Server
	//serverCommsLock.unlock();

	if (sendResult == SOCKET_ERROR) {
		printf("Failed to send get file request to server\n");
		return;
	}

	free(fullRequest);																	// free the malloc
}

void Client::clientResume(const char* filename) {

	std::string filePath = config.receiveDirectory;												// create filePath to .temp file
	filePath += "/";
	filePath += filename;

	// remove last saved config in .temp file
	std::ifstream inputFile(filePath.c_str(), std::ifstream::ate | std::ifstream::binary);		//	- open .temp file and seek to end
	size_t fileSize = inputFile.tellg();														//	- get fileSize
	inputFile.seekg(0, inputFile.beg);															//	- seek file back to start

	char* tempBuffer = (char*)malloc((fileSize - 200));											//	- malloc buffer for file rewrite
	inputFile.read(tempBuffer, (fileSize - 200));												//	- copy .temp file content in without temp configs 
																								// extract currPackedID information from .temp file for resuming download
	unsigned* currPacketID = (unsigned*)malloc(4);												//	- malloc memory for currPacketID
	inputFile.seekg(fileSize - 200, inputFile.beg);												//	- seek to temp config portion
	inputFile.read((char*)currPacketID, 4);														//	- read first 4 bytes info unsigned, "currPacketID"
	inputFile.close();																			// close file 
																								// create a copy of .temp file with temp configs 
	std::ofstream outFile(filePath.c_str(), std::ofstream::binary);								//	- open new output stream file
	outFile.write(tempBuffer, (fileSize - 200));												//	- write tempBuffer into it
	outFile.close();																			//	- close and save output file

	free(tempBuffer);																			// free tempBuffer
																								// construct getRequest 
	std::string tempFilename = "\x04";															//	- set command byte
	tempFilename += filename;																	//	- write filename
	char* getRequest = (char*)malloc(tempFilename.size());										//	- malloc getRequest to write everything in
	ZeroMemory(getRequest, tempFilename.size());												//		- zero the memory
	memcpy(getRequest, tempFilename.c_str(), tempFilename.size() - 5);							//		- mem copy the <command byte> and <filename> portaion
	memcpy((getRequest + (tempFilename.size() - 5)), (char*)currPacketID, 4);					//		- mem copy in the currPacketID
	getRequest[tempFilename.size() - 1] = 0;													//		- null terminate the buffer


	int sendResult = TCPsend(serverSocket, getRequest, tempFilename.size());					// send request to Server
	if (sendResult == SOCKET_ERROR) {
		printf("Failed to send get file request to server\n");
		return;
	}

	free(getRequest);																			// free getRequest buffer

}

int Client::clientQuit() {

	isQuit = true;
	// more quitting managements 
	return 0;
}



void Client::requestManager() {

	int iResult = 0;
	char** dataIn = (char**)malloc(sizeof(char*));
	if (dataIn == nullptr) {
		printf("Malloc fail...\n");
		closesocket(serverSocket);
		WSACleanup();
	}

	char directive;

	do {

		iResult = TCPreceive(serverSocket, dataIn);
		//std::cout << "\niResult size :" << iResult << std::endl;
		if (iResult > 0) {
			directive = (*dataIn)[0];
			char* payload = &((*dataIn)[1]);

			switch (directive) {

			case 1:
				// client does not support receving x01 directive
				break;
			case 2:
				// other party sending filelist
				std::cout << "\nListing files from Server :\n" << payload << std::endl;
				break;
			case 3:
				// client does not support receving x03 directive
				break;
			case 4:
				// client does not support receving x04 directive
				break;
			case 5:
				// receive file from address
				executeDownload(sendSocket, recvSocket, payload, config.receiveDirectory);
				TCPsend(serverSocket, "\x05", 2);
				break;
			case 6:
				// send file to address
				executeTransfer(sendSocket, recvSocket, payload, config.shareDirectory);
				TCPsend(serverSocket, "\x06", 2);
				break;
			default:
				break;
			}
			free(*dataIn);
		}
		else {
			std::cout << "Server disconnected..." << std::endl;
			closesocket(serverSocket);
			free(dataIn);
			//WSACleanup();
			return;
		}
	} while ((iResult > 0)& (isQuit == false));
	std::cout << "Request Manager shut down as Server disconnected." << std::endl;
	return;
}
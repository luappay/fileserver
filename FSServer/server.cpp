#include "server.h"
#include "serverutils.h"
#include <thread>
#include <chrono>

std::mutex clientListLock;
std::mutex cleanupCounterLock;

Server::Server(PCSTR Address, PCSTR portNumber)
	: serverAddrs(Address), serverPort(portNumber), addrSpecs(nullptr),
	listenSocket(INVALID_SOCKET), cleanupCounter(0), isQuit(false)
{}

Server::~Server() {
	freeaddrinfo(addrSpecs);
	closesocket(listenSocket);
}

int Server::serverStart() {

	int iError = socketInit();
	if (iError) {
		printf("WSAStartup failed with error: %d\n", iError);
		return 1;
	}

	//addrSpecs = resolveAddress(NULL, DEFAULT_PORT);
	addrSpecs = resolveAddress(serverAddrs, serverPort);
	if (addrSpecs == nullptr) {
		return 1;
	}

	createSocket(addrSpecs, listenSocket);
	if (listenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(addrSpecs);
		WSACleanup();
		return 1;
	}

	iError = bindAndListen(listenSocket, addrSpecs);
	if (iError) {
		return 1;
	}
	return 0;
}


void Server::deployManagers() {

	std::thread adminThread(&Server::adminManager, this);
	std::thread clientThread(&Server::clientManager, this);
	std::thread clientCleanupThread(&Server::clientCleanupManager, this);

	adminThread.join();
	clientThread.join();
	clientCleanupThread.join();
}




void Server::adminManager() {

	std::string command;

	while (true) {
		std::cin >> command;

		if (command == "list") {
			this->serverList();
		}
		else if (command == "quit") {
			this->serverQuit();
			closesocket(listenSocket);
			break;
		}
		else {
			continue;
		}
	}
}

void Server::serverList() {

	ClientInfo* currClient;

	clientListLock.lock();

	std::list<ClientInfo*>::iterator clientIter = clientList.begin();
	std::cout << "\nListing files...\n";

	while (clientIter != clientList.end()) {
		currClient = *clientIter;
		std::cout << "Belonging to Client ID: " << currClient << std::endl;
		std::cout << currClient->fileList.c_str() << std::endl;
		++clientIter;
	}
	clientListLock.unlock();
}

int Server::serverQuit() {

	isQuit = true;
	// wait for all threads to quit
	return 0;
}


void Server::clientManager() {

	int count = 0;
	while (isQuit == false) {
		SOCKET* clientSocket = new SOCKET;
		*clientSocket = INVALID_SOCKET;
		ClientInfo* clientInfo = new ClientInfo(clientSocket);


		int iError = acceptClient(listenSocket, *clientSocket);
		if (iError) {
			delete clientInfo;
			continue;
		}
		std::thread* newThread = new std::thread(&Server::manageSingleClient, this, clientSocket, clientInfo);
		clientInfo->threadPointer = newThread;
		std::cout << "New Client with ID = " << clientInfo << " connected!" << std::endl;

		clientListLock.lock();
		clientList.push_back(clientInfo);
		clientListLock.unlock();

		++count;
	}

	closeAllClients();
}

void Server::manageSingleClient(SOCKET* clientSocket, ClientInfo* clientInfo) {

	int isConnectionClosed = 0;
	while ((isQuit == false) & (isConnectionClosed == 0)) {
		isConnectionClosed = serveClient(*clientSocket, clientInfo); // pass in mutex to serverClient also so that every action can be locked
	}

	if (isConnectionClosed) {
		clientInfo->isDisconnected = true;
		cleanupCounterLock.lock();
		++cleanupCounter;
		cleanupCounterLock.unlock();
	}
}


int Server::serveClient(SOCKET& clientSocket, ClientInfo* clientInfo) {

	int iResult = 0;
	char** dataIn = (char**)malloc(sizeof(char*));
	if (dataIn == nullptr) {
		printf("Malloc fail...\n");
		closesocket(clientSocket);
		WSACleanup();
	}

	char clientRequest;
	do {

		iResult = TCPreceive(clientSocket, dataIn);

		if (iResult > 0) {
			clientRequest = (*dataIn)[0];
			char* payload = &((*dataIn)[1]);
			ClientInfo* fileOwner;

			switch (clientRequest) {

			case 1:
				// request filelist
				sendFileList(clientSocket);
				break;
			case 2:
				// other party sending filelist
				clientListLock.lock();
				clientInfo->fileList += payload;
				clientListLock.unlock();
				break;
			case 3:
				// other party sending Address and port
				clientListLock.lock();
				loadClientAddrs(clientInfo, &((*dataIn)[1]));
				clientListLock.unlock();
				break;
			case 4:
				// get filename
				fileOwner = searchFileOwner(payload, iResult - 1);

				clientListLock.lock();
				liaiseTransfer(clientInfo, fileOwner, payload, iResult - 1);
				clientListLock.unlock();
				break;
			case 5:
				// file owner n info
				clientListLock.lock();
				clientInfo->isBusy = false;
				clientListLock.unlock();
				break;
			case 6:
				// file requester info
				clientListLock.lock();
				clientInfo->isBusy = false;
				clientListLock.unlock();
				break;
			default:
				break;
			}
			free(*dataIn);
		}
		else {
			std::cout << "Connection dropped with client ID : " << clientInfo << std::endl;
			closesocket(clientSocket);
			free(dataIn);
			return 1;
		}

	} while ((iResult > 0)& (isQuit == false));
	return 0;
}

void Server::clientCleanupManager() {

	ClientInfo* currClient;
	std::list<ClientInfo*>::iterator clientIter;

	while (isQuit == false) {

		if (!cleanupCounter) {
			continue;
		}

		clientListLock.lock();
		clientIter = clientList.begin();
		while (clientIter != clientList.end()) {

			currClient = *clientIter;
			++clientIter;
			if (currClient->isDisconnected) {
				currClient->threadPointer->join();
				clientList.remove(currClient);
				delete currClient;
				cleanupCounterLock.lock();
				--cleanupCounter;
				cleanupCounterLock.unlock();
			}
		}
		clientListLock.unlock();
	}
}

void Server::closeAllClients() {

	ClientInfo* currClient;

	clientListLock.lock();
	std::list<ClientInfo*>::iterator clientIter = clientList.begin();
	while (clientIter != clientList.end()) {
		currClient = *clientIter;
		closesocket((*(currClient->socketPointer)));
		++clientIter;
	}
	clientListLock.unlock();
}

void Server::joinAllThreads() {

	ClientInfo* currClient;
	std::list<ClientInfo*>::iterator clientIter = clientList.begin();
	while (clientIter != clientList.end()) {
		currClient = *clientIter;
		if (currClient->threadPointer != nullptr) {
			currClient->threadPointer->join();
			std::cout << "closing" << std::endl;
		}
		++clientIter;
	}
}


int Server::sendFileList(SOCKET& clientSocket) {

	ClientInfo* currClient;
	std::string sendBuffer = "\x02";

	clientListLock.lock();
	std::list<ClientInfo*>::iterator clientIter = clientList.begin();
	while (clientIter != clientList.end()) {
		currClient = *clientIter;
		if (currClient->socketPointer != &clientSocket) {
			sendBuffer += currClient->fileList;
		}
		++clientIter;
	}
	clientListLock.unlock();

	sendBuffer += '\0';
	TCPsend(clientSocket, sendBuffer.c_str(), sendBuffer.size() + 1);
	return 0;
}

void Server::loadClientAddrs(ClientInfo* clientInfo, char* data) {

	int i = 0;
	while (data[i] != '\n') {
		++i;
	}
	data[i] = '\0';
	++i;
	void* clientAddrs = malloc(i);
	memcpy(clientAddrs, &(data[0]), i);
	clientInfo->clientAddress = (PCSTR)clientAddrs;
	//std::cout << "Loaded client address : " << clientAddrs << std::endl;
	//std::cout << "in struct is : " << clientInfo->clientAddress << std::endl;

	int portInfoStart = i;
	while (data[i] != '\n') {
		++i;
	}
	data[i] = '\0';
	++i;
	void* clientPort = malloc(i - portInfoStart);
	memcpy(clientPort, &(data[portInfoStart]), i - portInfoStart);
	clientInfo->clientPort = (PCSTR)clientPort;

}


void Server::liaiseTransfer(ClientInfo* fileRequester, ClientInfo* fileOwner, char* filename, int payloadSize) {

	if (fileOwner == nullptr) {
		std::string errorMsg = "\x02";
		errorMsg += "File requested does not exist\n";
		TCPsend((*(fileRequester->socketPointer)), errorMsg.c_str(), errorMsg.size() + 1);
		return;
	}
	if ((fileRequester->isBusy) | (fileOwner->isBusy)) {
		std::string errorMsg = "\x02";
		errorMsg += "Client(s) are busy, please try again later.\n";
		TCPsend((*(fileRequester->socketPointer)), errorMsg.c_str(), errorMsg.size() + 1);
		return;
	}

	unsigned* currPacketID = (unsigned*)(filename + (payloadSize - 5));

	std::string requesterMsg = "\x05";
	std::string ownerMsg = "\x06";

	if ((*currPacketID)) {
		requesterMsg.append(filename, payloadSize - 5);
		ownerMsg.append(filename, payloadSize - 5);
	}
	else {
		requesterMsg += filename;
		ownerMsg += filename;
	}

	requesterMsg += "\n";
	requesterMsg += fileOwner->clientAddress;
	requesterMsg += "\n";
	requesterMsg += fileOwner->clientPort;
	requesterMsg += "\n";
	char* fullRequesterMsg = (char*)malloc(requesterMsg.size() + 4);
	ZeroMemory(fullRequesterMsg, requesterMsg.size() + 4);
	memcpy(fullRequesterMsg, requesterMsg.c_str(), requesterMsg.size());
	memcpy(fullRequesterMsg + requesterMsg.size(), (char*)currPacketID, 4);



	ownerMsg += "\n";
	ownerMsg += fileRequester->clientAddress;
	ownerMsg += "\n";
	ownerMsg += fileRequester->clientPort;
	ownerMsg += "\n";
	char* fullOwnerMsg = (char*)malloc(ownerMsg.size() + 4);
	ZeroMemory(fullOwnerMsg, ownerMsg.size() + 4);
	memcpy(fullOwnerMsg, ownerMsg.c_str(), ownerMsg.size());
	memcpy(fullOwnerMsg + ownerMsg.size(), (char*)currPacketID, 4);

	TCPsend(*(fileRequester->socketPointer), fullRequesterMsg, requesterMsg.size() + 4);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	TCPsend(*(fileOwner->socketPointer), fullOwnerMsg, ownerMsg.size() + 4);

	free(fullRequesterMsg);
	free(fullOwnerMsg);
	fileRequester->isBusy = true;
	fileOwner->isBusy = true;

}


ClientInfo* Server::searchFileOwner(const char* filename, int payloadSize) {

	std::string fileExact;

	fileExact.append(filename, payloadSize - 5);
	fileExact += "\n";

	ClientInfo* currClient;
	clientListLock.lock();
	std::list<ClientInfo*>::iterator clientIter = clientList.begin();
	while (clientIter != clientList.end()) {
		currClient = *clientIter;

		if (currClient->fileList.find(fileExact.c_str()) != std::string::npos) {
			clientListLock.unlock();
			return currClient;
		}
		++clientIter;
	}
	clientListLock.unlock();
	return nullptr;
}
#pragma once
#include "clientutils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include "dirent.h"
#include <algorithm>


int socketInit() {

	WSADATA wsaData;
	int startResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	return startResult;
}

/**/
addrinfo* resolveAddress(PCSTR address, PCSTR portNumber, bool isTCP) {

	struct addrinfo* resolution = NULL;
	struct addrinfo guidelines;

	ZeroMemory(&guidelines, sizeof(guidelines));
	guidelines.ai_family = AF_INET;
	guidelines.ai_socktype = SOCK_DGRAM;
	guidelines.ai_protocol = IPPROTO_UDP;
	if (isTCP) {
		guidelines.ai_socktype = SOCK_STREAM;
		guidelines.ai_protocol = IPPROTO_TCP;
	}


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
	//	if (listenSocket == INVALID_SOCKET) {
	//		printf("socket failed with error: %ld\n", WSAGetLastError());
	//		freeaddrinfo(addrSettings);
	//		WSACleanup();
	//		return nullptr;
}

int bindUDP(SOCKET& bindSocket, addrinfo* addrSpecs) {

	int iError = bind(bindSocket, addrSpecs->ai_addr, (int)addrSpecs->ai_addrlen);
	if (iError == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(addrSpecs);
		closesocket(bindSocket);
		WSACleanup();
		return iError;
	}
	return 0;
}

int acceptClient(SOCKET& listenSocket, SOCKET& freeClientSocket) {

	freeClientSocket = accept(listenSocket, NULL, NULL);
	if (freeClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}
	return 0;
}



/**/
int connectSocket(addrinfo* addrSettings, SOCKET& freeSocket, bool exhaustive) {

	if (addrSettings == nullptr) {
		return 1;
	}

	createSocket(addrSettings, freeSocket);
	if (freeSocket == INVALID_SOCKET) {
		printf("socket (creation) failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	int iError = connect(freeSocket, addrSettings->ai_addr, (int)addrSettings->ai_addrlen);
	if (iError == SOCKET_ERROR) {
		closesocket(freeSocket);
		freeSocket = INVALID_SOCKET;
		if (exhaustive) {
			return connectSocket(addrSettings->ai_next, freeSocket, true);
		}
	}
	return 0;
}

/**/
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
			currData = (char*)malloc((size_t)(totalMsgLength + packetSize));
			memcpy(currData, prevData, totalMsgLength);
			memcpy((currData + totalMsgLength), buffer, packetSize);
			totalMsgLength += packetSize;
			free(prevData);
			prevData = nullptr;
		}
		else if (packetSize == 0) {
			free(currData);
			continue;
		}
		else {
			free(currData);
			return -1;
		}
		if (currData[totalMsgLength - 1] == '\0') {
			break;
		}
		packetSize = 0;

	} while (true);

	*dump = currData;
	return totalMsgLength;
}


int UDPsend(SOCKET& sendSocket, SOCKET& recvSocket, addrinfo* targetAddrInfo, std::ifstream& file, unsigned packetIDStart) {

																													// Initialize variables for progress bar use
	char progressBar[] = ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";									//	- initialize forward progress string
	char padBar[] = "                                                      ";										//	- initialize blank string to pad progress bar
	unsigned padNum = 54;																							//	- pad number to be used for progress bar calculation later on
																													// Initialize booleans for flow control
	bool transferCompleted = false;																					//	- initialize transferCompleted boolean to signify that Receiving/Target Client have acknowledge receipt of all packets

	file.seekg(0, file.end);																						// seek file to end
	size_t fileSize = file.tellg();																					// get file size

																													// initialize packer size variable
	unsigned i;																										// initialize i for transversing
	int payloadLen = DEFAULT_BUFLEN - DEFAULT_HEADER_SIZE;															// define payload length with is default buffer length minus header or 5 btyes
	sockaddr* targetSockAddr = targetAddrInfo->ai_addr;
	char* targetAddrData = (char*)targetSockAddr->sa_data;															
	unsigned* targetIP = (unsigned*)(targetAddrData + 2);
																													// Prepare all variables needed to receive packets from the Receving Client
	int packetSize;																									//	- initialize receive packet size
	sockaddr recvSockAddr;																							//	- initialize sockaddr to store address information of packet Receiver
	int recvSockSize = sizeof(recvSockAddr);																		//	- get size of sockaddr
	char recvBuffer[DEFAULT_BUFLEN] = { 0 };																		//	- initialize buffer to receive packet and set buffer to 0 (or Null)
	unsigned* recvPacketID = (unsigned*)(recvBuffer + 1);															//	- get the pointer to the segment/region in the buffer to start writing the payload in 
	char* recvAddrData;																								//	- initialize variable to pointer to "data" segment/region in the sockaddr struct for later use 
	unsigned* recvIP;																								//	- initialize variable to pointer to the "IP Address" portion of the "data" segment/region in the sockaddr struct for later use
	
																													// Prepare all variables needed to send packets to the Receving Client
	char directive;																									//	- initialize variable to store directive/command byte
	char sendBuffer[DEFAULT_BUFLEN] = { 0 };																		//	- initialize buffer to be used for sending packets and set them to 0s
	int sendBufferLen;																								//	- initialize variable tto record send buffer's length
	sendBuffer[0] = 6;																								//	- set the directive/command byte of the send buffer to "\x06"
	unsigned* totalPackets = (unsigned*)(sendBuffer + 1);															//	- initialize variable to the pointer to the portion of the segment/region of the buffer where the total number of packets sending information will be written in
	unsigned* packetID = (unsigned*)(sendBuffer + 5);																//	- initialize variable to the pointer to the portion of the segment/region of the buffer where the current packet ID information will be written in
	char* payload = sendBuffer + DEFAULT_HEADER_SIZE;																//	- initialize variable to the pointer to the portion of the segment/region of the buffer where the payload will be written in 



	unsigned numPackets = (fileSize / payloadLen);
	if (fileSize % payloadLen) {
		++numPackets;
	}
	(*totalPackets) = numPackets;


	unsigned currWindowStart = packetIDStart;
	unsigned nextSlide = currWindowStart + SLIDE_WINDOW_SIZE;
	auto lastCorrespond = std::chrono::system_clock::now();
	auto timeSinceLastCorrespond = std::chrono::system_clock::now();
	std::chrono::duration<double> coldTurkeyTime = timeSinceLastCorrespond - lastCorrespond;
	while ((sendSocket != INVALID_SOCKET) | (recvSocket != INVALID_SOCKET)) {										// While both sending and receiving Sockets are not closed 
		if (currWindowStart == (numPackets - 1)) {																	//	- If currWindowStart is numPackets stop data, finished sending send end directive instead
			sendto(sendSocket, "\x08\0", 2, 0, targetSockAddr, sizeof((*targetSockAddr)));							//		- send end of packet directive 
		}
		else {																										//	- Else bulk send packets
			for (i = min(currWindowStart, numPackets - 1); i < min(currWindowStart + SLIDE_WINDOW_SIZE, numPackets); i = min(i + 1, numPackets)) {

				ZeroMemory(sendBuffer + 5, DEFAULT_BUFLEN - 5);														//		- Zero the volatile part of the sendBuffer
				(*packetID) = i;																					//		- set the packetID of the current packet/data
				file.seekg((i * payloadLen), file.beg);																//		- seek to the current segment of the file that corresponds to the current packet

				if (i == (numPackets - 1)) {																		//		- If i is the last packetID, note: ID is numPacket - 1
					file.read(payload, (fileSize % payloadLen));													//			- copy (to sendBuffer, payload) to the length of the remaining segment of the file 
					sendBufferLen = (fileSize % payloadLen) + DEFAULT_HEADER_SIZE;									//			- get the sendBufferLen 
				}
				else {																								//		- Else
					file.read(payload, payloadLen);																	//			- copy (to sendBuffer, payload) to the length of the buffer length- header size
					sendBufferLen = DEFAULT_BUFLEN;																	//			- get the sendBufferLen
				}

				sendto(sendSocket, sendBuffer, sendBufferLen, 0, targetSockAddr, sizeof((*targetSockAddr)));		//			- send packet to receiving/target Client
			}
		}
		auto start = std::chrono::system_clock::now();																//		- start time used to track timeout for UDP sequence	
		while ((sendSocket != INVALID_SOCKET) | (recvSocket != INVALID_SOCKET)) {									//		- While sliding window part
			auto end = std::chrono::system_clock::now();															//			- update end time to track timeout
			std::chrono::duration<double> elapsed_seconds = end - start;											//			- calculate time elapsed for current packet's ID 

			timeSinceLastCorrespond = std::chrono::system_clock::now();												//			- get timeSinceLastCorrespond timestamp 
			coldTurkeyTime = timeSinceLastCorrespond - lastCorrespond;												//			- calculate period between last correspond
			ZeroMemory(recvBuffer, DEFAULT_BUFLEN);																	//			- clear buffer for receiving acknowledgement
			packetSize = recvfrom(recvSocket, recvBuffer, DEFAULT_BUFLEN, 0, &recvSockAddr, &recvSockSize);			//			- check socket for information 
			if (packetSize > 0) {																					//			- If packet size is greater than 0
				lastCorrespond = std::chrono::system_clock::now();													//				- update last corresponding time
				recvAddrData = (char*)recvSockAddr.sa_data;															//				- retrieve recvAddr
				recvIP = (unsigned*)(recvAddrData + 2);																//				- get the IP of the receiving/target Client
				if ((*targetIP) == (*recvIP)) {																		//				- If the recevied IP address is the target's IP address
					directive = recvBuffer[0];																		//					- get the directive byte
					if (directive == 7) {																			//					- If the directive/command is \x07, i.e. acknowledgement
						if ((*recvPacketID) == (currWindowStart)) {													//						- If the acknowledgement is the next packet in sequence

							padNum = (currWindowStart * 54) / numPackets;											//							- update progress bar
							std::cout << "[" << progressBar + (54 - padNum) << padBar + padNum << "]" << "  " << currWindowStart << "/" << (numPackets - 1) << " Packets Sent \r";
							std::cout.flush();

							start = std::chrono::system_clock::now();												//							- update timestamp 
							if ((*recvPacketID) == (numPackets - 1)) {												//							- If it is the acknowledgement for the last packet
								transferCompleted = true;															//								- set transferCompleted to true
								sendto(sendSocket, "\x08\0", 2, 0, targetSockAddr, sizeof((*targetSockAddr)));		//								- send end of packet directive/command
								continue;																			//								- continue
							}
																													//							- Pesudo else
							ZeroMemory(sendBuffer + 1, DEFAULT_BUFLEN - 1);											//								- clear sendBuffer
							nextSlide = min(numPackets - 1, currWindowStart + SLIDE_WINDOW_SIZE);					//								- move sliding window by 1
							(*packetID) = nextSlide;																//								- set packetID
							file.seekg((nextSlide * payloadLen), file.beg);											//								- seek to segment/region of file corresponding to the next packet
							currWindowStart = min(currWindowStart + 1, numPackets - 1);								//								- set current sliding window starting point

							if (nextSlide == (numPackets - 1)) {													//							- If nextSlide is the last packet
								file.read(payload, (fileSize % payloadLen));										//								- copy the remainder
								sendBufferLen = (fileSize % payloadLen) + DEFAULT_HEADER_SIZE;						//								- set sendBufferLen
							}
							else {																					//							- Else 
								file.read(payload, payloadLen);														//								- copy to payload length 
								sendBufferLen = DEFAULT_BUFLEN;														//								- set sendBufferLen
							}
							sendto(sendSocket, sendBuffer, sendBufferLen, 0, targetSockAddr, sizeof((*targetSockAddr)));	//						- send packet

						}
						else if ((*recvPacketID) > (currWindowStart)) {												//						- Else IF packet ID of acknowledgement is greater than the acknowledgement we are waiting for
							break;																					//							- break to resend all packets in current Window
						}
						else {																						//						- Else
							continue;																				//							- continue and check socket again
						}
					}
					else if (directive == 8) {																		//					- Else If directive/command is "\x06"
						if (transferCompleted) {																	//						- If transfer is already completed as flagged earlier
							padNum = (currWindowStart * 54) / numPackets;											//							- update progress bar, this time without rolling back 
							std::cout << "[" << progressBar + (54 - padNum) << padBar + padNum << "]" << "  " << currWindowStart << "/" << (numPackets - 1) << " Packets " << std::endl;
							return 0;																				//							- exit function and return success
						}
						continue;																					//						- Pesudo else continue
					}
					else {																							//					- Else
						continue;																					//						- continue
					}
				}
			}
			else if ((elapsed_seconds.count() > 2.2)) {																//			- Else If the acknowledgement for the correct packet did not arrive after 2.2 seconds
				break;																								//				- break and resend all packets in current Window
			}
			else if ((packetSize == 0)) {																			//			- Else If packetSize is 0
				continue;																							//				- continue, note: no implementation here as of yet, potential for expansion
			}
			else if ((coldTurkeyTime.count() > 30)) {																//			- Else If no response from the Receiving/Target Client for more than 30 seconds
				return -1;																							//				- exit function and return error
			}
			else {																									//			- Else 
				continue;																							//				- Just continue
			}

		}

	}

}


long UDPreceive(SOCKET& sendSocket, SOCKET& recvSocket, addrinfo* targetAddrInfo, const std::string& saveFilePath, unsigned packetIDStart) {

																																// Initialize variables for progress bar use
	char progressBar[] = ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";												//	- initialize forward progress string
	char padBar[] = "                                                      ";													//	- initialize blank string to pad progress bar
	unsigned padNum = 54;																										//	- pad number to be used for progress bar calculation later on
																																// Initialize booleans for flow control
	bool lastPacketReceived = false;																							//	- initially <false> to be flipped <true> when last packet is received (i.e. end packet with \x08 command)
	bool sizeDetermined = false;																								//	- initially <false> to be flipped <true> when first packet is received and total number of packets to be received is determined
																																// Create file path for .temp file to nest at while stream in the full file data
	std::string tempFilePath = saveFilePath.c_str();																			//	- initialize a std::string object
	tempFilePath += ".temp";																									//	- append ".temp" to current saveFilePath
																																// Prepare all variables needed to ack received packets and identify the Sending (or <Target>) Client
	sockaddr* targetSockAddr = targetAddrInfo->ai_addr;																			//	- extract out the sockaddr of the target from the target's addrinfo
	char* targetAddrData = (char*)targetSockAddr->sa_data;																		//	- extract out the data section from the target's sockaddr
	unsigned* targetIP = (unsigned*)(targetAddrData + 2);																		//	- extract out the IP address of the target from the target's data section
	char sendBuffer[5];																											//	- initialize/identify sendBuffer, start of the segment to write the payload into
	sendBuffer[0] = 7;																											//	- set sendBuffer directive/command as \x07
	unsigned* sendPacketID = (unsigned*)(sendBuffer + 1);																		//	- get pointer of the segment in the buffer to write sendPacketID information 

																																// Prepare all variables needed to receive packets from the Sending Client
	int packetSize;																												//	- initialize receive packet size
	sockaddr recvSockAddr;																										//	- initialize sockaddr to store address information of packet Sender
	int recvSockSize = sizeof(recvSockAddr);																					//	- get size of sockaddr
	char recvBuffer[DEFAULT_BUFLEN] = { 0 };																					//	- initialize buffer to receive packet and set buffer to 0 (or Null)
	unsigned* recvNumPackets = (unsigned*)(recvBuffer + 1);																		//	- get the pointer to the segment/region in the buffer that will store the total number of packets information 
	unsigned* recvPacketID = (unsigned*)(recvBuffer + 5);																		//	- get the pointer to the segment/region in the buffer that will store the Current Packet ID information 
	char* recvPayload = recvBuffer + DEFAULT_HEADER_SIZE;																		//	- get the pointer to the segment/region in the buffer to start writing the payload in 
	char* recvAddrData;																											//	- initialize variable to pointer to "data" segment/region in the sockaddr struct for later use 
	unsigned* recvIP;																											//	- initialize variable to pointer to the "IP Address" portion of the "data" segment/region in the sockaddr struct for later use 
																								
																																// Prepare all variables needed for function flow control
	char directive;																												//	- initialize variable to store directive/command byte
	unsigned currPacketID = packetIDStart;																						//	- initialize current packet ID from the user input to determine when/where to start receiving the packets
	int payloadLen = DEFAULT_BUFLEN - DEFAULT_HEADER_SIZE;																		//	- calculate payload length to be use when accepting and writing data to output file stream
	unsigned totalNumPackets;																									//	- initialize variable to store total number of packets information
	unsigned totalDataLength = 0;																								//	- start total size/length of receiving file as 0

																																// Open an output stream to write data into
	std::ofstream outFile;																										//	- initialize ofstream
	if (packetIDStart) {																										//	- If packetIDStart is not 0 (i.e. resuming file transfer)
		outFile = std::ofstream(tempFilePath.c_str(), std::ofstream::out | std::ofstream::app | std::ofstream::binary);			//		- open the current .temp file to continue writing new data in
	}
	else {																														//	- Else
		outFile = std::ofstream(tempFilePath.c_str(), std::ifstream::binary);													//		- open a fresh .temp file to start writing
	}

																																// Starting the receiving process
	auto lastCorrespond = std::chrono::system_clock::now();																		//	- initialize time to track connection timeout from Sender/Target
	auto timeSinceLastCorrespond = std::chrono::system_clock::now();															//	- ''
	std::chrono::duration<double> coldTurkeyTime = timeSinceLastCorrespond - lastCorrespond;									//	- the time since Sender/Target last packet
	while ((sendSocket != INVALID_SOCKET) | (recvSocket != INVALID_SOCKET)) {													// While both send and receive Sockets are not closed

		timeSinceLastCorrespond = std::chrono::system_clock::now();																//	- get the current time now 
		coldTurkeyTime = timeSinceLastCorrespond - lastCorrespond;																//	- calculate the time since last response with Sender/Target Client
		packetSize = recvfrom(recvSocket, recvBuffer, DEFAULT_BUFLEN, 0, &recvSockAddr, &recvSockSize);							//	- check recvSocket for a packet

		if (packetSize > 0) {																									//	- If packetSize != 0, i.e. an actual packet was sent over
			lastCorrespond = std::chrono::system_clock::now();																	//		- update the time of last corresponding
			recvAddrData = (char*)recvSockAddr.sa_data;																			//		- retrieve recvAddr from Sender/Target Client
			recvIP = (unsigned*)(recvAddrData + 2);																				//		- retrieve IP of Sender/Target Client

			if ((*targetIP) == (*recvIP)) {																						//		- If IP Address was from the intended Client
				directive = recvBuffer[0];																						//			- get the directive/command from the Sending/Target Client

				if (directive == 6) {																							//			- If directive/command is 6 (i.e. sending data packet over)

					if ((*recvPacketID) == (currPacketID)) {																	//				- If the current packet received is the next packet that we are waiting for to write to file

						if (sizeDetermined) {																					//					- If we already know the total number of packets to receive
							outFile.write(recvPayload, (packetSize - DEFAULT_HEADER_SIZE));										//						- write the received payload into the filestream 
							totalDataLength += (packetSize - DEFAULT_HEADER_SIZE);												//						- increment the totalDataLength by the payload size
							++currPacketID;																						//						- increment currPacketID by 1
						}
						else {																									//					- Else
							totalNumPackets = (*recvNumPackets);																//						- get the total number of packets that will be sent over information
							outFile.write(recvPayload, (packetSize - DEFAULT_HEADER_SIZE));										//						- write the received payload into the filestream
							sizeDetermined = true;																				//						- set sizeDetermined boolean to true since totalNumPackets information is known at this point
							totalDataLength += (packetSize - DEFAULT_HEADER_SIZE);												//						- increment the totalDataLength by the payload size
							++currPacketID;																						//						- increment currPacketID by 1
						}
					}
					ZeroMemory(sendPacketID, 4);																				//				- clear memory for send buffer
					(*sendPacketID) = *recvPacketID;																			//				- enter the ID of the packet received
					sendto(sendSocket, sendBuffer, 5, 0, targetSockAddr, sizeof((*targetSockAddr)));							//				- send acknowledgement to Sending/Target Client

					padNum = (currPacketID * 54) / totalNumPackets;																//				- update progress bar
					std::cout << "[" << progressBar + (54 - padNum) << padBar + padNum << "]" << "  " << currPacketID - 1 << "/" << (totalNumPackets - 1) << " Packets Received \r";
					std::cout.flush();
				}

				else if (directive == 8) {																						//			- If directive/command is 8 (i.e. all packets sent)
					sendto(sendSocket, "\x08\0", 2, 0, targetSockAddr, sizeof((*targetSockAddr)));								//				- send acknowledgement
					lastPacketReceived = true;																					//				- set lastPacketReceived to true
					continue;																									//				- continue loop
				}
				else {																											//			- Else 
					continue;																									//				- continue loop
				}
			}
		}
		else if (packetSize == 0) {																								//		- Else if packetSize == 0 (no real implemmentation to capitalize this)
			continue;																											//			- continue loop
		}
		else if ((coldTurkeyTime.count() > 30)) {																				//		- Else if Sending/Target Client failed to respond in for more than 30 seconds
			char* tempStamp = (char*)malloc(200);																				//			- write current checkpoint information to .temp file to be used for resuming  
			memcpy(tempStamp, &currPacketID, 4);
			memcpy(tempStamp + 4, saveFilePath.c_str(), saveFilePath.size() + 1);
			outFile.write(tempStamp, 200);
			free(tempStamp);
			outFile.close();																									//			- close file  
			return -1;																											//			- exit function return error
		}
		else {																													//		- Else
			if (lastPacketReceived) {																							//			- If last packet already received
				outFile.close();																								//				- save and close file 
				rename(tempFilePath.c_str(), saveFilePath.c_str());
				std::cout << "[" << progressBar + (54 - padNum) << padBar + padNum << "]" << "  " << currPacketID - 1 << "/" << (totalNumPackets - 1) << " Packets Received " << std::endl;
				return totalDataLength;																							//				- return total file size information
			}
			else {																												//			- Else
				continue;																										//				- continue
			}
		}

	}
	return 0;																													//	return 0 (i.e. all clear!) though will loop will never reach this point 
}



int executeTransfer(SOCKET& sendSocket, SOCKET& recvSocket, const char* transferInfo, const std::string& folderPath) {

	std::string filename;
	std::string targetAddr;
	std::string targetPort;

	std::string* Info[3] = { &filename, &targetAddr, &targetPort };;
	const char* currSegment = transferInfo;
	int i = 0;
	int iInfo = 0;
	int infoStart;
	int strCount = 0;
	while (iInfo < 3) {
		strCount = 0;
		infoStart = i;
		while (transferInfo[i] != '\n') {
			++i;
			++strCount;
		}
		Info[iInfo]->append(transferInfo, infoStart, strCount);
		++i;
		++iInfo;
	}
	unsigned* packetIDStart = (unsigned*)(transferInfo + i);


	addrinfo* targetAddrInfo = resolveAddress(targetAddr.c_str(), targetPort.c_str(), false);
	if (targetAddrInfo == nullptr) {
		printf("target address info resolution fail\n");
		return -1;
	}

	std::string filePath = folderPath.c_str();
	filePath += "/";
	filePath += filename;
	std::ifstream inputFile(filePath.c_str(), std::ifstream::binary);



	std::cout << "Starting transfer of " << filename << "..." << std::endl;
	auto start = std::chrono::system_clock::now();
	int sendResult = UDPsend(sendSocket, recvSocket, targetAddrInfo, inputFile, (*packetIDStart));
	auto end = std::chrono::system_clock::now();
	if (sendResult != 0) {
		printf("send failed somewhere\n");
		flushSocketBacklog(recvSocket, 1.2);
		return 1;
	}


	inputFile.close();
	flushSocketBacklog(recvSocket, 1.2);
	std::chrono::duration<double> elapsed_seconds = end - start;
	std::cout << "Transfer completed, elapsed seconds: " << elapsed_seconds.count() << std::endl << std::endl;
	return 0;
}



int executeDownload(SOCKET& sendSocket, SOCKET& recvSocket, const char* downloadInfo, const std::string& folderPath) {

	std::string filename;
	std::string targetAddr;
	std::string targetPort;
	std::string* Info[3] = { &filename, &targetAddr, &targetPort };

	const char* currSegment = downloadInfo;
	int i = 0;
	int iInfo = 0;
	int infoStart;
	int strCount = 0;

	while (iInfo < 3) {
		strCount = 0;
		infoStart = i;
		while (downloadInfo[i] != '\n') {
			++i;
			++strCount;
		}

		Info[iInfo]->append(downloadInfo, infoStart, strCount);
		++i;
		++iInfo;
	}
	unsigned* packetIDStart = (unsigned*)(downloadInfo + i);

	addrinfo* targetAddrInfo = resolveAddress(targetAddr.c_str(), targetPort.c_str(), false);
	if (targetAddrInfo == nullptr) {
		printf("target address info resolution fail\n");
		return -1;
	}

	std::string saveFilePath = folderPath.c_str();
	saveFilePath += "/";
	saveFilePath += filename;

	std::cout << "Starting download of " << filename << "..." << std::endl;
	auto start = std::chrono::system_clock::now();
	long receiveResult = UDPreceive(sendSocket, recvSocket, targetAddrInfo, saveFilePath, (*packetIDStart));
	auto end = std::chrono::system_clock::now();
	if (receiveResult < 0) {
		printf("receive failed somewhere\n");
		flushSocketBacklog(recvSocket, 1.2);
		return -1;
	}

	flushSocketBacklog(recvSocket, 1.2);

	std::chrono::duration<double> elapsed_seconds = end - start;
	std::cout << "Download completed, elapsed seconds: " << elapsed_seconds.count() << std::endl << std::endl;
	return 0;
}


void flushSocketBacklog(SOCKET& recvSocket, unsigned numSeconds) {

	auto start = std::chrono::system_clock::now();
	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds;
	sockaddr recvSockAddr;																							// setup variables for receive from
	int recvSockSize = sizeof(recvSockAddr);
	char recvBuffer[DEFAULT_BUFLEN] = { 0 };

	while (elapsed_seconds.count() < numSeconds) {
		recvfrom(recvSocket, recvBuffer, DEFAULT_BUFLEN, 0, &recvSockAddr, &recvSockSize);
		end = std::chrono::system_clock::now();
		elapsed_seconds = end - start;
	}
	return;
}



std::string readConfigFile(const char* filepath) {

	std::ostringstream buffer;
	std::ifstream configFile(filepath);
	buffer << configFile.rdbuf();
	return buffer.str();
}

void loadConfig(ConfigInfo* configInfo, std::string& configStr) {

	configInfo->configString = configStr;

	char* strStarts[6];
	strStarts[0] = &(configInfo->configString[0]);
	int nextConfig = 1;
	for (int i = 0; i < configInfo->configString.size(); ++i) {
		if (configInfo->configString[i] == '\n') {
			configInfo->configString[i] = '\0';
			strStarts[nextConfig] = &(configInfo->configString[i + 1]);
			++nextConfig;
		}
	}
	for (int j = 0; j < 4; ++j) {
		if (strcmp(strStarts[j], "NULL") == 0) {
			strStarts[j] = 0;
		}
	}
	configInfo->serverPort = strStarts[0];
	configInfo->serverHostname = strStarts[1];
	configInfo->clientPort = strStarts[2];
	configInfo->clientHostname = strStarts[3];
	configInfo->shareDirectory = strStarts[4];
	configInfo->receiveDirectory = strStarts[5];
}

std::string scanFolder(const char* folderName) {

	std::string folderPath;
	std::string fileList;

	folderPath = "./";
	folderPath += folderName;

	dirent* de;
	DIR* dir = opendir(folderPath.c_str());

	if (dir == NULL) {
		printf("Could not open current directory");
		return fileList;
	}

	int cmpResult;
	while ((de = readdir(dir)) != NULL) {
		cmpResult = strcmp("..", de->d_name);
		if (!cmpResult) {
			continue;
		}
		cmpResult = strcmp(".", de->d_name);
		if (!cmpResult) {
			continue;
		}
		fileList += de->d_name;
		fileList += "\n";
	}
	closedir(dir);
	return fileList;
}

#pragma once
#include "server.h"
#include "serverutils.h"

#include <thread>
#include <iostream>
#include <Windows.h>
#include <stdlib.h>

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>



int main(int argc, char* argv[]) {

	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	if (argc != 2) {
		std::cout << "Exe takes EXACTLY ONE arguement, a .cfg file, please try again!" << std::endl;
		return 0;
	}

	std::string config = readConfigFile(argv[1]);
	if (config.size() == 0) {
		std::cout << "\nserver.cfg is missing or empty, please try again after fixing!\n";
		return 0;
	}

	std::cout << "\nThe config is :" << config << std::endl;

	unsigned portNumber = atoi(config.c_str());
	if ((portNumber > 65535) | (config.size() > 5)) {
		std::cout << "\nserver.cfg file is corrupted, please try again after fixing the file\n";
		return 0;
	}


	Server server = Server(NULL, config.c_str());
	server.serverStart();
	std::cout << "Server started up!" << std::endl;
	server.deployManagers();

	return 0;
}

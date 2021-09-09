#pragma once
#include "client.h"
#include "clientutils.h"

#include <Windows.h>
#include <iostream>
#include <stdlib.h>

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

int main() {

	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	ConfigInfo config;
	std::string configStr = readConfigFile("client.cfg");
	std::cout << "The configuration is : \n" << configStr << std::endl;
	loadConfig(&config, configStr);
	std::string fileList = scanFolder(config.shareDirectory.c_str());



	Client client(config, fileList);
	client.clientStart();
	client.connectToServer();
	client.deployManagers();
}
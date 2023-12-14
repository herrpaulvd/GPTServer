#pragma once

#include <Windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <io.h>
#include <random>
#include <fstream>
#include "rapidxml/rapidxml.hpp"

using namespace std;
namespace xml = rapidxml;

mt19937 random;

vector<HANDLE> storedHandles;

void storeHandle(HANDLE h) { storedHandles.push_back(h); }

void closeAllStoredHandles() 
{ 
	for (auto h : storedHandles) 
		CloseHandle(h); 
	storedHandles.clear(); 
}

void closeLastStoredHandle()
{
	CloseHandle(storedHandles.back());
	storedHandles.pop_back();
}

void reportError(wstring error)
{
	closeAllStoredHandles();
	wcout << error << endl;
	exit(-1);
}

#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <string>
#include <iostream>
#include <set>
#include <vector>
#include <stack>
#include <fcntl.h>
#include <io.h>
#include <filesystem>
#include <functional>
#include <queue>

using namespace std;
namespace fs = std::filesystem;

UINT64 ceildiv(UINT64 a, UINT64 b)
{
	return a / b + (a % b ? 1 : 0);
}

DWORD combine(WORD lo, WORD hi)
{
	return lo | (hi << 16);
}

HANDLE handleToClose = INVALID_HANDLE_VALUE;

void reportError(wstring error)
{
	if(handleToClose != INVALID_HANDLE_VALUE)
		CloseHandle(handleToClose);
	wcout << error << endl;
	exit(-1);
}



#include "includes.h"
#include "FAT32routines.h"

FileTree::InputDirectoryEntry* parseArgs(int argc, wchar_t** args, UINT64& bytesPerSec, UINT64& secPerTrack, UINT64& heads, UINT64& cylinders, UINT64& LBAoffset, UINT64& totSec, wstring& path)
{
	if (argc < 8)
		reportError(L"Protocol error: too little args");

	using namespace FileTree;

	InputDirectoryEntry* entry;
	InputDirectoryEntry* root = new InputDirectoryEntry();
	root->name = L"root";
	root->type = InputEntryType::Directory;
	stack<InputDirectoryEntry*> entries;
	entries.push(root);

	int i = 1;

	try
	{
		bytesPerSec = stoll(args[i++]);
		secPerTrack = stoll(args[i++]);
		heads = stoll(args[i++]);
		cylinders = stoll(args[i++]);
		LBAoffset = stoll(args[i++]);
		totSec = stoll(args[i++]);
	}
	catch (...)
	{
		reportError(L"Protocol error: missing geometry or LBA offset");
	}

	path = args[i++];

	for (; i < argc; i++)
	{
		wstring s = args[i];

		if (s == L"-e")
		{
			entries.pop();
			if (entries.empty())
				return root;
		}
		else if (s == L"-d")
		{
			if (argc - i < 2) return root;

			entry = new InputDirectoryEntry();
			entry->name = args[++i];
			entry->type = InputEntryType::Directory;
			entries.top()->children.push_back(entry);
			entries.push(entry);
		}
		else if (s == L"-c")
		{
			if (argc - i < 3) return root;

			entry = new InputDirectoryEntry();
			entry->name = args[++i];
			entry->path = args[++i];
			entry->type = InputEntryType::Copy;
			entries.top()->children.push_back(entry);
		}
		else if (s == L"-cf")
		{
			if (argc - i < 2) return root;

			entry = new InputDirectoryEntry();
			entry->path = args[++i];
			entry->type = InputEntryType::CopyFrom;
			entries.top()->children.push_back(entry);
		}
	}

	return root;
}

HANDLE hPipe;
UINT64 secWritten = 0;
UINT64 bytesPerSector;

void sendSector(BYTE* sector)
{
	DWORD written = 0;
	if (!WriteFile(hPipe, sector, bytesPerSector, &written, 0) || !written)
		reportError(L"Protocol error: cannot write pipe GLE=" + to_wstring(GetLastError()));
	secWritten++;
}

int main()
{
	UINT64 secPerTrack, numHeads, numCylinders, LBAoffset, totSec;
	wstring pipePath;
	int argc;
	wchar_t** args = CommandLineToArgvW(GetCommandLineW(), &argc);
	auto root = parseArgs(argc, args, bytesPerSector, secPerTrack, numHeads, numCylinders, LBAoffset, totSec, pipePath);

	handleToClose = hPipe = CreateFileW(pipePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (hPipe == INVALID_HANDLE_VALUE)
		reportError(L"Protocol error: cannot open pipe GLE=" + to_wstring(GetLastError()));

	FATrout::createFAT(root, sendSector, bytesPerSector, secPerTrack, numHeads, totSec, LBAoffset);

	CloseHandle(hPipe);

	return 0;
}

#include "includes.h"
#include "MBR.h"
#include "GPT.h"
#include "Disk.h"
#include "xmlparser.h"

void printDiskInfo(vector<DiskInfo> disks)
{
	wcout.precision(3);
	wcout << fixed;
	for (int i = 1; i < disks.size(); i++)
	{
		wcout << L"Disk #" << i << L": size = " << disks[i].sizeInGB << L" gb, volumes = ";
		for (int q = 0; q < 26; q++)
			if (disks[i].volumes & (1 << q))
				wcout << (wchar_t)(L'A' + q);
		if (!disks[i].volumes)
			wcout << L"<no volumes>";
		wcout << endl;
	}
}

wstring generatePipeName()
{
	uniform_int_distribution<int> uid('a', 'z');
	wstring res = L"\\\\.\\pipe\\";

	for (int i = 0; i < 20; i++)
	{
		int c = uid(random);
		res.push_back((wchar_t)c);
	}

	return res;
}

UINT64 bytesPerSector;
BYTE* buffer;

void useInput(UINT64 totSec, HANDLE hInputPipe, HANDLE hProcess, HANDLE hDest, int partIndex)
{
	if (!ConnectNamedPipe(hInputPipe, 0))
		reportError(L"Client process cannot connect the pipe GLE=" + to_wstring(GetLastError()));

	while (totSec--)
	{
		DWORD read = 0;
		if (!ReadFile(hInputPipe, buffer, bytesPerSector, &read, 0))
		{
			if (GetLastError() == ERROR_BROKEN_PIPE)
			{
				DWORD status;
				while (true)
				{
					if (!GetExitCodeProcess(hProcess, &status))
						reportError(L"Cannot get exit code of child process");
					if (status != STILL_ACTIVE)
					{
						if (status)
							reportError(L"Child process exited with non-zero error code " + to_wstring(status));
						wcout << L"Formatting process for part #" + to_wstring(partIndex) + L" exited successful" << endl;
						return;
					}
				}
			}
			reportError(L"Cannot read from input pipe GLE=" + to_wstring(GetLastError()));
		}
		else
		{
			if (!WriteFile(hDest, buffer, bytesPerSector, &read, 0))
				reportError(L"Cannot write on destination object GLE=" + to_wstring(GetLastError()));
		}
	}

	TerminateProcess(hProcess, 0);
}

void recBuildCmdline(InputDirectoryEntry* entry, wstring& cmdline)
{
	for (auto e : entry->children)
	{
		switch (e->type)
		{
		case InputEntryType::Directory:
			cmdline.append(L" -d ");
			cmdline.append(e->name);
			recBuildCmdline(e, cmdline);
			cmdline.append(L" -e");
			break;
		case InputEntryType::Copy:
			cmdline.append(L" -c ");
			cmdline.append(e->name);
			cmdline.append(L" \"");
			cmdline.append(e->path);
			cmdline.push_back(L'\"');
			break;
		case InputEntryType::CopyFrom:
			cmdline.append(L" -cf \"");
			cmdline.append(e->path);
			cmdline.push_back(L'\"');
			break;
		default:
			reportError(L"Internal error at rec build cmd line");
			break;
		}
	}
}

HANDLE startProcess(wstring commandLine, wstring error)
{
	STARTUPINFOW startupInfo;
	memset(&startupInfo, 0, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);

	PROCESS_INFORMATION processInfo;
	memset(&processInfo, 0, sizeof(processInfo));

	int len = commandLine.length();
	wchar_t* noconststr = new wchar_t[len + 1];
	memcpy(noconststr, commandLine.c_str(), len * 2);
	noconststr[len] = 0;

	if (!CreateProcessW(0, noconststr, 0, 0, 0, 0, 0, 0, &startupInfo, &processInfo))
		reportError(error);
	delete[] noconststr;

	return processInfo.hProcess;
}

void makeProject(const wchar_t* fileName)
{
	ProjectInfo project = parse(fileName);
	vector<DiskInfo> disks;
	HANDLE hDest = INVALID_HANDLE_VALUE;
	UINT64 totSec;
	UINT64 secPerTrack = 1;
	UINT64 numHeads = 1;
	UINT64 numCylinders;

	if (project.partitions.size() > 128)
		reportError(L"Too many partitions");

	// инициализация целевого устройства
	if (project.disk != PARSER_NO_DISK)
	{
		disks = getDisksInfo();
		if (project.diskSure)
		{
			if (project.disk >= disks.size())
				reportError(L"No disk with the specified number detected");

			auto diskGeometryEx = disks[project.disk].geometry;
			auto& diskGeometry = diskGeometryEx.Geometry;
			bytesPerSector = diskGeometry.BytesPerSector;
			totSec = diskGeometryEx.DiskSize.QuadPart / bytesPerSector;
			secPerTrack = diskGeometry.SectorsPerTrack;
			numHeads = diskGeometry.TracksPerCylinder;
			numCylinders = diskGeometry.Cylinders.QuadPart;

			for (int i = 0; i < 26; i++)
				if (disks[project.disk].volumes & (1 << i))
				{
					wstring name;
					name.push_back((wchar_t)(i + L'A'));
					HANDLE hVolume = openVolume(name, GENERIC_READ | GENERIC_WRITE);
					if (hVolume == INVALID_HANDLE_VALUE)
						reportError(L"Cannot open some of disk's volumes GLE=" + to_wstring(GetLastError()));
					prepareVolume(hVolume);
				}
			hDest = openDisk(project.disk, GENERIC_READ | GENERIC_WRITE);
			if (hDest == INVALID_HANDLE_VALUE)
				reportError(L"Cannot open disk GLE=" + to_wstring(GetLastError()));
		}
		else
		{
			wcout << L"Printing disks info because disk-sure set to false:" << endl;
			printDiskInfo(disks);
			if (project.img.empty())
			{
				wcout << L"Assembling canceled because no image file selected and disk-sure set to false" << endl;
				return;
			}
		}
	}
	if (hDest == INVALID_HANDLE_VALUE)
	{
		if (project.img.empty())
			reportError(L"No goal set for the program");

		bytesPerSector = 512;
		totSec = 1024 * project.imgSize.value / bytesPerSector;
		switch (project.imgSize.type)
		{
		case SizeType::GB:
			totSec *= 1024;
			[[fallthrough]];
		case SizeType::MB:
			totSec *= 1024;
			[[fallthrough]];
		case SizeType::KB:
			break;
		default:
			reportError(L"Image size expected but not set");
		}
		numCylinders = totSec;

		if (project.img.size() < 4 || project.img.substr(project.img.size() - 4, 4) != L".img")
			project.img.append(L".img");

		hDest = CreateFileW(project.img.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (hDest == INVALID_HANDLE_VALUE)
			reportError(L"Cannot open or create image file");
		storeHandle(hDest);
	}

	// разметка и форматирование
	buffer = new BYTE[bytesPerSector];
	const int maxPartCount = 128;

	// prefix structures
	MBR mbr;
	GPTHeader gptHeader;
	GPTPartitionEntry gptEntries[maxPartCount];
	UINT64 prefixSec = 2 + sizeof(gptEntries) / bytesPerSector;
	// suffix structures
	// gptEntries
	GPTHeader alternativeHeader;
	UINT64 suffixSec = 1 + sizeof(gptEntries) / bytesPerSector;

	vector<UINT64> limits;
	limits.push_back(prefixSec);

	UINT64 freeSec = totSec - prefixSec - suffixSec;

	for (int i = 0; i < project.partitions.size(); i++)
	{
		auto& part = project.partitions[i];
		UINT64 partSec;

		if (part.size.type == SizeType::ALL)
		{
			if (freeSec == 0)
				reportError(L"Cannot assign space for partition #" + to_wstring(i + 1));
			partSec = freeSec;
			freeSec = 0;
		}
		else
		{
			partSec = 1024 * part.size.value / bytesPerSector;
			switch (part.size.type)
			{
			case SizeType::GB:
				partSec *= 1024;
				[[fallthrough]];
			case SizeType::MB:
				partSec *= 1024;
				[[fallthrough]];
			case SizeType::KB:
				break;
			default:
				reportError(L"Partition size expected but not set");
			}
			if (partSec > freeSec)
				reportError(L"Cannot assign space for partition #" + to_wstring(i + 1));
			freeSec -= partSec;
		}
		limits.push_back(limits.back() + partSec);
	}

	DWORD bytesRet = 0;

	MakeMbr(mbr, totSec);
	memcpy(buffer, &mbr, sizeof(mbr));
	memset(buffer + sizeof(mbr), 0, bytesPerSector - sizeof(mbr));
	if (!WriteFile(hDest, buffer, bytesPerSector, &bytesRet, 0))
		reportError(L"Cannot write on destination object GLE=" + to_wstring(GetLastError()));

	makeRawGPTHeader(gptHeader, totSec, bytesPerSector, maxPartCount);
	memset(gptEntries, 0, sizeof(gptEntries));
	for (int i = 0; i < project.partitions.size(); i++)
	{
		UINT8* GUID = nullptr;
		wstring name;
		switch (project.partitions[i].type)
		{
		case PartitionType::ESP:
			GUID = EFI_PART_GUID;
			name = L"EFI System partition";
			break;
		case PartitionType::DATA:
			GUID = DATA_PART_GUID;
			name = L"Basic data partition";
			break;
		default:
			reportError(L"Internal error at GUID:=");
			break;
		}
		makePartitionEntry(gptEntries[i], GUID, limits[i], limits[i + 1] - 1, name);
	}

	gptHeader.PartitionEntryArrayCRC32 = CRC32(gptEntries, sizeof(gptEntries));
	gptHeader.HeaderCRC32 = CRC32(&gptHeader, gptHeader.HeaderSize);

	memcpy(buffer, &gptHeader, sizeof(gptHeader));
	memset(buffer + sizeof(gptHeader), 0, bytesPerSector - sizeof(gptHeader));
	if (!WriteFile(hDest, buffer, bytesPerSector, &bytesRet, 0))
		reportError(L"Cannot write GPT Header");
	if (!WriteFile(hDest, gptEntries, sizeof(gptEntries), &bytesRet, 0))
		reportError(L"Cannot write GPT Entries");

	LARGE_INTEGER distanceToMove, newFilePointer;
	for (int i = 0; i < project.partitions.size(); i++)
	{
		wstring inputPipeName = generatePipeName();
		HANDLE hInputPipe = CreateNamedPipeW(inputPipeName.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 1, bytesPerSector * 2, bytesPerSector * 2, 0, 0);
		if (hInputPipe == INVALID_HANDLE_VALUE)
			reportError(L"Cannot create input pipe GLE=" + to_wstring(GetLastError()));
		storeHandle(hInputPipe);

		wstring processName = project.partitions[i].fileSystem + L"Client.exe";
		wstring cmdline = processName;
		cmdline.push_back(L' ');
		cmdline.append(to_wstring(bytesPerSector));
		cmdline.push_back(L' ');
		cmdline.append(to_wstring(secPerTrack));
		cmdline.push_back(L' ');
		cmdline.append(to_wstring(numHeads));
		cmdline.push_back(L' ');
		cmdline.append(to_wstring(numCylinders));
		cmdline.push_back(L' ');
		cmdline.append(to_wstring(limits[i]));
		cmdline.push_back(L' ');
		cmdline.append(to_wstring(limits[i + 1] - limits[i]));
		cmdline.push_back(L' ');
		cmdline.append(inputPipeName);
		recBuildCmdline(project.partitions[i].root, cmdline);

		useInput(limits[i + 1] - limits[i], hInputPipe, startProcess(cmdline, L"Cannot start formatting process for the filesystem " + project.partitions[i].fileSystem), hDest, i + 1);
		closeLastStoredHandle();

		distanceToMove.QuadPart = limits[i + 1] * bytesPerSector;
		if (!SetFilePointerEx(hDest, distanceToMove, &newFilePointer, FILE_BEGIN))
			reportError(L"Cannot set fp");
	}

	distanceToMove.QuadPart = (totSec - suffixSec) * bytesPerSector;
	if (!SetFilePointerEx(hDest, distanceToMove, &newFilePointer, FILE_BEGIN))
		reportError(L"Cannot set fp to the GPT backup's position");

	makeAlternativeGPTHeader(gptHeader, alternativeHeader);

	if (!WriteFile(hDest, gptEntries, sizeof(gptEntries), &bytesRet, 0))
		reportError(L"Cannot write backup GPT entries");
	memcpy(buffer, &alternativeHeader, sizeof(alternativeHeader));
	memset(buffer + sizeof(alternativeHeader), 0, bytesPerSector - sizeof(alternativeHeader));
	if (!WriteFile(hDest, buffer, bytesPerSector, &bytesRet, 0))
		reportError(L"Cannot write alternative GPT header");
	wcout << L"Assembling process finished successful!" << endl;

	closeAllStoredHandles();
}

const wstring disksCommand = L"-disks";

int main()
{
	_setmode(_fileno(stdout), _O_U8TEXT);

	int argc;
	auto args = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (argc < 2)
		reportError(L"Too little args");

	if (args[1] == disksCommand)
		printDiskInfo(getDisksInfo());
	else
		makeProject(args[1]);

	return 0;
}


#pragma once
#include "includes.h"

HANDLE openDiskOrVolume(wstring name, DWORD desiredAccess)
{
	HANDLE h = CreateFileW(name.c_str(), desiredAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (h != INVALID_HANDLE_VALUE)
		storeHandle(h);
	return h;
}

HANDLE openVolume(wstring letter, DWORD desiredAccess)
{
	return openDiskOrVolume(L"\\\\.\\" + letter + L":", desiredAccess);
}

HANDLE openDisk(int index, DWORD desiredAccess)
{
	return openDiskOrVolume(L"\\\\.\\PhysicalDrive" + to_wstring(index), desiredAccess);
}

DISK_GEOMETRY_EX getDiskGeometry(HANDLE h)
{
	DISK_GEOMETRY_EX geometry;
	DWORD written = 0;
	if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, &geometry, sizeof(geometry), &written, 0))
		reportError(L"Cannot get disk geometry GLE=" + to_wstring(GetLastError()));
	return geometry;
}

DWORD getPhysicalDiskNumber(HANDLE h)
{
	VOLUME_DISK_EXTENTS result;
	DWORD written = 0;
	if (!DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, &result, sizeof(result), &written, 0))
		reportError(L"Cannot get volume extents GLE=" + to_wstring(GetLastError()));
	return result.Extents->DiskNumber;
}

void prepareVolume(HANDLE h)
{
	DWORD written = 0;
	if (!DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &written, 0))
		reportError(L"Cannot dismount volume GLE=" + to_wstring(GetLastError()));

	if (!DeviceIoControl(h, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, &written, 0))
		reportError(L"Cannot lock volume GLE=" + to_wstring(GetLastError()));
}

const double gb = 1024 * 1024 * 1024;

struct DiskInfo
{
	double sizeInGB;
	DWORD volumes;
	DISK_GEOMETRY_EX geometry;

	DiskInfo(DISK_GEOMETRY_EX geometry) : geometry(geometry), sizeInGB(geometry.DiskSize.QuadPart / gb), volumes(0) {}
};

vector<DiskInfo> getDisksInfo()
{
	// 0) вывод доступных дисков и ввод необходимого [Test only]
	vector<DiskInfo> disks;
	while (true)
	{
		HANDLE hDisk = openDisk(disks.size(), GENERIC_READ);
		if (hDisk == INVALID_HANDLE_VALUE) break;
		disks.push_back(DiskInfo(getDiskGeometry(hDisk)));
		closeLastStoredHandle();
	}

	DWORD logicalDrives = GetLogicalDrives();
	for (int i = 0; i < 26; i++)
		if (logicalDrives & (1 << i))
		{
			wstring name;
			name.push_back((wchar_t)(i + L'A'));
			HANDLE hVolume = openVolume(name, GENERIC_READ);
			if (hVolume == INVALID_HANDLE_VALUE)
				reportError(L"Cannot open some of volumes GLE=" + to_wstring(GetLastError()));

			disks[getPhysicalDiskNumber(hVolume)].volumes |= (1 << i);
			closeLastStoredHandle();
		}
	return disks;
}


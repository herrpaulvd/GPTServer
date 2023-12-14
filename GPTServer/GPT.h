#pragma once
#include "includes.h"

#pragma pack(1)
struct GPTHeader
{
	UINT8 Signature[8]; // "EFI PART" (ASCII)
	UINT32 Revision; // 0x10000
	UINT32 HeaderSize; // 92
	UINT32 HeaderCRC32; // CRC32(first 'HeaderSize' bytes)
	UINT32 Reserved1; // 0
	UINT64 MyLBA; // LBA of this Header
	UINT64 AlternateLBA; // LBA of alternative Header
	UINT64 FirstUsableLBA; // first LBA of volumes' data
	UINT64 LastUsableLBA; // last LBA of volumes' data
	UINT8 DiskGUID[16]; // unique GUID (RANDOM)
	UINT64 PartitionEntryLBA; // 2
	UINT32 NumberOfPartitionEntries; // max count of volumes
	UINT32 SizeOfPartitionEntry; // 128
	UINT32 PartitionEntryArrayCRC32; // CRC32(ptr = PartitionEntryLBA, count = Number * Size)
	UINT8 Reserved2[512 - 92]; // 0
};

struct GPTPartitionEntry
{
	UINT8 PartitionTypeGUID[16]; // ~ GUID of type of the volume 
	UINT8 UniquePartitionGUID[16];// unique GUID (RANDOM)
	UINT64 StartingLBA; // first LBA of volume's data
	UINT64 EndingLBA; // last LBA of volume's data
	UINT64 Attributes; // 0
	WCHAR PartitionName[36]; // name
};
#pragma pack()

UINT32 CRC32(void* val, UINT64 length)
{
	const UINT64 mod = 0xEDB88320U;

	BYTE* buf = (BYTE*)val;

	UINT32 crc_table[256];
	UINT32 crc;

	for (int i = 0; i < 256; i++)
	{
		crc = i;
		for (int j = 0; j < 8; j++)
			crc = crc & 1 ? (crc >> 1) ^ mod : crc >> 1;

		crc_table[i] = crc;
	};

	crc = 0xFFFFFFFFU;

	while (length--)
		crc = crc_table[(crc ^ *(buf++)) & 0xFF] ^ (crc >> 8);

	return crc ^ 0xFFFFFFFFU;
}

const int GUIDsize = 16;
int GUIDindicesOrder[] = { 3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15 };
BYTE EFI_PART_GUID[] = { 0xC1, 0x2A, 0x73, 0x28, 0xF8, 0x1F, 0x11, 0xD2, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B };
BYTE DATA_PART_GUID[] = { 0xEB, 0xD0, 0xA0, 0xA2, 0xB9, 0xE5, 0x44, 0x33, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 };

void makeGUID(UINT8* GUID)
{
	uniform_int_distribution<int> uid(0, 0xFF);
	for (int i = 0; i < GUIDsize; i++)
		GUID[i] = uid(random);
}

void assignGUID(UINT8* dest, UINT8* src)
{
	for (int i = 0; i < GUIDsize; i++)
		dest[GUIDindicesOrder[i]] = src[i];
}

// missing HeaderCRC32, ArraysCRC32
void makeRawGPTHeader(GPTHeader& result, UINT64 totSec, UINT64 bytesPerSector, UINT64 maxPartitionsCount)
{
	UINT64 gptStrucSize = 1 + maxPartitionsCount * sizeof(GPTPartitionEntry) / bytesPerSector;

	memset(&result, 0, sizeof(result));
	memcpy(result.Signature, "EFI PART", 8);
	result.Revision = 0x10000U;
	result.HeaderSize = 92;
	result.MyLBA = 1;
	result.AlternateLBA = totSec - 1;
	result.FirstUsableLBA = result.MyLBA + gptStrucSize;
	result.LastUsableLBA = totSec - 1 - gptStrucSize;
	makeGUID(result.DiskGUID);
	result.PartitionEntryLBA = 2;
	result.NumberOfPartitionEntries = maxPartitionsCount;
	result.SizeOfPartitionEntry = sizeof(GPTPartitionEntry);
}

void makeAlternativeGPTHeader(GPTHeader& source, GPTHeader& result)
{
	memcpy(&result, &source, sizeof(source));
	result.HeaderCRC32 = 0;
	swap(result.MyLBA, result.AlternateLBA);
	result.PartitionEntryLBA = result.MyLBA - (source.FirstUsableLBA - source.MyLBA) + 1;
	result.HeaderCRC32 = CRC32(&result, result.HeaderSize);
}

void makePartitionEntry(GPTPartitionEntry& result, UINT8* typeGUIDsrc, UINT64 startingLBA, UINT64 endingLBA, wstring name)
{
	memset(&result, 0, sizeof(result));
	assignGUID(result.PartitionTypeGUID, typeGUIDsrc);
	makeGUID(result.UniquePartitionGUID);
	result.StartingLBA = startingLBA;
	result.EndingLBA = endingLBA;

	int nameLen = name.length();
	const int nameLim = sizeof(result.PartitionName) / sizeof(result.PartitionName[0]);
	memcpy(result.PartitionName, name.c_str(), 2 * min(nameLen, nameLim));
}


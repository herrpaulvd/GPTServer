#pragma once
#include "includes.h"

#pragma pack(1)
struct MBRPartRecord
{
	UINT8 BootIndicator; // 0 
	UINT8 StartingCHS[3]; // 00 02 00
	UINT8 OSType; // 0xEE
	UINT8 EndingCHS[3]; // FF FF FF
	UINT32 StartingLBA; // 1
	UINT32 SizeInLBA; // (totSec - 1)
};

struct MBR
{
	UINT8 BootCode[440]; // 0
	UINT8 UniqueMBRDiskSignature[4]; // 0
	UINT8 Unknown[2]; // 0
	MBRPartRecord PartitionRecord[4];
	UINT16 Signature; // 0xAA55
};
#pragma pack()

void MakeMbr(MBR& result, UINT64 totalSectorsCount)
{
	memset(&result, 0, sizeof(result));
	result.Signature = 0xAA55U;
	
	auto& rec = result.PartitionRecord[0];
	rec.StartingCHS[1] = 2;
	rec.OSType = 0xEEU;
	rec.EndingCHS[0] = rec.EndingCHS[1] = rec.EndingCHS[2] = 0xFFU;
	rec.StartingLBA = 1;
	rec.SizeInLBA = min(totalSectorsCount - 1, 0xFFFFFFFFU);
}

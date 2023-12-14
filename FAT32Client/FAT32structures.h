#pragma once

#include "includes.h"

namespace FATstruc
{
#pragma pack(1)
	struct BootSector
	{
		BYTE BS_jmpBoot[3]; // ! 0xEB 0x58 0x90
		BYTE BS_OEMName[8]; // ! "MSWIN4.1"
		WORD BPB_BytsPerSec; // bytes per sector
		BYTE BPB_SecPerClus; // sectors per cluster
		WORD BPB_RsvdSecCnt; // 32
		BYTE BPB_NumFATs; // 2
		WORD BPB_RootEntCnt; // ! 0
		WORD BPB_TotSec16; // ! 0
		BYTE BPB_Media; // 0xF0
		WORD BPB_FATSz16; // 0
		WORD BPB_SecPerTrk; // sectors per track
		WORD BPB_NumHeads; // number of heads
		DWORD BPB_HiddSec; // first volume's LBA
		DWORD BPB_TotSec32; // total volume's sectors count
		DWORD BPB_FATSz32; // sectors per FAT
		WORD BPB_ExtFlags; // 0
		WORD BPB_FSVer; // 0
		DWORD BPB_RootClus; // 2
		WORD BPB_FSInfo; // 1
		WORD BPB_BkBootSec; // 6
		BYTE BPB_Reserved[12]; // 0
		BYTE BS_DrvNum; // 0x80
		BYTE BS_Reserved1; // 0
		BYTE BS_BootSig; // ! 0x29
		DWORD BS_VolID; // date & time combination
		BYTE BS_VolLab[11]; // "NO NAME    "
		BYTE BS_FilSysType[8]; // "FAT32   "
		BYTE BS_unused[512 - 2 - 90]; // 0
		WORD BS_Signature; // 0xAA55
	};

	struct FSInfo
	{
		DWORD FSI_LeadSig; // 0x41615252
		BYTE FSI_Reserved1[480]; // 0
		DWORD FSI_StrucSig; // 0x61417272
		DWORD FSI_Free_Count; // 0xFFFFFFFF
		DWORD FSI_Nxt_Free; // 0xFFFFFFFF
		BYTE FSI_Reserved2[12]; // 0
		DWORD FSI_TrailSig; // 0xAA550000
	};

	struct ShortDirectoryEntry
	{
		BYTE DIR_Name[11]; // short name
		BYTE DIR_Attr; // attributes
		BYTE DIR_NTRes; // 0
		BYTE DIR_CrtTimeTenth; // 0
		WORD DIR_CrtTime;
		WORD DIR_CrtDate;
		WORD DIR_LstAccDate;
		WORD DIR_FstClusHI;
		WORD DIR_WrtTime;
		WORD DIR_WrtDate;
		WORD DIR_FstClusLO;
		DWORD DIR_FileSize;
	};

	// располагаются в обратном порядке перед short entry
	struct LongDirectoryEntry
	{
		BYTE LDIR_Ord;
		WORD LDIR_Name1[5]; // [1]-[5]
		BYTE LDIR_Attr; // ATTR_LONG_NAME
		BYTE LDIR_Type; // 0
		BYTE LDIR_Chksum; // short name checksum
		WORD LDIR_Name2[6]; // [6]-[11]
		WORD LDIR_FstClusLO; // 0
		WORD LDIR_Name3[2]; // [12]-[13]
	};

	union DirectoryEntry
	{
		ShortDirectoryEntry shortDirectoryEntry;
		LongDirectoryEntry longDirectoryEntry;
	};
#pragma pack()

	// directory entry attributes
	const DWORD ATTR_READ_ONLY = 0x01;
	const DWORD ATTR_HIDDEN = 0x02;
	const DWORD ATTR_SYSTEM = 0x04;
	const DWORD ATTR_VOLUME_ID = 0x08;
	const DWORD ATTR_DIRECTORY = 0x10;
	const DWORD ATTR_ARCHIVE = 0x20;
	const DWORD ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;
	const DWORD ATTR_LONG_NAME_MASK = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE;
	const DWORD LAST_LONG_ENTRY = 0x40;

	const DWORD LONG_COMPONENT_CHARSLENGTH = 13;
	const DWORD CLUSTER_VALUE_MASK = 0x0FFFFFFFU;
	const DWORD EOC_CLUSTER_MIN = 0x0FFFFFF8U;
	const DWORD EOC_CLUSTER = 0x0FFFFFFFU;
	const DWORD BAD_CLUSTER = 0x0FFFFFF7U;
	const DWORD MAX_CLUSTER_VALUE = 0x0FFFFFF0U;
	const UINT64 MAX_FILE_LENGTH = 0xFFFFFFFFU;
	const UINT64 MAX_DIR_ENTRIES_COUNT = 0xFFFFU;
}


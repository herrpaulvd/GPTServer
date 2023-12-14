#pragma once
#include "includes.h"
#include "FAT32structures.h"
#include "FileTree.h"

namespace FATrout
{
	// формулы из fatgen103:

	// первый сектор с собственно данными
	// FirstDataSector = BPB_ResvdSecCnt + (BPB_NumFATs * BPB_FATSz32);
	
	// первый сектор N-го кластера
	// FirstSectorofCluster(N) = ((N – 2) * BPB_SecPerClus) + FirstDataSector; | N >= 2
	
	// количество секторов, отведённых под данные
	// DataSec = BPB_TotSec32 – (BPB_ResvdSecCnt + (BPB_NumFATs * BPB_FATSz32));
	
	// количество кластеров (не включая кластеры 0 и 1)
	// CountofClusters = DataSec / BPB_SecPerClus; 
	// min(CountOfClusters) = 65526, чтобы драйверы определяли FAT как FAT32
	
	// номер сектора и смещение в секторе ячейки кластера N в таблице FAT
	// FATOffset = N * 4
	// ThisFATSecNum = BPB_ResvdSecCnt + (FATOffset / BPB_BytsPerSec);
	// ThisFATEntOffset = FATOffset % BPB_BytsPerSec;
	
	// значение из таблицы, если сектор задан массивом SecBuff
	// FAT32ClusEntryVal = (*((DWORD *) &SecBuff[ThisFATEntOffset])) & 0x0FFFFFFF;
	
	// установка значения (без изменения флагов) значения в таблице только 28битные!!!
	// *((DWORD*)&SecBuff[ThisFATEntOffset]) = ((*((DWORD*)&SecBuff[ThisFATEntOffset])) & 0xF0000000) | FAT32ClusEntryVal;
	
	// конец файла
	// IsEOF = (FATContent >= 0x0FFFFFF8); | usually 0x0FFFFFFF
	
	// The first reserved cluster, FAT[0], contains the BPB_Media byte value in its low 8 bits, and all other bits are set to 1.
	// The second reserved cluster, FAT[1], is set by FORMAT to the EOC mark.
	
	// КАТАЛОГ:
	// ATTR_DIRECTORY is set in DIR_Attr
	// DIR_FileSize = 0 (размер описания папки определяется просто путём следования по цепочке кластеров, относящейся к ней)
	// пустая корневая папка - просто один кластер, целиком заполненный нулями
	// пустая некорневая папка - должно существовать два включения:
	// "dot": NAME = "." (доп. пробелами), DIR_Attr = ATTR_DIRECTORY, дата и время как у только что созданной директории, Size = 0, HI&LO = как у созд директории
	// "dotdot": NAME = "..", Attr = DIRECTORY, дата и время как у только что созданной директории, Size = 0, HI&LO = как у родителя созд директории
	
	// дата и время
	WORD makeDate(DWORD d, DWORD m, DWORD y)
	{
		return d | (m << 5U) | ((y - 1980U) << 9U);
	}

	WORD makeTime(DWORD s, DWORD m, DWORD h)
	{
		return (s / 2) | (m << 5U) | (h << 11U);
	}

	// чек-сумма (из документа fatgen103.doc, оптимизированная)
	unsigned char ChkSum(unsigned char* pFcbName)
	{
		short FcbNameLen = 11;
		unsigned char Sum = 0;

		while (FcbNameLen--)
		{
			Sum = (Sum & 1U) * 0x80U + (Sum >> 1) + *pFcbName++;
		}

		return Sum;
	}

	// корректность символов в именах файлов
	BYTE specialShortCharacters[] = { '$', '%',  '\'', '-', '_', '@', '~', '`', '!', '(' , ')', '{', '}', '^', '#', '&' };

	bool isCorrectShortCharacter(WORD c)
	{
		if (isalpha(c) || isdigit(c)) return true;
		const int specCharCnt = sizeof(specialShortCharacters) / sizeof(specialShortCharacters[0]);
		for (int i = 0; i < specCharCnt; i++)
			if (c == specialShortCharacters[i])
				return true;
		return false;
	}

	bool isCorrectLongCharacter(WORD c)
	{
		return c > 0x7FU || c == '.' || c == ' ' || isCorrectShortCharacter(c);
	}

	// убираем пробелы по краям строки и точки в конце
	wstring makeCorrectLongName(wstring name)
	{
		int begin = 0;
		int end = name.length() - 1;
		while (begin < name.length() && name[begin] == ' ') begin++;
		while (end >= 0 && (name[end] == ' ' || name[end] == '.')) end--;

		wstring result = L"";
		for (int i = begin; i <= end; i++)
			result.push_back(name[i]);
		return result;
	}

	// преобразование символа длинного имени в символ короткого имени
	BYTE convertToShort(WORD c, bool& lossy)
	{
		if (c > 0x7FU || !isCorrectShortCharacter(c))
		{
			lossy = true;
			return '_';
		}

		lossy = false;
		return toupper(c);
	}

	// получение базиса короткого имени (в котором при необходимости можно будет использовать числовой хвост)
	// true, если точно требуется числовой хвост
	// предполагается, что передаётся корректное длинное имя
	bool makeBasisName(wstring longName, BYTE* shortName)
	{
		int leadingPoints = 0;
		while (longName[leadingPoints] == L'.')
			leadingPoints++;
		longName.erase(0, leadingPoints);
		bool lossy = leadingPoints > 0;

		int noSpacesLength = 0;
		for (int i = 0; i < longName.length(); i++)
			if (longName[i] != L' ')
				longName[noSpacesLength++] = longName[i];

		if (noSpacesLength < longName.length())
		{
			longName.resize(noSpacesLength);
			lossy = true;
		}

		memset(shortName, ' ', sizeof(shortName));
		for (int i = 0; i < longName.length() && longName[i] != '.'; i++)
		{
			if (i == 8)
			{
				lossy = true;
				break;
			}

			shortName[i] = convertToShort(longName[i], lossy);
		}

		int dotStart = longName.length() - 1;
		while (dotStart >= 0 && longName[dotStart] != L'.') dotStart--;

		if (dotStart >= 0)
			for (int i = 1; i <= 3 && i + dotStart < longName.length(); i++)
				shortName[7 + i] = convertToShort(longName[dotStart + i], lossy);

		return lossy;
	}

	void addNumericTail(BYTE* basisName, BYTE* result, int index)
	{
		const int nameLen = 8;
		const int extLen = 3;
		string tail = "~" + to_string(index);
		int startIndex = nameLen - tail.length();
		while (startIndex > 0 && basisName[startIndex - 1] == ' ') startIndex--;
		
		memcpy(result, basisName, startIndex);
		memcpy(result + startIndex, tail.c_str(), tail.length());
		int filled = startIndex + tail.length();
		memset(result + filled, ' ', nameLen - filled);
		memcpy(result + nameLen, result + nameLen, extLen);
	}

	// распознавание элементов каталога
	// обнаружение long entry - должно быть первым шагом
	// (((LDIR_attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME) && (LDIR_Ord != 0xE5))

	// второй шаг:
	// обнаружение обычного файла
	// ((DIR_Attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == 0x00)
	// обнаружение каталога
	// ((DIR_Attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == ATTR_DIRECTORY)
	// обнаружение метки тома
	// ((DIR_Attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == ATTR_VOLUME_ID)
	// иначе элемент является некорректным

	// макс размер файла 0xFFFFFFFF
	// макс количество элементов каталога (вкл. long entries) 0xFFFF

	// если имя имеет число символов, кратное 13, то оно не оканчивается NULL'ом

	//--------------------------------------------------------------------------------------
	//Creating FS routines

	const UINT64 rsvdSecCnt = 32;
	const UINT64 numFATs = 2;
	const UINT64 rootClus = 2;

	UINT64 bytesPerSec;
	UINT64 secPerClus;
	UINT64 bytesPerClus;

	typedef function<void (PBYTE)> SectorSender;
	SectorSender sendSector;

	inline UINT64 calcCountOfClusters(UINT64 dataSec)
	{
		return dataSec / secPerClus;
	}

	inline UINT64 calcFATsz(UINT64 countOfClusters)
	{
		return (countOfClusters + rootClus) * 4 / (bytesPerSec);
	}

	inline UINT64 calcTotSec(UINT64 FATsz, UINT64 dataSec)
	{
		return rsvdSecCnt + numFATs * FATsz + dataSec;
	}

	inline wstring correctPath(wstring path)
	{
		if (path.size() > 1 && (path.back() == L'\\' || path.back() == L'/'))
			path.pop_back();
		return path;
	}

	void recEntriesNamesCorrection(FileTree::InputDirectoryEntry* entry)
	{
		entry->path = correctPath(entry->path);
		entry->name = makeCorrectLongName(entry->name);
		for (auto e : entry->children)
			recEntriesNamesCorrection(e);
	}

	void recCheckNames(FileTree::OutputDirectoryEntry* entry, wstring outputPath)
	{
		if (entry->type == FileTree::OutputEntryType::Group) return;

		if (entry->name.empty())
			reportError(L"Output file/directory with empty name at " + outputPath);
		if (outputPath.length() > 259)
			reportError(L"Output path " + outputPath + L" is more than 259 characters long");
		if (entry->name.length() > 255)
			reportError(L"Output file/directory at " + outputPath + L" has a name which is more than 255 characters long");

		for (auto c : entry->name)
			if (!isCorrectLongCharacter(c))
				reportError(L"Output file/directory at " + outputPath + L" has an invalid character in its name");

		for (auto e : entry->children)
			recCheckNames(e, outputPath + L"/" + e->name);
	}

	// возвращает количество кластеров
	UINT64 recSizeCalculation(FileTree::OutputDirectoryEntry* entry, wstring inputPath, wstring outputPath, bool root)
	{
		if (entry->type == FileTree::OutputEntryType::Group)
			return 0ULL;
		if (entry->type == FileTree::OutputEntryType::File)
		{
			if (entry->size > FATstruc::MAX_FILE_LENGTH)
				reportError(L"Input file " + inputPath + L" is more than 2^32-1 bytes long");
			return ceildiv(entry->size, bytesPerClus);
		}

		UINT64 res = 0;
		entry->size = root ? 0 : 2; //2 - директории . и ..
		// VOLUME ID у нас тут не будет
		for (auto e : entry->children)
		{
			auto eres = recSizeCalculation(e, FileTree::recoverInputPath(inputPath, entry, e), outputPath + L"/" + e->name, false);
			res += eres;
			if (e->type != FileTree::OutputEntryType::Group)
				entry->size += ceildiv(entry->name.length(), FATstruc::LONG_COMPONENT_CHARSLENGTH) + 1; // + 1 - short entry
		}
		if (entry->size > FATstruc::MAX_DIR_ENTRIES_COUNT)
			reportError(L"Output directory " + outputPath + L" has too many children or their names are summary too long");
		if (entry->size == 0)
			entry->size = 1;

		entry->size *= sizeof(FATstruc::DirectoryEntry);
		return res + ceildiv(entry->size, bytesPerClus);
	}

	BYTE* buffer;
	DWORD bufferIndex;
	UINT64 bufferSent;
	DWORD freeCluster;
	WORD currTime, currDate;

	template<typename T> T& selectPlace()
	{
		UINT64 bufferLength = bytesPerSec / sizeof(T);

		if (bufferIndex == bufferLength)
		{
			bufferSent++;
			bufferIndex = 0;
			sendSector(buffer);
			memset(buffer, 0, bytesPerSec);
		}

		return ((T*)buffer)[bufferIndex++];
	}

	void startSending()
	{
		bufferIndex = 0;
		bufferSent = 0;
		memset(buffer, 0, bytesPerSec);
	}

	void endSending(UINT64 needToSend)
	{
		while (bufferSent++ < needToSend)
		{
			sendSector(buffer);
			if (bufferSent < needToSend)
				memset(buffer, 0, bytesPerSec);
		}
	}

	void recWriteClusterChains(FileTree::OutputDirectoryEntry* entry)
	{
		if (entry->size == 0) return;

		auto clusSize = ceildiv(entry->size, bytesPerClus);
		entry->startClus = freeCluster;
		for (int i = 1; i < clusSize; i++)
			selectPlace<DWORD>() = freeCluster + i;

		selectPlace<DWORD>() = FATstruc::EOC_CLUSTER;
		freeCluster += clusSize;

		for (auto e : entry->children)
			recWriteClusterChains(e);
	}

	void setNameChar(FATstruc::LongDirectoryEntry& entry, DWORD pos, WORD c)
	{
		const DWORD sz1 = sizeof(entry.LDIR_Name1) / sizeof(c);
		const DWORD sz2 = sizeof(entry.LDIR_Name2) / sizeof(c);

		if (pos < sz1)
			entry.LDIR_Name1[pos] = c;
		else
		{
			pos -= sz1;
			if (pos < sz2)
				entry.LDIR_Name2[pos] = c;
			else
				entry.LDIR_Name3[pos - sz2] = c;
		}
	}

	wstring getShortName(BYTE* shortName)
	{
		wstring result = L"";
		for (int i = 0; i < 8; i++)
			if (shortName[i] != ' ')
				result.push_back((wchar_t)shortName[i]);
		if (shortName[8] == ' ') return result;
		result.push_back('.');
		for (int i = 8; i < 11; i++)
			if (shortName[i] != ' ')
				result.push_back((wchar_t)shortName[i]);
		return result;
	}

	void recWriteClusters(FileTree::OutputDirectoryEntry* entry, FileTree::OutputDirectoryEntry* parent, wstring inputPath, wstring fullShortPath)
	{
		if (entry->type == FileTree::OutputEntryType::Group) return;

		if (entry->type == FileTree::OutputEntryType::File)
		{
			HANDLE hFile = CreateFileW(inputPath.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if (hFile == INVALID_HANDLE_VALUE)
				reportError(L"Cannot open file " + inputPath);

			DWORD bytesRead = 0;
			DWORD sent = 0;
			while ((ReadFile(hFile, buffer, bytesPerSec, &bytesRead, 0) | 1) && (bytesRead | (sent++) % secPerClus))
				sendSector(buffer);
			CloseHandle(hFile);
			return;
		}

		startSending();

		const int maxShortPath = 80;
		wstring incorrectShortPath = L" is more than " + to_wstring(maxShortPath) + L" characters long";
		fullShortPath.push_back(L'/');

		if (parent)
		{
			if (fullShortPath.length() + 3 > maxShortPath)
				reportError(L"Path " + fullShortPath + L".." + incorrectShortPath);

			auto& dotEntry = selectPlace<FATstruc::ShortDirectoryEntry>();
			memcpy(dotEntry.DIR_Name, ".          ", 11);
			dotEntry.DIR_Attr = FATstruc::ATTR_DIRECTORY;
			dotEntry.DIR_CrtTime = currTime;
			dotEntry.DIR_CrtDate = currDate;
			dotEntry.DIR_LstAccDate = currDate;
			dotEntry.DIR_WrtTime = currTime;
			dotEntry.DIR_WrtDate = currDate;
			dotEntry.DIR_FstClusLO = LOWORD(entry->startClus);
			dotEntry.DIR_FstClusHI = HIWORD(entry->startClus);

			auto& dotdotEntry = selectPlace<FATstruc::ShortDirectoryEntry>();
			memcpy(dotdotEntry.DIR_Name, "..         ", 11);
			dotdotEntry.DIR_Attr = FATstruc::ATTR_DIRECTORY;
			dotdotEntry.DIR_CrtTime = currTime;
			dotdotEntry.DIR_CrtDate = currDate;
			dotdotEntry.DIR_LstAccDate = currDate;
			dotdotEntry.DIR_WrtTime = currTime;
			dotdotEntry.DIR_WrtDate = currDate;
			dotdotEntry.DIR_FstClusLO = LOWORD(parent->startClus);
			dotdotEntry.DIR_FstClusHI = HIWORD(parent->startClus);
		}

		set<string> shortNamePull;
		queue<wstring> shortPaths;

		for (auto e : entry->children)
		{
			if (e->type == FileTree::OutputEntryType::Group)
				continue;

			// short entry creation
			FATstruc::ShortDirectoryEntry shortEntry;
			memset(&shortEntry, 0, sizeof(shortEntry));
			BYTE basisName[12];
			BYTE shortName[sizeof(basisName) / sizeof(basisName[0])];
			memset(basisName, 0, sizeof(basisName));
			memset(shortName, 0, sizeof(shortName));
			bool lossy = makeBasisName(e->name, basisName);
			int nameTail = 0;

			if (lossy)
				addNumericTail(basisName, shortName, ++nameTail);
			else
				memcpy(shortName, basisName, sizeof(basisName));

			// всё нормально, shortName завершается нулём (12 + memset)
			while (shortNamePull.count(string((char*)shortName)))
				addNumericTail(basisName, shortName, ++nameTail);
			shortNamePull.insert(string((char*)shortName));
			memcpy(shortEntry.DIR_Name, shortName, sizeof(shortEntry.DIR_Name));

			wstring childShortPath = fullShortPath + getShortName(shortEntry.DIR_Name);
			if(childShortPath.length() > maxShortPath)
				reportError(L"Path " + childShortPath + incorrectShortPath);
			shortPaths.push(childShortPath);

			if (e->type == FileTree::OutputEntryType::File)
				shortEntry.DIR_FileSize = e->size;
			else
				shortEntry.DIR_Attr = FATstruc::ATTR_DIRECTORY;

			shortEntry.DIR_CrtTime = currTime;
			shortEntry.DIR_CrtDate = currDate;
			shortEntry.DIR_LstAccDate = currDate;
			shortEntry.DIR_WrtTime = currTime;
			shortEntry.DIR_WrtDate = currDate;
			shortEntry.DIR_FstClusLO = LOWORD(e->startClus);
			shortEntry.DIR_FstClusHI = HIWORD(e->startClus);
			BYTE chksum = ChkSum(shortEntry.DIR_Name);

			// long entries setting
			DWORD normCompCnt = e->name.length() / FATstruc::LONG_COMPONENT_CHARSLENGTH;
			BYTE mask = FATstruc::LAST_LONG_ENTRY;
			DWORD end = FATstruc::LONG_COMPONENT_CHARSLENGTH * normCompCnt;

			if (end < e->name.length())
			{
				auto& tailLongEntry = selectPlace<FATstruc::LongDirectoryEntry>();
				tailLongEntry.LDIR_Ord = ((normCompCnt + 1) | mask);
				mask = 0;
				tailLongEntry.LDIR_Attr = FATstruc::ATTR_LONG_NAME;
				tailLongEntry.LDIR_Chksum = chksum;

				DWORD delta = e->name.length() - end;
				for (DWORD i = 0; i < delta; i++)
					setNameChar(tailLongEntry, i, e->name[end + i]);
				setNameChar(tailLongEntry, delta, 0);
				for (DWORD i = delta + 1; i < FATstruc::LONG_COMPONENT_CHARSLENGTH; i++)
					setNameChar(tailLongEntry, i, 0xFFFFU);
			}

			while (end > 0)
			{
				auto& longEntry = selectPlace<FATstruc::LongDirectoryEntry>();
				longEntry.LDIR_Ord = normCompCnt-- | mask;
				mask = 0;
				longEntry.LDIR_Attr = FATstruc::ATTR_LONG_NAME;
				longEntry.LDIR_Chksum = chksum;

				DWORD begin = end - FATstruc::LONG_COMPONENT_CHARSLENGTH;
				for (DWORD i = begin; i < end; i++)
					setNameChar(longEntry, i - begin, e->name[i]);
				end = begin;
			}

			selectPlace<FATstruc::ShortDirectoryEntry>() = shortEntry;
		}

		endSending(ceildiv(entry->size, bytesPerClus) * secPerClus);

		for (auto e : entry->children)
		{
			recWriteClusters(e, entry, FileTree::recoverInputPath(inputPath, entry, e), shortPaths.front());
			if(e->type != FileTree::OutputEntryType::Group)
				shortPaths.pop();
		}
	}

	void createFAT(
		FileTree::InputDirectoryEntry* inputRoot, 
		SectorSender sectorSender, 
		UINT64 _bytesPerSec,
		UINT64 secPerTrack, 
		UINT64 numHeads,
		UINT64 totSec,
		UINT64 LBAOffset)
	{
		if(totSec > 0xFFFFFFFFU)
			reportError(L"Volume is too large to be formatted into FAT32");

		sendSector = sectorSender;
		bytesPerSec = _bytesPerSec;
		secPerClus = max(1, 4096 / bytesPerSec);
		bytesPerClus = secPerClus * bytesPerSec;

		// binSearch for dataSec
		UINT64 l = 0;
		UINT64 r = totSec;

		UINT64 FATsz, total, totClus;
		while (r - l > 1)
		{
			UINT64 mid = (l + r) / 2;
			totClus = calcCountOfClusters(mid);
			FATsz = calcFATsz(totClus);
			total = calcTotSec(FATsz, mid);
			if (total < totSec)
				l = mid;
			else
				r = mid;
		}

		totClus = calcCountOfClusters(r);
		FATsz = calcFATsz(totClus);
		total = calcTotSec(FATsz, r);
		UINT64 dataSec = r;
		if (total >= totSec)
		{
			totClus = calcCountOfClusters(l);
			FATsz = calcFATsz(totClus);
			total = calcTotSec(FATsz, l);
			dataSec = l;
			if (total >= totSec)
				reportError(L"Volume is too small to be formatted into FAT32");
		}

		if (totClus <= 0xFFFF)
			reportError(L"Volume is too small to be formatted into FAT32");

		if (totClus >= FATstruc::MAX_CLUSTER_VALUE - rootClus)
			reportError(L"Volume is too large to be formatted into FAT32");

		recEntriesNamesCorrection(inputRoot);
		auto root = FileTree::prepare(inputRoot, L"");

		recCheckNames(root, L"");

		auto sizeCalcError = recSizeCalculation(root, L"", L"", true);
		if (sizeCalcError > totClus)
			reportError(L"Volume is too small to write all the files on it");

		// все размеры рассчитаны, ошибки обнаружены
		// непосредственная запись
		FATstruc::BootSector bootSector;
		memset(&bootSector, 0, sizeof(bootSector));

		bootSector.BS_jmpBoot[0] = 0xEBU;
		bootSector.BS_jmpBoot[1] = 0x58U;
		bootSector.BS_jmpBoot[2] = 0x90U;

		memcpy(bootSector.BS_OEMName, "MSWIN4.1", 8);
		bootSector.BPB_BytsPerSec = bytesPerSec;
		bootSector.BPB_SecPerClus = secPerClus;
		bootSector.BPB_RsvdSecCnt = rsvdSecCnt;
		bootSector.BPB_NumFATs = numFATs;
		bootSector.BPB_Media = 0xF8U;

		const UINT64 LIM_WORD = 0xFFFFU;
		const UINT64 LIM_DWORD = 0xFFFFFFFFU;
		bootSector.BPB_SecPerTrk = min(secPerTrack, LIM_WORD);
		bootSector.BPB_NumHeads = min(numHeads, LIM_WORD);
		bootSector.BPB_HiddSec = min(LBAOffset, LIM_DWORD);
		bootSector.BPB_TotSec32 = totSec;
		bootSector.BPB_FATSz32 = FATsz;
		bootSector.BPB_RootClus = rootClus;
		bootSector.BPB_FSInfo = 1;
		bootSector.BPB_BkBootSec = 6;
		bootSector.BS_DrvNum = 0x80U;
		bootSector.BS_BootSig = 0x29U;

		auto _currdatetime = time(0);
		auto currdatetime = localtime(&_currdatetime);
		currTime = makeTime(currdatetime->tm_sec, currdatetime->tm_min, currdatetime->tm_hour);
		const int deltaYear = 0 + 1900 - 1980;
		currDate = makeDate(currdatetime->tm_mday, currdatetime->tm_mon + 1, currdatetime->tm_year + deltaYear);
		bootSector.BS_VolID = combine(currTime, currDate);

		memcpy(bootSector.BS_VolLab, "NO NAME    ", 11);
		memcpy(bootSector.BS_FilSysType, "FAT32   ", 8);
		bootSector.BS_Signature = 0xAA55U;

		buffer = new BYTE[bytesPerSec];
		memcpy(buffer, &bootSector, sizeof(bootSector));
		memset(buffer + sizeof(bootSector), 0, bytesPerSec - sizeof(bootSector));
		sendSector(buffer);

		FATstruc::FSInfo fsinfo;
		memset(&fsinfo, 0, sizeof(fsinfo));
		fsinfo.FSI_LeadSig = 0x41615252U;
		fsinfo.FSI_StrucSig = 0x61417272U;
		fsinfo.FSI_Free_Count = 0xFFFFFFFFU;
		fsinfo.FSI_Nxt_Free = 0xFFFFFFFFU;
		fsinfo.FSI_TrailSig = 0xAA550000U;
		memcpy(buffer, &fsinfo, sizeof(fsinfo));
		memset(buffer + sizeof(fsinfo), 0, bytesPerSec - sizeof(fsinfo));
		sendSector(buffer);

		memset(buffer, 0, bytesPerSec);
		buffer[510] = 0x55U;
		buffer[511] = 0xAAU;
		sendSector(buffer);
		buffer[510] = 0;
		buffer[511] = 0;

		for (int i = 3; i < bootSector.BPB_BkBootSec; i++)
			sendSector(buffer);

		//backup
		memcpy(buffer, &bootSector, sizeof(bootSector));
		memset(buffer + sizeof(bootSector), 0, bytesPerSec - sizeof(bootSector));
		sendSector(buffer);

		memcpy(buffer, &fsinfo, sizeof(fsinfo));
		memset(buffer + sizeof(fsinfo), 0, bytesPerSec - sizeof(fsinfo));
		sendSector(buffer);

		memset(buffer, 0, bytesPerSec);
		buffer[510] = 0x55U;
		buffer[511] = 0xAAU;
		sendSector(buffer);
		buffer[510] = 0;
		buffer[511] = 0;

		for (int i = 3 + bootSector.BPB_BkBootSec; i < bootSector.BPB_RsvdSecCnt; i++)
			sendSector(buffer);
		
		// FATs
		for (int i = 0; i < 2; i++)
		{
			freeCluster = rootClus;
			startSending();
			selectPlace<DWORD>() = (FATstruc::CLUSTER_VALUE_MASK ^ 0xFFU ^ bootSector.BPB_Media);
			selectPlace<DWORD>() = FATstruc::EOC_CLUSTER;
			recWriteClusterChains(root);
			endSending(FATsz);
		}

		// data
		recWriteClusters(root, nullptr, L"", L"");
		FileTree::deepDelete(root);
	}
}

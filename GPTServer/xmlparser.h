#pragma once
#include "includes.h"

const wstring nodeRoot = L"project";
const wstring attrDisk = L"disk";
const wstring attrDiskSure = L"disk-sure";
const wstring attrImg = L"img";
const wstring attrImgSize = L"img-size";

const wstring nodePart = L"partition";
const wstring attrType = L"type";
const wstring attrFS = L"filesystem";
const wstring attrSize = L"size";

const wstring attrName = L"name";
const wstring attrPath = L"path";
const wstring nodeDir = L"directory";
const wstring nodeCopy = L"copy";
const wstring nodeFrom = L"copy-from";

const wstring valueAll = L"*";
const wstring valueTrue = L"true";
const wstring valueFalse = L"false";
const wstring valueESP = L"ESP";
const wstring valueDATA = L"DATA";
const wstring unitKB = L"kb";
const wstring unitMB = L"mb";
const wstring unitGB = L"gb";

enum class InputEntryType
{
	Directory, // (attr: name)
	Copy, // (attr: path, name)
	CopyFrom, // (attr: path)
};

struct InputDirectoryEntry
{
	InputEntryType type;
	wstring name;
	wstring path;
	vector<InputDirectoryEntry*> children;

	InputDirectoryEntry() : type(), name(), path(), children() {}
};

enum class PartitionType {ESP, DATA};
enum class SizeType {UNDEFINED, ALL, KB, MB, GB};

struct Size
{
	SizeType type;
	UINT64 value;

	Size() : type(SizeType::UNDEFINED) {}
	Size(SizeType type) : type(type) {}
	Size(SizeType type, UINT64 value) : type(type), value(value) {}
};

struct PartitionInfo
{
	PartitionType type;
	wstring fileSystem;
	Size size;
	InputDirectoryEntry* root;

	PartitionInfo() : type(PartitionType::DATA), fileSystem(L"FAT32"), size(), root(nullptr) {}
};

const int PARSER_NO_DISK = -1;

struct ProjectInfo
{
	int disk;
	bool diskSure;
	wstring img;
	Size imgSize;
	vector<PartitionInfo> partitions;

	ProjectInfo() : 
		disk(PARSER_NO_DISK),
		diskSure(false), 
		img(), 
		imgSize(),
		partitions()
	{}
};

int getPositiveIntValue(const wstring& attributeName, const wchar_t* value)
{
	try
	{
		int r = stoi(value);
		if (r < 1) throw 228;
		return r;
	}
	catch (...)
	{
		reportError(attributeName + L" must be valid positive integer value");
		return 0;
	}
}

bool getBoolValue(const wstring& attributeName, const wchar_t* value)
{
	if (value == valueTrue)
		return true;
	if (value == valueFalse)
		return false;
	reportError(attributeName + L" must be false or true");
	return false;
}

wstring getNonEmptyWstringValue(const wstring& attributeName, wstring value)
{
	if (value.empty())
		reportError(attributeName + L" must be non-empty");
	return value;
}

Size getSize(const wstring& attributeName, const wchar_t* value)
{
	string numpart;
	wstring unitpart;
	
	while (*value >= L'0' && *value <= L'9')
		numpart.push_back((char)*(value++));
	while (*value)
		unitpart.push_back(*(value++));

	if (unitpart == valueAll)
	{
		if (numpart.empty()) return Size(SizeType::ALL);
		reportError(L"Invalid " + attributeName + L" value");
	}

	UINT64 sizeval;
	try
	{
		sizeval = stoll(numpart);
	}
	catch (...)
	{
		reportError(L"Numeric part of " + attributeName + L" is too big to be parsed");
	}

	if (sizeval == 0)
		reportError(L"Numeric part of " + attributeName + L" must be non-zero");

	if (unitpart == unitKB)
		return Size(SizeType::KB, sizeval);
	if (unitpart == unitMB)
		return Size(SizeType::MB, sizeval);
	if (unitpart == unitGB)
		return Size(SizeType::GB, sizeval);
	reportError(L"Incorrect " + attributeName + L" value");
	return Size();
}

InputDirectoryEntry* parseDirectory(xml::xml_node<wchar_t>* node)
{
	InputDirectoryEntry* entry = new InputDirectoryEntry;
	for (auto child = node->first_node(); child; child = child->next_sibling())
	{
		if (child->name() == nodeDir)
		{
			auto e = parseDirectory(child);
			entry->children.push_back(e);
			e->type = InputEntryType::Directory;
			for (auto attr = child->first_attribute(); attr; attr = attr->next_attribute())
			{
				if (attr->name() == attrName)
					e->name = attr->value();
				else
					reportError(L"Unexpected attribute named " + wstring(attr->name()));
			}
		}
		else if (child->name() == nodeCopy)
		{
			auto e = new InputDirectoryEntry;
			entry->children.push_back(e);
			e->type = InputEntryType::Copy;
			for (auto attr = child->first_attribute(); attr; attr = attr->next_attribute())
			{
				if (attr->name() == attrName)
					e->name = attr->value();
				else if (attr->name() == attrPath)
					e->path = attr->value();
				else
					reportError(L"Unexpected attribute named " + wstring(attr->name()));
			}
		}
		else if (child->name() == nodeFrom)
		{
			auto e = new InputDirectoryEntry;
			entry->children.push_back(e);
			e->type = InputEntryType::CopyFrom;
			for (auto attr = child->first_attribute(); attr; attr = attr->next_attribute())
			{
				if (attr->name() == attrPath)
					e->path = attr->value();
				else
					reportError(L"Unexpected attribute named " + wstring(attr->name()));
			}
		}
		else
			reportError(L"Unexpected node named " + wstring(child->name()));
	}
	return entry;
}

PartitionInfo parsePartition(xml::xml_node<wchar_t>* node)
{
	PartitionInfo part;
	for (auto attr = node->first_attribute(); attr; attr = attr->next_attribute())
	{
		if (attr->name() == attrType)
		{
			if (attr->value() == valueESP)
				part.type = PartitionType::ESP;
			else if (attr->value() == valueDATA)
				part.type = PartitionType::DATA;
			else
				reportError(L"No other types of partiotion than ESP and DATA are supported");
		}
		else if (attr->name() == attrFS)
			part.fileSystem = attr->value();
		else if (attr->name() == attrSize)
			part.size = getSize(attrSize, attr->value());
		else
			reportError(L"Unexpected attribute named " + wstring(attr->name()));
	}

	part.root = parseDirectory(node);
	part.root->name = L"root";
	part.root->type = InputEntryType::Directory;
	return part;
}

ProjectInfo parse(const wchar_t* fileName)
{
	HANDLE hFile = CreateFileW(fileName, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		reportError(L"Cannot open project file");
	LARGE_INTEGER lisize;
	auto& size = lisize.QuadPart;
	if (!GetFileSizeEx(hFile, &lisize))
		reportError(L"Cannot get project file size");
	char* multibyteTXT = new char[size + 1];
	memset(multibyteTXT, 0, size + 1);
	DWORD rw = 0;
	if (!ReadFile(hFile, multibyteTXT, size, &rw, 0))
		reportError(L"Cannot read project file");
	CloseHandle(hFile);

	wchar_t* txt = new wchar_t[size + 1];
	memset(txt, 0, size + 1);
	int len = MultiByteToWideChar(CP_UTF8, 0, multibyteTXT, size, txt, size);
	if (!len)
		reportError(L"Cannot use project file");
	delete[] multibyteTXT;
	txt[len] = 0;

	xml::xml_document<wchar_t> doc;
	try
	{
		doc.parse<0>(txt);
	}
	catch (const std::runtime_error& e)
	{
		ofstream log("log.txt");
		log << e.what() << endl;
		log.close();
		reportError(L"Invalid project file");
	}
	catch (const rapidxml::parse_error& e)
	{
		ofstream log("log.txt");
		log << e.what() << endl;
		log.close();
		reportError(L"Invalid project file ");
	}
	catch (exception e)
	{
		ofstream log("log.txt");
		log << e.what() << endl;
		log.close();
		reportError(L"Invalid project file");
	}

	auto rootNode = doc.first_node();
	if (rootNode->name() != nodeRoot)
		reportError(L"First node must be named " + nodeRoot);
	ProjectInfo project;

	for (auto attr = rootNode->first_attribute(); attr; attr = attr->next_attribute())
	{
		if (attr->name() == attrDisk)
			project.disk = getPositiveIntValue(attrDisk, attr->value());
		else if (attr->name() == attrDiskSure)
			project.diskSure = getBoolValue(attrDiskSure, attr->value());
		else if (attr->name() == attrImg)
			project.img = getNonEmptyWstringValue(attrImg, attr->value());
		else if (attr->name() == attrImgSize)
		{
			project.imgSize = getSize(attrImgSize, attr->value());
			if (project.imgSize.type == SizeType::ALL)
				reportError(attrImgSize + L" must not be *");
		}
		else
			reportError(L"Unexpected attribute named " + wstring(attr->name()));
	}

	for (auto part = rootNode->first_node(); part; part = part->next_sibling())
		if (part->name() == nodePart)
			project.partitions.push_back(parsePartition(part));
		else
			reportError(L"Unexpected node named " + wstring(part->name()));
	delete[] txt;
	return project;
}



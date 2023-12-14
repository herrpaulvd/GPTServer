#pragma once
#include "includes.h"

namespace FileTree
{
	enum class InputEntryType
	{
		Directory, // (attr: name)
		Copy, // (attr: path, name)
		CopyFrom, // (attr: path)
	};

	enum class OutputEntryType
	{
		IntermediateDirectory, // (name)
		Directory, // (name, path)
		File, // (name, path)
		Group // (path)
	};

	UINT64 nxtgroupID = 0;
	wstring getGroupName()
	{
		return L":/:" + to_wstring(nxtgroupID++);
	}

	struct InputDirectoryEntry
	{
		InputEntryType type;
		wstring name;
		wstring path;
		vector<InputDirectoryEntry*> children;

		InputDirectoryEntry() : type(), name(), path(), children() {}
	};

	struct OutputDirectoryEntry
	{
		OutputEntryType type;
		wstring name;
		wstring path;
		UINT64 size;
		DWORD startClus;
		vector<OutputDirectoryEntry*> children;
		OutputDirectoryEntry* group;

		OutputDirectoryEntry() : type(), name(), path(), size(0), startClus(0), children(), group(nullptr) {}
	};

	// deleting trees
	void deepDelete(InputDirectoryEntry* entry)
	{
		for (auto e : entry->children)
			deepDelete(e);
		delete entry;
	}

	void deepDelete(OutputDirectoryEntry* entry)
	{
		for (auto e : entry->children)
			deepDelete(e);
		delete entry;
	}

	bool less(OutputDirectoryEntry* a, OutputDirectoryEntry* b)
	{
		wstring astr = a->name, bstr = b->name;
		transform(a->name.begin(), a->name.end(), astr.begin(), towupper);
		transform(b->name.begin(), b->name.end(), bstr.begin(), towupper);
		return astr < bstr;
	}

	OutputDirectoryEntry* unpack(fs::directory_entry src, wstring name, wstring path, bool isGroup, OutputDirectoryEntry* group)
	{
		if (!src.exists())
			reportError(L"Input file/directory " + src.path().wstring() + L" not found");

		if (isGroup && !src.is_directory())
			reportError(L"Input directory expected: " + src.path().wstring());

		OutputDirectoryEntry* res = new OutputDirectoryEntry();
		res->type = isGroup ? OutputEntryType::Group : (src.is_directory() ? OutputEntryType::Directory : OutputEntryType::File);
		res->name = isGroup ? getGroupName() : name;
		res->path = path;
		res->group = group;

		if (src.is_directory())
		{
			for (auto& subItem : fs::directory_iterator(src))
			{
				auto chldres = unpack(subItem, subItem.path().filename().wstring(), L"~", false, res);
				res->children.push_back(chldres);
			}
		}
		else
			res->size = src.file_size();

		return res;
	}

	OutputDirectoryEntry* prepare(InputDirectoryEntry* entry, wstring outputPath)
	{
		if (entry->type == InputEntryType::Directory)
		{
			OutputDirectoryEntry* res = new OutputDirectoryEntry();
			res->type = OutputEntryType::IntermediateDirectory;
			res->name = entry->name;
			auto comp = function(less);
			set<OutputDirectoryEntry*, decltype(comp)> entriesSet(comp);
			for (auto c : entry->children)
			{
				auto chldres = prepare(c, outputPath + L"/" + c->name);

				if (entriesSet.count(chldres))
					reportError(L"Duplicate names (" + chldres->name + L") at the output directory " + (outputPath.empty() ? wstring(L"/") : outputPath));

				entriesSet.insert(chldres);

				if (chldres->type == OutputEntryType::Group)
				{
					for (auto groupItem : chldres->children)
					{
						if (entriesSet.count(groupItem))
							reportError(L"Duplicate names (" + groupItem->name + L") at the output directory " + (outputPath.empty() ? wstring(L"/") : outputPath));
					}
					entriesSet.insert(chldres->children.begin(), chldres->children.end());
					chldres->children.clear();
				}
			}

			for (auto e : entriesSet)
				res->children.push_back(e);

			return res;
		}

		return unpack(fs::directory_entry(fs::path(entry->path)), entry->name, entry->path, entry->type == InputEntryType::CopyFrom, nullptr);
	}

	wstring recoverInputPath(wstring& parentInputPath, OutputDirectoryEntry* parent, OutputDirectoryEntry* child)
	{
		if (child->type == OutputEntryType::IntermediateDirectory) return L"";

		if (parent->type == OutputEntryType::IntermediateDirectory)
		{
			if (child->group)
				return child->group->path + L"\\" + child->name;
			return child->path;
		}

		return parentInputPath + L"\\" + child->name;
	}
}

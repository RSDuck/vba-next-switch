/******************************************************************************* 
 *  -- Cellframework -  Open framework to abstract the common tasks related to
 *                      PS3 application development.
 *
 *  Copyright (C) 2010-2011
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ********************************************************************************/

/******************************************************************************* 
 * FileBrowser.cpp - Cellframework
 *
 *  Created on:		Oct 29, 2010
 *  Last updated:	 
 ********************************************************************************/

#include "FileBrowser.hpp"

#include <algorithm>

#define delete_current_entry_set() \
	std::vector<DirectoryEntry*>::const_iterator iter; \
	for(iter = _cur.begin(); iter != _cur.end(); ++iter) \
		delete (*iter); \
	\
	_cur.clear();

// yeah sucks, not using const
static bool less_than_key(DirectoryEntry* a, DirectoryEntry* b)
{
	// dir compare to file, let us always make the dir less than
	if ((a->d_type == CELL_FS_TYPE_DIRECTORY && b->d_type == CELL_FS_TYPE_REGULAR))
		return true;
	else if (a->d_type == CELL_FS_TYPE_REGULAR && b->d_type == CELL_FS_TYPE_DIRECTORY)
		return false;

	// FIXME: add a way to customize sorting someday
	// 	also add a ignore filename, sort by extension

	// use this to ignore extension
	if (a->d_type == CELL_FS_TYPE_REGULAR && b->d_type == CELL_FS_TYPE_REGULAR)
	{
		char *pIndex1 = strrchr(a->d_name, '.');
		char *pIndex2 = strrchr(b->d_name, '.');

		// null the dots
		if (pIndex1 != NULL)
			*pIndex1 = '\0';

		if (pIndex2 != NULL)
			*pIndex2 = '\0';

		// compare
		int retVal = strcasecmp(a->d_name, b->d_name);

		// restore the dot
		if (pIndex1 != NULL)
			*pIndex1 = '.';

		if (pIndex2 != NULL)
			*pIndex2 = '.';
		return retVal < 0;
	}

	// both dirs at this points btw
	return strcasecmp(a->d_name, b->d_name) < 0;
}

FileBrowser::FileBrowser(const char * startDir, std::string extensions)
{
	_currentSelected = 0;
	_dirStackSize = 0;
	_dir[_dirStackSize].numEntries = 0;
	m_wrap = true;

	ParseDirectory(startDir, CELL_FS_TYPE_DIRECTORY | CELL_FS_TYPE_REGULAR, extensions);
}

FileBrowser::~FileBrowser()
{
	delete_current_entry_set();
}

void FileBrowser::ResetStartDirectory(const char * startDir, std::string extensions)
{
	delete_current_entry_set();
   
	_currentSelected = 0;
	_dirStackSize = 0;
	_dir[_dirStackSize].numEntries = 0;

	ParseDirectory(startDir, CELL_FS_TYPE_DIRECTORY | CELL_FS_TYPE_REGULAR, extensions);
}

const char * FileBrowser::get_current_directory_name()
{
	return _dir[_dirStackSize].dir;
}

uint32_t FileBrowser::get_current_directory_file_count()
{
	return _dir[_dirStackSize].numEntries;
}


//FIXME: so error prone. reason: app dependent, we return NULL for cur entry in this case
void FileBrowser::GotoEntry(uint32_t i)
{
	_currentSelected = i;
}

void FileBrowser::PushDirectory(const char * path, int types, std::string extensions)
{
	_dirStackSize++;
	ParseDirectory(path, types, extensions);
}


void FileBrowser::PopDirectory()
{
	if (_dirStackSize > 0)
		_dirStackSize--;

	ParseDirectory(_dir[_dirStackSize].dir, _dir[_dirStackSize].types, _dir[_dirStackSize].extensions);
}


bool FileBrowser::ParseDirectory(const char * path, int types, std::string extensions)
{
	// current file descriptor, used for reading entries
	int _fd;
	// for extension parsing
	uint32_t index = 0;
	uint32_t lastIndex = 0;

	// bad path
	if (strcmp(path,"") == 0)
		return false;

	// delete old path
	if (!_cur.empty())
	{
		delete_current_entry_set();
	}

	// FIXME: add FsStat calls or use cellFsDirectoryEntry
	if (cellFsOpendir(path, &_fd) == CELL_FS_SUCCEEDED)
	{
		uint64_t nread = 0;

		// set new dir
		strcpy(_dir[_dirStackSize].dir, path);
		_dir[_dirStackSize].extensions = extensions;
		_dir[_dirStackSize].types = types;

		// reset num entries
		_dir[_dirStackSize].numEntries = 0;

		// reset cur selected for safety FIXME: side effect?
		_currentSelected = 0;

		// read the directory
		DirectoryEntry dirent;
		while (cellFsReaddir(_fd, &dirent, &nread) == CELL_FS_SUCCEEDED)
		{
			// no data read, something is wrong... FIXME: bad way to handle this
			if (nread == 0)
				break;

			// check for valid types
			if (dirent.d_type != (types & CELL_FS_TYPE_REGULAR) && dirent.d_type != (types & CELL_FS_TYPE_DIRECTORY))
				continue;

			// skip cur dir
			if (dirent.d_type == CELL_FS_TYPE_DIRECTORY && !(strcmp(dirent.d_name, ".")))
				continue;

			// validate extensions
			if (dirent.d_type == CELL_FS_TYPE_REGULAR)
			{
				// reset indices from last search
				index = 0;
				lastIndex = 0;

				// get this file extension
				std::string ext = FileBrowser::GetExtension(dirent.d_name);

				// assume to skip it, prove otherwise
				bool bSkip = true;

				// build the extensions to compare against
				if (extensions.size() > 0)
				{
					index = extensions.find('|', 0);

					// only 1 extension
					if (index == std::string::npos)
					{
						if (strcmp(ext.c_str(), extensions.c_str()) == 0)
							bSkip = false;
					}
					else
					{
						lastIndex = 0;
						index = extensions.find('|', 0);
						char tmp[1024];
						while (index != std::string::npos)
						{
							strcpy(tmp,extensions.substr(lastIndex, (index-lastIndex)).c_str());
							if (strcmp(ext.c_str(), tmp) == 0)
							{
								bSkip = false;
								break;
							}

							lastIndex = index + 1;
							index = extensions.find('|', index+1);
						}

						// grab the final extension
						strcpy(tmp, extensions.substr(lastIndex).c_str());
						if (strcmp(ext.c_str(), tmp) == 0)
							bSkip = false;
					}
				}
				else
					bSkip = false; // no extensions we'll take as all extensions

				if (bSkip)
				{
					continue;
				}
			}

			// AT THIS POINT WE HAVE A VALID ENTRY

			// alloc an entry
			DirectoryEntry *entry = new DirectoryEntry();
			memcpy(entry, &dirent, sizeof(DirectoryEntry));

			_cur.push_back(entry);

			// next file
			++_dir[_dirStackSize].numEntries;

			// FIXME: hack, implement proper caching + paging
		}

		cellFsClosedir(_fd);
	}
	else
		return false;

	// FIXME: hack, forces '..' to stay on top by ignoring the first entry
	// this is always '..' in dirs
	std::sort(++_cur.begin(), _cur.end(), less_than_key);

	return true;
}

void FileBrowser::SetEntryWrap(bool wrapvalue)
{
	m_wrap = wrapvalue;
}

void FileBrowser::IncrementEntry()
{
	_currentSelected++;
	if (_currentSelected >= _cur.size() && m_wrap)
		_currentSelected = 0;
}


void FileBrowser::DecrementEntry()
{
	_currentSelected--;
	if (_currentSelected >= _cur.size() && m_wrap)
		_currentSelected = _cur.size() - 1;
}


const char * FileBrowser::get_current_filename()
{
	if (_currentSelected >= _cur.size())
		return NULL;

	return _cur[_currentSelected]->d_name;
}


uint32_t FileBrowser::GetCurrentEntryIndex()
{
	return _currentSelected;
}

std::string FileBrowser::GetExtension(std::string filename)
{
	uint32_t index = filename.find_last_of(".");
	if (index != std::string::npos)
		return filename.substr(index+1);
	return "";
}

bool FileBrowser::IsCurrentAFile()
{
	if (_currentSelected >= _cur.size())
		return false;

	return _cur[_currentSelected]->d_type == CELL_FS_TYPE_REGULAR;
}


bool FileBrowser::IsCurrentADirectory()
{
	if (_currentSelected >= _cur.size())
		return false;

	return _cur[_currentSelected]->d_type == CELL_FS_TYPE_DIRECTORY;
}

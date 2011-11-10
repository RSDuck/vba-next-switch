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

static bool less_than_key(CellFsDirent* a, CellFsDirent* b)
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

	return strcasecmp(a->d_name, b->d_name) < 0;
}

static std::string filebrowser_get_extension(std::string filename)
{
	uint32_t index = filename.find_last_of(".");
	if (index != std::string::npos)
		return filename.substr(index+1);
	else
		return "";
}

static bool filebrowser_parse_directory(filebrowser_t * filebrowser, const char * path, uint32_t types, std::string extensions)
{
	int fd;
	// for extension parsing
	uint32_t index = 0;
	uint32_t lastIndex = 0;

	// bad path
	if (strcmp(path,"") == 0)
		return false;

	// delete old path
	if (!filebrowser->cur.empty())
	{
		std::vector<CellFsDirent*>::const_iterator iter;
		for(iter = filebrowser->cur.begin(); iter != filebrowser->cur.end(); ++iter)
			delete (*iter);
		filebrowser->cur.clear();
	}

	// FIXME: add FsStat calls or use cellFsDirectoryEntry
	if (cellFsOpendir(path, &fd) == CELL_FS_SUCCEEDED)
	{
		uint64_t nread = 0;

		// set new dir
		strcpy(filebrowser->dir[filebrowser->directory_stack_size].dir, path);
		filebrowser->dir[filebrowser->directory_stack_size].extensions = extensions;
		filebrowser->dir[filebrowser->directory_stack_size].types = types;

		// reset num entries
		filebrowser->dir[filebrowser->directory_stack_size].file_count = 0;

		// reset currently selected variable for safety
		filebrowser->currently_selected = 0;

		// read the directory
		CellFsDirent dirent;
		while (cellFsReaddir(fd, &dirent, &nread) == CELL_FS_SUCCEEDED)
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
				std::string ext = filebrowser_get_extension(dirent.d_name);

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

			// allocate an entry
			CellFsDirent *entry = new CellFsDirent();
			memcpy(entry, &dirent, sizeof(CellFsDirent));

			filebrowser->cur.push_back(entry);

			// next file
			++filebrowser->dir[filebrowser->directory_stack_size].file_count;
		}

		cellFsClosedir(fd);
	}
	else
		return false;

	// FIXME: hack, forces '..' to stay on top by ignoring the first entry
	// this is always '..' in dirs
	std::sort(++filebrowser->cur.begin(), filebrowser->cur.end(), less_than_key);

	return true;
}

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, std::string extensions)
{
	filebrowser->currently_selected = 0;
	filebrowser->directory_stack_size = 0;
	filebrowser->dir[filebrowser->directory_stack_size].file_count = 0;

	filebrowser_parse_directory(filebrowser, start_dir, CELL_FS_TYPE_DIRECTORY | CELL_FS_TYPE_REGULAR, extensions);
}

void filebrowser_reset_start_directory(filebrowser_t * filebrowser, const char * start_dir, std::string extensions)
{
	std::vector<CellFsDirent*>::const_iterator iter;
	for(iter = filebrowser->cur.begin(); iter != filebrowser->cur.end(); ++iter)
		delete (*iter);
	filebrowser->cur.clear();
   
	filebrowser->currently_selected = 0;
	filebrowser->directory_stack_size = 0;
	filebrowser->dir[filebrowser->directory_stack_size].file_count = 0;

	filebrowser_parse_directory(filebrowser, start_dir, CELL_FS_TYPE_DIRECTORY | CELL_FS_TYPE_REGULAR, extensions);
}

void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path, uint32_t types, std::string extensions)
{
	filebrowser->directory_stack_size++;
	filebrowser_parse_directory(filebrowser, path, types, extensions);
}


void filebrowser_pop_directory (filebrowser_t * filebrowser)
{
	if (filebrowser->directory_stack_size > 0)
		filebrowser->directory_stack_size--;

	filebrowser_parse_directory(filebrowser, filebrowser->dir[filebrowser->directory_stack_size].dir, filebrowser->dir[filebrowser->directory_stack_size].types, filebrowser->dir[filebrowser->directory_stack_size].extensions);
}

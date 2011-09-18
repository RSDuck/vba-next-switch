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

#ifndef FILEBROWSER_H_
#define FILEBROWSER_H_

#define MAXJOLIET	255
#define MAX_PATH_LENGTH	1024
#define STRING_NPOS	-1

#include <cell/cell_fs.h>

#include <string>
#include <vector>

#include <sys/types.h>

#include "../../emu-ps3-constants.h"


//FIXME: shouldnt need this, ps3 has its own CellFsDirEnt and CellFsDirectoryEntry
//		-- the latter should be switched to eventually
struct DirectoryInfo
{
        int types;
        int size;
        uint32_t numEntries;
	char dir[MAX_PATH_LENGTH];
	std::string extensions;
};

typedef CellFsDirent DirectoryEntry;

class FileBrowser
{
	public:
	FileBrowser(const char * startDir, std::string extensions);
	~FileBrowser();

	std::vector<DirectoryEntry*> _cur;		// current file listing

	bool IsCurrentAFile();
	bool IsCurrentADirectory();
	const char * get_current_filename();
	const char * get_current_directory_name();
	uint32_t get_current_directory_file_count();
	uint32_t GetCurrentEntryIndex();
	static std::string GetExtension(std::string filename);
	void DecrementEntry();
	void GotoEntry(uint32_t i);
	void IncrementEntry();
	void ResetStartDirectory(const char * startDir, std::string extensions);
	void SetEntryWrap(bool wrapvalue);		// Set wrapping on or off
	void PushDirectory(const char * path, int types, std::string extensions);
	void PopDirectory();
	private:
	uint32_t _currentSelected;			// currently select browser entry
	bool m_wrap;					// wrap boolean variable
	DirectoryInfo _dir[128];			// info of the current directory
	int _dirStackSize;

	bool ParseDirectory(const char * path, int types, std::string extensions);
};


#endif /* FILEBROWSER_H_ */

// Copyright (c) 2007-2021 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "CacheBase.h"
#include "FileCacheSet.h"

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

// Returns one character at a time from the object id.  This allows
// the caller to build the path with some chars in different path
// components.
char
GetCharNFromObjectId(const cachedObjectId_t objectId, const int n)
{

	// Shift the mask into the requested character position
	cachedObjectId_t charMask = ((cachedObjectId_t) s_mask << (n * s_maskSize));

	// Mask and shift to get a 6 bit value used to index into the
	// encoding array
	size_t index = (size_t)(((objectId & charMask) >> (n * s_maskSize)));
	if (index > strlen(s_charMapping))
	{
		return ' ';
	}
	if (index > strlen(s_charMapping))
	{
		return ' ';
	}

	// Return the indexed value from the encoding array
	return s_charMapping[index];
}

// Returns the index of the character in the encoding array.  This
// index is the value of the 6 bits of the object id represented by
// this character.
paramValue_t
GetValueForChar(const int c)
{

	// find where in the mapping array the current character resides.
	const char *indexChar = index(s_charMapping, c);

	// If we don't find a match, there is an error since the encoding is
	// 1 to 1.
	if (indexChar == NULL)
	{
		return -1;
	}

	// The value at this character location is the index into the
	// encoding array.
	return (paramValue_t)(indexChar - s_charMapping);
}

// Returns the object id from the path.  This assumes the path is of
// the form
// /dir/subdir-1/.../typeName/objectid[0:m]/objectid[m+1:n].extension
// where extension does not include a '/' or a '.'.
cachedObjectId_t
GetObjectIdFromPath(const char *filePath)
{

	long endPos;

	// find the position of the last period that indicates the end of
	// the object id and the start of the extension
	const char *endChar = rindex(filePath, (int)'.');
	if (endChar == NULL)
	{
		endPos = (int) strlen(filePath);
	}
	else
	{
		endPos = endChar - filePath;
	}

	// The starting position is the ending position minus the number of
	// characters in the object id minus 1 for the directory '/'.
	long curPos = endPos - s_numChars - 1;

	// Walk the string and generate the object id char by char.
	int i = 0;
	bool foundDelimiter = false;
	cachedObjectId_t objectId = 0;
	while (curPos < endPos)
	{

		// Skip if this is the directory marker
		if (strncmp(&(filePath[curPos]), "/", 1))
		{

			// Get the value for the current character and make sure it's
			// not -1
			paramValue_t tmp =  GetValueForChar((int)filePath[curPos]);
			if (tmp < 0)
			{
				return 0;
			}
			cachedObjectId_t value = (cachedObjectId_t) tmp;

			// compute the mask shift value for this character index.
			int shiftValue = (s_numChars - i - 1) * s_maskSize;

			// accumulate the object id from each characters value.
			objectId += (value << shiftValue);
			i++;
		}
		else
		{
			// Make sure the delimiter is in the correct position or else
			// abort the loop
			if (i == s_dirChars)
			{
				foundDelimiter = true;
			}
			else
			{
				curPos = endPos;
			}
		}
		curPos++;
	}

	if (!foundDelimiter || (i != s_numChars))
	{
		objectId = 0;
	}

	return objectId;
}

// Returns the typeName from the path.  This assumes the path is of
// the form
// /dir/subdir-1/.../typeName/objectid[0:m]/objectid[m+1:n].extension
const std::string
GetTypeNameFromPath(const std::string &baseDirName,
                    const std::string &filePath)
{

	std::string typeName;
	if (baseDirName == filePath.substr(0, baseDirName.length()))
	{
		std::string::size_type startPos = baseDirName.length() + 1;
		std::string::size_type endPos = filePath.find('/', startPos);
		if ((endPos != std::string::npos) && (endPos > startPos))
		{
			typeName = filePath.substr(startPos, endPos - startPos);
		}
	}

	return typeName;
}

// returns the directory path for the cached object from the complete
// pathname.  This assumes the path is of the form
// /dir/subdir-1/.../typeName/objectid[0:m]/objectid[m+1:n].extension
// where extension does not include a '/' or a '.'.
const std::string
GetDirectoryFromPath(const std::string &pathname)
{

	std::string dirpath;
	std::string::size_type endPos = pathname.rfind('/');
	if ((endPos != std::string::npos) && (endPos > 0))
	{
		dirpath = pathname.substr(0, endPos);
	}

	return dirpath;

}

// Returns the extension from a filename where the extension does not
// include a '/' or a '.'
const std::string
GetFileExtension(const char *filePath)
{

	std::string tmp(filePath);
	std::string extension("");
	std::string::size_type startPos = tmp.find_last_of("./");
	if ((startPos != std::string::npos) && (tmp[startPos] == '.'))
	{
		extension = tmp.substr(startPos);
	}

	return extension;
}

// Returns the basename from a filename where the extension matches
// that returned by GetFileExtension.  The basename wil not include a
// trailing period
const std::string
GetFileBasename(const char *filePath)
{

	std::string tmp(filePath);
	std::string basename("");
	std::string::size_type startPos = tmp.find_last_of("./");
	if ((startPos != std::string::npos) && (tmp[startPos] == '.'))
	{
		basename = tmp.substr(0, startPos);
	}

	return basename;
}

// Return the path to an cached object identified by the objectId, the
// typeName, and the file name
std::string
BuildPathname(const cachedObjectId_t objectId, const std::string &basePath,
              const std::string &typeName, const std::string &fileName,
              bool createDir)
{

	if (objectId == 0)
	{
		return std::string("");
	}

	// Build the pathname from the parts, starting with the base path
	std::string pathname(basePath);

	// create the full path name from the file cache base directory, the
	// typename, the id and the filename
	pathname += std::string("/") + typeName + std::string("/");

	// Use the first s_dirChars encoded chars to define a directory name
	for (int i = (s_numChars - 1); i > (s_numChars - s_dirChars - 1); i--)
	{
		char c = GetCharNFromObjectId(objectId, i);
		if (c != ' ')
		{
			pathname += c;
		}
		else
		{
			pathname.clear();
			break;
		}
	}

	if (createDir && !pathname.empty())
	{
		// Make sure the directory exists and set the correct directory permissions
		int retVal = ::mkdir(pathname.c_str(), s_dirPerms);
		if (retVal != 0 && errno != EEXIST)
		{
			int savedErrno = errno;
			printf("Failed to create directory \'%s\' (%s)\n",
			       pathname.c_str(), ::strerror(savedErrno));
			pathname.clear();
		}
	}

	// The remaining chars will be the file name along with the
	// extension
	if (!pathname.empty())
	{
		pathname += std::string("/");
		for (int i = (s_numChars - s_dirChars - 1); i >= 0; i--)
		{
			char c = GetCharNFromObjectId(objectId, i);
			if (c != ' ')
			{
				pathname += c;
			}
			else
			{
				pathname.clear();
				break;
			}
		}
		if (!fileName.empty() && !pathname.empty())
		{
			pathname += GetFileExtension(fileName.c_str());
		}
	}

	return pathname;
}

// Return the filesize as it resides on disk after accounting for the
// filesystem blocksize
cacheSize_t
GetFilesystemFileSize(const cacheSize_t size)
{

	cacheSize_t realSize = 0;
	if (size > 0)
	{
		realSize = (size + s_blockSize - 1) / s_blockSize;
		realSize *= s_blockSize;
	}
	else
	{
		realSize = s_blockSize;
	}

	// This is a hack to account for the fact that sometime ext3 isn't
	// using the inode to store the extended attributes.
	realSize += s_blockSize;

	return realSize;
}

// call fsync on the provided file
bool
SyncFile(const std::string &pathname, std::string &msgText)
{

	bool suceeded = true;

#ifdef MOJ_MAC
	int fd = open(pathname.c_str(), O_RDWR | O_APPEND);
#else
	int fd = open(pathname.c_str(), O_RDWR | O_APPEND | O_NOATIME);
#endif // #ifdef MOJ_MAC
	if (fd == -1)
	{
		msgText = "File '" + pathname + "': could not open for sync, expiring.";
		suceeded = false;
	}
	else
	{
		int retVal = ::fsync(fd);
		if (retVal == -1)
		{
			int savedErrno = errno;
			msgText = "Failed to sync file '" + pathname + "' ("
			          + std::string(::strerror(savedErrno)) + ").";
			suceeded = false;
		}
		retVal = ::close(fd);
		if (retVal == -1)
		{
			int savedErrno = errno;
			msgText = "Failed to close file '" + pathname + "' after sync ("
			          + std::string(::strerror(savedErrno)) + ").";
			suceeded = false;
		}
	}

	return suceeded;
}

// This is the equivalent of rm -rf of the directory in a directory
// type cached object
bool
CleanupDir(const std::string &pathname, std::string &msgText)
{

	bool success = true;
	fs::path dirname(pathname);
	try
	{
		if (fs::exists(dirname))
		{
			fs::remove_all(dirname);
		}
	}
	catch (const fs::filesystem_error &ex)
	{
		msgText = "CleanupDir: " + std::string(ex.what()) + " ("
		          + ex.code().message() + ")";
		success = false;
	}

	return success;
}

static cacheSize_t s_dirSum;
static int
Sum(const char *fpath, const struct stat *sb, int flag, struct FTW *ftwbuf)
{

	int retVal = 0;
	if (flag == FTW_F || flag == FTW_SL || flag == FTW_D || flag == FTW_DP)
	{
		struct stat buf;
		retVal = ::stat(fpath, &buf);
		if (retVal == 0)
		{
			s_dirSum += GetFilesystemFileSize((cacheSize_t) buf.st_size);
		}
	}

	return retVal;
}

// This is the equivalent of du -s of the directory in a directory
// type cached object
cacheSize_t
SumDir(const std::string &pathname)
{

	s_dirSum = 0;
	// nftw will walk the directory dirName and call Sum on each
	// entry.  It will use at most 32 concurrent file descriptors.
	if (nftw(pathname.c_str(), Sum, 32, FTW_DEPTH | FTW_PHYS) != 0)
	{
		s_dirSum = -1;
	}

	return s_dirSum;
}

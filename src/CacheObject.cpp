// Copyright (c) 2007-2018 LG Electronics, Inc.
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

#include "CacheObject.h"
#include "FileCache.h"
#include "FileCacheSet.h"

MojLogger CCacheObject::s_log(_T("filecache.cacheobject"));
static MojLogger s_cleanuplog(_T("filecache.cacheobject"));

CCacheObject::CCacheObject(CFileCache *fileCache,
                           const cachedObjectId_t id,
                           const std::string &filename, cacheSize_t size,
                           paramValue_t cost,
                           paramValue_t lifetime,
                           bool written,
                           bool dirType): m_id(id)
	, m_fileCache(fileCache)
	, m_size(size)
	, m_cost(cost)
	, m_lifetime(lifetime)
	, m_subscriptionCount(0)
	, m_filename(filename)
	, m_written(written)
	, m_expired(false)
	, m_dirType(dirType)
{

	MojLogTrace(s_log);

	m_creationTime = m_lastAccessTime = ::time(0);
	if (m_cost > s_maxCost)
	{
		m_cost = s_maxCost;
	}
	if (m_lifetime < 1)
	{
		m_lifetime = 1;
	}
}

CCacheObject::~CCacheObject()
{

	MojLogTrace(s_log);

	const std::string pathname(GetPathname());
	if (pathname.size() > 0)
	{
		if (m_dirType)
		{
			MojLogDebug(s_log, _T("~CCacheObject: Cleaning directory '%s'."),
			            pathname.c_str());
			std::string msgText;
			if (CleanupDir(pathname, msgText))
			{
				MojLogDebug(s_log,
				            _T("~CCacheObject: cleaned '%s' to delete object '%llu'."),
				            pathname.c_str(), m_id);
			}
			else
			{
				MojLogNotice(s_log, _T("~CCacheObject: Failed to clean '%s'."),
				             pathname.c_str());
				if (!msgText.empty())
				{
					MojLogError(s_log, _T("~CCacheObject: %s."), msgText.c_str());
				}
			}
		}
		else
		{
			int retVal = ::unlink(pathname.c_str());
			if ((retVal != 0) && (errno != ENOENT))
			{
				int savedErrno = errno;
				MojLogNotice(s_log, _T("~CCacheObject: Failed to unlink '%s' (%s)."),
				             pathname.c_str(), ::strerror(savedErrno));
			}
			else
			{
				MojLogDebug(s_log,
				            _T("~CCacheObject: Unlinked '%s' to delete object '%llu'."),
				            pathname.c_str(), m_id);
				const std::string dirpath(GetDirname(pathname));
				retVal = ::rmdir(dirpath.c_str());
				if ((retVal != 0) && (errno != ENOTEMPTY) && (errno != ENOENT))
				{
					// This should also never happen.  If it does we will just print
					// out the error as there isn't anything we can do about it.
					int savedErrno = errno;
					MojLogError(s_log,
					            _T("~CCacheObject: Failed to rmdir directory '%s' (%s)."),
					            dirpath.c_str(), ::strerror(savedErrno));
				}
			}
		}
	}
}

bool
CCacheObject::CreateObject(const std::string &pathname)
{

	MojLogTrace(s_log);

	bool success = true;
	int retVal;

	if (m_dirType)
	{
		MojLogDebug(s_log,
		            _T("Initialize: Created cache directory '%s' for object '%llu'."),
		            pathname.c_str(), m_id);
		retVal = ::mkdir(pathname.c_str(), s_dirPerms);
		if (retVal != 0)
		{
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("Initialize: Failed to make directory '%s' (%s)."),
			            pathname.c_str(), ::strerror(savedErrno));
			success = false;
		}
	}
	else
	{
		FILE *fp = ::fopen(pathname.c_str(), "w");
		if (fp == NULL)
		{
			int savedErrno = errno;
			MojLogError(s_log, _T("Initialize: Failed to create file '%s' (%s)."),
			            pathname.c_str(), ::strerror(savedErrno));
			success = false;
		}
		else
		{
			::fclose(fp);

			MojLogDebug(s_log,
			            _T("Initialize: Created cache file '%s' for object '%llu'."),
			            pathname.c_str(), m_id);

			// Now set the permissions on the file so we can write the
			// extended attributes.
			retVal = ::chmod(pathname.c_str(), s_fileRWPerms);
			if (retVal != 0)
			{
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("Initialize: Failed to set permissions on '%s' (%s)."),
				            pathname.c_str(), ::strerror(savedErrno));
				success = false;
			}
			else
			{
				MojLogDebug(s_log,
				            _T("Initialize: Permissions set on '%s' to allow attribute setting."),
				            pathname.c_str());
			}
		}
	}
	return success;
}

bool
CCacheObject::SetFilenameAttribute(const std::string &pathname)
{

	MojLogTrace(s_log);

	bool success = true;
	// Add the real filename as an extended attribute
	int retVal = FC_setxattr(pathname.c_str(), "user.f", m_filename.c_str(),
	                         m_filename.length() + 1, XATTR_CREATE);
	if (retVal != 0)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("Initialize: Failed to set filename as attribute on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		success = false;
	}
	else
	{
		MojLogDebug(s_log,
		            _T("Initialize: Set user.f attribute on '%s' to '%s'."),
		            pathname.c_str(), m_filename.c_str());
	}

	return success;
}

bool
CCacheObject::SetSizeAttribute(const std::string &pathname,
                               const std::string &logname,
                               const bool replace)
{

	MojLogTrace(s_log);

	bool success = true;
	// Add the size as an extended attribute
	int retVal = FC_setxattr(pathname.c_str(), "user.s", &m_size,
	                         sizeof(m_size),
	                         replace ? XATTR_REPLACE : XATTR_CREATE);
	if (retVal != 0)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("%s: Failed to set size as attribute on '%s' (%s)."),
		            logname.c_str(), pathname.c_str(), ::strerror(savedErrno));
		success = false;
	}
	else
	{
		MojLogDebug(s_log,
		            _T("%s: Set user.s attribute on '%s' to '%d'."),
		            logname.c_str(), pathname.c_str(), m_size);
	}

	return success;
}

bool
CCacheObject::SetCostAttribute(const std::string &pathname)
{

	MojLogTrace(s_log);

	bool success = true;
	// Add the cost as an extended attribute
	int retVal = FC_setxattr(pathname.c_str(), "user.c", &m_cost,
	                         sizeof(m_cost), XATTR_CREATE);
	if (retVal != 0)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("Initialize: Failed to set cost as attribute on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		success = false;
	}
	else
	{
		MojLogDebug(s_log,
		            _T("Initialize: Set user.c attribute on '%s' to '%d'."),
		            pathname.c_str(), m_cost);
	}

	return success;
}

bool
CCacheObject::SetLifetimeAttribute(const std::string &pathname)
{

	MojLogTrace(s_log);

	bool success = true;
	// Add the lifetime as an extended attribute
	int retVal = FC_setxattr(pathname.c_str(), "user.l", &m_lifetime,
	                         sizeof(m_lifetime), XATTR_CREATE);
	if (retVal != 0)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("Initialize: Failed to set lifetime as attribute on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		success = false;
	}
	else
	{
		MojLogDebug(s_log,
		            _T("Initialize: Set user.l attribute on '%s' to '%d'."),
		            pathname.c_str(), m_lifetime);
	}

	return success;
}

bool
CCacheObject::SetWrittenAttribute(const std::string &pathname,
                                  const std::string &logname,
                                  const bool replace)
{

	MojLogTrace(s_log);

	bool success = true;

	// Add the written flag as an extended attribute.
	int writtenVal = m_written ? 1 : 0;
	int retVal = FC_setxattr(pathname.c_str(), "user.w", &writtenVal,
	                         sizeof(writtenVal),
	                         replace ? XATTR_REPLACE : XATTR_CREATE);
	if (retVal != 0)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("%s: Failed to set written flag as attribute on '%s' (%s)."),
		            logname.c_str(), pathname.c_str(), ::strerror(savedErrno));
		success = false;
	}
	else
	{
		MojLogDebug(s_log,
		            _T("%s: Set user.w attribute on '%s' to '%d'."),
		            logname.c_str(), pathname.c_str(), writtenVal);
	}

	if (success)
	{
		// Now set the permissions on the file so it can't be written,
		// they will be changed to read-write during the first subscribe.
		retVal = ::chmod(pathname.c_str(), s_fileROPerms);
		if (retVal != 0)
		{
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("%s: Failed to change permissions on '%s' (%s)."),
			            logname.c_str(), pathname.c_str(), ::strerror(savedErrno));
			success = false;
		}
		else
		{
			MojLogDebug(s_log, _T("%s: Permissions reset on '%s'."),
			            logname.c_str(), pathname.c_str());
		}
	}

	return success;
}

bool
CCacheObject::SetDirTypeAttribute(const std::string &pathname)
{

	MojLogTrace(s_log);

	bool success = true;

	// Add the dirType flag as an extended attribute.
	int dirtypeVal = m_dirType ? 1 : 0;
	int retVal = FC_setxattr(pathname.c_str(), "user.d", &dirtypeVal,
	                         sizeof(dirtypeVal), XATTR_CREATE);
	if (retVal != 0)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("Initialize: Failed to set dirType as attribute on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		success = false;
	}
	else
	{
		MojLogDebug(s_log,
		            _T("Initialize: Set user.d attribute on '%s' to '%d'."),
		            pathname.c_str(), dirtypeVal);
	}

	return success;
}

bool
CCacheObject::Initialize(bool isNew)
{

	MojLogTrace(s_log);

	bool success = true;
	if (isNew)
	{
		const std::string pathname(GetPathname(true));
		if (pathname.empty())
		{
			MojLogError(s_log, _T("Initialize: Failed to get pathname."));
			success = false;
		}

		// Now create the file and set the permissions and extended
		// attributes
		if (success)
		{
			success = CreateObject(pathname);
		}
		if (success)
		{
			success = SetFilenameAttribute(pathname);
		}
		if (success)
		{
			success = SetSizeAttribute(pathname, std::string("Initialize"));
		}
		if (success)
		{
			success = SetCostAttribute(pathname);
		}
		if (success)
		{
			success = SetLifetimeAttribute(pathname);
		}
		if (success)
		{
			success = SetDirTypeAttribute(pathname);
		}
		if (success)
		{
			success = SetWrittenAttribute(pathname, std::string("Initialize"));
		}
	}

	return success;
}

const std::string
CCacheObject::GetFileCacheType()
{

	MojLogTrace(s_log);

	return m_fileCache->GetType();
}

// This will increment the subscribe count and return the path to
// the file backing this object.  If the object doesn't exist, this
// will return an empty string.
std::string
CCacheObject::Subscribe(std::string &msgText)
{

	MojLogTrace(s_log);

	std::string pathname("");
	if (!isExpired())
	{
		if (m_written || (m_subscriptionCount == 0))
		{
			pathname = GetPathname();
			if (!m_written)
			{
				// Now set the permissions on the file so it can be written,
				// it will be changed to read-only during the unsubscribe.
				int retVal = ::chmod(pathname.c_str(),
				                     (m_dirType ? s_dirObjPerms : s_fileRWPerms));
				if (retVal != 0)
				{
					int savedErrno = errno;
					MojLogError(s_log,
					            _T("Subscribe: Failed to change permissions on '%s' (%s)."),
					            pathname.c_str(), ::strerror(savedErrno));
					pathname.clear();
				}
				else
				{
					MojLogDebug(s_log,
					            _T("Subscribe: Permissions set on '%s' to allow write."),
					            pathname.c_str());
				}
			}
			if (!pathname.empty())
			{
				MojLogInfo(s_log,
				           _T("Subscribe: subscription taken on object '%llu'."), m_id);
				m_subscriptionCount++;
			}
		}
		else
		{
			msgText = "Failed, only one writer allowed";
			MojLogError(s_log,
			            _T("Subscribe: %s for object '%llu'."),
			            msgText.c_str(), m_id);
		}
		UpdateAccessTime();
	}
	else
	{
		MojLogWarning(s_log,
		              _T("Subscribe: Failed, object '%llu' is already expired."),
		              m_id);
	}

	return pathname;
}

void
CCacheObject::UnSubscribe()
{

	MojLogTrace(s_log);

	m_subscriptionCount--;

	bool suceeded = true;
	const std::string pathname(GetPathname());

	if (m_dirType)
	{
		// By setting suceeded = false, it will be marked as expired and
		// set for deletion below
		MojLogDebug(s_log,
		            _T("UnSubscribe: Directory '%s' marked Expired."),
		            pathname.c_str());
		suceeded = false;
	}
	else if (!m_written)
	{
		// Check if this is the first subscription where the file gets
		// written
		if (!pathname.empty())
		{
			// Make sure the size is correct
			struct stat buf;
			if (::stat(pathname.c_str(), &buf) != 0)
			{
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("UnSubscribe: Failed to stat file '%s' (%s)."),
				            pathname.c_str(), ::strerror(savedErrno));
				suceeded = false;
			}
			else
			{
				cacheSize_t size = (cacheSize_t) buf.st_size;

				if (size > m_size)
				{
					MojLogError(s_log,
					            _T("UnSubscribe: File '%s' is larger than space allocated, expiring."),
					            pathname.c_str());
					suceeded = false;

					// This should be enabled when the tests can be fixed as the
					// best thing to do is to remove any zero length files
#if 0
				}
				else if (size == 0)
				{
					MojLogError(s_log,
					            _T("UnSubscribe: File '%s' is empty, expiring."),
					            pathname.c_str());
					m_size = 0;
					suceeded = false;
#endif
				}
				else if (size < m_size)
				{
					// If the real size is smaller, reset the specified size to
					// the real size and persist the value
					MojLogDebug(s_log,
					            _T("UnSubscribe: Resetting object size of '%llu' from '%d' to '%d'."),
					            m_id, m_size, size);
					m_size = size;
					suceeded = SetSizeAttribute(pathname, std::string("UnSubscribe"),
					                            true);
				}
			}
		}
		else
		{
			MojLogError(s_log, _T("UnSubscribe: Failed to get pathname."));
			suceeded = false;
		}

		if (suceeded)
		{
			std::string msgText;
			suceeded = SyncFile(pathname, msgText);
			MojLogDebug(s_log, _T("UnSubscribe: SyncFile was %s."),
			            suceeded ? "successful" : "unsuccessful");
			if (!suceeded && !msgText.empty())
			{
				MojLogError(s_log, _T("UnSubscribe: %s"), msgText.c_str());
			}
		}

		// Now persist the written flag by reseting the extended
		// attribute, this makes it a valid file for deserialize
		if (suceeded)
		{
			m_written = true;
			suceeded = SetWrittenAttribute(pathname, std::string("UnSubscribe"),
			                               true);
			if (!suceeded)
			{
				m_written = false;
			}
		}
	}
	MojLogDebug(s_log,
	            _T("UnSubscribe: subscription released on object '%llu'."),
	            m_id);

	if (!suceeded)
	{
		// Mark this expired and remove it from the FileCacheSet id map so
		// it will be orphaned and cleaned up next time we reap orphans
		MojLogDebug(s_log, _T("UnSubscribe: Object '%llu' marked as expired."), m_id);
		GetFileCacheSet()->RemoveObjectFromIdMap(m_id);
		m_expired = true;
	}
	else
	{
		UpdateAccessTime();
	}
}

// This updates the access time without needing to subscribe, it's
// like using touch on an existing file
time_t
CCacheObject::Touch()
{

	MojLogTrace(s_log);

	return UpdateAccessTime();
}

// The FileCache::Resize will have already checked for space so this
// just sets the new size and persists it.
cacheSize_t
CCacheObject::Resize(cacheSize_t newSize)
{

	MojLogTrace(s_log);

	// Since you can only resize a file while it's being written, we can
	// check and just return the saved size if already written
	if (!m_written && (m_subscriptionCount == 1))
	{

		const std::string pathname(GetPathname());
		int savedSize = m_size;
		m_size = newSize;
		if (!SetSizeAttribute(pathname, std::string("Resize"), true))
		{
			m_size = savedSize;
		}
	}
	else
	{
		if (m_written)
		{
			MojLogWarning(s_log,
			              _T("Resize: Operation not allowed on written object '%llu'."),
			              m_id);
		}
		else if (m_subscriptionCount == 0)
		{
			MojLogWarning(s_log,
			              _T("Resize: Operation not allowed on unsubscribed object '%llu'."),
			              m_id);
		}
	}

	return m_size;
}

bool
CCacheObject::Expire()
{

	MojLogTrace(s_log);

	bool successful = true;
	m_expired = true;
	if (m_subscriptionCount > 0)
	{
		MojLogInfo(s_log,
		           _T("Expire: Subscribed, cannot remove expired object."));
		successful = false;
	}
	else if (m_filename.size() > 0)
	{
		const std::string pathname(GetPathname());
		if (m_dirType)
		{
			std::string msgText;
			successful = CleanupDir(pathname, msgText);
			if (successful)
			{
				MojLogDebug(s_log,
				            _T("Expire: Cleaned directory '%s' to expire object '%llu'."),
				            pathname.c_str(), m_id);
			}
			else
			{
				MojLogError(s_log,
				            _T("Expire: Failed to clean directory '%s'."),
				            pathname.c_str());
				if (!msgText.empty())
				{
					MojLogDebug(s_log, _T("Expire: %s."), msgText.c_str());
				}
			}
		}
		else
		{
			int retVal = ::unlink(pathname.c_str());
			if (retVal != 0)
			{
				// This should never happen but if it does, the timer worker
				// method will walk the caches and look for these to try to
				// cleanup
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("Expire: Failed to unlink file '%s' (%s)."),
				            pathname.c_str(), ::strerror(savedErrno));
				successful = false;
			}
			else
			{
				MojLogDebug(s_log,
				            _T("Expire: unlinked file '%s' to expire object '%llu'."),
				            pathname.c_str(), m_id);
			}
		}
		const std::string dirpath(GetDirname(pathname));
		int retVal = ::rmdir(dirpath.c_str());
		if ((retVal != 0) && (errno != ENOTEMPTY) && (errno != ENOENT))
		{
			// This should also never happen.  If it does we will just print
			// out the error as there isn't anything we can do about it.
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("Expire: Failed to rmdir directory '%s' (%s)."),
			            dirpath.c_str(), ::strerror(savedErrno));
		}
	}
	else
	{
		MojLogDebug(s_log, _T("Expire: No filename to remove."));
		successful = false;
	}

	return successful;
}

// Validate a subscribed file that is writable.  For now, just ensure
// the file size is <= the specified size, otherwise log it as an
// error.
void
CCacheObject::Validate()
{

	MojLogTrace(s_log);

	if (m_filename.size() > 0)
	{
		const std::string pathname(GetPathname());
		if (!pathname.empty())
		{
			cacheSize_t size = -1;
			if (m_dirType)
			{
				size = SumDir(pathname);
			}
			else
			{
				// Make sure the size is correct
				struct stat buf;
				int retVal = ::stat(pathname.c_str(), &buf);
				if (retVal != 0)
				{
					int savedErrno = errno;
					MojLogError(s_log, _T("Validate: Failed to stat file '%s' (%s)."),
					            pathname.c_str(), ::strerror(savedErrno));
				}
				else
				{
					size = (cacheSize_t) buf.st_size;
				}
			}
			if ((size >= 0) && (size <= m_size))
			{
				MojLogInfo(s_log, _T("Validate: '%s' is valid."),
				           pathname.c_str());
			}
			else if (size >= 0)
			{
				MojLogError(s_log,
				            _T("Validate: '%s' is invalid, size = '%d', expected '%d'."),
				            pathname.c_str(), size, m_size);
			}
			else
			{
				MojLogError(s_log,
				            _T("Validate: Failed to get size of '%s'."),
				            pathname.c_str());
			}
		}
		else
		{
			MojLogError(s_log,
			            _T("Validate: Could not generate pathname for object '%llu'."),
			            m_id);
		}
	}
	else
	{
		MojLogError(s_log, _T("Validate: No filename found for object '%llu'."),
		            m_id);
	}
}

CFileCacheSet *
CCacheObject::GetFileCacheSet()
{

	MojLogTrace(s_log);

	return m_fileCache->GetFileCacheSet();
}

const std::string
CCacheObject::GetPathname(bool createDir)
{

	MojLogTrace(s_log);

	const std::string typeName(GetFileCacheType());
	const std::string dirBase(GetFileCacheSet()->GetBaseDirName());

	return BuildPathname(m_id, dirBase, typeName, m_filename, createDir);
}

std::string
CCacheObject::GetDirname(const std::string &pathname)
{

	MojLogTrace(s_log);

	return GetDirectoryFromPath(pathname);
}

paramValue_t
CCacheObject::GetCacheCost()
{

	MojLogTrace(s_log);

	paramValue_t cost;
	paramValue_t age = (paramValue_t)(::time(0) - m_lastAccessTime);
	if (age < m_lifetime)
	{
		MojLogDebug(s_log,
		            _T("GetCacheCost: Age < lifetime, setting cost to max"));
		cost = s_maxCost;
	}
	else
	{
		cacheSize_t sizeInPages = (m_size + s_blockSize - 1) / s_blockSize;
		cost = m_cost * sizeInPages / age;
	}

	return cost;
}

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

#ifndef __CACHE_OBJECT_H__
#define __CACHE_OBJECT_H__

#include "CacheBase.h"

inline int FC_setxattr(const char *path, const char *name, const void *value,
                       size_t size, int options)
{
#ifdef MOJ_MAC
	return ::setxattr(path, name, value, size, 0, options);
#else
	return ::setxattr(path, name, value, size, options);
#endif // #ifdef MOJ_MAC
}

class CFileCache;
class CFileCacheSet;

class CCacheObject
{
public:

	CCacheObject(CFileCache *fileCache,
	             const cachedObjectId_t id,
	             const std::string &filename,
	             cacheSize_t size, paramValue_t cost = 0,
	             paramValue_t lifetime = 0, bool written = false,
	             bool dirType = false);

	~CCacheObject();

	bool Initialize(bool isNew);

	cachedObjectId_t GetId()
	{
		return m_id;
	}

	time_t GetCreationTime()
	{
		return m_creationTime;
	}
	time_t GetLastAccessTime()
	{
		return m_lastAccessTime;
	}
	time_t UpdateAccessTime()
	{
		m_lastAccessTime = ::time(0);
		return m_lastAccessTime;
	}

	cacheSize_t GetSize()
	{
		return m_size;
	}
	paramValue_t GetCost()
	{
		return m_cost;
	}
	paramValue_t GetLifetime()
	{
		return m_lifetime;
	}
	paramValue_t GetCacheCost();

	// This will increment the subscribe count and return the path to
	// the file backing this object.  If the object doesn't exist, this
	// will return an empty string.
	std::string Subscribe(std::string &msgText);
	paramValue_t GetSubscriptionCount()
	{
		return m_subscriptionCount;
	}

	// This will decrement the subscribe count
	void UnSubscribe();

	// This updates the access time without needing to subscribe, it's
	// like using touch on an existing file
	time_t Touch();

	cacheSize_t Resize(cacheSize_t newSize);

	std::string GetFileName()
	{
		return m_filename;
	}

	// This will set the expire flag and return whether the object is
	// currently subscribed so the caller knows whether the next cache
	// cleaning pass will be guaranteeded to remove this item.
	bool Expire();
	bool isExpired()
	{
		return m_expired;
	}

	bool isWritten()
	{
		return m_written;
	}
	bool isDirType()
	{
		return m_dirType;
	}

	// Validate a subscribed file that is writable.  For now, just ensure
	// the file size is <= the specified size, otherwise log it as an
	// error.
	void Validate();

	const std::string GetPathname(bool createDir = false);
	const std::string GetFileCacheType();

private:

	std::string GetDirname(const std::string &pathname);
	CFileCacheSet *GetFileCacheSet();
	bool CreateObject(const std::string &pathname);
	bool SetFilenameAttribute(const std::string &pathname);
	bool SetSizeAttribute(const std::string &pathname, const std::string &logname,
	                      const bool replace = false);
	bool SetCostAttribute(const std::string &pathname);
	bool SetLifetimeAttribute(const std::string &pathname);
	bool SetWrittenAttribute(const std::string &pathname,
	                         const std::string &logname,
	                         const bool replace = false);
	bool SetDirTypeAttribute(const std::string &pathname);

	const cachedObjectId_t m_id;

	CFileCache *m_fileCache;

	cacheSize_t m_size;
	paramValue_t m_cost;
	paramValue_t m_lifetime;
	paramValue_t m_subscriptionCount;

	std::string m_filename;

	bool m_written;
	bool m_expired;
	bool m_dirType;

	time_t m_creationTime;
	time_t m_lastAccessTime;
	static MojLogger s_log;
};

#endif

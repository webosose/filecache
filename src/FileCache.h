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

#ifndef __FILE_CACHE_H__
#define __FILE_CACHE_H__

#include "CacheBase.h"
#include "CacheObject.h"

class CFileCacheSet;

static const std::string s_loWatermark("loWatermark");
static const std::string s_hiWatermark("hiWatermark");
static const std::string s_defaultSize("defaultSize");
static const std::string s_defaultLifetime("defaultLifetime");
static const std::string s_defaultCost("defaultCost");
static const std::string s_dirType("dirType");
static const uint32_t s_numLabels = 6;

class CFileCache
{
public:

	// This constructor is used to create a new type
	CFileCache(CFileCacheSet *cacheSet, const std::string &cacheType);

	~CFileCache();

	// Configure the cache configuration items.  Returns false if it
	// can't configure the cache based on the specified configuration
	// and continues to use the last configuration if one is available.
	bool Configure(CCacheParamValues *params = NULL, bool dirType = false);

	// Returns all the configuration values in the parameter object and returns
	// the current space used in the cache.
	cacheSize_t Describe(CCacheParamValues &params);

	// Insert a new object in the cache.  The cachedObjectId_t is provided by
	// the CFileCacheSet to maintain unique cache IDs across all cache
	// types.  You must specify the size of the object unless it matches
	// a specified non-zero default size.  If you don't specify non-zero values
	// for the lifetime and cost, the defaults for the cache type or zero will
	// be used.  The number of objects in the cache will be returned.
	paramValue_t Insert(CCacheObject *newObj);

	// Resize the object in the cache.  This is needed when inserting an
	// object where you don't know the final object size.  This is
	// likely when downloading an object from the network or while
	// creating a non-persistent object.  Be aware this call can fail if
	// the new size is larger than the old size and the requested space
	// is unavailable.  The return value will be the new size set and it
	// might be smaller than requested (and may be the same as the old
	// size) so the caller needs to check and handle it.
	cacheSize_t Resize(const cachedObjectId_t objId, cacheSize_t newSize);

	// Expire an object in the cache.  This will cause the object to be
	// deleted.  CFileCacheSet should remove the cachedObjectId_t from it's
	// m_idMap.  This will return false if the requested item is
	// currently pinned in the cache by a subscription and the object
	// will be deleted once the subscription expires.
	bool Expire(const cachedObjectId_t objId);

	// Subscribing to an object is the means to pin an object in the
	// cache.  This means that for the duration of the subscription, the
	// object is guaranteed not to be deleted from the cache.  This is also
	// how you obtain the pathname to the cached object.
	const std::string Subscribe(std::string &msgText, const cachedObjectId_t objId);

	// Unsubscribing an object removes the pin of the object in the
	// cache.  This means that there is no longer any guarantee of available
	// of the object in the cache.
	void UnSubscribe(const cachedObjectId_t objId);

	// This updates the access time without needing to subscribe, it's
	// like using touch on an existing file
	bool Touch(const cachedObjectId_t objId);

	// Get a vector containing pairs of all the objects in the cache and
	// their object IDs.
	std::vector<std::pair<cachedObjectId_t, CCacheObject *>> GetCachedObjects();

	// Check if there is space in the cache for a new object of size
	bool CheckForSize(cacheSize_t size);

	// Cleanup this cache
	void Cleanup(cacheSize_t size);

	// Cleanup orphaned objects
	void CleanupOrphanedObjects();

	// Get the best object from this cache for cleanup
	cachedObjectId_t GetCleanupCandidate();

	// Return information about the current state of the cache.  The
	// total space used by the cache as well as the number of cached
	// objects are returned in the parameters.  The typename of the
	// cache is returned.
	std::string &GetCacheStatus(cacheSize_t *cacheSize,
	                            paramValue_t *numCacheObjects);

	// This returns the space used by a cached object.
	cacheSize_t GetObjectSize(const cachedObjectId_t objId);

	// This returns the filename of a cached object
	const std::string GetObjectFilename(const cachedObjectId_t objId);

	// This returns the file cache type string
	const std::string GetType()
	{
		return m_cacheType;
	}

	// Return the object cost
	paramValue_t GetCacheCost(const cachedObjectId_t objId);

	// Returen the pointer to the CFileCacheSet object that created us.
	CFileCacheSet *GetFileCacheSet()
	{
		return m_fileCacheSet;
	}

	// Return the cumulative size of the cachedObjects
	cacheSize_t GetCacheSize()
	{
		return m_cacheSize;
	}

	// Return the number of objects in the cache
	paramValue_t GetNumObjects()
	{
		return m_numObjects;
	}

	// Cleans up the least recently used unsubscribed object
	cacheSize_t CleanupCache(cachedObjectId_t *cleanedId);

	// Returns whether true if none of the cached objects are
	// subscribed, the cache can be cleanly delete
	bool isCleanable();

	// Validate a subscribed object.
	void CheckSubscribedObject(const cachedObjectId_t objId);

	// Return if this type is a dirType
	bool isDirType()
	{
		return m_dirType;
	}

	// Cleanup any unsubscribed directory types
	void CleanupDirType();

private:

	CFileCache &operator=(const CFileCache &);

	CCacheObject *GetCacheObjectForId(const cachedObjectId_t id);
	void UpdateObject(const cachedObjectId_t objId);
	bool WriteConfig();
	bool ReadConfig();

	CFileCacheSet *m_fileCacheSet;
	std::string m_cacheType;

	paramValue_t m_numObjects;
	cacheSize_t m_cacheSize;
	paramValue_t m_loWatermark;
	paramValue_t m_hiWatermark;
	cacheSize_t m_defaultSize;
	paramValue_t m_defaultLifetime;
	paramValue_t m_defaultCost;
	bool m_dirType;

	std::map<cachedObjectId_t, CCacheObject *> m_cachedObjects;
	std::list<cachedObjectId_t> m_cacheList;
	static MojLogger s_log;
};

#endif

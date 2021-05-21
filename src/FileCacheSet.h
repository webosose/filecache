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

#ifndef __FILE_CACHE_SET_H__
#define __FILE_CACHE_SET_H__

#include "CacheBase.h"
#include "CacheObject.h"
#include "FileCache.h"

static const std::string s_totalCacheSpace("totalCacheSpace");
static const std::string s_baseDirName("baseDirName");
static const std::string s_seqNumFilename(".sequenceNumber");

inline ssize_t FC_getxattr(const char *path, const char *name,  void *value,
                           size_t size)
{
#ifdef MOJ_MAC
	return ::getxattr(path, name, value, size, 0, 0);
#else
	return ::getxattr(path, name, value, size);
#endif // #ifdef MOJ_MAC
}

class CFileCacheSet
{
public:

	CFileCacheSet(bool init = true);

	// This defines a new cache type and will cause a new CFileCache
	// object to be instantiated.  The typeName must be unique.  The sum
	// of the cache loWatermarks must be less than the total cache space
	// available or the cache type creation will fail.
	bool DefineType(std::string &msgText, const std::string &typeName,
	                CCacheParamValues *params = NULL, bool dirType = false);

	// This allows a value in the cache configuration to be modified.
	// Returns false if it can't configure the cache based on the
	// specified configuration and continues to use the last
	// configuration.
	bool ChangeType(std::string &msgText, const std::string &typeName,
	                CCacheParamValues *params);

	// Deletes an existing cache type.  All objects cached in this type
	// will be deleted.  The space used by the deleted cache (in KB) is
	// returned.  -1 is returned if the delete failed either because the
	// type didn't exist or because there are presently subscribed items
	// in the cache.
	cacheSize_t DeleteType(std::string &msgText,
	                       const std::string &typeName);

	// Return a vector of all defined cache types.
	const std::vector<std::string> GetTypes();

	// Get the configuration values for a cache type.
	CCacheParamValues DescribeType(const std::string &typeName);

	// Select the object from the best choice from each cache to remove
	CFileCache *SelectCandidateToExpire(std::map < CFileCache *,
	                                    const cachedObjectId_t > & m_cleanupMap);

	// Cleanup all registered types
	cacheSize_t CleanupAllTypes(cacheSize_t neededSpace);

	// Insert an object into the cache and returns the object id of that
	// cache object.  The size value must be provided unless the size is
	// the default non-zero size configured for the cache.  Any values
	// provided for cost and lifetime will override the default
	// configuration.

	// This is the general one for making new cached objects
	cachedObjectId_t InsertCacheObject(std::string &msgText,
	                                   const std::string &typeName,
	                                   const std::string &filename,
	                                   cacheSize_t size, paramValue_t cost = 0,
	                                   paramValue_t lifetime = 0);

	// This one is used on start-up when rebuilding from the filesystem
	cachedObjectId_t InsertCacheObject(std::string &msgText,
	                                   const std::string &typeName,
	                                   const std::string &filename,
	                                   const cachedObjectId_t objectId,
	                                   cacheSize_t size, paramValue_t cost,
	                                   paramValue_t lifetime, bool written,
	                                   bool isNew);

	// Request to change the size of an object.  This is only valid
	// while the initial writable subscription is in effect.  If there
	// isn't sufficient space, the resize will return the original size
	// of the object and that should be taken as a failure.  On success,
	// the new size will be returned.
	cacheSize_t Resize(const cachedObjectId_t objId, cacheSize_t newSize);

	// Expire an object from the cache.  This will cause the object to
	// be deleted from the cache.  This will return false if the
	// requested item is currently pinned in the cache by a subscription
	// and the object will be deleted once the subscription expires.
	bool ExpireCacheObject(const cachedObjectId_t objId);

	// Pin an object in the cache by allowing a client to subscribe to
	// the object.  This will guarantee the object will not be removed
	// from the cache while the subscription is active.  Returns the
	// file path associated with the objectId.
	const std::string SubscribeCacheObject(std::string &msgText,
	                                       const cachedObjectId_t objId);

	// Remove the client subscription of an object.  This will remove
	// the guarantee that the object will be kept in the cache.
	void UnSubscribeCacheObject(const std::string &typeName,
	                            const cachedObjectId_t objId);

	// This updates the access time without needing to subscribe, it's
	// like using touch on an existing file
	bool Touch(const cachedObjectId_t objId);

	// This will remove an object id from the id map and make it an
	// orphan to be cleaned up on expiration
	void RemoveObjectFromIdMap(const cachedObjectId_t objId)
	{
		m_idMap.erase(objId);
	}

	// Get the current status of the cache as a whole.  The current
	// amount of space used in all the caches will be returned in size.
	// The current number of active cached objects will be returned in
	// numCacheObjects.  The availableSpace is the sum of the cache
	// loWatermark values minus the current amount of space in use.
	// This is the guaranteed amount of space available.  Returns the
	// number of cache types.
	cacheSize_t GetCacheStatus(cacheSize_t *size,
	                           paramValue_t *numCacheObjects,
	                           cacheSize_t *availSpace);

	// Gets the current status of a specied cache type.  Returns the
	// amount of space and the number of cached objects used by the
	// items in that cache type.
	bool GetCacheTypeStatus(const std::string &typeName, cacheSize_t *size,
	                        paramValue_t *numCacheObjects);

	// Returns the size of a cached object or -1 if the object is no
	// longer in the cache.
	cacheSize_t CachedObjectSize(const cachedObjectId_t objId);

	// Returns the filename of a cachedObject
	const std::string CachedObjectFilename(const cachedObjectId_t objId);

	// Return the base directory name for the file cache directory tree
	virtual std::string &GetBaseDirName()
	{
		return m_baseDirName;
	}

	// Return the total configured cache space
	virtual cacheSize_t TotalCacheSpace()
	{
		return m_totalCacheSpace;
	}

	// Compute the sum of the loWatermarks for each of the configured caches
	virtual cacheSize_t SumOfLoWatermarks();

	// Compute the sum of current sizes for each of the configured caches
	virtual cacheSize_t SumOfCacheSizes();

	// Get the type that cooresponds to an objectId
	const std::string GetTypeForObjectId(const cachedObjectId_t objId);

	// Return the cache directory name
	const std::string GetCacheDirectory()
	{
		return m_baseDirName;
	}

	// Check if a type exists
	bool TypeExists(const std::string &typeName);

	// Check if a type is a directory type
	bool isTypeDirType(const std::string &typeName);

	// locate and try to cleanup any orphans
	void CleanupOrphans();

	// Validate a subscribed object.
	void CheckSubscribedObject(const std::string &typeName,
	                           const cachedObjectId_t objId);

	// Cleanup any unsubscribed directory types
	void CleanupDirTypes();

	// Walk the file cache directory tree to build the cache data
	// structures.
	int WalkDirTree();

	// Cleanup cache space at startup.
	void CleanupAtStartup();

	//Get Cache size
	int GetCacheSize();

protected:
	~CFileCacheSet() {};
	virtual cachedObjectId_t GetNextCachedObjectId();

private:

	CFileCache *GetFileCacheForType(const std::string &typeName);

	void ReadConfig(const std::string &configFile);
	void ReadSequenceNumber();
	void WriteSequenceNumber();

	enum ProcessStatus
	{
	    ERROR = 0,
	    COMPLETE,
	    CONTINUE
	};

	bool isTopLevelDirectory(const std::string &pathname);
	ProcessStatus CreateTypeIfNeeded(const std::string &pathname,
	                                 const std::string &typeName,
	                                 std::set<std::string> &types);
	ProcessStatus CheckForSpecialFile(const std::string &pathname,
	                                  std::set<std::string> &types);
	ProcessStatus GetWritten(const std::string &pathname, int *written,
	                         bool dirType);
	ProcessStatus GetSize(const std::string &pathname, const struct stat *sb,
	                      cacheSize_t *size, bool dirType);
	ProcessStatus GetFilename(const std::string &pathname, char *fileName);
	ProcessStatus GetCost(const std::string &pathname, paramValue_t *cost);
	ProcessStatus GetLifetime(const std::string &pathname, paramValue_t *lifetime);
	int ProcessFiles(const std::string &filepath);
	bool FileTreeWalk(const std::string &dirName);

	std::map<const std::string, CFileCache *> m_cacheSet;
	std::map<const cachedObjectId_t, const std::string> m_idMap;

	cacheSize_t m_totalCacheSpace;
	std::string m_baseDirName;
	sequenceNumber_t m_sequenceNumber;
	static MojLogger s_log;
};

#endif

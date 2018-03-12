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

#include "FileCache.h"
#include "FileCacheSet.h"

MojLogger CFileCache::s_log(_T("filecache.filecache"));

// This constructor is used to create a new type or deserialize an
// already constructed type
CFileCache::CFileCache(CFileCacheSet *cacheSet,
                       const std::string &cacheType) : m_fileCacheSet(cacheSet)
	, m_cacheType(cacheType)
	, m_numObjects(0)
	, m_cacheSize(0)
	, m_loWatermark(0)
	, m_hiWatermark(1)
	, m_defaultSize(0)
	, m_defaultLifetime(1)
	, m_defaultCost(0)
	, m_dirType(false)
{
	MojLogTrace(s_log);
}

CFileCache::~CFileCache()
{

	MojLogTrace(s_log);

	bool cleanable = isCleanable();

	// Get the full path name from the file cache base directory, the
	// typename, the id and the filename
	std::string pathname(GetFileCacheSet()->GetBaseDirName());
	pathname += "/" + m_cacheType;
	std::string configFile(pathname + "/Type.defaults");
	if (::unlink(configFile.c_str()) != 0)
	{
		MojLogError(s_log, _T("~CFileCache: Failed to unlink config file '%s'."),
		            configFile.c_str());
	}

	// Don't bother trying to remove the directory as it contains a file
	// for the cached object that couldn't be expired.
	if (cleanable)
	{
		if (::rmdir(pathname.c_str()) != 0)
		{
			MojLogError(s_log,
			            _T("~CFileCache: Failed to unlink cache directory '%s'."),
			            pathname.c_str());
		}
	}
	else
	{
		MojLogWarning(s_log, _T("~CFileCache: '%s' has orphans."),
		              m_cacheType.c_str());
	}
}

// Configure the cache configuration items.  Returns false if it
// can't configure the cache based on the specified configuration
// and continues to use the last configuration if one is available.
bool
CFileCache::Configure(CCacheParamValues *params, bool dirType)
{

	MojLogTrace(s_log);

	bool retVal = false;
	if (params == NULL)
	{
		MojLogDebug(s_log, _T("Configure: configuring '%s' from file."),
		            m_cacheType.c_str());
		retVal = ReadConfig();
	}
	else
	{
		cacheSize_t availSpace = GetFileCacheSet()->TotalCacheSpace() -
		                         GetFileCacheSet()->SumOfLoWatermarks();
		if (GetFilesystemFileSize(params->GetLoWatermark()) < availSpace)
		{
			if (params->GetLoWatermark() > 0)
			{
				m_loWatermark = GetFilesystemFileSize(params->GetLoWatermark());
				MojLogDebug(s_log,
				            _T("Configure: Configured '%s' low watermark to %d."),
				            m_cacheType.c_str(), m_loWatermark);
			}
			else if (params->GetLoWatermark() < 0)
			{
				MojLogError(s_log,
				            _T("Configure: FileCache '%s': Ignoring invalid value '%d' for low watermark."),
				            m_cacheType.c_str(), params->GetLoWatermark());
			}
			if (params->GetHiWatermark() > 0)
			{
				m_hiWatermark = GetFilesystemFileSize(params->GetHiWatermark());
				MojLogDebug(s_log,
				            _T("Configure: Configured '%s' high watermark to %d."),
				            m_cacheType.c_str(), m_hiWatermark);
			}
			else if (params->GetHiWatermark() < 0)
			{
				MojLogError(s_log,
				            _T("Configure: FileCache '%s': Ignoring invalid value '%d' for high watermark."),
				            m_cacheType.c_str(), params->GetHiWatermark());
			}
			if (params->GetSize() > 0)
			{
				m_defaultSize = params->GetSize();
				MojLogDebug(s_log,
				            _T("Configure: Configured '%s' size to %d."),
				            m_cacheType.c_str(), m_defaultSize);
			}
			else if (params->GetSize() < 0)
			{
				MojLogError(s_log,
				            _T("Configure: FileCache '%s': Ignoring invalid value '%d' for default size."),
				            m_cacheType.c_str(), params->GetSize());
			}
			if (params->GetLifetime() > 1)
			{
				m_defaultLifetime = params->GetLifetime();
				MojLogDebug(s_log,
				            _T("Configure: Configured '%s' lifetime to %d."),
				            m_cacheType.c_str(), m_defaultLifetime);
			}
			if (params->GetCost() > 0)
			{
				m_defaultCost = params->GetCost();
				MojLogDebug(s_log,
				            _T("Configure: Configured '%s' cost to %d."),
				            m_cacheType.c_str(), m_defaultCost);
			}
			m_dirType = dirType;
			retVal = WriteConfig();
		}
		else
		{
			MojLogWarning(s_log,
			              _T("Configure: Not enough cache space to configure '%s'."),
			              m_cacheType.c_str());
		}
	}

	return retVal;
}

// Returns all the configuration values in the parameter object and
// returns the current space used in the cache.
cacheSize_t
CFileCache::Describe(CCacheParamValues &params)
{

	MojLogTrace(s_log);

	params.SetLoWatermark(m_loWatermark);
	params.SetHiWatermark(m_hiWatermark);
	params.SetSize(m_defaultSize);
	params.SetLifetime(m_defaultLifetime);
	params.SetCost(m_defaultCost);

	return m_cacheSize;
}

// Insert a new object in the cache.  The cachedObjectId_t is provided
// by the CFileCacheSet to maintain unique cache IDs across all cache
// types.  The number of objects in the cache will be returned.
paramValue_t
CFileCache::Insert(CCacheObject *newObj)
{

	MojLogTrace(s_log);

	cachedObjectId_t objId = newObj->GetId();
	m_cachedObjects.insert(std::map < cachedObjectId_t,
	                       CCacheObject * >::value_type(objId, newObj));
	m_cacheList.push_front(objId);
	m_numObjects++;
	m_cacheSize += GetFilesystemFileSize(newObj->GetSize());
	MojLogInfo(s_log,
	           _T("Insert: Id '%llu'. Cache size '%d', object count '%d'."),
	           objId, m_cacheSize, m_numObjects);
	MojLogDebug(s_log,
	            _T("Insert: m_cachedObject.size() = '%zd', m_cacheList.size() = '%zd'."),
	            m_cachedObjects.size(), m_cacheList.size());

	return (paramValue_t) m_cachedObjects.size();
}

// Resize the object in the cache.  This is needed when inserting an
// object where you don't know the final object size.  This is likely
// when downloading an object from the network or while creating a new
// non-persistent object.  Be aware this call can fail if the new size
// is larger than the old size and the requested space is unavailable.
// The return value will be the new size set and it might be smaller
// than requested (and may be the same as the old size) so the caller
// needs to check and handle it.
cacheSize_t
CFileCache::Resize(const cachedObjectId_t objId, cacheSize_t newSize)
{

	MojLogTrace(s_log);

	cacheSize_t finalSize = 0;
	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	MojLogDebug(s_log, _T("Resize: Found object for id '%llu'."), objId);
	if (cachedObject != NULL)
	{
		cacheSize_t origSize = cachedObject->GetSize();
		cacheSize_t neededSpace = GetFilesystemFileSize(newSize) -
		                          GetFilesystemFileSize(origSize);
		if (!CheckForSize(neededSpace))
		{
			MojLogInfo(s_log,
			           _T("Resize: Attempting to cleanup cache for '%d' bytes."),
			           neededSpace);
			Cleanup(neededSpace);
		}
		if (CheckForSize(neededSpace))
		{
			finalSize = cachedObject->Resize(newSize);
			if (finalSize != origSize)
			{
				m_cacheSize += (GetFilesystemFileSize(finalSize) -
				                GetFilesystemFileSize(origSize));
				UpdateObject(objId);
				MojLogInfo(s_log, _T("Resize: Object '%llu' resized to '%d'."),
				           objId, finalSize);
			}
			else
			{
				MojLogInfo(s_log, _T("Resize: Object '%llu' not resized."),
				           objId);
			}
		}
		else
		{
			MojLogWarning(s_log,
			              _T("Resize: No space available to resize object '%llu'."),
			              objId);
		}
	}
	else
	{
		MojLogWarning(s_log, _T("Resize: Object '%llu' does not exists."), objId);
	}

	return finalSize;
}

// Expire an object in the cache.  This will cause the object to be
// deleted.  CFileCacheSet should remove the cachedObjectId_t from it's
// m_idMap.  This will return false if the requested item is
// currently pinned in the cache by a subscription and the object
// will be deleted once the subscription expires.
bool
CFileCache::Expire(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	bool retVal = true;
	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{

		cacheSize_t objSize = cachedObject->GetSize();

		// Remove it from the cache list if it is still there
		if (!cachedObject->isExpired())
		{
			std::list<cachedObjectId_t>::iterator iter;
			iter = m_cacheList.begin();
			while (iter != m_cacheList.end())
			{
				if (*iter == objId)
				{
					m_cacheList.erase(iter);
					MojLogDebug(s_log,
					            _T("Expire: Object '%llu' removed from active cache list."),
					            objId);
					break;
				}
				++iter;
			}
		}
		// Now try to actually remove the object, this will return false
		// if the object is still subscribed or if the unlink fails.  If
		// still subscribed, the unsubscribe will remove the object, if
		// the unlink fails, it will be retried by the timer worker call.
		retVal = cachedObject->Expire();
		if (retVal)
		{
			// Remove it from the map so no further work is done on it.
			// We'd like to do this earlier but then we can't look the
			// object up on unsubscribe calls after an object was expired.
			// This way we only remove the lookup reference once the expire
			// call is successful.
			m_cachedObjects.erase(objId);
			m_numObjects--;
			m_cacheSize -= GetFilesystemFileSize(objSize);
			delete cachedObject;
			MojLogWarning(s_log, _T("Expire: Object '%llu' removed from the cache."),
			              objId);
		}
		else
		{
			MojLogInfo(s_log, _T("Expire: Object '%llu' expired but still in use."),
			           objId);
		}
	}
	else
	{
		MojLogWarning(s_log, _T("Expire: Object '%llu' does not exist."), objId);
	}

	return retVal;
}

// Subscribing to an object is the means to pin an object in the
// cache.  This means that for the duration of the subscription, the
// object is guaranteed not to be deleted from the cache.  This is also
// how you obtain the pathname to the cached object.
const std::string
CFileCache::Subscribe(std::string &msgText, const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	std::string retVal("");
	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{
		retVal = cachedObject->Subscribe(msgText);
		if (!retVal.empty() && msgText.empty())
		{
			UpdateObject(objId);
			MojLogInfo(s_log,
			           _T("Subscribe: Subscribed to object '%llu' at path '%s'."),
			           objId, retVal.c_str());
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("Subscribe: Object '%llu' does not exists."), objId);
	}

	return retVal;
}

// Unsubscribing an object removes the pin of the object in the
// cache.  This means that there is no longer any guarantee of available
// of the object in the cache.
void
CFileCache::UnSubscribe(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{
		cacheSize_t origSize = cachedObject->GetSize();
		cachedObject->UnSubscribe();
		MojLogInfo(s_log,
		           _T("UnSubscribe: UnSubscribed from object '%llu'."), objId);
		cacheSize_t finalSize = cachedObject->GetSize();
		if (finalSize != origSize)
		{
			m_cacheSize += (GetFilesystemFileSize(finalSize) -
			                GetFilesystemFileSize(origSize));
			MojLogInfo(s_log,
			           _T("UnSubscribe: Adjusting cache for new file size of '%d' bytes."),
			           finalSize);
		}
		UpdateObject(objId);
	}
	else
	{
		MojLogWarning(s_log, _T("UnSubscribe: Object '%llu' does not exists."),
		              objId);
	}
}

// This updates the access time without needing to subscribe, it's
// like using touch on an existing file
bool
CFileCache::Touch(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	bool retVal = false;
	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{
		cachedObject->Touch();
		UpdateObject(objId);
		MojLogInfo(s_log, _T("Touch: Updated access time for object '%llu'."),
		           objId);
		retVal = true;
	}
	else
	{
		MojLogWarning(s_log, _T("Touch: Object '%llu' does not exists."), objId);
	}

	return retVal;
}

// Get a vector containing pairs of all the objects in the cache and
// their object IDs.
std::vector<std::pair<cachedObjectId_t, CCacheObject *>>
        CFileCache::GetCachedObjects()
{

	MojLogTrace(s_log);

	std::vector < std::pair < cachedObjectId_t,
	    CCacheObject * > > objs(m_cachedObjects.size());
	std::map<cachedObjectId_t, CCacheObject *>::iterator iter;
	iter = m_cachedObjects.begin();
	int i = 0;
	while (iter != m_cachedObjects.end())
	{
		objs[i++] = std::make_pair((*iter).first, (*iter).second);
		++iter;
	}
	MojLogDebug(s_log, _T("GetCachedObjects: Found '%zd' objects."),
	            m_cachedObjects.size());
	MojLogDebug(s_log, _T("GetCachedObjects: Returned '%zd' objects."),
	            objs.size());

	return objs;
}

// Check if there is space in the cache for a new object of size
bool
CFileCache::CheckForSize(cacheSize_t size)
{

	MojLogTrace(s_log);

	bool retVal = false;
	cacheSize_t availSpace = GetFileCacheSet()->TotalCacheSpace() -
	                         GetFileCacheSet()->SumOfCacheSizes();

	// This is part of the fix for NOV-128944.
	if (availSpace < 0)
	{
		availSpace = 0;
	}

	MojLogDebug(s_log,
	            _T("CheckForSize: Free cache space '%d', free space '%d'."),
	            (m_hiWatermark - m_cacheSize), availSpace);
	if (((m_cacheSize + size) < m_hiWatermark) && (size <= availSpace))
	{
		retVal = true;
	}

	return retVal;
}

// Cleanup an expirable object in the cache list. Return -1 if no more items in list.
cacheSize_t
CFileCache::CleanupCache(cachedObjectId_t *cleanedId)
{

	MojLogTrace(s_log);

	bool expired = false;
	cachedObjectId_t objId = 0;
	cacheSize_t size = -1;
	while (!m_cacheList.empty() && !expired)
	{
		objId = m_cacheList.back();
		m_cacheList.pop_back();
		size = GetObjectSize(objId); // size will always be >= 0
		expired = GetFileCacheSet()->ExpireCacheObject(objId);
	}
	if (expired)
	{
		if (cleanedId != NULL)
		{
			*cleanedId = objId;
		}
		MojLogInfo(s_log,
		           _T("CleanupCache: Expired object '%llu', freed space '%d'."),
		           objId, size);

		return size;
	}
	else
	{
		return -1;
	}
}

// Cleanup this cache
void
CFileCache::Cleanup(cacheSize_t size)
{

	MojLogTrace(s_log);

	if (size < m_hiWatermark)
	{
		while (((m_cacheSize + size) >= m_hiWatermark) &&
		        (CleanupCache(NULL) >= 0))
		{
		}
		cacheSize_t availSpace = GetFileCacheSet()->TotalCacheSpace() -
		                         GetFileCacheSet()->SumOfCacheSizes();

		// This is part of the fix for NOV-128944.
		if (availSpace < 0)
		{
			availSpace = 0;
		}
		if (size > availSpace)
		{
			GetFileCacheSet()->CleanupAllTypes(size - availSpace);
		}
	}
}

// Return the object cost
paramValue_t
CFileCache::GetCacheCost(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	paramValue_t cost = -1;
	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{
		cost = cachedObject->GetCacheCost();
		MojLogDebug(s_log,
		            _T("GetCacheCost: Object '%llu' has a cleanup cost of '%d'."),
		            objId, cost);
	}
	else
	{
		MojLogWarning(s_log, _T("GetCacheCost: Object '%llu' does not exists."),
		              objId);
	}

	return cost;
}

// Get the best object from this cache for cleanup
cachedObjectId_t
CFileCache::GetCleanupCandidate()
{

	MojLogTrace(s_log);

	cachedObjectId_t objId = 0;
	if ((m_cacheSize > m_loWatermark) && !m_cacheList.empty())
	{
		objId = m_cacheList.back();
	}

	return objId;
}

// Cleanup orphaned objects
void
CFileCache::CleanupOrphanedObjects()
{

	MojLogTrace(s_log);

	std::map<cachedObjectId_t, CCacheObject *>::const_iterator iter;
	std::vector<cachedObjectId_t> cleanups;
	iter = m_cachedObjects.begin();
	while (iter != m_cachedObjects.end())
	{
		if ((*iter).second->isExpired())
		{
			cleanups.push_back((*iter).first);
		}
		++iter;
	}
	while (!cleanups.empty())
	{
		cachedObjectId_t objId = cleanups.back();
		Expire(objId);
		cleanups.pop_back();
	}
}

// Return information about the current state of the cache.  The
// total space used by the cache as well as the number of cached
// objects are returned in the parameters.  The typename of the
// cache is returned.
std::string &
CFileCache::GetCacheStatus(cacheSize_t *cacheSize,
                           paramValue_t *numCacheObjects)
{

	MojLogTrace(s_log);

	if (cacheSize != NULL)
	{
		*cacheSize = m_cacheSize;
	}
	if (numCacheObjects != NULL)
	{
		*numCacheObjects = m_numObjects;
	}

	return m_cacheType;
}

// This returns the space used by a cached object.
cacheSize_t
CFileCache::GetObjectSize(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	cacheSize_t retVal = -1;
	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{
		retVal = cachedObject->GetSize();
	}
	else
	{
		MojLogWarning(s_log, _T("GetObjectSize: Object '%llu' does not exists."),
		              objId);
	}

	return retVal;
}

// This returns the filename of a cached object
const std::string
CFileCache::GetObjectFilename(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	std::string retVal;
	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{
		retVal = cachedObject->GetFileName();
	}
	else
	{
		MojLogWarning(s_log, _T("GetObjectFilename: Object '%llu' does not exists."),
		              objId);
	}

	return retVal;
}

bool
CFileCache::isCleanable()
{

	MojLogTrace(s_log);

	bool retVal = true;
	if (!m_cachedObjects.empty())
	{
		std::map<cachedObjectId_t, CCacheObject *>::const_iterator iter;
		iter = m_cachedObjects.begin();
		while (iter != m_cachedObjects.end())
		{
			if ((*iter).second->GetSubscriptionCount() > 0)
			{
				retVal = false;
				break;
			}
			++iter;
		}
	}

	return retVal;
}

CCacheObject *
CFileCache::GetCacheObjectForId(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	MojLogDebug(s_log,
	            _T("GetCacheObjectForId: Searching '%zd' objects for object '%llu'."),
	            m_cachedObjects.size(), objId);

	CCacheObject *retVal = NULL;
	std::map<cachedObjectId_t, CCacheObject *>::iterator iter;
	iter = m_cachedObjects.find(objId);
	if (iter != m_cachedObjects.end())
	{
		retVal = (*iter).second;
	}
#ifdef DEBUG
	else
	{
		MojLogDebug(s_log,
		            _T("GetCacheObjectForId: Failed to find object for id '%llu'."),
		            objId);
		int i = 0;
		iter = m_cachedObjects.begin();
		while (iter != m_cachedObjects.end())
		{
			MojLogDebug(s_log,
			            _T("GetCacheObjectForId: cached object '%d'  has id '%llu'."),
			            i++, (*iter).first);
			++iter;
		}
	}
#endif // #ifdef DEBUG

	return retVal;
}

// Update the cache list so the specified object is at the front of
// the list
void
CFileCache::UpdateObject(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	std::list<cachedObjectId_t>::iterator iter;
	iter = m_cacheList.begin();
	while (iter != m_cacheList.end())
	{
		if (*iter == objId)
		{
			m_cacheList.erase(iter);
			m_cacheList.push_front(objId);
			break;
		}
		++iter;
	}
}

// Validate a subscribed object.
void
CFileCache::CheckSubscribedObject(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	CCacheObject *cachedObject = GetCacheObjectForId(objId);
	if (cachedObject != NULL)
	{
		MojLogDebug(s_log,
		            _T("CheckSubscribedObject: Object '%llu' has subscription count '%d'."),
		            objId, cachedObject->GetSubscriptionCount());
		if (!cachedObject->isWritten())
		{
			cachedObject->Validate();
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("CheckSubscribedObject: Object '%llu' does not exists."),
		              objId);
	}
}

// Cleanup any unsubscribed directory types
void
CFileCache::CleanupDirType()
{

	MojLogTrace(s_log);

	std::map<cachedObjectId_t, CCacheObject *>::const_iterator iter;
	std::vector<cachedObjectId_t> cleanups;
	iter = m_cachedObjects.begin();
	while (iter != m_cachedObjects.end())
	{
		if ((*iter).second->GetSubscriptionCount() == 0)
		{
			cleanups.push_back((*iter).first);
		}
		++iter;
	}
	if (!cleanups.empty())
	{
		MojLogInfo(s_log, _T("CleanupDirType: Cleaning type '%s'."),
		           m_cacheType.c_str());
	}
	while (!cleanups.empty())
	{
		cachedObjectId_t objId = cleanups.back();
		MojLogDebug(s_log, _T("CleanupDirType: Cleaning object '%llu'."), objId);
		bool expired = false;
		if (m_cachedObjects[objId]->isExpired())
		{
			expired = Expire(objId);
		}
		else
		{
			expired = m_fileCacheSet->ExpireCacheObject(objId);
		}
		if (expired)
		{
			MojLogWarning(s_log, _T("CleanupDirType: Expired object '%llu'."), objId);
		}
		cleanups.pop_back();
	}
}

// This writes the configuration values for this type to a file in the
// type directory
bool
CFileCache::WriteConfig()
{

	MojLogTrace(s_log);

	bool retVal = false;
	// Create the full path name from the file cache base directory, the
	// typername, the id and the filename
	std::string pathname(GetFileCacheSet()->GetBaseDirName());
	pathname += "/" + m_cacheType;

	int errVal = ::mkdir(pathname.c_str(), s_dirPerms);
	if (errVal != 0 && errno != EEXIST)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("WriteConfig: Failed to create directory '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
	}
	else
	{
		// Now create the description file and write the defaults
		pathname += "/" + s_typeConfigFilename;

		std::string tmpFile(pathname + ".tmp");
		std::ofstream outfile(tmpFile.c_str());
		if (!outfile.fail())
		{
			MojLogInfo(s_log,
			           _T("WriteConfig: Writing configuration to file '%s'."),
			           pathname.c_str());

			outfile << s_loWatermark << " " << m_loWatermark << std::endl;
			outfile << s_hiWatermark << " " << m_hiWatermark << std::endl;
			outfile << s_defaultSize << " " << m_defaultSize << std::endl;
			outfile << s_defaultCost << " " << m_defaultCost << std::endl;
			outfile << s_defaultLifetime << " " << m_defaultLifetime << std::endl;
			outfile << s_dirType << " " << (m_dirType ? 1 : 0) << std::endl;
			outfile.close();
			bool writeOK = outfile.good();
			if (writeOK)
			{
				std::string msgText;
				writeOK = SyncFile(tmpFile, msgText);
				MojLogDebug(s_log, _T("WriteConfig: SyncFile was %s."),
				            writeOK ? "successful" : "unsuccessful");
				if (!writeOK && !msgText.empty())
				{
					MojLogError(s_log, _T("WriteConfig: %s"), msgText.c_str());
				}
			}
			else
			{
				MojLogError(s_log,
				            _T("WriteConfig: Failed to write file '%s'."),
				            tmpFile.c_str());
			}

			if (writeOK)
			{
				errVal = ::rename(tmpFile.c_str(), pathname.c_str());
				if (errVal != 0)
				{
					int savedErrno = errno;
					MojLogError(s_log,
					            _T("WriteConfig: Failed to rename temp file '%s' to '%s' (%s)."),
					            tmpFile.c_str(), pathname.c_str(), ::strerror(savedErrno));
					::unlink(tmpFile.c_str());
				}
				else
				{
					retVal = true;
				}
			}
		}
		else
		{
			MojLogError(s_log,
			            _T("WriteConfig: Failed to open temp configuration file '%s'."),
			            tmpFile.c_str());
		}
	}

	return retVal;
}

// This reads the configuration values for this type from a file in
// the type directory
bool
CFileCache::ReadConfig()
{

	MojLogTrace(s_log);

	// Create the full path name from the file cache base directory, the
	// typername, the id and the filename
	std::string pathname(GetFileCacheSet()->GetBaseDirName());
	pathname += "/" + m_cacheType + "/" + s_typeConfigFilename;

	bool retVal = false;
	std::ifstream infile(pathname.c_str());
	if (!infile.fail())
	{
		MojLogInfo(s_log,
		           _T("ReadConfig: Reading configuration from file '%s'."),
		           pathname.c_str());
		std::string label;
		paramValue_t value;
		std::set<std::string> labels;

		while ((infile >> label).good())
		{
			infile >> value;
			if (label == s_loWatermark)
			{
				m_loWatermark = value;
				labels.insert(s_loWatermark);
			}
			else if (label == s_hiWatermark)
			{
				m_hiWatermark = value;
				labels.insert(s_hiWatermark);
			}
			else if (label == s_defaultSize)
			{
				m_defaultSize = value;
				labels.insert(s_defaultSize);
			}
			else if (label == s_defaultCost)
			{
				m_defaultCost = value;
				labels.insert(s_defaultCost);
			}
			else if (label == s_defaultLifetime)
			{
				m_defaultLifetime = value;
				labels.insert(s_defaultLifetime);
			}
			else if (label == s_dirType)
			{
				if (value != 0)
				{
					m_dirType = true;
				}
				else
				{
					m_dirType = false;
				}
				labels.insert(s_dirType);
			}
		}
		infile.close();

		if (labels.size() != s_numLabels)
		{
			MojLogError(s_log,
			            _T("ReadConfig: Failed to read complete configuration"));
		}
		else
		{
			retVal = true;
		}
	}
	else
	{
		MojLogError(s_log,
		            _T("ReadConfig: Failed to open configuration file '%s'."),
		            pathname.c_str());
	}

	return retVal;
}

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

#include "FileCacheSet.h"

#include <iostream>
#include <time.h>
#include <sys/time.h>

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

MojLogger CFileCacheSet::s_log(_T("filecache.filecacheset"));

CFileCacheSet::CFileCacheSet(bool init) : m_totalCacheSpace(0)
{

	MojLogTrace(s_log);

	if (init)
	{
		// Read the configuration file to get the system wide settings
		ReadConfig(s_configFile);

		// Make sure the cache directory exists and set the correct
		// directory permissions
		int retVal = ::mkdir(m_baseDirName.c_str(), s_dirPerms);
		if (retVal != 0 && errno != EEXIST)
		{
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("CFileCacheSet: Failed to create cache directory '%s' (%s)."),
			            m_baseDirName.c_str(), ::strerror(savedErrno));
			exit(-1);
		}
	}

	// This provides a pseudo-random seed.and either reads the last
	// saved sequence number (and bumps the value by the write
	// interval) or else initializes the sequence number to 1.
	srand48((long) ::time(0));
	ReadSequenceNumber();
}

// This defines a new cache type and will cause a new CFileCache
// object to be instantiated.  The typeName must be unique.  The sum
// of the cache loWatermarks must be less than the total cache space
// available or the cache type creation will fail.
bool
CFileCacheSet::DefineType(std::string &msgText, const std::string &typeName,
                          CCacheParamValues *params, bool dirType)
{

	MojLogTrace(s_log);

	msgText = "DefineType: ";
	bool retVal = false;
	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache == NULL)
	{
		CFileCache *newType = new CFileCache(this, typeName);
		if (newType != NULL)
		{
			if (newType->Configure(params, dirType))
			{
				m_cacheSet.insert(std::map < const std::string,
				                  CFileCache * >::value_type(typeName, newType));
				retVal = true;
				msgText += "Created type '" + typeName + "'.";
				MojLogInfo(s_log, _T("%s"), msgText.c_str());
			}
			else
			{
				delete newType;
				msgText += "Failed to configure '" + typeName + "'.";
				MojLogError(s_log, _T("%s"), msgText.c_str());
			}
		}
		else
		{
			msgText += "Failed to allocate '" + typeName + "'.";
			MojLogCritical(s_log, _T("%s"), msgText.c_str());
		}
	}
	else
	{
		msgText += "Type '" + typeName + "'" + " already exists.";
		MojLogWarning(s_log, _T("%s"), msgText.c_str());
	}

	return retVal;
}

// This allows a value in the cache configuration to be modified.
// Returns false if it can't configure the cache based on the
// specified configuration and continues to use the last
// configuration.
bool
CFileCacheSet::ChangeType(std::string &msgText, const std::string &typeName,
                          CCacheParamValues *params)
{

	MojLogTrace(s_log);

	msgText = "ChangeType: ";
	bool retVal = false;
	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache != NULL)
	{
		retVal = fileCache->Configure(params);
		msgText += "Configured type '" + typeName + "'.";
		MojLogInfo(s_log, _T("%s"), msgText.c_str());
	}
	else
	{
		msgText += "Type '" + typeName + "'does not exist.";
		MojLogWarning(s_log, _T("%s"), msgText.c_str());
	}

	return retVal;
}

// Deletes an existing cache type.  All objects cached in this type
// will be deleted.  The space used by the deleted cache (in KB) is
// returned.
cacheSize_t
CFileCacheSet::DeleteType(std::string &msgText,
                          const std::string &typeName)
{

	MojLogTrace(s_log);

	msgText = "DeleteType: ";
	cacheSize_t retVal = -1;
	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache != NULL)
	{
		if (fileCache->isCleanable())
		{
			cacheSize_t size;
			paramValue_t numObjs;
			fileCache->GetCacheStatus(&size, &numObjs);
			retVal = size;

			// Go through and Expire any remaining cache items
			std::vector<std::pair<cachedObjectId_t, CCacheObject *>> curObjs;
			curObjs = fileCache->GetCachedObjects();
			while (!curObjs.empty())
			{
				std::pair<cachedObjectId_t, CCacheObject *> curObj = curObjs.back();
				curObjs.pop_back();
				if (!ExpireCacheObject(curObj.first))
				{
					MojLogWarning(s_log,
					              _T("DeleteType: object %llu is still subscribed."),
					              curObj.first);
				}
			}

			m_cacheSet.erase(typeName);
			delete fileCache;
			msgText += "Deleted type '" + typeName + "'.";
			MojLogInfo(s_log, _T("%s"), msgText.c_str());
		}
		else
		{
			msgText += "Type '" + typeName + "' has subscribed objects.";
			MojLogWarning(s_log, _T("%s"), msgText.c_str());
		}
	}
	else
	{
		msgText += "Type '" + typeName + "' does not exist.";
		MojLogWarning(s_log, _T("%s"), msgText.c_str());
	}

	return retVal;
}

// Return a vector of all defined cache types.
const std::vector<std::string>
CFileCacheSet::GetTypes()
{

	MojLogTrace(s_log);

	std::vector<std::string> cacheTypes(m_cacheSet.size());
	std::map<const std::string, CFileCache *>::const_iterator iter;
	iter = m_cacheSet.begin();
	int i = 0;
	while (iter != m_cacheSet.end())
	{
		cacheTypes[i++] = (*iter).first;
		++iter;
	}

	return cacheTypes;
}

// Get the configuration values for a cache type.
CCacheParamValues
CFileCacheSet::DescribeType(const std::string &typeName)
{

	MojLogTrace(s_log);

	CCacheParamValues params;
	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache != NULL)
	{
		fileCache->Describe(params);
	}
	else
	{
		MojLogWarning(s_log, _T("DescribeType: type '%s' does not exists."),
		              typeName.c_str());
	}

	return params;
}

// Select the object from the best choice from each cache to remove
CFileCache *
CFileCacheSet::SelectCandidateToExpire(std::map < CFileCache *,
                                       const cachedObjectId_t > & m_cleanupMap)
{

	MojLogTrace(s_log);

	std::map<CFileCache *, const cachedObjectId_t>::const_iterator iter;
	iter = m_cleanupMap.begin();
	CFileCache *retVal = NULL;
	paramValue_t lowCost = s_maxCost;
	while (iter != m_cleanupMap.end())
	{
		paramValue_t cost = ((*iter).first)->GetCacheCost((*iter).second);
		if (cost <= lowCost)
		{
			retVal = (*iter).first;
			lowCost = cost;
		}
		++iter;
	}
	return retVal;
}

// Cleanup all registered types, this is done when the cache set hits
// the total available space
cacheSize_t
CFileCacheSet::CleanupAllTypes(cacheSize_t neededSize)
{

	MojLogTrace(s_log);

	std::map<CFileCache *, const cachedObjectId_t> m_cleanupMap;
	std::map<const std::string, CFileCache *>::const_iterator iter;
	iter = m_cacheSet.begin();

	// This is part of the fix for bug NOV-128944.
	neededSize = GetFilesystemFileSize(neededSize);

	// Get the candidates for each cache type
	CFileCache *fileCache;
	while (iter != m_cacheSet.end())
	{
		fileCache = (*iter).second;
		if (fileCache != NULL)
		{
			const cachedObjectId_t candidate = fileCache->GetCleanupCandidate();
			if (candidate != 0)
			{
				m_cleanupMap.insert(std::map < CFileCache *,
				                    const cachedObjectId_t >::value_type(fileCache,
				                            candidate));
			}
		}
		++iter;
	}
	// Now continue clearing candidates until we've cleared requested space
	cacheSize_t cleanedSize = 0;
	while (cleanedSize < neededSize)
	{
		// Select which object to expire
		fileCache = SelectCandidateToExpire(m_cleanupMap);
		// If this comes back NULL, there is nothing else we can clean.
		if (fileCache == NULL)
		{
			break;
		}
		cachedObjectId_t objId = m_cleanupMap[fileCache];
		// This is part of the fix for bug NOV-128944.
		cacheSize_t size = GetFilesystemFileSize(CachedObjectSize(objId));
		m_cleanupMap.erase(fileCache);
		if (ExpireCacheObject(objId))
		{
			cleanedSize += size;
		}
		if (cleanedSize < neededSize)
		{
			cachedObjectId_t candidate = fileCache->GetCleanupCandidate();
			if (candidate != 0)
			{
				m_cleanupMap.insert(std::map < CFileCache *,
				                    const cachedObjectId_t >::value_type(fileCache,
				                            candidate));
			}
		}
	}

	return cleanedSize;
}

// Insert an object into the cache and returns the object id of that
// cache object.  The size value must be provided unless the size is
// the default non-zero size configured for the cache.  Any values
// provided for cost and lifetime will override the default
// configuration.

// This is the general one for making new cached objects
cachedObjectId_t
CFileCacheSet::InsertCacheObject(std::string &msgText,
                                 const std::string &typeName,
                                 const std::string &filename,
                                 cacheSize_t size, paramValue_t cost,
                                 paramValue_t lifetime)
{

	MojLogTrace(s_log);

	msgText = "InsertCacheObject: ";
	cachedObjectId_t retVal = 0;
	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache != NULL)
	{
		// If needed, overwrite values with the defaults for that cache
		// type.
		if ((size == 0) || (cost == 0) || (lifetime == 0))
		{
			CCacheParamValues params;
			fileCache->Describe(params);
			if (size == 0)
			{
				size = params.GetSize();
			}
			if (cost == 0)
			{
				cost = params.GetCost();
			}
			if (lifetime == 0)
			{
				lifetime = params.GetLifetime();
			}
		}
		// Check to ensure there is space in the cache We do this here so
		// we don't create the CCacheObject if the space doesn't exist
		cacheSize_t fsSize = GetFilesystemFileSize(size);
		if (!fileCache->CheckForSize(fsSize))
		{
			MojLogInfo(s_log,
			           _T("InsertCacheObject: Calling Cleanup to make space."));
			fileCache->Cleanup(fsSize);
		}
		if (fileCache->CheckForSize(fsSize))
		{
			cachedObjectId_t id = GetNextCachedObjectId();
			std::string subText;
			retVal = InsertCacheObject(subText, typeName, filename, id, size,
			                           cost, lifetime, false, true);
			if (retVal > 0)
			{
				msgText += "Inserted new object for filename '" + filename + "'.";
				MojLogInfo(s_log, _T("%s"), msgText.c_str());
			}
			else
			{
				msgText += subText;
			}
		}
		else
		{
			std::stringstream sizeString;
			sizeString << size;
			msgText += "Could not find '" + sizeString.str() +
			           "' bytes for object insert.";
			MojLogError(s_log, _T("%s"), msgText.c_str());
		}
	}
	else
	{
		msgText += "Type '" + typeName + "' does not exist.";
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}

	return retVal;
}

// This one is used on start-up when rebuilding from the filesystem
// Since the file tree walk will find all types, it will instantiate
// them so we won't check for them here.  We also assume the values
// for size, cost and lifetime were already substituted by defaults if
// necessary.
cachedObjectId_t
CFileCacheSet::InsertCacheObject(std::string &msgText,
                                 const std::string &typeName,
                                 const std::string &filename,
                                 const cachedObjectId_t objectId,
                                 cacheSize_t size, paramValue_t cost,
                                 paramValue_t lifetime, bool written,
                                 bool isNew)
{

	MojLogTrace(s_log);

	cachedObjectId_t retVal = 0;
	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache != NULL)
	{
		CCacheObject *newObj = new CCacheObject(fileCache, objectId, filename,
		                                        size, cost, lifetime, written,
		                                        fileCache->isDirType());
		if (newObj != NULL)
		{
			if (newObj->Initialize(isNew))
			{
				fileCache->Insert(newObj);
				m_idMap.insert(std::map < const cachedObjectId_t,
				               const std::string >::value_type(objectId, typeName));
				retVal = objectId;
			}
			else
			{
				delete newObj;
				msgText += "Failed to initialize new object for '" + filename + "'.";
				MojLogError(s_log, _T("%s"), msgText.c_str());
			}
		}
		else
		{
			msgText += "Failed to allocate new object for '" + filename + "'.";
			MojLogError(s_log, _T("%s"), msgText.c_str());
		}
	}
	else
	{
		msgText += "InsertCacheObject: Type '" + typeName + "' does not exists.";
		MojLogWarning(s_log, _T("%s"), msgText.c_str());
	}

	return retVal;
}

// Request to change the size of an object.  This is only valid
// while the initial writable subscription is in effect.  If there
// isn't sufficient space, the resize will return the original size
// of the object and that should be taken as a failure.  On success,
// the new size will be returned.
cacheSize_t
CFileCacheSet::Resize(const cachedObjectId_t objId, cacheSize_t newSize)
{

	MojLogTrace(s_log);

	cacheSize_t retVal = CachedObjectSize(objId);
	const std::string typeName(GetTypeForObjectId(objId));
	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			retVal = fileCache->Resize(objId, newSize);
		}
		else
		{
			MojLogWarning(s_log,
			              _T("Resize: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("Resize: Cache type not found for id '%llu'."), objId);
	}

	return retVal;
}

// Expire an object from the cache.  This will cause the object to
// be deleted from the cache.  This will return false if the
// requested item is currently pinned in the cache by a subscription
// and the object will be deleted once the subscription expires.
bool
CFileCacheSet::ExpireCacheObject(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	bool retVal = true;
	const std::string typeName(GetTypeForObjectId(objId));
	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			RemoveObjectFromIdMap(objId);
			retVal = fileCache->Expire(objId);
			if (!retVal)
			{
				MojLogInfo(s_log,
				           _T("ExpireCacheObject: expire deferred, object '%llu' in use"),
				           objId);
			}
		}
		else
		{
			MojLogWarning(s_log,
			              _T("ExpireCacheObject: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("ExpireCacheObject: Cache type not found for id '%llu'."),
		              objId);
	}

	return retVal;
}

// Pin an object in the cache by allowing a client to subscribe to
// the object.  This will guarantee the object will not be removed
// from the cache while the subscription is active.  Returns the
// file path associated with the objectId.
const std::string
CFileCacheSet::SubscribeCacheObject(std::string &msgText,
                                    const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	std::string retVal("");
	const std::string typeName(GetTypeForObjectId(objId));
	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			retVal = fileCache->Subscribe(msgText, objId);
			if (msgText.empty())
			{
				MojLogInfo(s_log,
				           _T("SubscribeCacheObject: Object '%llu' subscribed."), objId);
			}
		}
		else
		{
			MojLogWarning(s_log,
			              _T("SubscribeCacheObject: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("SubscribeCacheObject: Cache type not found for id '%llu'."),
		              objId);
	}

	return retVal;
}

// Remove the client subscription of an object.  This will remove
// the guarantee that the object will be kept in the cache.
void
CFileCacheSet::UnSubscribeCacheObject(const std::string &typeName,
                                      const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			fileCache->UnSubscribe(objId);
			MojLogInfo(s_log,
			           _T("UnSubscribeCacheObject: Object '%llu' unsubscribed."),
			           objId);
		}
		else
		{
			MojLogWarning(s_log,
			              _T("UnSubscribeCacheObject: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("UnSubscribeCacheObject: Cache type not found for id '%llu'."),
		              objId);
	}
}

// This updates the access time without needing to subscribe, it's
// like using touch on an existing file
bool
CFileCacheSet::Touch(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	bool retVal = false;
	const std::string typeName(GetTypeForObjectId(objId));
	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			retVal = fileCache->Touch(objId);
			MojLogInfo(s_log, _T("Touch: Object '%llu' touched."), objId);
		}
		else
		{
			MojLogWarning(s_log,
			              _T("Touch: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log, _T("Touch: Cache type not found for id '%llu'."),
		              objId);
	}

	return retVal;
}

// Get the current status of the cache as a whole.  The current
// amount of space used in all the caches will be returned in size.
// The current number of active cached objects will be returned in
// numCacheObjects.  The availableSpace is the sum of the cache
// loWatermark values minus the current amount of space in use.
// This is the guaranteed amount of space available.  Returns the
// number of cache types.
cacheSize_t
CFileCacheSet::GetCacheStatus(cacheSize_t *size,
                              paramValue_t *numCacheObjects,
                              cacheSize_t *availSpace)
{

	MojLogTrace(s_log);

	cacheSize_t cacheSize = 0;
	paramValue_t numObjects = 0;
	std::map<const std::string, CFileCache *>::const_iterator iter;
	iter = m_cacheSet.begin();
	while (iter != m_cacheSet.end())
	{
		CFileCache *fileCache = (*iter).second;

		// First accumulate the size and number of objects;
		cacheSize_t sz;
		paramValue_t nco;
		fileCache->GetCacheStatus(&sz, &nco);
		cacheSize += sz;
		numObjects += nco;
		++iter;
	}

	MojLogInfo(s_log,
	           _T("GetCacheStatus: numtypes = '%zd', size = '%d', numobjs = '%d', space = '%d'"),
	           m_cacheSet.size(), cacheSize, numObjects,
	           (SumOfLoWatermarks() - cacheSize));

	if (size != NULL)
	{
		*size = cacheSize;
	}
	if (numCacheObjects != NULL)
	{
		*numCacheObjects = numObjects;
	}
	if (availSpace != NULL)
	{
		*availSpace = SumOfLoWatermarks() - cacheSize;

		// Part of the fix for NOV-128944.
		if (*availSpace < 0)
		{
			*availSpace = 0;
		}
	}

	return (cacheSize_t) m_cacheSet.size();
}

// Gets the current status of a specied cache type.  Returns the
// amount of space and the number of cached objects used by the
// items in that cache type.
bool
CFileCacheSet::GetCacheTypeStatus(const std::string &typeName,
                                  cacheSize_t *size,
                                  paramValue_t *numCacheObjects)
{

	MojLogTrace(s_log);

	bool retVal = false;
	cacheSize_t cacheSize = 0;
	paramValue_t numObjects = 0;

	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache != NULL)
	{
		fileCache->GetCacheStatus(&cacheSize, &numObjects);
		if (size != NULL)
		{
			*size = cacheSize;
		}
		if (numCacheObjects != NULL)
		{
			*numCacheObjects = numObjects;
		}

		MojLogInfo(s_log, _T("GetCacheTypeStatus: size = '%d', numobjs = '%d'"),
		           cacheSize, numObjects);
		retVal = true;
	}
	else
	{
		MojLogWarning(s_log,
		              _T("GetCacheTypeStatus: No cache of type '%s' found."),
		              typeName.c_str());
	}

	return retVal;
}

// Returns the size of a cached object or -1 if the object is no
// longer in the cache.
cacheSize_t
CFileCacheSet::CachedObjectSize(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	cacheSize_t retVal = -1;
	const std::string typeName(GetTypeForObjectId(objId));
	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			retVal = fileCache->GetObjectSize(objId);
			MojLogInfo(s_log,
			           _T("CachedObjectSize: Object '%llu' is size '%d'."),
			           objId, retVal);
		}
		else
		{
			MojLogWarning(s_log,
			              _T("CachedObjectSize: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("CachedObjectSize: Cache type not found for id '%llu'."),
		              objId);
	}

	return retVal;
}

// Returns the filename of a cachedObject
const std::string
CFileCacheSet::CachedObjectFilename(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	std::string retVal;
	const std::string typeName(GetTypeForObjectId(objId));
	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			retVal = fileCache->GetObjectFilename(objId);
			MojLogInfo(s_log,
			           _T("CachedObjectFilename: Object '%llu' has name '%s'."),
			           objId, retVal.c_str());
		}
		else
		{
			MojLogWarning(s_log,
			              _T("CachedObjectFilename: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("CachedObjectFilename: Cache type not found for id '%llu'."),
		              objId);
	}

	return retVal;

}

// Read the FileCache configuration file to get the system wide
// default values
void
CFileCacheSet::ReadConfig(const std::string &configFile)
{

	MojLogTrace(s_log);

	m_totalCacheSpace = s_defaultCacheSpace;
	m_baseDirName = s_defaultBaseDirName;

	std::ifstream infile(configFile.c_str());
	if (infile)
	{
		std::string label;

		while ((infile >> label).good())
		{
			if (label == s_totalCacheSpace)
			{
				infile >> m_totalCacheSpace;
				MojLogInfo(s_log, _T("ReadConfig: '%s' = '%d'."),
				           s_totalCacheSpace.c_str(), m_totalCacheSpace);
			}
			else if (label == s_baseDirName)
			{
				infile >> m_baseDirName;
				MojLogInfo(s_log, _T("ReadConfig: '%s' = '%s'."),
				           s_baseDirName.c_str(), m_baseDirName.c_str());
			}
		}
		infile.close();
	}
	else
	{
		MojLogInfo(s_log,
		           _T("ReadConfig: Failed to open config file '%s'."),
		           configFile.c_str());
	}
}

void
CFileCacheSet::ReadSequenceNumber()
{

	MojLogTrace(s_log);

	std::string seqNumFile(m_baseDirName + "/" + s_seqNumFilename);
	std::ifstream infile(seqNumFile.c_str());
	if (infile)
	{
		infile >> m_sequenceNumber;
		MojLogDebug(s_log,
		            _T("ReadSequenceNumber: read %d, will add %d."),
		            m_sequenceNumber, s_sequenceBumpCnt);
		m_sequenceNumber += s_sequenceBumpCnt;
		if (m_sequenceNumber < 1 || m_sequenceNumber > s_maxAllowSeqNum)
		{
			m_sequenceNumber = 1;
			MojLogDebug(s_log,
			            _T("ReadSequenceNumber: Sequence number roll-over observed."));
		}
		infile.close();
	}
	else
	{
		m_sequenceNumber = 1;
	}
	WriteSequenceNumber();
	MojLogInfo(s_log,
	           _T("ReadSequenceNumber: Beginning with sequence number %d"),
	           m_sequenceNumber);
}

void
CFileCacheSet::WriteSequenceNumber()
{

	MojLogTrace(s_log);

	if (m_baseDirName.empty())
	{
		MojLogWarning(s_log,
		              _T("WriteSequenceNumber: No directory set, not saving sequence number."));
	}
	else
	{
		std::string seqNumFile(m_baseDirName + "/" + s_seqNumFilename);
		std::string tmpFile(seqNumFile + ".tmp");
		std::ofstream outfile(tmpFile.c_str());
		if (!outfile.fail())
		{
			MojLogInfo(s_log,
			           _T("WriteSequenceNumber: Writing sequence number %d to file '%s'."),
			           m_sequenceNumber, tmpFile.c_str());
			outfile << m_sequenceNumber << std::endl;
			outfile.close();
			bool writeOK = outfile.good();
			if (writeOK)
			{
				std::string msgText;
				writeOK = SyncFile(tmpFile, msgText);
				MojLogDebug(s_log, _T("WriteSequenceNumber: SyncFile was %s."),
				            writeOK ? "successful" : "unsuccessful");
				if (!writeOK && !msgText.empty())
				{
					MojLogError(s_log, _T("WriteSequenceNumber: %s"), msgText.c_str());
				}
			}
			else
			{
				MojLogError(s_log,
				            _T("WriteSequenceNumber: Failed to write file '%s'."),
				            tmpFile.c_str());
			}

			if (writeOK)
			{
				int retVal = ::rename(tmpFile.c_str(), seqNumFile.c_str());
				if (retVal != 0)
				{
					int savedErrno = errno;
					MojLogError(s_log,
					            _T("WriteSequenceNumber: Failed to rename file '%s' to '%s' (%s)."),
					            tmpFile.c_str(), seqNumFile.c_str(), ::strerror(savedErrno));
					::unlink(tmpFile.c_str());
				}
			}
		}
		else
		{
			MojLogError(s_log,
			            _T("WriteSequenceNumber: Failed to open file '%s'."),
			            seqNumFile.c_str());
		}
	}
}

// Get the sum of the loWatermark values for each configured cache
cacheSize_t
CFileCacheSet::SumOfLoWatermarks()
{

	MojLogTrace(s_log);

	cacheSize_t lwm = 0;
	std::map<const std::string, CFileCache *>::const_iterator iter;
	iter = m_cacheSet.begin();
	while (iter != m_cacheSet.end())
	{
		CFileCache *fileCache = (*iter).second;
		CCacheParamValues params;
		fileCache->Describe(params);
		lwm += params.GetLoWatermark();

		++iter;
	}

	return lwm;
}

// Compute the sum of current sizes for each of the configured caches
cacheSize_t
CFileCacheSet::SumOfCacheSizes()
{

	MojLogTrace(s_log);

	cacheSize_t sumOfSizes = 0;
	std::map<const std::string, CFileCache *>::const_iterator iter;
	iter = m_cacheSet.begin();
	while (iter != m_cacheSet.end())
	{
		CFileCache *fileCache = (*iter).second;
		CCacheParamValues params;
		sumOfSizes += fileCache->Describe(params);
		++iter;
	}

	return sumOfSizes;
}


// Get the type that corresponds to an objectId
const std::string
CFileCacheSet::GetTypeForObjectId(const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	std::string retVal("");
	std::map<const cachedObjectId_t, const std::string>::iterator iter;
	iter = m_idMap.find(objId);
	if (iter != m_idMap.end())
	{
		retVal = (*iter).second;
	}

	return retVal;
}

// Check if a type exists
bool
CFileCacheSet::TypeExists(const std::string &typeName)
{

	MojLogTrace(s_log);

	return (m_cacheSet.find(typeName) != m_cacheSet.end());
}

// Check if a type is a directory type
bool
CFileCacheSet::isTypeDirType(const std::string &typeName)
{

	MojLogTrace(s_log);

	bool isDirType = false;
	CFileCache *fileCache = GetFileCacheForType(typeName);
	if (fileCache != NULL)
	{
		isDirType = fileCache->isDirType();
	}

	return isDirType;
}

// Get the FileCache for a specified type
CFileCache *
CFileCacheSet::GetFileCacheForType(const std::string &typeName)
{

	MojLogTrace(s_log);

	CFileCache *retVal = NULL;
	std::map<const std::string, CFileCache *>::iterator iter;
	iter = m_cacheSet.find(typeName);
	if (iter != m_cacheSet.end())
	{
		retVal = (*iter).second;
	}

	return retVal;
}

int CFileCacheSet::GetCacheSize()
{
	return m_cacheSet.size();
}

// Locate and try to cleanup any orphans
void
CFileCacheSet::CleanupOrphans()
{

	MojLogTrace(s_log);

	std::map<const std::string, CFileCache *>::const_iterator iter;
	iter = m_cacheSet.begin();
	while (iter != m_cacheSet.end())
	{
		(*iter).second->CleanupOrphanedObjects();
		++iter;
	}
}

// Generate a unique object id that isn't duplicated in the set of
// currently existing object ids.
cachedObjectId_t
CFileCacheSet::GetNextCachedObjectId()
{

	MojLogTrace(s_log);

	cachedObjectId_t objId;
	bool validId = false;

	while (!validId)
	{
		// The lrand48 function returns a non-negative long interger
		// uniformly distributed between 0 and 2^31.  By shifting it left
		// by s_maxSeqBits bits and then adding the counter we get the
		// s_objIdBits bit objId.  WriteSequenceNumber will wrap the
		// sequenceNumber when it reaches s_maxAllowSeqNum (the highest
		// s_maxSeqBits bit number that will trigger a WriteSequenceNumber
		// operation).
		uint32_t randVal = (uint32_t) lrand48();
		objId = ((cachedObjectId_t) randVal << s_maxSeqBits) + m_sequenceNumber;
		MojLogDebug(s_log,
		            _T("GetNextCachedObjectId: Random value = %ud, seq num = %ud."),
		            randVal, m_sequenceNumber);
		MojLogDebug(s_log, _T("GetNextCachedObjectId: Generated objId = %llu."),
		            objId);
		m_sequenceNumber++;
		if ((m_sequenceNumber % s_sequenceBumpCnt) == 0)
		{
			WriteSequenceNumber();
		}
		if (objId < 1 || objId > s_maxId)
		{
			MojLogError(s_log, _T("GetNextCachedObjectId: Invalid objectId %llu"),
			            objId);
		}
		else
		{
			validId = true;
		}
	}

	return objId;
}

// Validate a subscribed object.
void
CFileCacheSet::CheckSubscribedObject(const std::string &typeName,
                                     const cachedObjectId_t objId)
{

	MojLogTrace(s_log);

	if (!typeName.empty())
	{
		CFileCache *fileCache = GetFileCacheForType(typeName);
		if (fileCache != NULL)
		{
			fileCache->CheckSubscribedObject(objId);
		}
		else
		{
			MojLogWarning(s_log,
			              _T("CheckSubscribedObject: No cache of type '%s' found for id '%llu'."),
			              typeName.c_str(), objId);
		}
	}
	else
	{
		MojLogWarning(s_log,
		              _T("CheckSubscribedObject: Cache type not found for id '%llu'."),
		              objId);
	}

}

// Cleanup any unsubscribed directory types
void
CFileCacheSet::CleanupDirTypes()
{

	MojLogTrace(s_log);

	std::map<const std::string, CFileCache *>::const_iterator iter;
	iter = m_cacheSet.begin();
	while (iter != m_cacheSet.end())
	{
		if ((*iter).second->isDirType())
		{
			(*iter).second->CleanupDirType();
		}
		++iter;
	}
}

// Below is the handling for the file tree walker function to process
// each entry found.

enum ProcessStatus
{
    ERROR = 0,
    COMPLETE,
    CONTINUE
};

bool
CFileCacheSet::isTopLevelDirectory(const std::string &pathname)
{

	MojLogTrace(s_log);

	bool retVal = false;

	std::string::size_type endPos = pathname.rfind('/');
	if ((endPos != std::string::npos) && (endPos > 0))
	{
		if ((GetBaseDirName() == pathname) ||
		        (GetBaseDirName() == pathname.substr(0, endPos)))
		{
			retVal = true;
		}
	}

	return retVal;
}

CFileCacheSet::ProcessStatus
CFileCacheSet::CreateTypeIfNeeded(const std::string &pathname,
                                  const std::string &typeName,
                                  std::set<std::string> &types)
{

	MojLogTrace(s_log);

	ProcessStatus stat = CONTINUE;
	MojLogDebug(s_log,
	            _T("CreateTypeIfNeeded: Checking for type '%s' for path '%s'."),
	            typeName.c_str(), pathname.c_str());
	// Check if this type has been instantiated by checking in the type set.
	if (types.find(typeName) == types.end())
	{
		std::string msgText;
		if (DefineType(msgText, typeName))
		{
			types.insert(typeName);
		}
		else
		{
			MojLogError(s_log,
			            _T("ProcessFiles: DefineType failed to create type '%s' (%s)"),
			            typeName.c_str(), msgText.c_str());
			// Since we failed to create a type for this file, we can't
			// really do anything but delete the file.
			int retVal = ::unlink(pathname.c_str());
			if (retVal != 0)
			{
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("ProcessFiles: Failed to unlink file '%s' (%s)."),
				            pathname.c_str(), ::strerror(savedErrno));
			}
			stat = ERROR;
		}
	}

	return stat;
}

CFileCacheSet::ProcessStatus
CFileCacheSet::CheckForSpecialFile(const std::string &pathname,
                                   std::set<std::string> &types)
{

	MojLogTrace(s_log);

	MojLogDebug(s_log,
	            _T("CheckForSpecialFile: Checking if '%s' is a special file."),
	            pathname.c_str());
	// First check if this is a Type.defaults file. If this is a
	// Type.defaults file, check if the type has already been
	// instantiated and do so if not to handle the case where a type
	// is defined with no cached objects.
	ProcessStatus stat = CONTINUE;
	std::string::size_type pos =
	    pathname.rfind(s_typeConfigFilename,
	                   pathname.length() - s_typeConfigFilename.length());
	if (pos != std::string::npos)
	{
		// Get the type name from the path
		std::string typeName(GetTypeNameFromPath(GetBaseDirName(), pathname));
		stat = CreateTypeIfNeeded(pathname, typeName, types);
		if (stat != ERROR)
		{
			stat = COMPLETE;
		}
	}
	if (stat == CONTINUE)
	{
		// Now check if this file is the sequence number file and skip it
		// if it is
		std::string::size_type pos =
		    pathname.rfind(s_seqNumFilename, pathname.length() -
		                   s_seqNumFilename.length());
		if (pos != std::string::npos)
		{
			stat = COMPLETE;
		}
	}

	return stat;
}

CFileCacheSet::ProcessStatus
CFileCacheSet::GetWritten(const std::string &pathname, int *written,
                          bool dirType)
{

	MojLogTrace(s_log);

	ProcessStatus stat = CONTINUE;
	// Let's start by checking if this file was completely written as
	// we will remove it if not.
	ssize_t attrSize = FC_getxattr(pathname.c_str(), "user.w", written,
	                               sizeof(*written));
	if ((attrSize == -1) || (!(*written)))
	{
		if (attrSize == -1)
		{
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("ProcessFiles: Failed to read attribute written on '%s' (%s)."),
			            pathname.c_str(), ::strerror(savedErrno));
		}
		else if (!dirType)
		{
			MojLogError(s_log,
			            _T("ProcessFiles: Cleaning up un-written cache object on '%s'."),
			            pathname.c_str());
		}

		// Since dir type entries will never be written, we will only
		// remove them if we failed to read the attribute
		if (!dirType)
		{
			// returning COMPLETE here will cause this file not to be added
			// to the cache but let the file tree walk continue
			stat = COMPLETE;
			int retVal = ::unlink(pathname.c_str());
			if (retVal != 0)
			{
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("ProcessFiles: Failed to unlink file '%s' (%s)."),
				            pathname.c_str(), ::strerror(savedErrno));
				stat = ERROR;
			}
			const std::string dirpath(GetDirectoryFromPath(pathname.c_str()));
			retVal = ::rmdir(dirpath.c_str());
			if ((retVal != 0) && (errno != ENOTEMPTY) && (errno != ENOENT))
			{
				// This should also never happen.  If it does we will just print
				// out the error as there isn't anything we can do about it.
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("ProcessFiles: Failed to rmdir directory '%s' (%s)."),
				            dirpath.c_str(), ::strerror(savedErrno));
			}
		}
		else if (attrSize == -1)
		{
			std::string msgText;
			CleanupDir(pathname, msgText);
			if (!msgText.empty())
			{
				MojLogDebug(s_log, _T("ProcessFiles: %s."), msgText.c_str());
			}
		}
	}
	else
	{
		// If the file was written, make sure the perms are correct as
		// we could have crashed after writing the attribute but before
		// we reset the permissions.
		int retVal = ::chmod(pathname.c_str(), s_fileROPerms);
		if (retVal != 0)
		{
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("ProcessFiles: Failed to set permissions file '%s' (%s)."),
			            pathname.c_str(), ::strerror(savedErrno));
			stat = ERROR;
		}
	}

	return stat;
}

CFileCacheSet::ProcessStatus
CFileCacheSet::GetSize(const std::string &pathname, const struct stat *sb,
                       cacheSize_t *size, bool dirType)
{

	MojLogTrace(s_log);

	ProcessStatus stat = CONTINUE;

	// Now get the size and validate it is correct or else remove the
	// file as it was tampered with after the attributes were written
	// and the cache statistics won't add up.
	ssize_t attrSize = FC_getxattr(pathname.c_str(), "user.s", size, sizeof(*size));
	if (attrSize == -1)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("ProcessFiles: Failed to read attribute size on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		stat = ERROR;
	}
	// Now check that the size on disk is equal to the specified size
	if (!dirType && ((cacheSize_t) sb->st_size != *size))
	{
		int retVal = ::unlink(pathname.c_str());
		if (retVal != 0)
		{
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("ProcessFiles: Failed to unlink file '%s' (%s)."),
			            pathname.c_str(), ::strerror(savedErrno));
		}
		else
		{
			const std::string dirpath(GetDirectoryFromPath(pathname.c_str()));
			retVal = ::rmdir(dirpath.c_str());
			if ((retVal != 0) && (errno != ENOTEMPTY) && (errno != ENOENT))
			{
				// This should also never happen.  If it does we will just print
				// out the error as there isn't anything we can do about it.
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("ProcessFiles: Failed to rmdir directory '%s' (%s)."),
				            dirpath.c_str(), ::strerror(savedErrno));
			}
			stat = ERROR;
		}
		stat = COMPLETE;
	}

	return stat;
}

CFileCacheSet::ProcessStatus
CFileCacheSet::GetFilename(const std::string &pathname, char *fileName)
{

	MojLogTrace(s_log);

	ProcessStatus stat = CONTINUE;

	// Get the real filename from the extended attribute
	ssize_t attrSize = FC_getxattr(pathname.c_str(), "user.f", fileName,
	                               s_maxFilenameLength);
	if (attrSize == -1)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("ProcessFiles: Failed to read attribute filename on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		stat = ERROR;
	}

	return stat;
}

CFileCacheSet::ProcessStatus
CFileCacheSet::GetCost(const std::string &pathname, paramValue_t *cost)
{

	MojLogTrace(s_log);

	ProcessStatus stat = CONTINUE;

	// Get the code from the extended attribute
	ssize_t attrSize = FC_getxattr(pathname.c_str(), "user.c", cost, sizeof(*cost));
	if (attrSize == -1)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("ProcessFiles: Failed to read attribute cost on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		stat = ERROR;
	}

	return stat;
}

CFileCacheSet::ProcessStatus
CFileCacheSet::GetLifetime(const std::string &pathname, paramValue_t *lifetime)
{

	MojLogTrace(s_log);

	ProcessStatus stat = CONTINUE;

	// Get the lifetime from the extended attribute
	ssize_t attrSize = FC_getxattr(pathname.c_str(), "user.l", lifetime,
	                               sizeof(*lifetime));
	if (attrSize == -1)
	{
		int savedErrno = errno;
		MojLogError(s_log,
		            _T("ProcessFiles: Failed to read attribute lifetime on '%s' (%s)."),
		            pathname.c_str(), ::strerror(savedErrno));
		stat = ERROR;
	}

	return stat;
}

int
CFileCacheSet::ProcessFiles(const std::string &filepath)
{

	MojLogTrace(s_log);

	static std::set<std::string> types;
	static std::string s_dirTypeDir;
	ProcessStatus flowStat = CONTINUE;

	char fileName[s_maxFilenameLength];
	std::string msgText;

	std::string typeName(GetTypeNameFromPath(GetBaseDirName(), filepath));
	cachedObjectId_t objectId = GetObjectIdFromPath(filepath.c_str());
	bool dirType = false;

	struct stat buf;

	int fd;
	if((fd = open(filepath.c_str(), O_RDONLY)) == -1)
	{
		int savedErrno = errno;
		MojLogError(s_log, _T("ProcessFiles: Failed to open file '%s' (%s)."),
		            filepath.c_str(), ::strerror(savedErrno));
		flowStat = ERROR;
	}
	else if (::fstat(fd, &buf) == -1)
	{
		int savedErrno = errno;
		MojLogError(s_log, _T("ProcessFiles: Failed to stat file '%s' (%s)."),
		            filepath.c_str(), ::strerror(savedErrno));
		flowStat = ERROR;
	}

	close(fd);

	if ((flowStat == CONTINUE) && S_ISDIR(buf.st_mode) &&
	        isTopLevelDirectory(filepath))
	{
		flowStat = COMPLETE;
	}
	else if (!s_dirTypeDir.empty())
	{
		if (s_dirTypeDir == filepath.substr(0, s_dirTypeDir.length()))
		{
			flowStat = COMPLETE;
		}
		else
		{
			s_dirTypeDir.clear();
		}
	}

	if (flowStat == CONTINUE)
	{
		if (S_ISREG(buf.st_mode))
		{
			MojLogDebug(s_log, _T("ProcessFiles: processing file '%s'."),
			            filepath.c_str());
			flowStat = CheckForSpecialFile(filepath, types);

			//  Make sure the type has already been defined and define it if
			//  not.  This should never happen since the pre-order traversal
			//  should have processed the type defaults file first but is done
			//  just for safety.
			if (flowStat == CONTINUE)
			{
				flowStat = CreateTypeIfNeeded(filepath, typeName, types);
			}
		}
		else if (S_ISDIR(buf.st_mode))
		{
			if ((types.find(typeName) != types.end()) && isTypeDirType(typeName) &&
			        (objectId != 0))
			{
				dirType = true;
				s_dirTypeDir = filepath;
			}
			else
			{
				int retVal = ::rmdir(filepath.c_str());
				if ((retVal != 0) && (errno != ENOTEMPTY) && (errno != ENOENT))
				{
					// This should also never happen.  If it does we will just print
					// out the error as there isn't anything we can do about it.
					int savedErrno = errno;
					MojLogError(s_log,
					            _T("ProcessFiles: Failed to rmdir directory '%s' (%s)."),
					            filepath.c_str(), ::strerror(savedErrno));
				}
				else if (retVal == 0)
				{
					MojLogError(s_log,
					            _T("ProcessFiles: Removing empty directory '%s'."),
					            filepath.c_str());
				}
				flowStat = COMPLETE;
			}
		}
	}

	if ((flowStat == CONTINUE) && (objectId <= 0))
	{
		flowStat = COMPLETE;
		int retVal = ::unlink(filepath.c_str());
		if (retVal != 0)
		{
			int savedErrno = errno;
			MojLogError(s_log,
			            _T("ProcessFiles: Failed to unlink file '%s' (%s)."),
			            filepath.c_str(), ::strerror(savedErrno));
			flowStat = ERROR;
		}
		else
		{
			MojLogError(s_log,
			            _T("ProcessFiles: Unlinked non-cache file '%s'."),
			            filepath.c_str());
			retVal = ::rmdir(GetDirectoryFromPath(filepath).c_str());
			if ((retVal != 0) && (errno != ENOTEMPTY) && (errno != ENOENT))
			{
				int savedErrno = errno;
				MojLogError(s_log,
				            _T("ProcessFiles: Failed to rmdir directory '%s' (%s)."),
				            filepath.c_str(), ::strerror(savedErrno));
			}
			else
			{
				if (retVal == 0)
				{
					MojLogError(s_log,
					            _T("ProcessFiles: Removing empty directory '%s'."),
					            GetDirectoryFromPath(filepath).c_str());
				}
			}
		}
	}

	int written = 0;
	if (flowStat == CONTINUE)
	{
		flowStat = GetWritten(filepath, &written, dirType);
	}

	cacheSize_t size = 0;
	if (flowStat == CONTINUE)
	{
		flowStat = GetSize(filepath, &buf, &size, dirType);
	}

	if (flowStat == CONTINUE)
	{
		flowStat = GetFilename(filepath, fileName);
	}

	paramValue_t cost = 1;
	if (flowStat == CONTINUE)
	{
		flowStat = GetCost(filepath, &cost);
	}

	paramValue_t lifetime = 1;
	if (flowStat == CONTINUE)
	{
		flowStat = GetLifetime(filepath, &lifetime);
	}

	if (flowStat == CONTINUE)
	{
		MojLogDebug(s_log,
		            _T("ProcessFiles: Path %s yielded objectId %llu and filename %s."),
		            filepath.c_str(), objectId, fileName);
		InsertCacheObject(msgText, typeName, std::string(fileName),
		                  objectId, size, cost, lifetime,
		                  written ? true : false, false);
	}

	int retVal = 0;
	if (flowStat == ERROR)
	{
		// returning 1 causes ftw to return an error
		retVal = 1;
	}

	// Complete so move on and try the next file
	return retVal;
}

bool
CFileCacheSet::FileTreeWalk(const std::string &dirName)
{

	MojLogTrace(s_log);

	bool retVal = true;
	fs::path pathname(dirName);
	fs::directory_iterator endIter;
	fs::directory_iterator dirIter1(pathname);
	while ((dirIter1 != endIter) && (retVal == true))
	{
		try
		{
			if (ProcessFiles(dirIter1->path().string()) != 0)
			{
				retVal = false;
			}
		}
		catch (const fs::filesystem_error &ex)
		{
			if (ex.code().value() != 0)
			{
				MojLogDebug(s_log, _T("FileTreeWalk: %s (%s)"),
				            ex.what(), ex.code().message().c_str());
			}
			retVal = false;
		}
		++dirIter1;
	}
	if (fs::exists(pathname))
	{
		fs::directory_iterator dirIter2(pathname);
		while ((dirIter2 != endIter) && (retVal == true))
		{
			try
			{
				fs::file_status iterStatus = fs::status(dirIter2->path());
				if (fs::status_known(iterStatus) &&
				        fs::exists(iterStatus) &&
				        fs::is_directory(iterStatus))
				{
					retVal = FileTreeWalk(dirIter2->path().string());
				}
			}
			catch (const fs::filesystem_error &ex)
			{
				MojLogError(s_log, _T("FileTreeWalk: %s (%s)"),
				            ex.what(), ex.code().message().c_str());
			}
			++dirIter2;
		}
	}

	return retVal;
}

// Walk the file cache directory tree to build the cache data
// structures.
int
CFileCacheSet::WalkDirTree()
{

	MojLogTrace(s_log);

	int retVal = true;
#ifdef DEBUG
	long long startTime;
	long long stopTime;

#ifdef MOJ_MAC
	startTime = ::clock() * 1000 / CLOCKS_PER_SEC;
#else
	struct timespec tm;
	::clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tm);
	startTime = tm.tv_sec * 1000LL + tm.tv_nsec / 1000000;
#endif // #ifdef MOJ_MAC
#endif // #ifdef DEBUG

	std::string dirName(GetCacheDirectory());
	// walk the directory dirName and call ProcessFiles on each
	// entry.
	try
	{
		if (!FileTreeWalk(dirName))
		{
			MojLogError(s_log, _T("WalkDirTree: Failed to complete file tree walk."));
			retVal = false;
		}
	}
	catch (const fs::filesystem_error &ex)
	{
		MojLogError(s_log, _T("FileTreeWalk: %s (%s)"),
		            ex.what(), ex.code().message().c_str());
		MojLogError(s_log, _T("WalkDirTree: Failed to complete file tree walk."));
		retVal = false;
	}

#ifdef DEBUG
#ifdef MOJ_MAC
	stopTime = ::clock() * 1000 / CLOCKS_PER_SEC;
#else
	::clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tm);
	stopTime = tm.tv_sec * 1000LL + tm.tv_nsec / 1000000;
#endif // #ifdef MOJ_MAC

	MojLogDebug(s_log, _T("Walking object directory/files took %lld ms."),
	            stopTime - startTime);
#endif // #ifdef DEBUG

	return retVal;
}

// Go through the different CFileCache objects and clean up each one.
// This is meant to be called at service startup time, and it's part of
// the fix for NOV-128944.
void
CFileCacheSet::CleanupAtStartup()
{
	if (SumOfCacheSizes() > TotalCacheSpace())
	{
		cacheSize_t overRun = SumOfCacheSizes() - TotalCacheSpace();
		MojLogWarning(s_log, _T("CleanupAtStartup: overRun = %d bytes"),
		              overRun);
		CleanupAllTypes(overRun);
	}
}

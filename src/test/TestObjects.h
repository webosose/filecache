// Copyright (c) 2009-2018 LG Electronics, Inc.
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

#ifndef __TESTOBJECTS_H__
#define __TESTOBJECTS_H__

#include "FileCacheSet.h"
#include "core/MojLogEngine.h"

static const std::string s_baseTestDirName("/tmp/test");

class CTestFileCacheSet : public CFileCacheSet
{
public:

	CTestFileCacheSet() : CFileCacheSet(false)
		, m_dirName(s_baseTestDirName)
		, m_cacheSpace(8 * 1024 * 1024)
		, m_loWatermarks(4 * 1024 * 1024)
		, m_cacheSizes(2 * 1024 * 1024)
	{

		//    MojLogEngine::instance()->reset(MojLogger::LevelDebug);
	}

	// Return the base directory name for the file cache directory tree
	std::string &GetBaseDirName()
	{
		return m_dirName;
	}

	// Return the total configured cache space
	cacheSize_t TotalCacheSpace()
	{
		return m_cacheSpace;
	}

	// Compute the sum of the loWatermarks for each of the configured
	// caches
	cacheSize_t SumOfLoWatermarks()
	{
		return m_loWatermarks;
	}

	// Compute the sum of current sizes for each of the configured
	// caches
	cacheSize_t SumOfCacheSizes()
	{
		return m_cacheSizes;
	}

	// Cleanup all registered types
	cacheSize_t CleanupAllTypes(cacheSize_t neededSpace)
	{
		return neededSpace;
	}

	// setters used for testing
	cacheSize_t SetCacheSpace(cacheSize_t space)
	{
		m_cacheSpace = space;
		return m_cacheSpace;
	}
	cacheSize_t SetLoWatermarks(cacheSize_t space)
	{
		m_loWatermarks = space;
		return m_loWatermarks;
	}
	cacheSize_t SetCacheSizes(cacheSize_t space)
	{
		m_cacheSizes = space;
		return m_cacheSizes;
	}

	// By masking the sequence number we get a sequential number for testing
	cachedObjectId_t GetNextCachedObjectId()
	{
		cachedObjectId_t objId = CFileCacheSet::GetNextCachedObjectId();
		// mask off just the sequence number (lower 22 bits)
		return (objId & 0x3FFFFF);
	}

private:
	std::string m_dirName;
	cacheSize_t m_cacheSpace;
	cacheSize_t m_loWatermarks;
	cacheSize_t m_cacheSizes;
};

#endif

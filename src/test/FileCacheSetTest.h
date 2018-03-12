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

#ifndef __FILECACHESETTEST_H__
#define __FILECACHESETTEST_H__

#include <cxxtest/TestSuite.h>
#include "FileCache.h"
#include "FileCacheSet.h"
#include "TestObjects.h"

class FileCacheSetTest : public CxxTest::TestSuite
{

	std::string typeName;
	std::string fileName;
	std::string pathName;
	std::string msgText;
	CFileCacheSet *fileCacheSet;
	cachedObjectId_t curObjId;

public:

	FileCacheSetTest()
	{
		fileCacheSet = NULL;
		curObjId = 1;
	}

	void setUp()
	{
		typeName = "type";
		fileName = "testfile.ext";
		if (!fileCacheSet)
		{
			fileCacheSet = new CTestFileCacheSet();
		}
	}

	void tearDown()
	{
	}

	void testDefineType()
	{
		CCacheParamValues params(10 * s_blockSize, 20 * s_blockSize, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		std::vector<std::string> types = fileCacheSet->GetTypes();
		TS_ASSERT_EQUALS(types.size(), (size_t) 1);
		TS_ASSERT_EQUALS(types[0].length(), typeName.length());
		TS_ASSERT_SAME_DATA(types[0].c_str(), typeName.c_str(),
		                    (unsigned int) typeName.length());
		CCacheParamValues config = fileCacheSet->DescribeType(typeName);
		TS_ASSERT_EQUALS(config.GetLoWatermark(), params.GetLoWatermark());
		TS_ASSERT_EQUALS(config.GetHiWatermark(), params.GetHiWatermark());
		TS_ASSERT_EQUALS(config.GetSize(), params.GetSize());
		TS_ASSERT_EQUALS(config.GetCost(), params.GetCost());
		TS_ASSERT_EQUALS(config.GetLifetime(), params.GetLifetime());
	}

	void testChangeType()
	{
		CCacheParamValues params(20 * s_blockSize, 40 * s_blockSize, 200, 2, 2);
		TS_ASSERT(fileCacheSet->ChangeType(msgText, typeName, &params));
		CCacheParamValues config = fileCacheSet->DescribeType(typeName);
		TS_ASSERT_EQUALS(config.GetLoWatermark(), params.GetLoWatermark());
		TS_ASSERT_EQUALS(config.GetHiWatermark(), params.GetHiWatermark());
		TS_ASSERT_EQUALS(config.GetSize(), params.GetSize());
		TS_ASSERT_EQUALS(config.GetCost(), params.GetCost());
		TS_ASSERT_EQUALS(config.GetLifetime(), params.GetLifetime());
	}

	void testDeleteType()
	{
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName), 0);
		std::vector<std::string> types = fileCacheSet->GetTypes();
		TS_ASSERT_EQUALS(types.size(), (size_t) 0);
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName + "123"),
		                 -1);
	}

	void testInsertCacheObject()
	{
		CCacheParamValues params(10000, 20000, 4097, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		// The test object should return object ids incrementing starting
		// from 1
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 0),
		                 curObjId++);
		// 4097 is the default size from above so it is expected to
		// provide a size that's really 8192 (2 4096 blocks)
		int size = GetFilesystemFileSize(4097);
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 123),
		                 curObjId++);
		size += GetFilesystemFileSize(123);
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName), size);
	}

	void testResizeFailureCase()
	{
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 123),
		                 curObjId);
		// This should return the original size since we are not subscribed
		TS_ASSERT_EQUALS(fileCacheSet->Resize(curObjId++, 234), 123);
		// This one should fail the object id lookup
		TS_ASSERT_EQUALS(fileCacheSet->Resize((cachedObjectId_t) 0, 234), -1);
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName),
		                 GetFilesystemFileSize(123));
	}

	void testSubscribe()
	{
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 123),
		                 curObjId);
		// Subscribe should return a pathname that is not empty and the
		// file the pathname points to should exist and have write
		// permissions since this is the first subscription.
		const std::string pathname(fileCacheSet->SubscribeCacheObject(msgText,
		                           curObjId));
		TS_ASSERT_LESS_THAN((size_t) 7, pathname.length());
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), 0);

		// Write to the file so it stays around
		FILE *fp = ::fopen(pathname.c_str(), "w");
		TS_ASSERT(fp != NULL);
		::fwrite(&curObjId, sizeof(curObjId), 1, fp);
		::fclose(fp);

		// After unsubscribe, the file should not be readable but should
		// still exist
		fileCacheSet->UnSubscribeCacheObject(typeName, curObjId++);
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), -1);
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK), 0);
		// Try subscribing to a bad object id
		TS_ASSERT_EQUALS(fileCacheSet->SubscribeCacheObject(msgText, 0).length(),
		                 (size_t) 0);
		// The delete will return the space freed and since we always
		// allocate at least 1 block, we will get 4096 even though the
		// file size is zero
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName),
		                 GetFilesystemFileSize(4096));
	}

	void testResize()
	{
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 123),
		                 curObjId);
		// Subscribe sp the resize should work
		const std::string pathname(fileCacheSet->SubscribeCacheObject(msgText,
		                           curObjId));
		TS_ASSERT_LESS_THAN((size_t) 7, pathname.length());
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), 0);

		// Write to the file so it stays around
		FILE *fp = ::fopen(pathname.c_str(), "w");
		TS_ASSERT(fp != NULL);
		::fwrite(&curObjId, sizeof(curObjId), 1, fp);
		::fclose(fp);

		// This should return the original size since we are not subscribed
		TS_ASSERT_EQUALS(fileCacheSet->Resize((cachedObjectId_t) curObjId, 234),
		                 234);
		TS_ASSERT_EQUALS(fileCacheSet->Resize((cachedObjectId_t) curObjId, 345),
		                 345);
		fileCacheSet->UnSubscribeCacheObject(typeName, curObjId++);
		// The returned size should be 4096 since the unsubscribe will
		// reset the file size to the actual disk size (which since we
		// didn't write is still 1 block)
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName),
		                 GetFilesystemFileSize(4096));
	}

	void testExpireCacheObject()
	{
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 123),
		                 curObjId);
		TS_ASSERT(fileCacheSet->ExpireCacheObject(curObjId++));
		// Expiring an object that doesn't exist should still return true
		// as it defines the state of the cache (meaning you requested the
		// object not be there and it isn't)
		TS_ASSERT(fileCacheSet->ExpireCacheObject(0));
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName), 0);
	}

	void testGetCacheStatus()
	{
		cacheSize_t size;
		paramValue_t numCacheObjects;
		cacheSize_t availSpace;

		std::string type = typeName + "1";
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, type, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, type,
		                 fileName, 5000),
		                 curObjId++);

		type = typeName + "2";
		TS_ASSERT(fileCacheSet->DefineType(msgText, type, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, type,
		                 fileName, 5000),
		                 curObjId++);

		type = typeName + "3";
		TS_ASSERT(fileCacheSet->DefineType(msgText, type, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, type,
		                 fileName, 5000),
		                 curObjId++);
		TS_ASSERT_EQUALS(fileCacheSet->GetCacheStatus(&size, &numCacheObjects,
		                 &availSpace), 3);
		TS_ASSERT_EQUALS(size, 3 * GetFilesystemFileSize(5000));
		TS_ASSERT_EQUALS(numCacheObjects, 3);
		TS_ASSERT_EQUALS(availSpace, fileCacheSet->SumOfLoWatermarks() - size);

		numCacheObjects = 0;
		availSpace = 0;
		TS_ASSERT_EQUALS(fileCacheSet->GetCacheStatus(NULL, &numCacheObjects,
		                 &availSpace), 3);
		TS_ASSERT_EQUALS(numCacheObjects, 3);
		TS_ASSERT_EQUALS(availSpace, fileCacheSet->SumOfLoWatermarks() - size);

		size = 0;
		availSpace = 0;
		TS_ASSERT_EQUALS(fileCacheSet->GetCacheStatus(&size, NULL,
		                 &availSpace), 3);
		TS_ASSERT_EQUALS(size, 3 * GetFilesystemFileSize(5000));
		TS_ASSERT_EQUALS(availSpace, fileCacheSet->SumOfLoWatermarks() - size);

		size = 0;
		numCacheObjects = 0;
		TS_ASSERT_EQUALS(fileCacheSet->GetCacheStatus(&size, &numCacheObjects,
		                 NULL), 3);
		TS_ASSERT_EQUALS(size, 3 * GetFilesystemFileSize(5000));
		TS_ASSERT_EQUALS(numCacheObjects, 3);

		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, type),
		                 GetFilesystemFileSize(5000));
		type = typeName + "2";
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, type),
		                 GetFilesystemFileSize(5000));
		type = typeName + "1";
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, type),
		                 GetFilesystemFileSize(5000));
		TS_ASSERT_EQUALS(fileCacheSet->GetCacheStatus(&size, &numCacheObjects,
		                 NULL), 0);
	}

	void testGetCacheTypeStatus()
	{
		cacheSize_t size;
		paramValue_t numCacheObjects;

		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 5000),
		                 curObjId++);
		TS_ASSERT(fileCacheSet->GetCacheTypeStatus(typeName, &size,
		          &numCacheObjects));
		TS_ASSERT_EQUALS(size, GetFilesystemFileSize(5000));
		TS_ASSERT_EQUALS(numCacheObjects, 1);
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 5000),
		                 curObjId);
		TS_ASSERT(fileCacheSet->GetCacheTypeStatus(typeName, &size,
		          &numCacheObjects));
		TS_ASSERT_EQUALS(size, 2 * GetFilesystemFileSize(5000));
		TS_ASSERT_EQUALS(numCacheObjects, 2);
		TS_ASSERT(fileCacheSet->ExpireCacheObject(curObjId));
		curObjId++;
		TS_ASSERT(fileCacheSet->GetCacheTypeStatus(typeName, &size,
		          &numCacheObjects));
		TS_ASSERT_EQUALS(size, GetFilesystemFileSize(5000));
		TS_ASSERT_EQUALS(numCacheObjects, 1);

		size = 0;
		numCacheObjects = 0;
		TS_ASSERT(fileCacheSet->GetCacheTypeStatus(typeName, NULL,
		          &numCacheObjects));
		TS_ASSERT_EQUALS(numCacheObjects, 1);
		TS_ASSERT(fileCacheSet->GetCacheTypeStatus(typeName, &size, NULL));
		TS_ASSERT_EQUALS(size, GetFilesystemFileSize(5000));

		std::string type("foo");
		TS_ASSERT(!fileCacheSet->GetCacheTypeStatus(type, &size,
		          &numCacheObjects));
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName),
		                 GetFilesystemFileSize(5000));
	}

	void testCachedObjectSize()
	{
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		TS_ASSERT_EQUALS(fileCacheSet->InsertCacheObject(msgText, typeName,
		                 fileName, 1234),
		                 curObjId);
		TS_ASSERT_EQUALS(fileCacheSet->CachedObjectSize(curObjId++), 1234);
		TS_ASSERT_EQUALS(fileCacheSet->CachedObjectSize(0), -1);
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName),
		                 GetFilesystemFileSize(1234));
	}

	void testIsTypeDirType()
	{
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params));
		TS_ASSERT(!fileCacheSet->isTypeDirType(typeName));
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName), 0);

		TS_ASSERT(fileCacheSet->DefineType(msgText, typeName, &params, true));
		TS_ASSERT(fileCacheSet->isTypeDirType(typeName));
		TS_ASSERT_EQUALS(fileCacheSet->DeleteType(msgText, typeName), 0);
	}
};

#endif

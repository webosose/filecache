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

#ifndef __FILECACHETEST_H__
#define __FILECACHETEST_H__

#include <cxxtest/TestSuite.h>
#include "FileCache.h"
#include "FileCacheSet.h"
#include "TestObjects.h"

class FileCacheTest : public CxxTest::TestSuite
{

	char filename[32];
	cachedObjectId_t objId;
	std::string pathname;
	CFileCache *fileCache;
	CFileCacheSet *fileCacheSet;
	std::string msgText;


public:

	FileCacheTest()
	{
		fileCacheSet = NULL;
		fileCache = NULL;
	}

	void setUp()
	{
		strncpy(filename, "testfile.ext", 12);
		objId = 1000ULL;
		if (!fileCache)
		{
			fileCacheSet = new CTestFileCacheSet();
			fileCache = new CFileCache(fileCacheSet, typeName);
		}
	}

	void tearDown()
	{
	}

	void testConfigure()
	{
		CCacheParamValues params(10000, 20000, 10, 0, 0);
		TS_ASSERT(fileCache->Configure(&params));
		TS_ASSERT_EQUALS(params.GetLoWatermark(), 10000);
		TS_ASSERT_EQUALS(params.GetHiWatermark(), 20000);
		TS_ASSERT_EQUALS(params.GetSize(), 10);
		TS_ASSERT_EQUALS(params.GetLifetime(), 1);
		TS_ASSERT_EQUALS(params.GetCost(), 0);
		TS_ASSERT(!fileCache->isDirType());

		// Now test the dirType configure.
		std::string type1(typeName + "1");
		CFileCache *fc1 = new CFileCache(fileCacheSet, type1);
		TS_ASSERT_EQUALS(fc1->Configure(&params, true), true);
		TS_ASSERT(fc1->isDirType());
		delete fc1;
	}

	void testFileCacheConstructorAndGetters()
	{
		TS_ASSERT_EQUALS(fileCache->GetType(), typeName);
	}

	void testInsert()
	{
		CCacheObject *co1 = new CCacheObject(fileCache, objId, filename, 1000);
		TS_ASSERT(co1->Initialize(true));
		TS_ASSERT_EQUALS(fileCache->Insert(co1), 1);
		TS_ASSERT_EQUALS(fileCache->GetObjectSize(objId), 1000);
		pathname = co1->GetPathname();
	}

	void testFileCacheDestructor()
	{
		std::string type1(typeName + "1");
		CFileCache *fc1 = new CFileCache(fileCacheSet, type1);
		TS_ASSERT_EQUALS(fc1->Configure(NULL), false);
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT_EQUALS(fc1->Configure(&params), true);
		// Now make sure the file cache directory exists
		std::string dirname(s_baseTestDirName + "/" + type1);
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		delete fc1;
		// And now it should be gone
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
	}

	void testDescribe()
	{
		CCacheParamValues params(-1, -1, -1, 99999, -1);
		TS_ASSERT_EQUALS(params.GetCost(), 255);
		TS_ASSERT_EQUALS(params.GetLifetime(), 1);
		fileCache->Describe(params);
		// These are the params the cache was configured with so check against them.
		// CCacheParamValues params(10000, 20000, 10, 0, 0);
		TS_ASSERT_EQUALS(params.GetLoWatermark(), GetFilesystemFileSize(10000));
		TS_ASSERT_EQUALS(params.GetHiWatermark(), GetFilesystemFileSize(20000));
		TS_ASSERT_EQUALS(params.GetSize(), 10);
		TS_ASSERT_EQUALS(params.GetLifetime(), 1);
		TS_ASSERT_EQUALS(params.GetCost(), 0);
	}

	void testResize()
	{
		// This resize should fail (and return the original size) since we
		// are not subscribed
		TS_ASSERT_EQUALS(fileCache->Resize(objId, 2000), 1000);
		TS_ASSERT_EQUALS(fileCache->Subscribe(msgText, objId), pathname);
		// This one should suceed
		TS_ASSERT_EQUALS(fileCache->Resize(objId, 2000), 2000);
		// UnSubscribe will set the file size to 0 because that is the
		// size of the backing file
		fileCache->UnSubscribe(objId);
		// This resize will fail because the file is already written.
		TS_ASSERT_EQUALS(fileCache->Resize(objId, 3000), 0);
		// This one should fail due to a bad objId
		TS_ASSERT_EQUALS(fileCache->Resize(12345, 12345), 0);
	}

	void testGetCachedObjects()
	{
		int i;

		std::string type2(typeName + "2");
		CFileCache *fc2 = new CFileCache(fileCacheSet, type2);
		TS_ASSERT_EQUALS(fc2->Configure(NULL), false);
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT_EQUALS(fc2->Configure(&params), true);

		for (i = 1; i <= 10; i++)
		{
			CCacheObject *co = new CCacheObject(fc2, (objId + i), filename,
			                                    (1000 + i));
			TS_ASSERT(co->Initialize(true));
			TS_ASSERT_EQUALS(fc2->Insert(co), i);
		}
		std::vector<std::pair<cachedObjectId_t, CCacheObject *>> objs;
		objs = fc2->GetCachedObjects();
		TS_ASSERT_EQUALS(objs.size(), (size_t) 10);

		for (i = 0; i < 10; i++)
		{
			// Check that the returned obj ids are correct and in order
			TS_ASSERT_EQUALS(objs[i].first, (objId + i + 1));
			// Also make sure the object pointers point to the correct
			// object by verifying the size
			TS_ASSERT_EQUALS((objs[i].second)->GetSize(), (1000 + i + 1));
		}
		std::string dirname(s_baseTestDirName + "/" + type2);
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		for (i = 1; i <= 10; i++)
		{
			TS_ASSERT(fc2->Expire(objId + i));
		}
		delete fc2;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
	}

	void testCheckForSize()
	{

		// Set the high watermark to 5 blocks
		cacheSize_t hwm1 = s_blockSize * 5;
		// Make sure we have a known configuration
		CCacheParamValues params(10000, hwm1, 100, 1, 1);
		TS_ASSERT(fileCache->Configure(&params));
		cacheSize_t curSize = fileCache->GetCacheSize();
		// Make sure we can allocate a small file
		TS_ASSERT(fileCache->CheckForSize(10));
		// This is the largest file size we should be able to allocate
		TS_ASSERT(fileCache->CheckForSize(hwm1 - curSize - 1));
		// This should fail
		TS_ASSERT(!fileCache->CheckForSize(hwm1));
		// Now set the high watermark to 10 blocks
		cacheSize_t hwm2 = s_blockSize * 10;
		// Increase the hi watermark
		params.SetHiWatermark(hwm2);
		TS_ASSERT(fileCache->Configure(&params));
		// The previous check should now pass
		TS_ASSERT(fileCache->CheckForSize(hwm1 - curSize));
		// The test fileCacheSet object will always return that 7.5MB are
		// available so the next check should always fail.
		TS_ASSERT(!fileCache->CheckForSize(8 * 1024 * 1024));
	}

	void testCleanupCache()
	{
		int i;

		std::string type3(typeName + "3");
		CFileCache *fc3 = new CFileCache(fileCacheSet, type3);
		TS_ASSERT_EQUALS(fc3->Configure(NULL), false);
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT_EQUALS(fc3->Configure(&params), true);

		for (i = 1; i <= 10; i++)
		{
			CCacheObject *co = new CCacheObject(fc3, (objId + i), filename,
			                                    (1000 + i));
			TS_ASSERT(co->Initialize(true));
			TS_ASSERT_EQUALS(fc3->Insert(co), i);
		}
		// Make sure we have all expected objects
		TS_ASSERT_EQUALS(fc3->GetNumObjects(), 10);
		// The first cleanup should free the oldest object and return it's
		// size and then there should be one fewer objects
		cachedObjectId_t cleanedId;
		TS_ASSERT_EQUALS(fc3->CleanupCache(&cleanedId), 1001);
		TS_ASSERT_EQUALS(cleanedId, objId + 1);
		// If an object is subscribed, it should be skipped by the cleanup
		// code.
		fc3->Subscribe(msgText, objId + 2);
		// Make sure this skipped objId + 2, the size of objId + 3 is 1003
		TS_ASSERT_EQUALS(fc3->CleanupCache(&cleanedId), 1003);
		TS_ASSERT_EQUALS(cleanedId, objId + 3);
		// The objId + 2 object should now be at the front of the list so
		// the next cleaned object should be objId + 4 whose size is 1004
		fc3->UnSubscribe(objId + 2);
		TS_ASSERT_EQUALS(fc3->CleanupCache(&cleanedId), 1004);
		TS_ASSERT_EQUALS(cleanedId, objId + 4);

		std::string dirname(s_baseTestDirName + "/" + type3);
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		for (i = 1; i <= 10; i++)
		{
			fc3->Expire(objId + i);
		}
		delete fc3;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
	}

	void testCleanup()
	{
	}

	void testGetCacheCost()
	{
		cachedObjectId_t oid = 564738;
		CCacheObject *co = new CCacheObject(fileCache, oid, filename, 1, 10, 5);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->Initialize(true), true);
		TS_ASSERT_EQUALS(fileCache->Insert(co), 2);

		// Make sure the bad ID check works.
		TS_ASSERT_EQUALS(fileCache->GetCacheCost(0), -1);
		// Since the lifetime is 5, make sure the initial check of cost is
		// set to the max.
		TS_ASSERT_EQUALS(fileCache->GetCacheCost(oid), s_maxCost);
		::sleep(5);
		TS_ASSERT_EQUALS(fileCache->GetCacheCost(oid), 2);
		fileCache->Expire(oid);
	}

	void testGetCleanupCandidate()
	{
		int i;

		std::string type4(typeName + "4");
		CFileCache *fc4 = new CFileCache(fileCacheSet, type4);
		CCacheParamValues params(100, 20000, 100, 1, 1);
		TS_ASSERT_EQUALS(fc4->Configure(&params), true);

		for (i = 1; i <= 5; i++)
		{
			CCacheObject *co = new CCacheObject(fc4, (objId + i), filename,
			                                    (s_blockSize + i));
			TS_ASSERT(co->Initialize(true));
			TS_ASSERT_EQUALS(fc4->Insert(co), i);
		}
		// Make sure we have all expected objects
		TS_ASSERT_EQUALS(fc4->GetNumObjects(), 5);
		// Now make sure the candidates are offered in the reverse order
		// of insertion
		TS_ASSERT_EQUALS(fc4->GetCleanupCandidate(), (objId + 1));
		TS_ASSERT(fc4->Expire(fc4->GetCleanupCandidate()));
		TS_ASSERT_EQUALS(fc4->GetCleanupCandidate(), (objId + 2));
		TS_ASSERT(fc4->Expire(fc4->GetCleanupCandidate()));
		// Touching the object should move it to the front of the list so
		// objId + 3 should be found last.
		fc4->Touch(objId + 3);
		TS_ASSERT_EQUALS(fc4->GetCleanupCandidate(), (objId + 4));
		TS_ASSERT(fc4->Expire(fc4->GetCleanupCandidate()));
		TS_ASSERT_EQUALS(fc4->GetCleanupCandidate(), (objId + 5));
		TS_ASSERT(fc4->Expire(fc4->GetCleanupCandidate()));
		TS_ASSERT_EQUALS(fc4->GetCleanupCandidate(), (objId + 3));
		TS_ASSERT(fc4->Expire(fc4->GetCleanupCandidate()));
		// The list should now be empty so the returned ID should be zero.
		TS_ASSERT_EQUALS(fc4->GetCleanupCandidate(), (cachedObjectId_t) 0);

		std::string dirname(s_baseTestDirName + "/" + type4);
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		delete fc4;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
	}

	void testCleanupOrphanedObjects()
	{
	}

	void testGetCacheStatus()
	{
		int i;

		std::string type5(typeName + "5");
		CFileCache *fc5 = new CFileCache(fileCacheSet, type5);
		CCacheParamValues params(10000, 20000, 100, 1, 1);
		TS_ASSERT_EQUALS(fc5->Configure(&params), true);

		int size = 0;
		for (i = 1; i <= 5; i++)
		{
			CCacheObject *co = new CCacheObject(fc5, (objId + i), filename,
			                                    (1000 + i));
			TS_ASSERT(co->Initialize(true));
			TS_ASSERT_EQUALS(fc5->Insert(co), i);
			size += GetFilesystemFileSize(1000 + i);
		}
		cacheSize_t cacheSize;
		paramValue_t numCacheObjects;
		std::string returnType = fc5->GetCacheStatus(&cacheSize,
		                         &numCacheObjects);
		TS_ASSERT_EQUALS(numCacheObjects, 5);
		TS_ASSERT_EQUALS(cacheSize, size);
		TS_ASSERT_EQUALS(type5.length(), returnType.length());
		TS_ASSERT_SAME_DATA(type5.c_str(), returnType.c_str(),
		                    (unsigned int)  type5.length());

		std::string dirname(s_baseTestDirName + "/" + type5);
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		for (i = 1; i <= 5; i++)
		{
			TS_ASSERT(fc5->Expire(objId + i));
		}
		delete fc5;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
	}

	void testGetObjectSize()
	{

		cachedObjectId_t oid = 657483;
		CCacheObject *co = new CCacheObject(fileCache, oid, filename, 12345);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->Initialize(true), true);
		TS_ASSERT_EQUALS(fileCache->Insert(co), 2);

		// Make sure the bad ID check works.
		TS_ASSERT_EQUALS(fileCache->GetObjectSize(0), -1);
		TS_ASSERT_EQUALS(fileCache->GetObjectSize(oid), 12345);
		fileCache->Subscribe(msgText, oid);
		fileCache->UnSubscribe(oid);
		TS_ASSERT_EQUALS(fileCache->GetObjectSize(oid), 0);
		fileCache->Expire(oid);
	}

	void testUpdateObject()
	{
		// We'll use Touch instead of the private UpdateObject here as
		// Touch calls Touch on the object and then calls UpdateObject so
		// we get the same effect.
		int i;

		std::string type6(typeName + "6");
		CFileCache *fc6 = new CFileCache(fileCacheSet, type6);
		CCacheParamValues params(100, 20000, 100, 1, 1);
		TS_ASSERT_EQUALS(fc6->Configure(&params), true);

		for (i = 1; i <= 3; i++)
		{
			CCacheObject *co = new CCacheObject(fc6, (objId + i), filename,
			                                    (s_blockSize + i));
			TS_ASSERT(co->Initialize(true));
			TS_ASSERT_EQUALS(fc6->Insert(co), i);
		}
		// The first created should be the last one on the list
		TS_ASSERT_EQUALS(fc6->GetCleanupCandidate(), (objId + 1));
		// After the update, the second created should be the last one
		fc6->Touch(objId + 1);
		TS_ASSERT_EQUALS(fc6->GetCleanupCandidate(), (objId + 2));
		// Updating the last object created will move it to the head of
		// the list so the last will still be the second created
		fc6->Touch(objId + 3);
		TS_ASSERT_EQUALS(fc6->GetCleanupCandidate(), (objId + 2));
		// Updating the second then the first will create a 1, 2, 3 order
		fc6->Touch(objId + 2);
		fc6->Touch(objId + 1);
		TS_ASSERT_EQUALS(fc6->GetCleanupCandidate(), (objId + 3));
		TS_ASSERT(fc6->Expire(objId + 3));
		TS_ASSERT_EQUALS(fc6->GetCleanupCandidate(), (objId + 2));
		TS_ASSERT(fc6->Expire(objId + 2));
		TS_ASSERT_EQUALS(fc6->GetCleanupCandidate(), (objId + 1));
		TS_ASSERT(fc6->Expire(objId + 1));

		std::string dirname(s_baseTestDirName + "/" + type6);
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		delete fc6;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
	}

	void testConfig()
	{
		// Create a cache, configure it, delete the cache with a file
		// existing so the cache directory can't be deleted, then
		// re-create the cache and ensure the params aren't read from the
		// config file.
		std::string type7(typeName + "7");
		CFileCache *fc7 = new CFileCache(fileCacheSet, type7);
		CCacheParamValues params(12345, 67890, 123, 4, 5);
		TS_ASSERT_EQUALS(fc7->Configure(&params), true);
		std::string dirname(s_baseTestDirName + "/" + type7 + "/Type.defaults");
		// Ensure the config file is removed after the delete
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		std::string filename(s_baseTestDirName + "/" + type7 + "/foo.bar");
		FILE *fp = ::fopen(filename.c_str(), "w");
		TS_ASSERT(fp);
		::fclose(fp);
		delete fc7;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
		TS_ASSERT_EQUALS(::unlink(filename.c_str()), 0);
		fc7 = new CFileCache(fileCacheSet, type7);
		TS_ASSERT_DIFFERS(fc7, (CFileCache *) NULL);
		// Calling Configure with NULL params should read the config file
		// but it's not there
		TS_ASSERT_EQUALS(fc7->Configure(NULL), false);
		TS_ASSERT_EQUALS(fc7->Configure(&params), true);
		// Now check the current params
		fc7->Describe(params);
		TS_ASSERT_EQUALS(params.GetLoWatermark(), GetFilesystemFileSize(12345));
		TS_ASSERT_EQUALS(params.GetHiWatermark(), GetFilesystemFileSize(67890));
		TS_ASSERT_EQUALS(params.GetSize(), 123);
		TS_ASSERT_EQUALS(params.GetCost(), 4);
		TS_ASSERT_EQUALS(params.GetLifetime(), 5);

		dirname = s_baseTestDirName + "/" + type7;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), 0);
		delete fc7;
		TS_ASSERT_EQUALS(::access(dirname.c_str(), F_OK), -1);
	}

	void testExpire()
	{
		TS_ASSERT_EQUALS(::access(pathname.c_str(), F_OK), 0);
		TS_ASSERT(fileCache->Expire(objId));
		TS_ASSERT_EQUALS(::access(pathname.c_str(), F_OK), -1);
	}

	void testCleanupDirType()
	{
		// Create a directory type file cache and insert two objects,
		// verify the directories exist and subscribe both and unsubscribe
		// one, then call CleanupDirType and see that the unsubscribed one
		// is deleted
		std::string type8(typeName + "8");
		CFileCache *fc8 = new CFileCache(fileCacheSet, type8);
		CCacheParamValues params(12345, 67890, 123, 4, 5);
		// Set the dirType true (second arg to Configure)
		TS_ASSERT_EQUALS(fc8->Configure(&params, true), true);

		CCacheObject *co = new CCacheObject(fc8, objId, filename, 1000,
		                                    0, 1, false, true);
		TS_ASSERT(co->Initialize(true));
		TS_ASSERT_EQUALS(fc8->Insert(co), 1);
		// Make sure the subscribe returns a path and not an empty string
		TS_ASSERT_LESS_THAN((size_t) 7, fc8->Subscribe(msgText, objId).length());

		co = new CCacheObject(fc8, objId + 1, filename, 1000,
		                      0, 1, false, true);
		TS_ASSERT(co->Initialize(true));
		TS_ASSERT_EQUALS(fc8->Insert(co), 2);
		// Make sure the subscribe returns a path and not an empty string
		TS_ASSERT_LESS_THAN((size_t) 7, fc8->Subscribe(msgText, objId + 1).length());

		fc8->UnSubscribe(objId);
		TS_ASSERT_EQUALS(fc8->GetNumObjects(), 2);
		// This should see the first object is unsubscribed (and expired)
		// and remove it
		fc8->CleanupDirType();
		TS_ASSERT_EQUALS(fc8->GetNumObjects(), 1);
		fc8->UnSubscribe(objId + 1);
		fc8->CleanupDirType();
		TS_ASSERT_EQUALS(fc8->GetNumObjects(), 0);
		delete fc8;
	}

	void testFinalize()
	{
		fileCache->Expire(objId);
		delete fileCache;
	}
};

#endif

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

#ifndef __CACHEOBJECTTEST_H__
#define __CACHEOBJECTTEST_H__

#include <cxxtest/TestSuite.h>
#include <cxxtest/GlobalFixture.h>
#include "CacheObject.h"
#include "FileCache.h"
#include "FileCacheSet.h"
#include "TestObjects.h"

static const std::string typeName("type");

class SetupTeardownFixture : public CxxTest::GlobalFixture
{

public:
	bool setUpWorld()
	{
		printf("\nBeginning world setup\n");
		// Make sure the directory exists and set the correct directory permissions
		std::string pathname(s_baseTestDirName);
		int retVal = ::mkdir(pathname.c_str(), 0760);
		if ((retVal == 0) || (errno == EEXIST))
		{
			pathname += "/" + typeName;
			retVal = ::mkdir(pathname.c_str(), 0760);
			if ((retVal != 0) && (errno == EEXIST))
			{
				retVal = 0;
			}
		}
		if (retVal == 0)
		{
			printf("Completed world setup\n");
			return true;
		}
		else
		{
			printf("Failed world setup!\n");
			return false;
		}
	}

	bool tearDownWorld()
	{
		printf("\nBeginning world teardown\n");
		std::string pathname(s_baseTestDirName + "/" + typeName);
		int retVal = ::rmdir(pathname.c_str());
		if ((retVal == 0) || (errno == ENOENT))
		{
			pathname = s_baseTestDirName;
			retVal = ::rmdir(pathname.c_str());
		}
		if (retVal == 0)
		{
			printf("Completed world teardown\n");
			return true;
		}
		else
		{
			int savedErrno = errno;
			printf("Failed world teardown!\n");
			printf("Failed to remove directory \'%s\' (%s)\n",
			       pathname.c_str(), strerror(savedErrno));
			return false;
		}
	}
};

static SetupTeardownFixture setupTeardownFixture;

class CacheObjectTest : public CxxTest::TestSuite
{

	char filename[32];
	size_t filenameLength;
	CCacheObject *cachedObject;
	CCacheObject *cachedDirObject;
	CFileCache *fileCache;
	CFileCacheSet *fileCacheSet;
	std::string msgText;

public:

	CacheObjectTest()
	{
		fileCacheSet = NULL;
		fileCache = NULL;
	}

	void setUp()
	{
		strcpy(filename, "testfile.ext");
		filenameLength = strlen(filename);
		if (!fileCache)
		{
			fileCacheSet = new CTestFileCacheSet();
			fileCache = new CFileCache(fileCacheSet, typeName);
		}
		if (!cachedObject)
		{
			cachedObject = new CCacheObject(fileCache, 1, filename, 123);
		}
		if (!cachedDirObject)
		{
			cachedDirObject = new CCacheObject(fileCache, 2, filename, 123,
			                                   0, 0, false, true);
		}
	}

	void tearDown()
	{
	}

	void testCacheObjectConstructorAndGetters()
	{

		cacheSize_t size = 12345;
		paramValue_t cost = 50;
		paramValue_t lifetime = 10;
		bool written = true;
		bool dirType = true;

		time_t createTime = ::time(0);
		CCacheObject *co = new CCacheObject(fileCache, 1, filename, size);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->GetCreationTime(), createTime);
		TS_ASSERT_EQUALS(co->GetLastAccessTime(), createTime);
		TS_ASSERT_EQUALS(co->GetId(), (cachedObjectId_t) 1);
		TS_ASSERT_EQUALS(co->GetSize(), size);
		TS_ASSERT_EQUALS(co->GetCost(), 0);
		TS_ASSERT_EQUALS(co->GetLifetime(), 1);
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);
		TS_ASSERT_SAME_DATA(co->GetFileName().c_str(), filename,
		                    (unsigned int) filenameLength);
		TS_ASSERT_EQUALS(co->GetFileName().length(), filenameLength);
		TS_ASSERT_EQUALS(co->isExpired(), false);
		TS_ASSERT_EQUALS(co->isWritten(), false);
		TS_ASSERT_EQUALS(co->isDirType(), false);
		delete co;

		createTime = ::time(0);
		co = new CCacheObject(fileCache, 2, filename, size, cost);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->GetCreationTime(), createTime);
		TS_ASSERT_EQUALS(co->GetLastAccessTime(), createTime);
		TS_ASSERT_EQUALS(co->GetId(), (cachedObjectId_t) 2);
		TS_ASSERT_EQUALS(co->GetSize(), size);
		TS_ASSERT_EQUALS(co->GetCost(), cost);
		TS_ASSERT_EQUALS(co->GetLifetime(), 1);
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);
		TS_ASSERT_SAME_DATA(co->GetFileName().c_str(), filename,
		                    (unsigned int) filenameLength);
		TS_ASSERT_EQUALS(co->GetFileName().length(), filenameLength);
		TS_ASSERT_EQUALS(co->isExpired(), false);
		TS_ASSERT_EQUALS(co->isWritten(), false);
		TS_ASSERT_EQUALS(co->isDirType(), false);
		delete co;

		createTime = ::time(0);
		co = new CCacheObject(fileCache, 3, filename, size, cost, lifetime);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->GetCreationTime(), createTime);
		TS_ASSERT_EQUALS(co->GetLastAccessTime(), createTime);
		TS_ASSERT_EQUALS(co->GetId(), (cachedObjectId_t) 3);
		TS_ASSERT_EQUALS(co->GetSize(), size);
		TS_ASSERT_EQUALS(co->GetCost(), cost);
		TS_ASSERT_EQUALS(co->GetLifetime(), lifetime);
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);
		TS_ASSERT_SAME_DATA(co->GetFileName().c_str(), filename,
		                    (unsigned int) filenameLength);
		TS_ASSERT_EQUALS(co->GetFileName().length(), filenameLength);
		TS_ASSERT_EQUALS(co->isExpired(), false);
		TS_ASSERT_EQUALS(co->isWritten(), false);
		TS_ASSERT_EQUALS(co->isDirType(), false);
		delete co;

		createTime = ::time(0);
		co = new CCacheObject(fileCache, 4, filename, size, cost, lifetime,
		                      written);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->GetCreationTime(), createTime);
		TS_ASSERT_EQUALS(co->GetLastAccessTime(), createTime);
		TS_ASSERT_EQUALS(co->GetId(), (cachedObjectId_t) 4);
		TS_ASSERT_EQUALS(co->GetSize(), size);
		TS_ASSERT_EQUALS(co->GetCost(), cost);
		TS_ASSERT_EQUALS(co->GetLifetime(), lifetime);
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);
		TS_ASSERT_SAME_DATA(co->GetFileName().c_str(), filename,
		                    (unsigned int) filenameLength);
		TS_ASSERT_EQUALS(co->GetFileName().length(), filenameLength);
		TS_ASSERT_EQUALS(co->isExpired(), false);
		TS_ASSERT_EQUALS(co->isWritten(), written);
		TS_ASSERT_EQUALS(co->isDirType(), false);
		delete co;

		createTime = ::time(0);
		co = new CCacheObject(fileCache, 5, filename, size, cost, lifetime,
		                      written, dirType);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->GetCreationTime(), createTime);
		TS_ASSERT_EQUALS(co->GetLastAccessTime(), createTime);
		TS_ASSERT_EQUALS(co->GetId(), (cachedObjectId_t) 5);
		TS_ASSERT_EQUALS(co->GetSize(), size);
		TS_ASSERT_EQUALS(co->GetCost(), cost);
		TS_ASSERT_EQUALS(co->GetLifetime(), lifetime);
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);
		TS_ASSERT_SAME_DATA(co->GetFileName().c_str(), filename,
		                    (unsigned int) filenameLength);
		TS_ASSERT_EQUALS(co->GetFileName().length(), filenameLength);
		TS_ASSERT_EQUALS(co->isExpired(), false);
		TS_ASSERT_EQUALS(co->isWritten(), written);
		TS_ASSERT_EQUALS(co->isDirType(), dirType);
		delete co;
	}

	void testInitialize()
	{
		std::string pathname(cachedObject->GetPathname());
		TS_ASSERT_EQUALS(cachedObject->Initialize(true), true);

		// make sure we don't have write permissions at this point
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK), 0);
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), -1);
		struct stat buf;
		TS_ASSERT_EQUALS(::stat(pathname.c_str(), &buf), 0);
		TS_ASSERT(S_ISREG(buf.st_mode));

		int maxLength = 256;
		char fileName[maxLength];
		memset(fileName, 0, maxLength);
		fileName[0] = '\0';

		// Now validate all the set extened attr
		FC_getxattr(pathname.c_str(), "user.f", fileName, maxLength);
		TS_ASSERT_EQUALS(strlen(fileName), filenameLength);
		TS_ASSERT_SAME_DATA(fileName, filename, (unsigned int) filenameLength);

		cacheSize_t sz = 0;
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 123);

		paramValue_t val = 9;
		FC_getxattr(pathname.c_str(), "user.c", &val, sizeof(val));
		TS_ASSERT_EQUALS(val, 0);

		val = 9;
		FC_getxattr(pathname.c_str(), "user.l", &val, sizeof(val));
		TS_ASSERT_EQUALS(val, 1);

		int wrt = 9;
		FC_getxattr(pathname.c_str(), "user.w", &wrt, sizeof(wrt));
		TS_ASSERT_EQUALS(wrt, 0);

		val = 9;
		FC_getxattr(pathname.c_str(), "user.d", &val, sizeof(val));
		TS_ASSERT_EQUALS(val, 0);
	}

	void testDirInitialize()
	{
		std::string pathname(cachedDirObject->GetPathname());
		TS_ASSERT_EQUALS(cachedDirObject->Initialize(true), true);

		// make sure we don't have write permissions at this point
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK), 0);
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), -1);
		struct stat buf;
		TS_ASSERT_EQUALS(::stat(pathname.c_str(), &buf), 0);
		TS_ASSERT(S_ISDIR(buf.st_mode));

		int maxLength = 256;
		char fileName[maxLength];
		memset(fileName, 0, maxLength);
		fileName[0] = '\0';

		// Now validate all the set extened attr
		FC_getxattr(pathname.c_str(), "user.f", fileName, maxLength);
		TS_ASSERT_EQUALS(strlen(fileName), filenameLength);
		TS_ASSERT_SAME_DATA(fileName, filename, (unsigned int) filenameLength);

		cacheSize_t sz = 0;
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 123);

		paramValue_t val = 9;
		FC_getxattr(pathname.c_str(), "user.c", &val, sizeof(val));
		TS_ASSERT_EQUALS(val, 0);

		val = 9;
		FC_getxattr(pathname.c_str(), "user.l", &val, sizeof(val));
		TS_ASSERT_EQUALS(val, 1);

		int wrt = 9;
		FC_getxattr(pathname.c_str(), "user.w", &wrt, sizeof(wrt));
		TS_ASSERT_EQUALS(wrt, 0);

		val = 9;
		FC_getxattr(pathname.c_str(), "user.d", &val, sizeof(val));
		TS_ASSERT_EQUALS(val, 1);
	}

	void testGetFileCacheType()
	{
		TS_ASSERT_EQUALS(cachedObject->GetFileCacheType().length(),
		                 typeName.length());
		TS_ASSERT_SAME_DATA(cachedObject->GetFileCacheType().c_str(),
		                    typeName.c_str(), (unsigned int) typeName.length());
	}

	void testSubscribe()
	{
		CCacheObject *co = new CCacheObject(fileCache, 54321, filename, 54321);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		std::string pathname(co->GetPathname());

		// Initialize so the file exists
		TS_ASSERT_EQUALS(co->Initialize(true), true);

		// validate the size attr
		cacheSize_t sz = 0;
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 54321);

		// Before the subscribe, we shouldn't have write permissions
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK), 0);
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), -1);

		// There should not yet be any subscriptions
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);

		// When we subscribe, we should get back the correct file path and
		// the access time should update
		TS_ASSERT_SAME_DATA(co->Subscribe(msgText).c_str(), pathname.c_str(),
		                    (unsigned int) pathname.length());
		TS_ASSERT_EQUALS(co->GetLastAccessTime(), ::time(0));

		// Once we are subscribed (first subscription) we should have a
		// writable file.
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), 0);
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 1);

		// Now see if we can subscribe again which should fail by
		// returning a zero length string for the file path and the
		// subscription count should not be incremented.
		TS_ASSERT_EQUALS(co->Subscribe(msgText).length(), (size_t) 0);
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 1);

		// Write to the file so it stays around
		FILE *fp = ::fopen(pathname.c_str(), "w");
		TS_ASSERT(fp != NULL);
		::fwrite(&sz, sizeof(sz), 1, fp);
		::fclose(fp);

		// Now unsubscribe and make sure the post write cleanup happens
		co->UnSubscribe();

		// Verify the access time was updated and the subscription count
		// decremented.
		TS_ASSERT_EQUALS(co->GetLastAccessTime(), ::time(0));
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);

		// Since we didn't actually write to the file, the size should be
		// updated to zero, let's verify it on the file as well as from
		// the attribute
		sz = 9;
		struct stat buf;
		::stat(pathname.c_str(), &buf);
		TS_ASSERT_EQUALS(buf.st_size, 4);
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 4);

		// Additionally the written attribute should now be set to 1 on
		// the file.
		int wrt = 9;
		FC_getxattr(pathname.c_str(), "user.w", &wrt, sizeof(wrt));
		TS_ASSERT_EQUALS(wrt, 1);

		// The file should no longer be writable.
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), -1);

		// Now add a couple of new subscriptions
		TS_ASSERT_SAME_DATA(co->Subscribe(msgText).c_str(), pathname.c_str(),
		                    (unsigned int) pathname.length());
		TS_ASSERT_SAME_DATA(co->Subscribe(msgText).c_str(), pathname.c_str(),
		                    (unsigned int) pathname.length());

		// Validate the count
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 2);

		// Make sure expire recognizes the subscriptions
		TS_ASSERT_EQUALS(co->Expire(), false);

		// Validate it is expired
		TS_ASSERT_EQUALS(co->isExpired(), true);

		// After the first unsubscribe, the file should still exist.
		co->UnSubscribe();
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 1);
		TS_ASSERT_EQUALS(co->isExpired(), true);
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK), 0);

		// After the second, it should get cleaned up
		co->UnSubscribe();
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);
		TS_ASSERT_EQUALS(co->isExpired(), true);
		delete co;
	}

	void testTouch()
	{
		TS_ASSERT_EQUALS(cachedObject->Touch(), ::time(0));
	}

	void testResize()
	{

		// Create an object of size 1
		CCacheObject *co = new CCacheObject(fileCache, 987654, filename, 1);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->Initialize(true), true);
		std::string pathname(co->GetPathname());

		// Validate the size attribute
		cacheSize_t sz = 0;
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 1);

		// Make sure resize fails (returns the original size) as there is
		// no subscription
		TS_ASSERT_EQUALS(co->Resize(10), 1);
		sz = 0;
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 1);

		// Now subscribe so the file is writable and can be resized
		TS_ASSERT_SAME_DATA(co->Subscribe(msgText).c_str(), pathname.c_str(),
		                    (unsigned int) pathname.length());

		// This time the resize should return the desired new size
		TS_ASSERT_EQUALS(co->Resize(10), 10);

		// and the size attribute should be updated to reflect that.
		sz = 0;
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 10);

		// Write to the file so it stays around
		FILE *fp = ::fopen(pathname.c_str(), "w");
		TS_ASSERT(fp != NULL);
		::fwrite(&sz, sizeof(sz), 1, fp);
		::fclose(fp);

		// When we unsubscribe, the size will be reset to match the actual
		// size (since it is smaller than the resize set size) and
		// therefore size should be four.
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 1);
		co->UnSubscribe();
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);
		TS_ASSERT_EQUALS(co->GetSize(), 4);
		sz = 0;
		FC_getxattr(pathname.c_str(), "user.s", &sz, sizeof(sz));
		TS_ASSERT_EQUALS(sz, 4);

		// At this point a resize should fail again since there is no
		// subscription.  It will return the original size.
		TS_ASSERT_EQUALS(co->Resize(20), 4);

		// Now lets validate that after a new subscription to an already
		// written object, we can't resize it.
		TS_ASSERT_EQUALS(co->isWritten(), true);
		TS_ASSERT_SAME_DATA(co->Subscribe(msgText).c_str(), pathname.c_str(),
		                    (unsigned int) pathname.length());
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 1);
		TS_ASSERT_EQUALS(co->Resize(20), 4);
		co->UnSubscribe();
		TS_ASSERT_EQUALS(co->GetSubscriptionCount(), 0);

		// Cleanup the object
		TS_ASSERT_EQUALS(co->Expire(), true);
		delete co;
	}

	void testGetCacheCost()
	{
		CCacheObject *co = new CCacheObject(fileCache, 564738, filename, 0);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->Initialize(true), true);

		// Since the minimum lifetime is 1, make sure the initial check of
		// cost is set to the max.
		TS_ASSERT_EQUALS(co->GetCacheCost(), s_maxCost);

		// Now wait a second and check again
		::sleep(1);

		// The cost should be zero since the size is zero.
		TS_ASSERT_EQUALS(co->GetCacheCost(), 0);
		TS_ASSERT_EQUALS(co->Expire(), true);
		delete co;

		// Now create an object with a size of 1, a cost of 10 and a
		// lifetime of 5
		co = new CCacheObject(fileCache, 786950, filename, 1, 10, 5);
		TS_ASSERT_DIFFERS(co, (CCacheObject *) NULL);
		TS_ASSERT_EQUALS(co->Initialize(true), true);

		// Again the cost should be max until the lifetime expires.
		TS_ASSERT_EQUALS(co->GetCacheCost(), s_maxCost);

		// After the specified lifetime the cost should be set to 2 (cost
		// of 10 * size in pages of 1 divided by lifetime of 5)
		::sleep(5);
		TS_ASSERT_EQUALS(co->GetCacheCost(), 2);
		TS_ASSERT_EQUALS(co->Expire(), true);
		delete co;
	}

	void testUnsubscribeDirType()
	{
		std::string pathname(cachedDirObject->GetPathname());

		// There should not yet be any subscriptions
		TS_ASSERT_EQUALS(cachedDirObject->GetSubscriptionCount(), 0);

		// When we subscribe, we should get back the correct path and
		// the access time should update
		TS_ASSERT_SAME_DATA(cachedDirObject->Subscribe(msgText).c_str(),
		                    pathname.c_str(),
		                    (unsigned int) pathname.length());
		TS_ASSERT_EQUALS(cachedDirObject->GetLastAccessTime(), ::time(0));

		// Once we are subscribed (first subscription) we should have a
		// writable object
		TS_ASSERT_EQUALS(::access(pathname.c_str(), R_OK | W_OK), 0);
		TS_ASSERT_EQUALS(cachedDirObject->GetSubscriptionCount(), 1);

		// Now see if we can subscribe again which should fail by
		// returning a zero length string for the path and the
		// subscription count should not be incremented.
		TS_ASSERT_EQUALS(cachedDirObject->Subscribe(msgText).length(), (size_t) 0);
		TS_ASSERT_EQUALS(cachedDirObject->GetSubscriptionCount(), 1);

		// Now create a few files while it's writable so we can test
		// cleanup
		std::string dir1(pathname + "/foo");
		std::string dir2(pathname + "/bar");
		std::string file1(dir1 + "/bar");
		std::string file2(dir2 + "/baz");
		TS_ASSERT_EQUALS(::mkdir(dir1.c_str(), 0770), 0);
		TS_ASSERT_EQUALS(::mkdir(dir2.c_str(), 0770), 0);
		FILE *fp = ::fopen(file1.c_str(), "w");
		TS_ASSERT(fp);
		::fclose(fp);
		fp = ::fopen(file2.c_str(), "w");
		TS_ASSERT(fp);
		::fclose(fp);
		// Now unsubscribe and make sure the post write cleanup happens
		// which for a dirType object is it gets expired.
		cachedDirObject->UnSubscribe();
		TS_ASSERT(cachedDirObject->isExpired());
	}

	void testExpire()
	{
		TS_ASSERT_EQUALS(cachedObject->Expire(), true);
		TS_ASSERT_EQUALS(cachedDirObject->Expire(), true);
	}

	void testFinalize()
	{
		delete cachedObject;
		delete cachedDirObject;
		delete fileCache;
	}
};

#endif

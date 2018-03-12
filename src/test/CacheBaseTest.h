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


#ifndef __CACHEBASETEST_H__
#define __CACHEBASETEST_H__

#include <cxxtest/TestSuite.h>
#include "CacheBase.h"
#include "TestObjects.h"

class CacheBaseTest : public CxxTest::TestSuite
{

	cachedObjectId_t objId;
	char cObjId[s_numChars + 1];

	// data for positive testing
	std::string goodPath1;
	std::string goodPath2;
	std::string goodPath3;

	// data for negative testing
	std::string badPath1;
	std::string badPath2;
	std::string badPath3;
	std::string badPath4;

public:
	void setUp()
	{
		objId = 4538775134664ULL;
		goodPath1 = "/dir/subdir1/.../type/A/BCDEFGHI.ext";
		goodPath2 = s_baseTestDirName + "/type/A/BCDEFGHI.ext";
		goodPath3 = s_baseTestDirName + "/type/A/BCDEFGHI";
		badPath1 = "/dir/subdir1/.../type/A/BCDEFG.ext";
		badPath2 = "/dir/subdir1/.../type/ABCDEFGHI.ext";
		badPath3 = "/dir.subdir1.type.ABCDEFGHI.ext";
		badPath4 = "dirsubdir1typeABCDEFGHI";
	}

	void testGetCharNFromObjectId()
	{
		// Get each character from the object id and check the array
		// against the set value
		for (int i = 0; i < s_numChars; i++)
		{
			cObjId[s_numChars - 1 - i] = GetCharNFromObjectId(objId, i);
		}
		cObjId[s_numChars] = 0;
		TS_ASSERT_SAME_DATA(cObjId, "ABCDEFGHI", s_numChars);
	}

	void testGetValueForChar()
	{
		for (int i = 0; i < s_numChars; i++)
		{
			char c = cObjId[i];
			TS_ASSERT_EQUALS(GetValueForChar(c), i);
		}
		TS_ASSERT_EQUALS(GetValueForChar('@'), -1);
	}

	void testGetObjectIdFromPath()
	{
		cachedObjectId_t cmpId = 0ULL;
		TS_ASSERT_EQUALS(GetObjectIdFromPath(goodPath1.c_str()), objId);
		TS_ASSERT_EQUALS(GetObjectIdFromPath(goodPath3.c_str()), objId);
		TS_ASSERT_EQUALS(GetObjectIdFromPath(badPath1.c_str()), cmpId);
		TS_ASSERT_EQUALS(GetObjectIdFromPath(badPath2.c_str()), cmpId);
	}

	void testGetTypeNameFromPath()
	{
		std::string typeName = GetTypeNameFromPath(s_baseTestDirName,
		                       goodPath2.c_str());
		TS_ASSERT_SAME_DATA(typeName.c_str(), "type", 4);
		TS_ASSERT(typeName.length() == 4);
		typeName = GetTypeNameFromPath(s_baseTestDirName,
		                               badPath3.c_str());
		TS_ASSERT_SAME_DATA(typeName.c_str(), "", 1);
		TS_ASSERT(typeName.length() == 0);
	}

	void testGetFileExtension()
	{
		TS_ASSERT_SAME_DATA(GetFileExtension(goodPath1.c_str()).c_str(),
		                    ".ext", 4);
		TS_ASSERT_SAME_DATA(GetFileExtension(badPath4.c_str()).c_str(),
		                    "", 1);
	}

	void testBuildPathname()
	{
		const std::string type("type");
		const std::string file("foo.ext");
		const std::string dirBase(s_baseTestDirName);

		std::string pathname(BuildPathname(objId, dirBase, type, file, false));
		TS_ASSERT_SAME_DATA(pathname.c_str(), goodPath2.c_str(),
		                    (unsigned int) goodPath2.length());
		pathname = BuildPathname(0, dirBase, type, file, false);
		TS_ASSERT_SAME_DATA(pathname.c_str(), "", 1);
	}

	void testCacheParamValuesConstructor()
	{
		CCacheParamValues params1;
		CCacheParamValues params2(1, 2, 3, 4, 5);
		TS_ASSERT_EQUALS(params1.GetLoWatermark(), 0);
		TS_ASSERT_EQUALS(params1.GetHiWatermark(), 0);
		TS_ASSERT_EQUALS(params1.GetSize(), 0);
		TS_ASSERT_EQUALS(params1.GetCost(), 0);
		TS_ASSERT_EQUALS(params1.GetLifetime(), 1);
		TS_ASSERT_EQUALS(params2.GetLoWatermark(), 1);
		TS_ASSERT_EQUALS(params2.GetHiWatermark(), 2);
		TS_ASSERT_EQUALS(params2.GetSize(), 3);
		TS_ASSERT_EQUALS(params2.GetCost(), 4);
		TS_ASSERT_EQUALS(params2.GetLifetime(), 5);
	}

	void testCacheParamValuesSettersandGetters()
	{
		CCacheParamValues params(1, 2, 3, 4, 5);
		params.SetLoWatermark(10);
		params.SetHiWatermark(20);
		params.SetSize(30);
		params.SetCost(40);
		params.SetLifetime(50);
		TS_ASSERT_EQUALS(params.GetLoWatermark(), 10);
		TS_ASSERT_EQUALS(params.GetHiWatermark(), 20);
		TS_ASSERT_EQUALS(params.GetSize(), 30);
		TS_ASSERT_EQUALS(params.GetCost(), 40);
		TS_ASSERT_EQUALS(params.GetLifetime(), 50);
	}

	void testCacheParamValuesOperators()
	{
		CCacheParamValues params1;
		CCacheParamValues params2(1, 2, 3, 4, 5);
		CCacheParamValues params3(1, 2, 3, 4, 6);
		TS_ASSERT(params1 != params2);
		TS_ASSERT(!(params1 == params2));
		TS_ASSERT(params1 != params3);
	}

	void testCleanupDir()
	{
		struct stat buf;
		FILE *fp;
		char tempbase[20] = "/tmp/test/fooXXXXXX";
		std::string pathname(::mkdtemp(tempbase));

		std::string dirname(pathname + "/foo");
		TS_ASSERT(::mkdir(dirname.c_str(), 0700) == 0);
		TS_ASSERT_EQUALS(::stat(dirname.c_str(), &buf), 0);
		TS_ASSERT(S_ISDIR(buf.st_mode));
		std::string filename(dirname + "/foo");
		TS_ASSERT((fp = fopen(filename.c_str(), "w")) != NULL);
		::fclose(fp);
		TS_ASSERT_EQUALS(::access(filename.c_str(), F_OK), 0);
		TS_ASSERT_EQUALS(::stat(filename.c_str(), &buf), 0);
		TS_ASSERT(S_ISREG(buf.st_mode));

		dirname = pathname + "/bar";
		TS_ASSERT(::mkdir(dirname.c_str(), 0700) == 0);
		TS_ASSERT_EQUALS(::stat(dirname.c_str(), &buf), 0);
		TS_ASSERT(S_ISDIR(buf.st_mode));
		filename = dirname + "/foo";
		TS_ASSERT((fp = fopen(filename.c_str(), "w")) != NULL);
		::fclose(fp);
		TS_ASSERT_EQUALS(::access(filename.c_str(), F_OK), 0);
		TS_ASSERT_EQUALS(::stat(filename.c_str(), &buf), 0);
		TS_ASSERT(S_ISREG(buf.st_mode));

		dirname = pathname + "/baz";
		TS_ASSERT(::mkdir(dirname.c_str(), 0700) == 0);
		TS_ASSERT_EQUALS(::stat(dirname.c_str(), &buf), 0);
		TS_ASSERT(S_ISDIR(buf.st_mode));
		filename = dirname + "/foo";
		TS_ASSERT((fp = fopen(filename.c_str(), "w")) != NULL);
		::fclose(fp);
		TS_ASSERT_EQUALS(::access(filename.c_str(), F_OK), 0);
		TS_ASSERT_EQUALS(::stat(filename.c_str(), &buf), 0);
		TS_ASSERT(S_ISREG(buf.st_mode));

		TS_ASSERT_EQUALS(::access(pathname.c_str(), F_OK), 0);
		TS_ASSERT_EQUALS(::stat(pathname.c_str(), &buf), 0);
		TS_ASSERT(S_ISDIR(buf.st_mode));
		std::string msgText;
		CleanupDir(pathname, msgText);
		TS_ASSERT_EQUALS(::access(pathname.c_str(), F_OK), -1);
	}
};

#endif

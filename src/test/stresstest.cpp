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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <lunaservice.h>
#include <pthread.h>

#include "ls-test.h"

#include <pbnjson.hpp>
#include <string>
#include <iostream>

#define FILECACHE_SERVICE_URI   "palm://com.palm.filecache"

#define FILECACHE_SERVICE   "com.palm.filecache"

LSHandle *filecacheService = NULL;

int send_count = 10;
int fileSize;
std::string typeStr;
std::string nFilesStr;
int nFiles;

//GMainLoop *mainLoop = NULL;
bool handledResponse = false;
std::string jsonResponseStr;

static bool
FilecacheServiceCb(LSHandle *sh, LSMessage *message, void *ctx)
{
	printf("got callback\n");
	LSMessagePrint(message, stdout);
	jsonResponseStr = LSMessageGetPayload(message);
	handledResponse = true;
	//waiter_signal(&response_waiter);

	return true;
}

void Usage()
{
	fprintf(stderr, "Usage: stresstest -t typename [-n nfiles] [-s filesize ]\n");
	exit(1);
}


int
main(int argc, char *argv[])
{
	bool haveNFlag = false;
	bool haveTFlag = false;
	std::string sizeStr;
	std::string typeStr;

	for (int i = 1; i < argc; i++)
	{

		std::string thisArg = argv[i];
		if (thisArg == "-s")
		{
			if (i == (argc - 1))
			{
				Usage();
			}
			sizeStr = argv[i + 1];
			fileSize = atoi(sizeStr.c_str());
			if (fileSize < 0)
			{
				Usage();
			}
			i++;
		}
		else if (thisArg == "-n")
		{
			haveNFlag = true;
			if (i == (argc - 1))
			{
				Usage();
			}
			std::string nFileStr = argv[i + 1];
			nFiles = atoi(nFilesStr.c_str());
			if (nFiles < 0)
			{
				Usage();
			}
			i++;
		}
		else if (thisArg == "-t")
		{
			haveTFlag = true;
			if (i == (argc - 1))
			{
				Usage();
			}
			typeStr = argv[i + 1];
			i++;
		}
		else
		{
			Usage();
		}
	}

	if (!haveTFlag)
	{
		Usage();
	}

	if (haveNFlag)
	{
		printf("Run for %d files\n", nFiles);
	}
	else
	{
		printf("Run forever\n");
	}
	printf("File size %d bytes\n", fileSize);
	printf("Use filecache type %s\n", typeStr.c_str());



	LSError lserror;
	LSErrorInit(&lserror);

	GMainLoop *mainLoop = NULL;

	mainLoop = g_main_loop_new(NULL, FALSE);

	bool ret = LSRegister(NULL, &filecacheService, &lserror);
	if (!ret)
	{
		fprintf(stderr, "LSRegister failed\n");
		exit(1);
	}

	ret = LSGmainAttach(filecacheService, mainLoop, &lserror);
	if (!ret)
	{
		fprintf(stderr, "LSGmainAttach failed\n");
		exit(1);
	}

	//
	// Create types in filecache.
	//
	std::string typeName = typeStr;
	std::string loWatermark = "10000";
	std::string hiWatermark = "100000000";
	std::string size = sizeStr;
	std::string cost = "1";
	std::string lifetime = "100000";
	std::string payload =
	    "{\"typeName\":\"" + typeName + "\"" +
	    ", \"loWatermark\": " + loWatermark +
	    ", \"hiWatermark\": " + hiWatermark +
	    ", \"size\": " + size +
	    ", \"cost\": " + cost +
	    ", \"lifetime\": " + lifetime +
	    ", \"dirType\": false }";


	std::string DefineTypeURI = std::string(FILECACHE_SERVICE_URI) +
	                            "/DefineType";

	printf("calling DefineType %s\n", payload.c_str());

	ret = LSCall(filecacheService, DefineTypeURI.c_str(), payload.c_str(),
	             FilecacheServiceCb,
	             GINT_TO_POINTER(send_count - 1), NULL, &lserror);
	if (!ret)
	{
		fprintf(stderr, "DefineType failed\n");
		exit(1);
	}

	//
	// Wait for response from this LSCall()
	//
	handledResponse = false;
	while (!handledResponse)
	{
		g_main_context_iteration(NULL, true);
	}

	printf("created filecache type %s\n", typeName.c_str());

	//
	// OK now create file in the cache.
	//
	int filesCreated = 0;
	while (true)
	{
		std::string fileName = "a.txt";
		size = sizeStr;
		std::string cost = "1";
		std::string lifetime = "100000";
		std::string subscribe = "true";

		payload =
		    "{\"typeName\":\"" + typeName + "\"" +
		    ", \"fileName\":\"" + fileName + "\"" +
		    ", \"size\": " + size +
		    ", \"cost\": " + cost +
		    ", \"lifetime\": " + lifetime +
		    ", \"subscribe\": true }";

		std::string InsertCacheObjectURI =
		    std::string(FILECACHE_SERVICE_URI) +
		    "/InsertCacheObject";

		LSMessageToken token;
		handledResponse = false;
		printf("calling InsertCacheObject %s\n", payload.c_str());
		ret = LSCall(filecacheService, InsertCacheObjectURI.c_str(),
		             payload.c_str(), FilecacheServiceCb,
		             GINT_TO_POINTER(send_count - 1), &token, &lserror);
		if (!ret)
		{
			fprintf(stderr, "InsertCacheObject failed\n");
			goto error;
		}

		while (!handledResponse)
		{
			g_main_context_iteration(NULL, true);
		}

		printf("created cache object\n");

		//
		// Parse the returned json.
		//
		pbnjson::JSchemaFragment inputSchema("{}");
		pbnjson::JDomParser parser(NULL);
		if (!parser.parse(jsonResponseStr, inputSchema, NULL))
		{
			// handle error
			fprintf(stderr, "Error parsing json response\n");
			exit(1);
		}

		pbnjson::JValue parsed = parser.getDom();

		std::string cacheFileName = parsed["pathName"].asString();
		printf("cacheFileName = %s\n", cacheFileName.c_str());

		//
		// Write data to the file.
		//
		char buf[fileSize];
		int fd;
		if ((fd = open(cacheFileName.c_str(), O_WRONLY)) == -1)
		{
			fprintf(stderr, "Error, unable to open file %s for writing\n",
			        cacheFileName.c_str());
			exit(1);
		}

		if (write(fd, buf, fileSize) != fileSize)
		{
			fprintf(stderr, "Error, write to %s failed\n",
			        cacheFileName.c_str());
			exit(1);
		}

		close(fd);

		//
		// Cancel the subscription.
		//
		ret = LSCallCancel(filecacheService, token, &lserror);
		if (!ret)
		{
			fprintf(stderr, "Cancel of subscription failed\n");
			goto error;
		}
		printf("subscription cancelled\n");

		filesCreated++;
		if (filesCreated == nFiles)
		{
			printf("Done with %d files created\n", nFiles);
			exit(0);
		}

	}


	exit(0);

error:
	fprintf(stderr, "stresstest exits\n");
	exit(1);
}

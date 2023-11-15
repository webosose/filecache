// Copyright (c) 2007-2022 LG Electronics, Inc.
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

#include <assert.h>
#include <giomm/init.h>
#include "FileCacheServiceApp.h"
#include "core/MojLogEngine.h"
#include "CacheBase.h"
#include <luna-service2/lunaservice-meta.h>
#include <luna-service2/lunaservice.h>
#include "boost/filesystem.hpp"

const char *const ServiceApp::ServiceName = "com.palm.filecache";

namespace
{
	PmLogContext LogContext()
	{
		static PmLogContext context = NULL;
		if (!context)
			PmLogGetContext(NULL, &context);
		return context;
	}
}

void handle_idle_timeout_cb(void *userData)
{
	ServiceApp* app = static_cast<ServiceApp*>(userData);
	if(app)
	{
		app->powerdown();
	}
}

void create_default_cachedir()
{
	//Creating default directory if not exists
	std::string defaultDestinationPath("/media/internal/downloads");
	if (!boost::filesystem::exists(defaultDestinationPath))
	{
		boost::filesystem::create_directories(defaultDestinationPath);
	}
}

int main(int argc, char **argv)
{

	// This makes sure using an unsigned long long for
	// cachedObjectId_t will yield 64 bits.  Using uint64_t caused
	// issues with printf formats between 32 and 64 bit machines
	assert(sizeof(cachedObjectId_t) == 8);
	create_default_cachedir();
	ServiceApp app;
	LSIdleTimeout(LUNA_IDLE_TIMEOUT_MSEC, handle_idle_timeout_cb, &app,NULL);
	int mainResult = app.main(argc, argv);

	return mainResult;
}

ServiceApp::ServiceApp() : m_service()
{

	//  MojLogEngine::instance()->reset(MojLogger::LevelTrace);

	// When creating the service app, walk the directory tree and build
	// the cache data structures for objects already cached.
	m_fileCacheSet = new CFileCacheSet;
	m_fileCacheSet->WalkDirTree();

	// This is part of the fix for NOV-128944.
	m_fileCacheSet->CleanupAtStartup();
}

void ServiceApp::powerdown()
{
	MojErr err = MojErrNone;

	if(!(m_fileCacheSet->GetCacheSize() || m_handler.get()->GetSubscriberCount()))
	{
		Base::shutdown();
	}
}

MojErr ServiceApp::close()
{
	MojErr err = MojErrNone;
	MojErr errClose = m_service.close();
	MojErrAccumulate(err, errClose);
	errClose = Base::close();
	MojErrAccumulate(err, errClose);
	if(m_fileCacheSet)
	{
		free(m_fileCacheSet);
	}
	return err;
}

MojErr ServiceApp::open()
{

	Gio::init();

	MojErr err = Base::open();
	MojErrCheck(err);

	err = m_service.open(ServiceName);
	MojErrCheck(err);

	err = m_service.attach(m_reactor.impl());
	MojErrCheck(err);

	m_handler.reset(new CategoryHandler(m_fileCacheSet));
	MojAllocCheck(m_handler.get());

	err = m_handler->RegisterMethods();
	MojErrCheck(err);

	err = m_service.addCategory(MojLunaService::DefaultCategory,
	                            m_handler.get());
	MojErrCheck(err);

	LSError error;
	LSErrorInit(&error);

        if (!LSCategorySetDescription(m_service.getHandle(), MojLunaService::DefaultCategory, m_handler->GetMethodsDescription(), &error))
        {
           LSErrorLog(LogContext(), "CATEGORY_DESCRIPTION", &error);
           LSErrorFree(&error);
           return MojErrInternal;
        }

#if (!defined(TARGET_DESKTOP) && defined(FILECACHE_UPSTART))
//Disabling logic for emitting ls-hub-ready(wrong) upstart signal as filecache-ready is already being emitted from upstart - files/launch/filecache.conf
	char *upstartJob = ::getenv("UPSTART_JOB");
	if (upstartJob)
	{
		char *upstartEvent = g_strdup_printf("%s emit %s-ready",
		                                     (gchar *)s_InitctlCommand.c_str(),
		                                     upstartJob);
		if (upstartEvent)
		{
			int retVal = ::system(upstartEvent);
			if (retVal == -1)
			{
				MojLogError(s_globalLogger,
				            _T("ServiceApp: Failed to emit upstart event"));
			}
			g_free(upstartEvent);
		}
		else
		{
			MojLogError(s_globalLogger,
			            _T("ServiceApp: Failed to allocate memory for upstart emit"));
		}
	}
#endif // #if (!defined(TARGET_DESKTOP) && defined(FILECACHE_UPSTART))

	return MojErrNone;
}

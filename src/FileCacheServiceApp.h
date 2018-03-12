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

#ifndef __FILECACHE_SERVICEAPP_H__
#define __FILECACHE_SERVICEAPP_H__

#include "core/MojGmainReactor.h"
#include "core/MojReactorApp.h"
#include "luna/MojLunaService.h"
#include "CategoryHandler.h"
#include "FileCacheSet.h"

#define LUNA_IDLE_TIMEOUT_MSEC  30000

class ServiceApp : public MojReactorApp<MojGmainReactor>
{
public:
	ServiceApp();
	virtual MojErr open();
	virtual MojErr close();
	virtual void powerdown();
private:
	typedef MojReactorApp<MojGmainReactor> Base;

	static const char *const ServiceName;
	CFileCacheSet *m_fileCacheSet;

	MojRefCountedPtr<CategoryHandler> m_handler;
	MojLunaService m_service;
};

#endif /* __FILECACHE_SERVICEAPP_H__ */

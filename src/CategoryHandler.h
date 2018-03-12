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

#ifndef __FILECACHE_CATEGORYHANDLER_H__
#define __FILECACHE_CATEGORYHANDLER_H__

#include "CacheBase.h"
#include "FileCacheSet.h"
#include "core/MojService.h"
#include "luna/MojLunaMessage.h"
#include "glib.h"
#include <vector>
#include <pbnjson.h>

static const std::string s_InterfaceVersion("1.1");

class CategoryHandler : public MojService::CategoryHandler
{
public:
	CategoryHandler(CFileCacheSet *cacheSet);
	virtual ~CategoryHandler();
	MojErr RegisterMethods();
	const jvalue_ref GetMethodsDescription() const;
	int GetSubscriberCount() const
	{
		return m_subscribers.size();
	}
private:
	class Subscription : public MojSignalHandler
	{
	public:
		Subscription(CategoryHandler &handler, MojServiceMessage *msg,
		             MojString &pathName);
		~Subscription();
		MojString GetPathName()
		{
			return m_pathName;
		}

	private:
		MojErr HandleCancel(MojServiceMessage *msg);

		CategoryHandler &m_handler;
		MojRefCountedPtr<MojServiceMessage> m_msg;
		MojString m_pathName;
		MojServiceMessage::CancelSignal::Slot<Subscription> m_cancelSlot;
	};

	MojErr DefineType(MojServiceMessage *msg, MojObject &payload);
	MojErr ChangeType(MojServiceMessage *msg, MojObject &payload);
	MojErr DeleteType(MojServiceMessage *msg, MojObject &payload);
	MojErr DescribeType(MojServiceMessage *msg, MojObject &payload);
	MojErr InsertCacheObject(MojServiceMessage *msg, MojObject &payload);
	MojErr ResizeCacheObject(MojServiceMessage *msg, MojObject &payload);
	MojErr ExpireCacheObject(MojServiceMessage *msg, MojObject &payload);
	MojErr SubscribeCacheObject(MojServiceMessage *msg, MojObject &payload);
	MojErr TouchCacheObject(MojServiceMessage *msg, MojObject &payload);
	MojErr CopyCacheObject(MojServiceMessage *msg, MojObject &payload);
	MojErr GetCacheStatus(MojServiceMessage *msg, MojObject &payload);
	MojErr GetCacheTypeStatus(MojServiceMessage *msg, MojObject &payload);
	MojErr GetCacheObjectSize(MojServiceMessage *msg, MojObject &payload);
	MojErr GetCacheObjectFilename(MojServiceMessage *msg, MojObject &payload);
	MojErr GetCacheTypes(MojServiceMessage *msg, MojObject &payload);
	MojErr GetVersion(MojServiceMessage *msg, MojObject &payload);

	MojErr CancelSubscription(Subscription *sub, MojServiceMessage *msg,
	                          MojString &pathName);

	typedef MojRefCountedPtr<Subscription> SubscriptionPtr;
	typedef std::vector<SubscriptionPtr> SubscriptionVec;

	MojErr SetupWorkerTimer();
	MojErr WorkerHandler();
	static gboolean TimerCallback(void *data);
	MojErr CleanerHandler();
	static gboolean CleanerCallback(void *data);
	MojErr CopyFile(MojServiceMessage *msg, const std::string &source,
	                const std::string &destination);
	std::string CallerID(MojServiceMessage *msg);

	CFileCacheSet *m_fileCacheSet;

	SubscriptionVec m_subscribers;
	static const Method s_Methods[];
	static MojLogger s_log;

	jvalue_ref categoryDescription;
	void InitCategoryDescription();
};

#endif /* __FILECACHE_CATEGORYHANDLER_H__ */

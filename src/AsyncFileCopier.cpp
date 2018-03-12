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

#include "AsyncFileCopier.h"
#include "FileCacheError.h"

#include <iostream>

using namespace std;

CAsyncCopier::CAsyncCopier(const std::string &sourcePath,
                           const std::string &destinationPath, MojServiceMessage *msg) :
	m_msg(msg)
	, m_destinationPath(destinationPath)
	, m_sourceFile(Gio::File::create_for_path(sourcePath))
	, m_destinationFile(Gio::File::create_for_path(destinationPath))
	, m_ready(sigc::mem_fun(*this, &CAsyncCopier::Ready))
{
}

void CAsyncCopier::StartCopy()
{
	m_sourceFile->copy_async(m_destinationFile, m_ready);
}


void CAsyncCopier::Ready(Glib::RefPtr<Gio::AsyncResult> &r)
{
	bool copyWorked = false;
	string what = "";
	MojObject reply;
	MojErr err = reply.putString(_T("newPathName"), m_destinationPath.c_str());

	try
	{
		m_sourceFile->copy_finish(r);
		copyWorked = true;
	}
	catch (const Gio::Error &err)
	{
		what = err.what();
	}
	catch (...)
	{
	}

	if (copyWorked)
	{
		err = m_msg->replySuccess(reply);
	}
	else
	{
		std::string msgText("Copy object to '");
		msgText += m_destinationPath.data();
		msgText += "' failed.";
		if (what.length() > 0)
		{
			msgText += "(";
			msgText += what;
			msgText += ")";
		}
		err = m_msg->replyError((MojErr) FCCopyObjectError, msgText.c_str());
	}
	delete this;
}

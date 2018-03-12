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

#ifndef __ASYNC_FILE_COPIER_H__
#define __ASYNC_FILE_COPIER_H__

#include <boost/noncopyable.hpp>
#include <giomm/file.h>
#include <giomm/asyncresult.h>
#include <string>

#include "core/MojService.h"
#include "luna/MojLunaMessage.h"

class CAsyncCopier : public boost::noncopyable
{
public:
	CAsyncCopier(const std::string &sourcePath, const std::string &destinationPath,
	             MojServiceMessage *msg);

	void StartCopy();
	void Ready(Glib::RefPtr<Gio::AsyncResult> &);

private:
	MojRefCountedPtr<MojServiceMessage> m_msg;
	std::string m_destinationPath;
	Glib::RefPtr<Gio::File> m_sourceFile;
	Glib::RefPtr<Gio::File> m_destinationFile;
	Gio::SlotAsyncReady m_ready;
};

#endif

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

#include "CategoryHandler.h"
#include "FileCacheError.h"
#include "AsyncFileCopier.h"
#include "CacheBase.h"

#include "sandbox.h"
#include "boost/filesystem.hpp"
#include <sstream>

namespace fs = boost::filesystem;

MojLogger CategoryHandler::s_log(_T("filecache.categoryhandler"));

void CategoryHandler::InitCategoryDescription()
{
	const std::string commonProperties = R"(
	    "typeName": {
	        "type": "string",
	        "maxLength": 64,
	        "pattern" : "^[^.]",
	        "description": "A typeName is a string up to 64 characters that uniquely identifies the new cache type. A typeName cannot start with a . ( the period character) "
	    },
	    "size": {
	        "type": "integer",
	        "minimum": 0,
	        "description": "Default value in bytes for any object inserted into the cache type that does not specify a value for size"
	    },
	    "cost": {
	        "type": "integer",
	        "minimum": 0,
	        "maximum": 100,
	        "description": "Default value between 0 and 100 for any object inserted into this cache type that does not specify a value for cost"
	    },
	    "lifetime": {
	        "type": "integer",
	        "minimum": 0,
	        "description": "Default value in seconds for any object inserted into this cache type that does not specify a value for lifetime"
	    }
	)";

	const std::string defineTypeDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "A DefineType ​method defines a new cache type",
	        "required": ["typeName", "loWatermark", "hiWatermark"],
	        "additionalProperties": false,
	        "properties": {
	            )" + commonProperties + R"(,
	            "dirType": {
	                "type": "boolean",
	                "description": "Specifies whether the cache type should create directory entries. This is intended for use by the backup service."
	            },
	            "loWatermark": {
	                "type": "integer",
	                "minimum": 0,
	                "exclusiveMinimum": true,
	                "description": "The minimum space in bytes guaranteed to be available for the cache type"
	            },
	            "hiWatermark": {
	                "type": "integer",
	                "description": "The maximum space in bytes allowed to be used by the cache type"
	            }
	        }
	    }}
	)";

	const std::string changeTypeDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The ChangeType method allows apps to modify the parameters of a cache type other than it’s name. You must specify the name of the cache type, and those cache type parameters that have to be modified. The cache type values are all as defined in DefineType",
	        "additionalProperties": false,
	        "required": ["typeName"],
	        "properties": {
	            )" + commonProperties + R"(,
	            "loWatermark": {
	                "type": "integer",
	                "minimum": 0,
	                "exclusiveMinimum": true,
	                "description": "The minimum space in bytes guaranteed to be available for the cache type"
	            },
	            "hiWatermark": {
	                "type": "integer",
	                "description": "The maximum space in bytes allowed to be used by the cache type"
	            }
	        }
	    }}
	)";

	const std::string deleteTypeDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The DeleteType method allows you to delete a previously defined cache type and frees up its space. All objects in the cache must be expired in order to delete a cache type. The app can call the ExpireCacheObject to force the objects to expire in the cache type.",
	        "additionalProperties": false,
	        "properties": {
	            "typeName": {
	                "type": "string",
	                "description": "The typeName is the name of the cache type to be deleted"
	            }
	        },
	        "required": ["typeName"]
	    }}
	)";

	const std::string copyCacheObjectDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The CopyCacheObject method enables copying of an object from the file cache to a non-cached location. On successful completion, newPathName will be returned as it may be different than expected due to filename collisions. If there is a name collision, the name will be made unique by adding a number to the file basename (i.e. foo.bar may become foo-(1).bar).",
	        "additionalProperties": false,
	        "properties": {
	            "pathName": {
	                "type": "string",
	                "description": "The pathName ​is the path of the cache object to be copie"
	            },
	            "destination": {
	                "type": "string",
	                "description": "The destination is the path to a target directory, this path will be validated to ensure you have write permissions to that director"
	            },
	            "fileName": {
	                "type": "string",
	                "description": "The fileName is the name for the target file. If not passed, the fileName will be the value passed when calling InsertCacheObject."
	            }
	        },
	        "required": ["pathName"]
	    }}
	)";

	const std::string describeTypeDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The DescribeType returns cache type details. The DescribeType will return the currently assigned values for all cache type parameters.",
	        "additionalProperties": false,
	        "properties": {
	            "typeName": {
	                "type": "string",
	                "description": "The typeName is the name of the cache type for which more information about the cache type is needed"
	            }
	        },
	        "required": ["typeName"]
	    }}
	)";

	const std::string insertCacheObjectDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The InsertCacheObject defines a new object in the specified cache type",
	        "additionalProperties": false,
	        "required": ["typeName", "fileName"],
	        "properties": {
	                )" + commonProperties + R"(,
	                "fileName": {
	                    "type": "string",
	                    "description": "The filename is stored with the object and is used to ensure the correct extension is provided on the cache object. The filename will help any code, for example,  one that determines a file viewer, to operate correctly on a cached object file."
	                },
	                "subscribe": {
	                    "type": "boolean",
	                    "description": "Subscribe should be set to true so that after the object is inserted one can continues to make updates to the file."
	                }
	        }
	    }}
	)";

	const std::string resizeCacheObjectDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The ResizeCache method tries to resize a cached object. It could be usefull when the size of the final object is not known, a best guess should be used with the InsertCacheObject call and then a ResizeCacheObject can be called on a subscribed object when the object size has been changed.",
	        "additionalProperties": false,
	        "properties": {
	            "pathName": {
	                "type": "string",
	                "description": "The path of the object to be resized."
	            },
	            "newSize": {
	                "type": "integer",
	                "minimum": 0,
	                "exclusiveMinimum": true,
	                "description": "The new size of the object in bytes."
	            }
	        },
	        "required": ["pathName", "newSize"]
	    }}
	)";

	const std::string expireCacheObjectDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The ExpireCacheObject method manually expires objects in a cache type.  In case of subscribed objects the ExpireCacheObject​ method will mark the object to be removed only when the subscription is complete.  Apps can use this method to expire objects in a cache type prior to calling the DeleteType method. ",
	        "additionalProperties": false,
	        "properties": {
	            "pathName": {
	                "type": "string",
	                "description": "Path of the object to be manually expired."
	            }
	        },
	        "required": ["pathName"]
	    }}
	)";

	const std::string subscribeCacheObjectDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The SubscribeCacheObject method enables you to subscribe an object in the cache and hold a subscription to the object for the duration of your usage. An object will not be expired from the cache while it is subscribed.",
	        "additionalProperties": true,
	        "properties": {
	            "pathName": {
	                "type": "string",
	                "description": "The path for the object to be subscribed."
	            }
	        },
	        "required": ["pathName"]
	    }}
	)";

	const std::string touchCacheObjectDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The TouchCacheObject method allows you to mark an object as recently used. This decreases the chances of the object from getting expired from the cache when the space is being reclaimed.",
	        "additionalProperties": false,
	        "properties": {
	            "pathName": {
	            "type": "string",
	                "description": "path of the object to be marked as touched."
	            }
	        },
	        "required": ["pathName"]
	    }}
	)";

	const std::string getCacheStatusDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The GetCacheStatus method will give you the status of the cache as a whole.",
	        "additionalProperties": false
	    }}
	)";

	const std::string getCacheTypeStatusDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The GetCacheTypeStatus method allows you to query the status of a particular cache type to obtain: the total space used by the objects in the specified cache type, he number of objects in the specified cache type",
	        "additionalProperties": false,
	        "properties": {
	            "typeName": {
	                "type": "string",
	                "description": "The name of the cache type for which the status information is to be retrieved."
	            }
	        },
	        "required": ["typeName"]
	    }}
	)";

	const std::string getCacheObjectSizeDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The GetCacheObjectSize method allows you to query a particular object size in a cache type. In order to get the cache object size, the app must specify the pathname of the object.",
	        "additionalProperties": false,
	        "properties": {
	            "pathName": {
	                "type": "string",
	                "description": "Path for the cache object to be queried for its size."
	            }
	        },
	        "required": ["pathName"]
	    }}
	)";

	const std::string getCacheObjectFilenameDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The GetCacheObjectFilename method allows you to query the original filename of a particular object in a cache type.  In order to get the cache object file name the app must specify the path name of the object.",
	        "additionalProperties": false,
	        "properties": {
	            "pathName": {
	                "type": "string",
	                "description": "Path of the object for which the filename is to be queried."
	            }
	    },
	        "required": ["pathName"]
	    }}
	)";

	const std::string getCacheTypesDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The GetCacheType method returns an array of all defined cache types.",
	        "additionalProperties": false
	    }}
	)";

	const std::string getVersionDescription = R"(
	    {"call": {
	        "type": "object",
	        "description": "The GetVersion method returns the version of the File Cache API.",
	        "additionalProperties": false
	    }}
	)";


	const std::string methodDescriptionString = std::string("{ \"methods\": {")
	        + "  \"DefineType\":" + defineTypeDescription
	        + ", \"ChangeType\":" + changeTypeDescription
	        + ", \"DeleteType\":" + deleteTypeDescription
	        + ", \"CopyCacheObject\":" + copyCacheObjectDescription
	        + ", \"DescribeType\":" + describeTypeDescription
	        + ", \"InsertCacheObject\":" + insertCacheObjectDescription
	        + ", \"ResizeCacheObject\":" + resizeCacheObjectDescription
	        + ", \"ExpireCacheObject\":" + expireCacheObjectDescription
	        + ", \"SubscribeCacheObject\":" + subscribeCacheObjectDescription
	        + ", \"TouchCacheObject\":" + touchCacheObjectDescription
	        + ", \"GetCacheStatus\":" + getCacheStatusDescription
	        + ", \"GetCacheTypeStatus\":" + getCacheTypeStatusDescription
	        + ", \"GetCacheObjectSize\":" + getCacheObjectSizeDescription
	        + ", \"GetCacheObjectFilename\":" + getCacheObjectFilenameDescription
	        + ", \"GetCacheTypes\":" + getCacheTypesDescription
	        + ", \"GetVersion\":" + getVersionDescription
	+ "}}";

	auto log_error = [](jerror *err, const char *errorSummary) {
		char errorDescr[1024];
		int written = jerror_to_string(err, errorDescr, sizeof(errorDescr));
		MojLogError(s_log, _T("%s. Details: %.*s"), errorSummary, written, errorDescr);
		jerror_free(err);
	};

	jerror *err = nullptr;
	categoryDescription = jdom_create(raw_buffer{methodDescriptionString.c_str(), methodDescriptionString.size()}
	                                    , jschema_all()
	                                    , &err);
	if (!jis_valid(categoryDescription)) {
		log_error(err, "Failed to parse a schema for methods");
		throw std::runtime_error("Failed to parse a schema for methods");
	}
}

const CategoryHandler::Method CategoryHandler::s_Methods[] =
{
	Method(_T("DefineType"), (Callback) &CategoryHandler::DefineType, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("ChangeType"), (Callback) &CategoryHandler::ChangeType, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("DeleteType"), (Callback) &CategoryHandler::DeleteType, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("CopyCacheObject"), (Callback) &CategoryHandler::CopyCacheObject, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("DescribeType"), (Callback) &CategoryHandler::DescribeType, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("InsertCacheObject"), (Callback) &CategoryHandler::InsertCacheObject, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("ResizeCacheObject"), (Callback) &CategoryHandler::ResizeCacheObject, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("ExpireCacheObject"), (Callback) &CategoryHandler::ExpireCacheObject, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("SubscribeCacheObject"), (Callback) &CategoryHandler::SubscribeCacheObject, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("TouchCacheObject"), (Callback) &CategoryHandler::TouchCacheObject, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("GetCacheStatus"), (Callback) &CategoryHandler::GetCacheStatus, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("GetCacheTypeStatus"), (Callback) &CategoryHandler::GetCacheTypeStatus, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("GetCacheObjectSize"), (Callback) &CategoryHandler::GetCacheObjectSize, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("GetCacheObjectFilename"), (Callback) &CategoryHandler::GetCacheObjectFilename, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("GetCacheTypes"), (Callback) &CategoryHandler::GetCacheTypes, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(_T("GetVersion"), (Callback) &CategoryHandler::GetVersion, LUNA_METHOD_FLAG_VALIDATE_IN),
	Method(NULL, NULL, 0)
};

CategoryHandler::CategoryHandler(CFileCacheSet *cacheSet)
	: m_fileCacheSet(cacheSet)
	, categoryDescription(nullptr)
{
	MojLogTrace(s_log);

	InitCategoryDescription();

	SetupWorkerTimer();
}

CategoryHandler::~CategoryHandler()
{
	MojLogTrace(s_log);

	j_release(&categoryDescription);
}

const jvalue_ref CategoryHandler::GetMethodsDescription() const
{
	return categoryDescription;
}

MojErr
CategoryHandler::RegisterMethods()
{

	MojLogTrace(s_log);

	MojErr err = addMethods(s_Methods);
	MojErrCheck(err);
	MojLogDebug(s_log, _T("RegisterMethods: Registered all service methods."));

	return MojErrNone;
}

MojErr
CategoryHandler::DefineType(MojServiceMessage *msg, MojObject &payload)
{
	MojLogTrace(s_log);

	MojString typeName;
	MojInt64 loWatermark = 0;
	MojInt64 hiWatermark = 0;
	MojInt64 size = 0;
	MojInt64 cost = 0;
	MojInt64 lifetime = 0;
	bool dirType = false;

	payload.getRequired(_T("typeName"), typeName);
	payload.get(_T("loWatermark"), loWatermark);
	payload.get(_T("hiWatermark"), hiWatermark);
	payload.get(_T("size"), size);
	payload.get(_T("cost"), cost);
	payload.get(_T("lifetime"), lifetime);
	payload.get(_T("dirType"), dirType);

	MojLogDebug(s_log, _T("DefineType: new type '%s' to be defined."), typeName.data());

	MojErr err = MojErrNone;
	std::string msgText;
	if (hiWatermark <= loWatermark)
	{
		msgText = "DefineType: Invalid params: hiWatermark must be greater than loWatermark.";
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}
	if (!msgText.empty())
	{
		err = msg->replyError((MojErr) FCInvalidParams, msgText.c_str());
		MojErrCheck(err);
		return MojErrNone;
	}

	MojLogDebug(s_log,
	            _T("DefineType: params: loWatermark = '%lld', hiWatermark = '%lld',"),
	            loWatermark, hiWatermark);
	MojLogDebug(s_log,
	            _T("DefineType: params: size = '%lld', cost = '%lld', lifetime = '%lld'."),
	            size, cost, lifetime);

	CCacheParamValues params((cacheSize_t) loWatermark,
	                         (cacheSize_t) hiWatermark,
	                         (cacheSize_t) size, (paramValue_t) cost,
	                         (paramValue_t) lifetime);

	if (m_fileCacheSet->TypeExists(std::string(typeName.data())))
	{
		msgText = "DefineType: Type '";
		msgText += typeName.data();
		msgText += "' ";
#ifdef NEEDS_CONFIGURATOR_FIX
		CCacheParamValues curParams =
			m_fileCacheSet->DescribeType(std::string(typeName.data()));
		if (params != curParams)
		{
			MojLogError(s_log,
			            _T("DefineType: cur params: loWatermark = '%lld', hiWatermark = '%lld',"),
			            (long long int) curParams.GetLoWatermark(),
			            (long long int) curParams.GetHiWatermark());
			MojLogError(s_log,
			            _T("DefineType: cur params: size = '%lld', cost = '%lld', lifetime = '%lld'."),
			            (long long int) curParams.GetSize(),
			            (long long int) curParams.GetCost(),
			            (long long int) curParams.GetLifetime());
			MojLogError(s_log,
			            _T("DefineType: new params: loWatermark = '%lld', hiWatermark = '%lld',"),
			            (long long int) params.GetLoWatermark(),
			            (long long int) params.GetHiWatermark());
			MojLogError(s_log,
			            _T("DefineType: new params: size = '%lld', cost = '%lld', lifetime = '%lld'."),
			            (long long int) params.GetSize(),
			            (long long int) params.GetCost(),
			            (long long int) params.GetLifetime());
			msgText += "has different configuration.";
			err = msg->replyError((MojErr) FCConfigurationError, msgText.c_str());
		}
		else
		{
#endif
			msgText += "already exists.";
			err = msg->replyError((MojErr) FCExistsError, msgText.c_str());
#ifdef NEEDS_CONFIGURATOR_FIX
		}
#endif
	}
	else
	{
		if (m_fileCacheSet->DefineType(msgText, std::string(typeName.data()),
		                               &params, dirType))
		{
			err = msg->replySuccess();
		}
		else
		{
			err = msg->replyError((MojErr) FCDefineError, msgText.c_str());
		}
	}

	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::ChangeType(MojServiceMessage *msg, MojObject &payload)
{

	MojLogTrace(s_log);

	MojString typeName;
	MojInt64 loWatermark = 0;
	MojInt64 hiWatermark = 0;
	MojInt64 size = 0;
	MojInt64 cost = 0;
	MojInt64 lifetime = 0;
	MojErr err = MojErrNone;

	payload.getRequired(_T("typeName"), typeName);
	payload.get(_T("loWatermark"), loWatermark);
	payload.get(_T("hiWatermark"), hiWatermark);
	payload.get(_T("size"), size);
	payload.get(_T("cost"), cost);
	payload.get(_T("lifetime"), lifetime);

	MojLogDebug(s_log, _T("ChangeType: existing type '%s' to be changed."), typeName.data());

	std::string msgText;
	if ((hiWatermark != 0) && (hiWatermark <= loWatermark))
	{
		msgText = "ChangeType: Invalid params: hiWatermark must be greater than loWatermark.";
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}

	if (!msgText.empty())
	{
		err = msg->replyError((MojErr) FCInvalidParams, msgText.c_str());
		MojErrCheck(err);
		return MojErrNone;
	}

	MojLogDebug(s_log,
	            _T("ChangeType: params: loWatermark = '%lld', hiWatermark = '%lld',"),
	            loWatermark, hiWatermark);
	MojLogDebug(s_log,
	            _T("ChangeType: params: size = '%lld', cost = '%lld', lifetime = '%lld'."),
	            size, cost, lifetime);

	CCacheParamValues params((cacheSize_t) loWatermark,
	                         (cacheSize_t) hiWatermark,
	                         (cacheSize_t) size, (paramValue_t) cost,
	                         (paramValue_t) lifetime);

	if (m_fileCacheSet->ChangeType(msgText, std::string(typeName.data()),
	                               &params))
	{
		err = msg->replySuccess();
	}
	else
	{
		err = msg->replyError((MojErr) FCChangeError, msgText.c_str());
	}

	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::DeleteType(MojServiceMessage *msg, MojObject &payload)
{
	MojLogTrace(s_log);

	MojString typeName;
	MojInt64 freedSpace = 0;
	MojErr err = MojErrNone;

	payload.getRequired(_T("typeName"), typeName);

	MojLogDebug(s_log, _T("DeleteType: existing type '%s' to be deleted."), typeName.data());

	std::string msgText;
	freedSpace = m_fileCacheSet->DeleteType(msgText, std::string(typeName.data()));

	if (freedSpace >= 0)
	{
		MojLogDebug(s_log, _T("DeleteType: deleting type '%s' freed '%lld' bytes."),
		            typeName.data(), freedSpace);
		MojObject reply;
		err = reply.putInt(_T("freedSpace"), freedSpace);
		MojErrCheck(err);
		err = msg->replySuccess(reply);
	}
	else
	{
		err = msg->replyError((MojErr) FCDeleteError, msgText.c_str());
	}
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::DescribeType(MojServiceMessage *msg,
                              MojObject &payload)
{

	MojLogTrace(s_log);

	MojString typeName;
	MojErr err = MojErrNone;

	payload.getRequired(_T("typeName"), typeName);

	MojLogDebug(s_log, _T("DescribeType: existing type '%s' to be queried."), typeName.data());

	if (m_fileCacheSet->TypeExists(std::string(typeName.data())))
	{
		CCacheParamValues params =
		    m_fileCacheSet->DescribeType(std::string(typeName.data()));

		MojLogDebug(s_log,
		            _T("DescribeType: params: loWatermark = '%d', hiWatermark = '%d',"),
		            params.GetLoWatermark(), params.GetHiWatermark());
		MojLogDebug(s_log,
		            _T("DescribeType: params: size = '%d', cost = '%d', lifetime = '%d'."),
		            params.GetSize(), params.GetCost(), params.GetLifetime());

		MojObject reply;
		err = reply.putInt(_T("loWatermark"), (MojInt64) params.GetLoWatermark());
		MojErrCheck(err);
		err = reply.putInt(_T("hiWatermark"), (MojInt64) params.GetHiWatermark());
		MojErrCheck(err);
		err = reply.putInt(_T("size"), (MojInt64) params.GetSize());
		MojErrCheck(err);
		err = reply.putInt(_T("cost"), (MojInt64) params.GetCost());
		MojErrCheck(err);
		err = reply.putInt(_T("lifetime"), (MojInt64) params.GetLifetime());
		MojErrCheck(err);
		err = msg->replySuccess(reply);
	}
	else
	{
		std::string msgText("DescribeType: Type '");
		msgText += typeName.data();
		msgText += "' does not exists.";
		err = msg->replyError((MojErr) FCExistsError, msgText.c_str());
	}

	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::InsertCacheObject(MojServiceMessage *msg,
                                   MojObject &payload)
{
	MojLogTrace(s_log);

	MojString typeName, fileName;
	MojInt64 size = 0;
	MojInt64 cost = 0;
	MojInt64 lifetime = 0;
	bool subscribed = false;
	MojErr err = MojErrNone;

	payload.getRequired(_T("typeName"), typeName);
	payload.getRequired(_T("fileName"), fileName);

	MojLogDebug(s_log,
	            _T("InsertCacheObject: inserting object into type '%s' for file '%s',"),
	            typeName.data(), fileName.data());

	std::string msgText;
	if (!m_fileCacheSet->TypeExists(std::string(typeName.data())))
	{
		msgText = "InsertCacheObject: No type '" + std::string(typeName.data()) + "' defined.";
	}

	CCacheParamValues params =
		m_fileCacheSet->DescribeType(std::string(typeName.data()));

	payload.get(_T("subscribe"), subscribed);

	if (!payload.get(_T("size"), size))
		size = params.GetSize();

	if (!payload.get(_T("cost"), cost))
		cost = params.GetCost();

	if (!payload.get(_T("lifetime"), lifetime))
		lifetime = params.GetLifetime();

	if ((size <= GetFilesystemFileSize(1)) &&
			 m_fileCacheSet->isTypeDirType(typeName.data()))
	{
		msgText = "InsertCacheObject: Invalid params: size must be greater than 1 block when dirType = true.";
	}

	MojLogDebug(s_log,
	            _T("InsertCacheObject: params: size = '%lld', cost = '%lld', lifetime = '%lld'."),
	            size, cost, lifetime);

	if (!msgText.empty())
	{
		MojLogError(s_log, _T("%s"), msgText.c_str());
		err = msg->replyError((MojErr) FCInvalidParams, msgText.c_str());
		MojErrCheck(err);
		return MojErrNone;
	}

	cachedObjectId_t objId =
	    m_fileCacheSet->InsertCacheObject(msgText, std::string(typeName.data()),
	                                      std::string(fileName.data()),
	                                      (cacheSize_t) size,
	                                      (paramValue_t) cost,
	                                      (paramValue_t) lifetime);

	MojLogDebug(s_log, _T("InsertCacheObject: new object id = %llu."), objId);
	if (objId > 0)
	{
		MojString pathName;
		MojObject reply;
		if (subscribed)
		{
			const std::string fpath(m_fileCacheSet->SubscribeCacheObject(msgText, objId));
			if (!fpath.empty())
			{
				err = pathName.assign(fpath.c_str());
				MojErrCheck(err);
				MojRefCountedPtr<Subscription> cancelHandler(new Subscription(*this,
						msg,
						pathName));
				MojAllocCheck(cancelHandler.get());
				m_subscribers.push_back(cancelHandler.get());
				MojLogDebug(s_log, _T("InsertCacheObject: subscribed new object '%s'."),
							fpath.c_str());
				err = reply.putBool(_T("subscribed"), true);
				MojErrCheck(err);
			}
			else if (!msgText.empty())
			{
				msgText = "SubscribeCacheObject: " + msgText;
				MojLogError(s_log, _T("%s"), msgText.c_str());
			}
		}
		else
		{
			const std::string dirBase(m_fileCacheSet->GetBaseDirName());
			err = pathName.assign(BuildPathname(objId, dirBase,
			                                    std::string(typeName.data()),
			                                    std::string(fileName.data())).c_str());
			MojErrCheck(err);
		}
		err = reply.putString(_T("pathName"), pathName);
		MojErrCheck(err);

		err = msg->replySuccess(reply);
	}
	else
	{
		err = msg->replyError((MojErr) FCExistsError, msgText.c_str());
	}

	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::ResizeCacheObject(MojServiceMessage *msg,
                                   MojObject &payload)
{
	MojLogTrace(s_log);

	MojString pathName;
	MojInt64 newSize;
	MojErr err = MojErrNone;
	std::string msgText;

	payload.getRequired(_T("pathName"), pathName);
	payload.getRequired(_T("newSize"), newSize);

	MojLogDebug(s_log, _T("ResizeCacheObject: resizing file '%s' to '%lld'."),
	            pathName.data(), newSize);

	cacheSize_t size = -1;
	const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
	MojLogDebug(s_log,
	            _T("ResizeCacheObject: file '%s' produced object id '%llu'."),
	            pathName.data(), objId);
	FCErr errCode = FCErrorNone;
	if (objId > 0)
	{
		if (GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
		                        pathName.data()) ==
		    m_fileCacheSet->GetTypeForObjectId(objId))
		{
			size = m_fileCacheSet->Resize(objId, (cacheSize_t) newSize);
			MojLogDebug(s_log, _T("ResizeCacheObject: final size is '%d'."), size);

			if (size == (cacheSize_t) newSize)
			{
				MojObject reply;
				err = reply.putInt(_T("newSize"), (MojInt64) size);
				MojErrCheck(err);
				err = msg->replySuccess(reply);
				MojErrCheck(err);
			}
			else
			{
				msgText = "ResizeCacheObject: Unable to resize object.";
				errCode = FCResizeError;
			}
		}
		else
		{
			msgText = "ResizeCacheObject: pathName no longer found in cache.";
			errCode = FCExistsError;
			MojLogError(s_log, _T("%s"), msgText.c_str());
		}
	}
	else
	{
		msgText = "ResizeCacheObject: Invalid object id derived from pathname.";
		errCode = FCExistsError;
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}

	if (!msgText.empty())
	{
		err = msg->replyError((MojErr) errCode, msgText.c_str());
	}

	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::ExpireCacheObject(MojServiceMessage *msg,
                                   MojObject &payload)
{

	MojLogTrace(s_log);

	MojString pathName;
	std::string msgText;
	MojErr err = MojErrNone;

	payload.getRequired(_T("pathName"), pathName);

	MojLogDebug(s_log, _T("ExpireCacheObject: expiring object '%s'."), pathName.data());

	FCErr errCode = FCErrorNone;
	const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
	if (objId > 0)
	{
		if (GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
		                        pathName.data()) ==
		        m_fileCacheSet->GetTypeForObjectId(objId))
		{
			if (m_fileCacheSet->ExpireCacheObject(objId))
			{
				MojLogWarning(s_log,
				              _T("ExpireCacheObject: Object '%s' expired by user '%s'."),
				              pathName.data(), (CallerID(msg)).c_str());
			}
			else
			{
				msgText = "ExpireCacheObject: Expire deferred, object in use.";
				errCode = FCInUseError;
			}
		}
		else
		{
			MojLogError(s_log,
			            _T("GetTypeFromPath = %s, GetTypeForObjectId = %s, objId = %llu"),
			            GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
			                                pathName.data()).c_str(),
			            m_fileCacheSet->GetTypeForObjectId(objId).c_str(), objId);

			msgText = "ExpireCacheObject: pathName no longer found in cache.";
			MojLogError(s_log, _T("%s"), msgText.c_str());
			if (GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
			                        pathName.data()).empty())
			{
				errCode = FCExistsError;
			}
			else
			{
				msgText.clear();
			}
		}
	}
	else
	{
		msgText = "ExpireCacheObject: Invalid object id derived from pathname.";
		errCode = FCExistsError;
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}

	if (!msgText.empty())
	{
		err = msg->replyError((MojErr) errCode, msgText.c_str());
	}
	else
	{
		err = msg->replySuccess();
	}
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::SubscribeCacheObject(MojServiceMessage *msg,
                                      MojObject &payload)
{

	MojLogTrace(s_log);

	MojErr err = MojErrNone;
	std::string errorText;

	do
	{
		MojString pathName;
		payload.getRequired(_T("pathName"), pathName);

		MojLogDebug(s_log, _T("SubscribeCacheObject: subscribing to file '%s'."),
		            pathName.data());

		const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
		if (objId == 0)
		{
			err = (MojErr)FCExistsError;
			errorText = "Invalid object id derived from pathname.";
			break;
		}

		const std::string type = GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
		                         pathName.data());
		if (type != m_fileCacheSet->GetTypeForObjectId(objId))
		{
			err = (MojErr)FCExistsError;
			errorText = std::string("'pathName': ") + pathName.data() +
			            " no longer found in cache.";
			break;
		}

		MojObject reply;
		const std::string fpath(m_fileCacheSet->SubscribeCacheObject(errorText, objId));
		if (fpath.empty() || !errorText.empty())
		{
			err = (MojErr)FCExistsError;
			errorText = !errorText.empty() ? errorText :
			            "Could not find object to match derived id.";
			break;
		}

		MojRefCountedPtr<Subscription> cancelHandler(new Subscription(*this, msg,
		        pathName));
		MojAllocCheck(cancelHandler.get());
		m_subscribers.push_back(cancelHandler.get());
		MojLogDebug(s_log, _T("SubscribeCacheObject: subscribed object '%s'."),
		            fpath.c_str());

		reply.putBool(_T("subscribed"), true);
		err = msg->replySuccess(reply);

	}
	while (false);

	if (!errorText.empty())
	{
		MojLogError(s_log, _T("%s"), errorText.c_str());
		err = msg->replyError(err, errorText.c_str());
	}

	MojErrCheck(err);
	return MojErrNone;
}

MojErr
CategoryHandler::CancelSubscription(Subscription *sub,
                                    MojServiceMessage *msg,
                                    MojString &pathName)
{

	MojLogTrace(s_log);

	const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
	if (objId > 0)
	{
		const std::string typeName(GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
		                           pathName.data()));
		if (!typeName.empty())
		{
			m_fileCacheSet->UnSubscribeCacheObject(typeName, objId);
		}
		else
		{
			MojLogError(s_log,
			            _T("CancelSubscription: pathName no longer found in cache."));
		}
	}
	for (SubscriptionVec::iterator it = m_subscribers.begin();
	        it != m_subscribers.end(); ++it)
	{
		if (it->get() == sub)
		{
			m_subscribers.erase(it);
			MojLogInfo(s_log,
			           _T("CancelSubscription: Removed subscription on pathName '%s'."),
			           pathName.data());
			break;
		}
	}

	return MojErrNone;
}

MojErr
CategoryHandler::TouchCacheObject(MojServiceMessage *msg,
                                  MojObject &payload)
{

	MojLogTrace(s_log);

	MojString pathName;
	MojErr err = MojErrNone;

	payload.getRequired(_T("pathName"), pathName);

	MojLogDebug(s_log, _T("TouchCacheObject: touching file '%s'."),
	            pathName.data());

	std::string msgText;
	const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
	if (objId > 0)
	{
		if (GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
		                        pathName.data()) ==
		        m_fileCacheSet->GetTypeForObjectId(objId))
		{
			if (m_fileCacheSet->Touch(objId))
			{
				err = msg->replySuccess();
			}
			else
			{
				msgText = "TouchCacheObject: Could not locate object";
			}
		}
		else
		{
			msgText = "TouchCacheObject: pathName no longer found in cache.";
			MojLogError(s_log, _T("%s"), msgText.c_str());
		}
	}
	else
	{
		msgText = "TouchCacheObject: Invalid object id derived from pathname.";
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}

	if (!msgText.empty())
	{
		err = msg->replyError((MojErr) FCExistsError, msgText.c_str());
	}
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::CopyCacheObject(MojServiceMessage *msg,
                                 MojObject &payload)
{

	MojLogTrace(s_log);

	MojString pathName, param;
	std::string destination, fileName;
	MojErr err = MojErrNone;
	bool found = false;

	payload.getRequired(_T("pathName"), pathName);
	payload.get(_T("destination"), param, found);
	if (found && !param.empty())
	{
		destination = param.data();
	}
	else
	{
		destination = s_defaultDownloadDir;
	}

	payload.get(_T("fileName"), param, found);

	MojLogDebug(s_log, _T("CopyCacheObject: attempting to copy file '%s'."),
	            pathName.data());

	std::string msgText;
	MojErr errCode = MojErrNone;
	const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
	if (objId > 0)
	{
		if (GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
		                        pathName.data()) ==
		        m_fileCacheSet->GetTypeForObjectId(objId))
		{
			if (m_fileCacheSet->CachedObjectSize(objId) < 0)
			{
				msgText = "CopyCacheObject: Could not locate object";
				errCode = (MojErr) FCExistsError;
				MojLogError(s_log, _T("%s"), msgText.c_str());
			}
			else
			{
				if (found && !param.empty())
				{
					fileName = param.data();
				}
				else
				{
					fileName = m_fileCacheSet->CachedObjectFilename(objId);
					if (fileName.empty())
					{
						msgText = "CopyCacheObject: No fileName specified or found.";
						errCode = (MojErr) FCArgumentError;
						MojLogError(s_log, _T("%s"), msgText.c_str());
					}
				}
			}
		}
		else
		{
			msgText = "CopyCacheObject: pathName no longer found in cache.";
			errCode = (MojErr) FCExistsError;
			MojLogError(s_log, _T("%s"), msgText.c_str());
		}
	}
	else
	{
		msgText = "CopyCacheObject: Invalid object id derived from pathname.";
		errCode = (MojErr) FCExistsError;
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}

	std::string destFileName;
	if (!SBIsPathAllowed(destination.c_str(), msg->senderName(),
	                     SB_WRITE | SB_CREATE))
	{
		msgText = "CopyCacheObject: Invalid destination, no write permission.";
		errCode = (MojErr) FCPermError;
		MojLogError(s_log, _T("%s"), msgText.c_str());
	}
	else
	{
		try
		{
			fs::path filepath(destination);
			if (!fs::exists(filepath))
			{
				fs::create_directories(filepath);
			}
			if (fs::is_directory(filepath))
			{
				int i = 1;
				std::string extension(GetFileExtension(fileName.c_str()));
				std::string basename(GetFileBasename(fileName.c_str()));
				while (fs::exists(filepath / fileName) &&
				        (i < s_maxUniqueFileIndex))
				{
					std::stringstream newFileName;
					newFileName << basename << "-(" << i++ << ")" << extension;
					fileName = newFileName.str();
				}
				if (i == s_maxUniqueFileIndex)
				{
					msgText = "CopyCacheObject: No unique destination name found.";
					errCode = (MojErr) FCArgumentError;
					MojLogError(s_log, _T("%s"), msgText.c_str());
				}
				else
				{
					destFileName = filepath.string() + "/" + fileName;
				}
			}
			else
			{
				msgText = "CopyCacheObject: Invalid destination, not a directory.";
				errCode = (MojErr) FCArgumentError;
				MojLogError(s_log, _T("%s"), msgText.c_str());
			}
		}
		catch (const fs::filesystem_error &ex)
		{
			if (ex.code().value() != 0)
			{
				msgText = "CopyCacheObject: ";
				msgText += ex.what();
				msgText += " (" + ex.code().message() + ").";
				MojLogError(s_log, _T("%s"), msgText.c_str());
				errCode = (MojErr) FCDirectoryError;
			}
		}
	}

	if (!msgText.empty())
	{
		err = msg->replyError(errCode, msgText.c_str());
	}
	else
	{
		err = CopyFile(msg, pathName.data(), destFileName);
	}
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::GetCacheStatus(MojServiceMessage *msg,
                                MojObject &payload)
{

	MojLogTrace(s_log);

	cacheSize_t numTypes = 0;
	cacheSize_t size = 0;
	cacheSize_t space = 0;
	paramValue_t numObjs = 0;

	numTypes = m_fileCacheSet->GetCacheStatus(&size, &numObjs, &space);

	MojObject reply;
	MojErr err = reply.putInt(_T("numTypes"), (MojInt64) numTypes);
	MojErrCheck(err);
	err = reply.putInt(_T("size"), (MojInt64) size);
	MojErrCheck(err);
	err = reply.putInt(_T("numObjs"), (MojInt64) numObjs);
	MojErrCheck(err);
	err = reply.putInt(_T("availSpace"), (MojInt64) space);
	MojErrCheck(err);
	MojLogDebug(s_log,
	            _T("GetCacheStatus: numTypes = '%d', size = '%d', numObjs = '%d', availSpace = '%d'."),
	            numTypes, size, numObjs, space);

	err = msg->replySuccess(reply);
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::GetCacheTypeStatus(MojServiceMessage *msg,
                                    MojObject &payload)
{

	MojLogTrace(s_log);

	cacheSize_t size = 0;
	paramValue_t numObjs = 0;

	MojString typeName;

	MojErr err = MojErrNone;
	payload.getRequired(_T("typeName"), typeName);

	MojLogDebug(s_log, _T("GetCacheTypeStatus: getting status for type '%s'."),
	            typeName.data());
	bool suceeded =
	    m_fileCacheSet->GetCacheTypeStatus(std::string(typeName.data()),
	                                       &size, &numObjs);
	MojObject reply;
	if (suceeded)
	{
		err = reply.putInt(_T("size"), (MojInt64) size);
		MojErrCheck(err);
		err = reply.putInt(_T("numObjs"), (MojInt64) numObjs);
		MojErrCheck(err);
		MojLogDebug(s_log, _T("GetCacheTypeStatus: size = '%d', numObjs = '%d'."),
		            size, numObjs);
		err = msg->replySuccess(reply);
	}
	else
	{
		std::string msgText("GetCacheTypeStatus: Type '");
		msgText += typeName.data();
		msgText += "' doesn't exist";
		MojLogInfo(s_log, _T("%s"), msgText.c_str());
		err = msg->replyError((MojErr) FCExistsError, msgText.c_str());
	}

	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::GetCacheObjectSize(MojServiceMessage *msg,
                                    MojObject &payload)
{

	MojLogTrace(s_log);

	MojString pathName;

	MojErr err = MojErrNone;
	payload.getRequired(_T("pathName"), pathName);
	MojLogDebug(s_log, _T("GetCacheObjectSize: getting size for '%s'."),
	            pathName.data());

	MojObject reply;
	const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
	cacheSize_t objSize = 0;
	if ((objId > 0) && ((objSize = m_fileCacheSet->CachedObjectSize(objId)) >= 0))
	{
		err = reply.putInt(_T("size"), (MojInt64) objSize);
		MojErrCheck(err);
		MojLogDebug(s_log, _T("GetCacheObjectSize: found size '%d'."), objSize);
		err = msg->replySuccess(reply);
	}
	else
	{
		std::string msgText("GetCacheObjectSize: Object '");
		msgText += pathName.data();
		msgText += "' doesn't exist";
		MojLogInfo(s_log, _T("%s"), msgText.c_str());
		err = msg->replyError((MojErr) FCExistsError, msgText.c_str());
	}
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::GetCacheObjectFilename(MojServiceMessage *msg,
                                        MojObject &payload)
{

	MojLogTrace(s_log);

	MojString pathName;

	MojErr err = MojErrNone;
	payload.getRequired(_T("pathName"), pathName);
	MojLogDebug(s_log, _T("GetCacheObjectFilename: getting filename for '%s'."),
	            pathName.data());

	MojObject reply;
	const cachedObjectId_t objId = GetObjectIdFromPath(pathName.data());
	if (objId > 0)
	{
		std::string filename = m_fileCacheSet->CachedObjectFilename(objId);
		err = reply.putString(_T("fileName"), filename.c_str());
		MojErrCheck(err);
		MojLogDebug(s_log, _T("GetCacheObjectFilename: found filename '%s'."),
		            filename.c_str());
		err = msg->replySuccess(reply);
	}
	else
	{
		std::string msgText("GetCacheObjectFilename: Object '");
		msgText += pathName.data();
		msgText += "' doesn't exist";
		MojLogInfo(s_log, _T("%s"), msgText.c_str());
		err = msg->replyError((MojErr) FCExistsError, msgText.c_str());
	}
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::GetCacheTypes(MojServiceMessage *msg,
                               MojObject &payload)
{

	MojLogTrace(s_log);

	MojErr err = MojErrNone;

	MojObject reply;

	const std::vector<std::string> cacheTypes = m_fileCacheSet->GetTypes();
	if (!cacheTypes.empty())
	{
		MojObject typeArray;
		std::vector<std::string>::const_iterator iter = cacheTypes.begin();
		while (iter != cacheTypes.end())
		{
			err = typeArray.pushString((*iter).c_str());
			MojErrCheck(err);
			++iter;
		}
		err = reply.put(_T("types"), typeArray);
		MojErrCheck(err);
		MojLogDebug(s_log, _T("GetCacheTypes: found '%zd' types."),
		            cacheTypes.size());
	}
	err = msg->replySuccess(reply);
	MojErrCheck(err);

	return MojErrNone;
}

MojErr
CategoryHandler::GetVersion(MojServiceMessage *msg,
                            MojObject &payload)
{

	MojLogTrace(s_log);

	MojObject reply;

	MojErr err = reply.putString(_T("version"), s_InterfaceVersion.c_str());
	MojErrCheck(err);
	err = msg->replySuccess(reply);
	MojErrCheck(err);

	return MojErrNone;

}

MojErr
CategoryHandler::WorkerHandler()
{

	MojLogTrace(s_log);

	MojLogDebug(s_log, _T("WorkerHandler: Attempting to cleanup any orphans."));
	m_fileCacheSet->CleanupOrphans();

	// For each subscribed object, if it's still being written, do a
	// validity check
	for (SubscriptionVec::const_iterator it = m_subscribers.begin();
	        it != m_subscribers.end(); ++it)
	{
		MojLogDebug(s_log, _T("WorkerHandler: Validating subscribed object '%s'."),
		            (*it)->GetPathName().data());
		const cachedObjectId_t objId =
		    GetObjectIdFromPath((*it)->GetPathName().data());
		const std::string typeName(GetTypeNameFromPath(m_fileCacheSet->GetBaseDirName(),
		                           (*it)->GetPathName().data()));
		m_fileCacheSet->CheckSubscribedObject(typeName, objId);
	}

	return MojErrNone;
}

MojErr
CategoryHandler::CleanerHandler()
{

	MojLogTrace(s_log);

	MojLogDebug(s_log, _T("CleanerHandler: Attempting to cleanup dirTypes."));
	m_fileCacheSet->CleanupDirTypes();

	return MojErrNone;
}

MojErr
CategoryHandler::SetupWorkerTimer()
{

	MojLogTrace(s_log);

	g_timeout_add_seconds(15, &TimerCallback, this);
	g_timeout_add_seconds(120, &CleanerCallback, this);

	return MojErrNone;
}

gboolean
CategoryHandler::TimerCallback(void *data)
{

	MojLogTrace(s_log);

	CategoryHandler *self = static_cast<CategoryHandler *>(data);
	self->WorkerHandler();

	return true;
}

gboolean
CategoryHandler::CleanerCallback(void *data)
{

	MojLogTrace(s_log);

	CategoryHandler *self = static_cast<CategoryHandler *>(data);
	self->CleanerHandler();

	// return false here as this is a one shot
	return false;
}

CategoryHandler::Subscription::Subscription(CategoryHandler &handler,
        MojServiceMessage *msg,
        MojString &pathName)
	: m_handler(handler),
	  m_msg(msg),
	  m_pathName(pathName),
	  m_cancelSlot(this, &Subscription::HandleCancel)
{

	MojLogTrace(s_log);

	msg->notifyCancel(m_cancelSlot);
}

CategoryHandler::Subscription::~Subscription()
{

	MojLogTrace(s_log);
}

MojErr
CategoryHandler::Subscription::HandleCancel(MojServiceMessage *msg)
{

	MojLogTrace(s_log);

	return m_handler.CancelSubscription(this, msg, m_pathName);
}

MojErr
CategoryHandler::CopyFile(MojServiceMessage *msg,
                          const std::string &source,
                          const std::string &destination)
{

	MojLogTrace(s_log);

	MojErr err = MojErrNone;

	CAsyncCopier *c = new CAsyncCopier(source, destination, msg);
	c->StartCopy();

	return err;
}

std::string
CategoryHandler::CallerID(MojServiceMessage *msg)
{

	MojLogTrace(s_log);

	std::string caller;
	MojLunaMessage *lunaMsg = dynamic_cast<MojLunaMessage *>(msg);
	if (lunaMsg)
	{
		const char *appId = lunaMsg->appId();
		if (appId)
		{
			caller = appId;
			size_t firstSpace = caller.find_first_of(' ');
			if (firstSpace != std::string::npos)
			{
				caller.resize(firstSpace);
			}
		}
		else
		{
			const char *serviceId = lunaMsg->senderId();
			if (serviceId)
			{
				caller = serviceId;
			}
		}
	}

	return caller;
}

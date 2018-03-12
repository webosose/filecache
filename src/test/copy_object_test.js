// LICENSE@@@
//
//      Copyright (c) 2012-2013 LG Electronics, Inc.
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
// @@@LICENSE

var testHandle;
function printO (label, o) {
	console.log(label, ": ", JSON.stringify(o, undefined, 2));
}

include("mojoloader.js");

var libs;
libs = MojoLoader.require(
	{name: "underscore", version: "1.0"});

_ = libs.underscore._;


function CopyObjectTests() {
}

CopyObjectTests.prototype = {
	before: function(cb) {
		if (!testHandle) {
			testHandle = new webOS.Handle("com.palm.filecachetest", false);
			Foundations.Comms.PalmCall.register(testHandle);			
		}
		cb();
	},
	
	testFileCacheOnBus: function(recordResults) {
		var uri = 'palm://com.palm.lunabus/signal';
		var cmd = 'registerServerStatus';
		var args = {"serviceName":"com.palm.filecache"};
		Foundations.Comms.PalmCall.call(uri, cmd, args).then(function(future) {
			var response = future.result;
			if (response.connected && response.serviceName) {
				future.result=Test.passed;
			} else {
				future.result=Test.failed;
			}
		}).then(function(future) {
			recordResults(future);
		});
	},
	
	testCopyNonExistantObject: function(recordResults) {
		var uri = 'palm://com.palm.filecache';
		var cmd = 'CopyCacheObject';
		var args = {
			destination: "/tmp/",
			pathName: "foo"
		};
		Foundations.Comms.PalmCall.call(uri, cmd, args).then(function(future) {
			try {
				future.result=Test.failed;
			} catch (e) {
				if (e.errorCode === -199) {
					future.result=Test.passed;					
				} else {
					future.result=Test.failed;					
				}
			}
		}).then(function(future) {
			recordResults(future);
		});
	},
	
	deleteFileCacheKind: function(future) {
		var uri = 'palm://com.palm.filecache';
		var cmd = 'DeleteType';
		var args = {
		    typeName: "cacheTestType"
		};
		var f = Foundations.Comms.PalmCall.call(uri, cmd, args);
		future.nest(f);
	},

	createFileCacheKind: function(future) {
		var uri = 'palm://com.palm.filecache';
		var cmd = 'DefineType';
		var args = {
		    typeName: "cacheTestType",
		    loWatermark: 102400,
		    hiWatermark: 409600
		};
		var f = Foundations.Comms.PalmCall.call(uri, cmd, args);
		future.nest(f);
	},
	
	createCacheObject: function(fileName, future) {
		var uri = 'palm://com.palm.filecache';
		var cmd = 'InsertCacheObject';
		var args = {
		    typeName: "cacheTestType",
			fileName: fileName,
			subscribe: true,
		    size: 4096
		};
		var f = Foundations.Comms.PalmCall.call(uri, cmd, args);
		future.nest(f);
	},

	copyFileOutOfCache: function(targetPath, testData, future) {
		palmPutResource(future.result.pathName, testData);
		var uri = 'palm://com.palm.filecache';
		var cmd = 'CopyCacheObject';
		var args = {
			destination: targetPath,
			pathName: future.result.pathName
		};
		var f = Foundations.Comms.PalmCall.call(uri, cmd, args);
		future.nest(f);
	},

	somethingFailed: function(recordResults, future) {
		recordResults(future.message);
	},
	
	testCopyFile: function(recordResults) {
		var testData = palmGetResource("test/test_data.txt");
		var future = new Foundations.Control.Future();
		var fileName = "cacheTestFileName-" + Date.now() + "-" + Math.floor(Math.random() * 100000) + ".txt";
		
		function recordTestResults(future) {
			var s;
			try {
				s = palmGetResource(future.result.newPathName);				
			} catch (e) {
				recordResults("Can't open copied file.");
				return;
			}
			if (s === testData) {
				recordResults(Test.passed);
				return;
			}
			recordResults("Cache file contents did not match source data.");
		}
		
		future.now(this.deleteFileCacheKind);
		future.then(this.createFileCacheKind);
		future.then(_.bind(this.createCacheObject,this, fileName));
		future.then(_.bind(this.copyFileOutOfCache, this, "/tmp/", testData));
		future.then(recordTestResults);
		future.onError(_.bind(this.somethingFailed, undefined, recordResults));
	},
	
	testCopyFileWithError: function(recordResults) {
		var testData = palmGetResource("test/test_data.txt");
		var targetSize = 1024*1024;
		while(testData.length < targetSize) {
			testData = testData + testData;
		}
		var future = new Foundations.Control.Future();
		var fileName = "cacheTestFileName-" + Date.now() + "-" + Math.floor(Math.random() * 100000) + ".txt";
		
		function recordTestResults(future) {
			try {
				if (future.result.returnValue) {
					recordResults("Copy to full volume was expected to fail, but didn't.");				
				}				
			} catch (e) {
				if (e.errorCode !== -190) {
					recordResults("Expected error code -190 but got " + e.errorCode + ".");									
				} else {
					recordResults(Test.passed);					
				}
			}
		}
		
		future.now(_.bind(this.createCacheObject,this, fileName));
		future.then(_.bind(this.copyFileOutOfCache, this, "/Volumes/FileCacheTarget/", testData));
		future.then(recordTestResults);
	},
	
	after: function(cb) {
		try {
			this.handle.detach();			
		} catch(e) {
			
		}
		cb();
	}
}	
;

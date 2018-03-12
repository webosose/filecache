# Copyright (c) 2007-2018 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
FILE_CACHE_RUNNING=`ps -c | grep -c filecache`
if [ $FILE_CACHE_RUNNING == 0 ]
then
    ./debug-mac-x86/filecache &
    sleep 0.1
fi
$BEDLAM_ROOT/bin/js $BEDLAM_ROOT/palm/frameworks/unittest/version/1.0/tools/test_runner.js -- test/all_tests.json
if [ $FILE_CACHE_RUNNING == 0 ]
then
    killall -KILL filecache
fi

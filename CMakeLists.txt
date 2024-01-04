# Copyright (c) 2009-2024 LG Electronics, Inc.
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
#
# From the filecache bb file:
# depends: "jemalloc luna-service mojodb glibmm boost libsandbox"
#
# but from log.do_compile, I find that there are more dependencies:
# libgiomm-2.4 libgio-2.0 libglibmm-2.4 libgmodule-2.0 libgobject-2.0 libglib-2.0 libgthread-2.0
#
# also found that while -lsigc-2.0 is on the command line, the package is sigc++-2.0

cmake_minimum_required (VERSION 2.8.3)

project (filecache CXX)

include(webOS/webOS)

webos_modules_init(1 0 0 QUALIFIER RC2)
webos_component(2 0 1)
webos_add_compiler_flags(ALL -std=c++11 -Wall -Wconversion -DMOJ_LINUX)

include(FindPkgConfig)

# Use the mulithreaded jemalloc as we use multithreaded BOOST
pkg_check_modules(JEM REQUIRED jemalloc_mt)
include_directories(${JEM_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${JEM_CFLAGS_OTHER})

pkg_check_modules(LS2 REQUIRED luna-service2)
include_directories(${LS2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LS2_CFLAGS_OTHER})

pkg_check_modules(PBNJSON_C REQUIRED pbnjson_c)
include_directories(${PBNJSON_C_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_C_CFLAGS_OTHER})

pkg_check_modules(GBMM REQUIRED glibmm-2.4)
include_directories(${GBMM_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GBMM_CFLAGS_OTHER})

pkg_check_modules(SAND REQUIRED sandbox)
include_directories(${SAND_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${SAND_CFLAGS_OTHER})

pkg_check_modules(GIOMM REQUIRED giomm-2.4)
include_directories(${GIOMM_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GIOMM_CFLAGS_OTHER})

pkg_check_modules(SIGC REQUIRED sigc++-2.0)
include_directories(${SIGC_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${SIGC_CFLAGS_OTHER})

pkg_check_modules(GLIB_2 REQUIRED glib-2.0)
include_directories(${GLIB_2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB_2_CFLAGS_OTHER})

pkg_check_modules(DB8 REQUIRED db8)
include_directories(${DB8_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${DB8_CFLAGS_OTHER})

find_package(Boost REQUIRED COMPONENTS filesystem system)
include_directories(${Boost_INCLUDE_DIRS})

pkg_check_modules(PMLOGLIB REQUIRED PmLogLib)
include_directories(${PMLOGLIB_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOGLIB_CFLAGS_OTHER})

include_directories(src)
webos_add_linker_options(ALL --no-undefined)

file(GLOB SOURCE_FILES src/*.cpp)
add_executable(filecache ${SOURCE_FILES})

target_link_libraries(filecache
			${DB8_LDFLAGS}
			${GIOMM_LDFLAGS}
			${GBMM_LDFLAGS}
			${JEM_LDFLAGS}
			${LS2_LDFLAGS}
			${SAND_LDFLAGS}
			${SIGC_LDFLAGS}
			${Boost_LIBRARIES}
			${GLIB_2_LDFLAGS}
			${PBNJSON_C_LIBRARIES}
			${PMLOGLIB_LDFLAGS}
)

webos_configure_header_files(src)
webos_build_daemon()
webos_build_system_bus_files()
install(FILES files/conf/FileCache.conf DESTINATION ${WEBOS_INSTALL_WEBOS_SYSCONFDIR})

# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

# CMake include(s).
cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building TBB as part of the CoopSync_TBB project")

set(COOPSYNC_TBB_TBB_FETCH_VERSION
    "2023.1" "none"
    CACHE
      STRING
      "Fetch version for TBB: 2021.13|2022.0|2022.1|2022.2|2022.3|2023.0|2023.1"
)
# Declare where to get TBB from.

if(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2021.13")

elseif(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2022.0")
  set(COOPSYNC_TBB_TBB_SOURCE_DEFAULT
      "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2022.0.0.tar.gz;URL_MD5;383ebd7f3c84ff7124a215ff73364e76"
  )
elseif(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2022.0")
  set(COOPSYNC_TBB_TBB_SOURCE_DEFAULT
      "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2022.0.0.tar.gz;URL_MD5;383ebd7f3c84ff7124a215ff73364e76"
  )
elseif(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2022.1")
  set(COOPSYNC_TBB_TBB_SOURCE_DEFAULT
      "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2022.1.0.tar.gz;URL_MD5;383ebd7f3c84ff7124a215ff73364e76"
  )
elseif(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2022.2")
  set(COOPSYNC_TBB_TBB_SOURCE_DEFAULT
      "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2022.2.0.tar.gz;URL_MD5;383ebd7f3c84ff7124a215ff73364e76"
  )
elseif(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2022.3")
  set(COOPSYNC_TBB_TBB_SOURCE_DEFAULT
      "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2022.3.0.tar.gz;URL_MD5;383ebd7f3c84ff7124a215ff73364e76"
  )
elseif(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2023.0")
  set(COOPSYNC_TBB_TBB_SOURCE_DEFAULT
      "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2023.0.0.tar.gz;URL_MD5;383ebd7f3c84ff7124a215ff73364e76"
  )
elseif(COOPSYNC_TBB_TBB_FETCH_VERSION STREQUAL "2023.1")
  set(COOPSYNC_TBB_TBB_SOURCE_DEFAULT
      "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2023.1.0.tar.gz;URL_MD5;383ebd7f3c84ff7124a215ff73364e76"
  )
endif()

set(COOPSYNC_TBB_TBB_SOURCE
    ${COOPSYNC_TBB_TBB_SOURCE_DEFAULT}
    CACHE STRING "Source for TBB, when built as part of this project")
unset(COOPSYNC_TBB_TBB_SOURCE_DEFAULT)

mark_as_advanced(COOPSYNC_TBB_TBB_SOURCE)
FetchContent_Declare(TBB SYSTEM ${COOPSYNC_TBB_TBB_SOURCE}
                         OVERRIDE_FIND_PACKAGE)
# Options used in the build of TBB.
set(TBB_TEST
    FALSE
    CACHE BOOL "Turn off the TBB tests")
set(TBB_STRICT
    FALSE
    CACHE BOOL "Do not throw errors on compiler warnings")

if(NOT ${COOPSYNC_TBB_SANITIZER} STREQUAL "none")
  set(TBB_SANITIZE
      ${COOPSYNC_TBB_SANITIZER}
      CACHE STRING "Sanitizer to use for TBB build")
endif()

# Make TBB work without warnings with modern CMake versions.
set(CMAKE_POLICY_VERSION_MINIMUM "3.10")

# Get it into the current directory.
FetchContent_MakeAvailable(TBB)

# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

# CMake include(s).
cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building alpaka as part of the CoopSync_TBB project")

# Declare where to get alpaka from.
set(
  COOPSYNC_TBB_ALPAKA_SOURCE
  "URL;https://github.com/alpaka-group/alpaka/archive/refs/tags/3.0.0.tar.gz;URL_MD5;35a799c8ee8e391d05504d83448c9bdc"
  CACHE STRING
  "Source for alpaka, when built as part of this project"
)
mark_as_advanced(COOPSYNC_TBB_ALPAKA_SOURCE)
FetchContent_Declare(
  alpaka
  SYSTEM
  ${COOPSYNC_TBB_ALPAKA_SOURCE}
  OVERRIDE_FIND_PACKAGE
  EXCLUDE_FROM_ALL
)

# Turn off build testing.
set(BUILD_TESTING FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(alpaka)

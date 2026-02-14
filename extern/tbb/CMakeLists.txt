# CMake include(s).
cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building TBB as part of the CoopSync_TBB project")

# Declare where to get TBB from.
set(COOPSYNC_TBB_TBB_SOURCE
    "URL;https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2022.3.0.tar.gz;URL_MD5;2b242c465b194ac8e1451ea1354873ae"
    CACHE STRING "Source for TBB, when built as part of this project")
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

# Make TBB work without warnings with modern CMake versions.
set(CMAKE_POLICY_VERSION_MINIMUM "3.10")

# Get it into the current directory.
FetchContent_MakeAvailable(TBB)

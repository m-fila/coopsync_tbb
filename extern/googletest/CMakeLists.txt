# CMake include(s).
cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building GoogleTest as part of the CoopSync_TBB project")

# Declare where to get GoogleTest from.
set(COOPSYNC_TBB_GOOGLETEST_SOURCE
    "URL;https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz;URL_MD5;b6f100bc2a5853a48046aa168ececf84"
    CACHE STRING "Source for GoogleTest, when built as part of this project")
mark_as_advanced(COOPSYNC_TBB_GOOGLETEST_SOURCE)
FetchContent_Declare(GoogleTest SYSTEM ${COOPSYNC_TBB_GOOGLETEST_SOURCE})
# Options used in the build of GoogleTest.
set(BUILD_GMOCK
    TRUE
    CACHE BOOL "Turn off the build of GMock")
set(INSTALL_GTEST
    FALSE
    CACHE BOOL "Turn off the installation of GoogleTest")
if(WIN32)
  set(gtest_force_shared_crt
      TRUE
      CACHE BOOL
            "Use shared (DLL) run-time library, even with static libraries")
endif()

# Silence some warnings with modern versions of CMake on macOS.
set(CMAKE_MACOSX_RPATH TRUE)

# Get it into the current directory.
FetchContent_MakeAvailable(GoogleTest)

# Set up aliases for the GTest targets with the same name that they have when we
# find GTest pre-installed.
if(NOT TARGET GTest::gtest)
  add_library(GTest::gtest ALIAS gtest)
endif()
if(NOT TARGET GTest::gtest_main)
  add_library(GTest::gtest_main ALIAS gtest_main)
endif()

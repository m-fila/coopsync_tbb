# CMake include(s).
cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building Boost.Fiber as part of the CoopSync_TBB project")

# Declare where to get TBB from.
set(COOPSYNC_TBB_BOOST_FIBER_SOURCE
    "URL;https://github.com/boostorg/fiber/archive/refs/tags/boost-1.90.0.tar.gz;URL_MD5;e390ee70fe876445c3a784d75bb1f25c"
    CACHE STRING "Source for Boost.Fiber, when built as part of this project")
mark_as_advanced(COOPSYNC_TBB_BOOST_FIBER_SOURCE)
FetchContent_Declare(BoostFiber SYSTEM ${COOPSYNC_TBB_BOOST_FIBER_SOURCE}
                         OVERRIDE_FIND_PACKAGE)

# Get it into the current directory.
FetchContent_MakeAvailable(BoostFiber)

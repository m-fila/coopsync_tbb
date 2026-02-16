include(CMakeFindDependencyMacro)

find_dependency(TBB REQUIRED)

include(${CMAKE_CURRENT_LIST_DIR}/CoopSync_TBBTargets.cmake)

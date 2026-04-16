# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

include(CMakeFindDependencyMacro)

find_dependency(TBB REQUIRED)

include(${CMAKE_CURRENT_LIST_DIR}/CoopSync_TBBTargets.cmake)

// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

/// @file
/// @brief Feature test macros for the CoopSync_TBB public API.

#pragma once

#if defined(__has_include) && __has_include(<version>)
#include <version>
#endif

#ifdef DOXYGEN
/// @brief Macro that indicates whether atomic_flag is supported on the current
/// platform.
#define COOPSYNC_TBB_HAS_ATOMIC_FLAG 1
#endif

#if defined(__cpp_lib_atomic_flag_test) && __cpp_lib_atomic_flag_test >= 201907L
#define COOPSYNC_TBB_HAS_ATOMIC_FLAG 1  // NOLINT(cppcoreguidelines-macro-usage)
#endif

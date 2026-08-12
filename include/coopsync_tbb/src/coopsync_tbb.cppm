// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

/// @file coopsync_tbb.cppm
/// @brief C++20 module interface.

module;

#include "coopsync_tbb/coopsync_tbb.hpp"

/// \module coopsync_tbb
/// \brief C++20 module interface for the CoopSync_TBB library.
///
/// The module interface exports all public APIs of the library, including
/// classes, aliases and free functions. The module interface is intended to be
/// used with C++20 modules, and can be imported using the
/// \code{.cpp}
/// import coopsync_tbb;
/// \endcode
/// directive.
///
/// The module provides all the public APIs of the library except for the
/// integrations with other libraries and compute accelerators (CUDA, HIP, etc.)
/// which are available only through headers.
///
/// The module also provides optional classes and free functions when supported
/// by the detected compiler, such as \ref coopsync_tbb::atomic_flag and
/// \ref coopsync_tbb::atomic_ref_condition. Compiler support for these optional
/// APIs can be checked using the feature-test macros defined in the \ref
/// coopsync_tbb/feature_test.hpp header.
///
export module coopsync_tbb;

export namespace coopsync_tbb {
// classes
using ::coopsync_tbb::atomic_condition;
using ::coopsync_tbb::barrier;
using ::coopsync_tbb::condition_variable;
using ::coopsync_tbb::counting_semaphore;
using ::coopsync_tbb::future;
using ::coopsync_tbb::future_error;
using ::coopsync_tbb::latch;
using ::coopsync_tbb::mutex;
using ::coopsync_tbb::packaged_task;
using ::coopsync_tbb::promise;
using ::coopsync_tbb::shared_future;
using ::coopsync_tbb::shared_mutex;
#if defined(COOPSYNC_TBB_HAS_ATOMIC_FLAG) && COOPSYNC_TBB_HAS_ATOMIC_FLAG == 1
using ::coopsync_tbb::atomic_flag;
#endif  // COOPSYNC_TBB_HAS_ATOMIC_FLAG
#if defined(COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION) && \
    COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION == 1
using ::coopsync_tbb::atomic_ref_condition;
#endif  // COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION
// aliases
#ifndef COOPSYNC_TBB_DOXYGEN  // hide from doxygen to avoid duplication
using ::coopsync_tbb::binary_semaphore;
using ::coopsync_tbb::rw_mutex;
#endif  // COOPSYNC_TBB_DOXYGEN
// free functions
#ifndef COOPSYNC_TBB_DOXYGEN  // hide from doxygen to avoid duplication
using ::coopsync_tbb::atomic_notify_all;
using ::coopsync_tbb::atomic_notify_one;
using ::coopsync_tbb::atomic_wait;
using ::coopsync_tbb::atomic_wait_explicit;
using ::coopsync_tbb::swap;
#if defined(COOPSYNC_TBB_HAS_ATOMIC_FLAG) && COOPSYNC_TBB_HAS_ATOMIC_FLAG == 1
using ::coopsync_tbb::atomic_flag_clear;
using ::coopsync_tbb::atomic_flag_clear_explicit;
using ::coopsync_tbb::atomic_flag_notify_all;
using ::coopsync_tbb::atomic_flag_notify_one;
using ::coopsync_tbb::atomic_flag_test;
using ::coopsync_tbb::atomic_flag_test_and_set;
using ::coopsync_tbb::atomic_flag_test_and_set_explicit;
using ::coopsync_tbb::atomic_flag_test_explicit;
using ::coopsync_tbb::atomic_flag_wait;
using ::coopsync_tbb::atomic_flag_wait_explicit;
#endif  // COOPSYNC_TBB_HAS_ATOMIC_FLAG
#endif  // COOPSYNC_TBB_DOXYGEN
}  // namespace coopsync_tbb

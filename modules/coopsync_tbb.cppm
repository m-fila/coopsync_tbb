// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

/// @file coopsync_tbb.cppm
/// @brief C++20 module interface.

module;

#include "coopsync_tbb/coopsync_tbb.hpp"

/// \module coopsync_tbb
/// \brief C++20 module interface for the CoopSync_TBB library.
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
// aliases
#ifndef DOXYGEN  // hide from doxygen to avoid duplication
using ::coopsync_tbb::binary_semaphore;
using ::coopsync_tbb::rw_mutex;
#endif
// free functions
#ifndef DOXYGEN  // hide from doxygen to avoid duplication
using ::coopsync_tbb::atomic_notify_all;
using ::coopsync_tbb::atomic_notify_one;
using ::coopsync_tbb::atomic_wait;
using ::coopsync_tbb::atomic_wait_explicit;
using ::coopsync_tbb::swap;
#endif
}  // namespace coopsync_tbb

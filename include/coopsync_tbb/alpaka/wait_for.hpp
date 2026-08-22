// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <oneapi/tbb/task.h>

#include <alpaka/alpaka.hpp>

/// @brief alpaka integration.
namespace coopsync_tbb::alpaka {

/// @brief Suspends the current TBB task until all the work in a given alpaka
/// queue is complete.
/// @tparam Queue A type fulfilling the alpaka queue requirements.
/// @param queue The alpaka queue to wait for.
/// @throws std::exception Rethrows any exception thrown by the alpaka enqueue
/// operation.
/// @note In case of error during enqueue, the task is resumed immediately and
/// the exception is rethrown.
template <typename Queue>
    requires ::alpaka::isQueue<Queue>
inline static void wait_for(Queue& queue) {
    ::tbb::task::suspend([&queue](::tbb::task::suspend_point suspend_point) {
        try {
            ::alpaka::enqueue(queue, [suspend_point]() {
                ::tbb::task::resume(suspend_point);
            });
        } catch (...) {
            ::tbb::task::resume(suspend_point);
            throw;
        }
    });
}

}  // namespace coopsync_tbb::alpaka

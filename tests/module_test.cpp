// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

import coopsync_tbb;

TEST(ModuleImport, Classes) {
    static_assert(sizeof(coopsync_tbb::atomic_condition<int>) > 0);
    static_assert(sizeof(coopsync_tbb::barrier<>) > 0);
    static_assert(sizeof(coopsync_tbb::condition_variable) > 0);
    static_assert(sizeof(coopsync_tbb::future<void>) > 0);
    static_assert(sizeof(coopsync_tbb::future<int>) > 0);
    static_assert(sizeof(coopsync_tbb::future<int&>) > 0);
    static_assert(sizeof(coopsync_tbb::promise<void>) > 0);
    static_assert(sizeof(coopsync_tbb::promise<int>) > 0);
    static_assert(sizeof(coopsync_tbb::promise<int&>) > 0);
    static_assert(sizeof(coopsync_tbb::shared_future<void>) > 0);
    static_assert(sizeof(coopsync_tbb::shared_future<int>) > 0);
    static_assert(sizeof(coopsync_tbb::shared_future<int&>) > 0);
    static_assert(sizeof(coopsync_tbb::packaged_task<void()>) > 0);
    static_assert(sizeof(coopsync_tbb::latch) > 0);
    static_assert(sizeof(coopsync_tbb::mutex) > 0);
    static_assert(sizeof(coopsync_tbb::counting_semaphore<6>) > 0);
    static_assert(sizeof(coopsync_tbb::binary_semaphore) > 0);
    static_assert(sizeof(coopsync_tbb::shared_mutex) > 0);
    static_assert(sizeof(coopsync_tbb::rw_mutex) > 0);
}

TEST(ModuleImport, FreeFunctions) {
    static_assert(requires(coopsync_tbb::atomic_condition<int>* c,
                           std::memory_order order) {
        coopsync_tbb::atomic_wait_explicit<int>(c, 0, order);
    });
    static_assert(requires(coopsync_tbb::atomic_condition<int>* c) {
        coopsync_tbb::atomic_wait<int>(c, 0);
    });

    static_assert(requires(coopsync_tbb::atomic_condition<int>* c) {
        coopsync_tbb::atomic_notify_one<int>(c);
    });

    static_assert(requires(coopsync_tbb::atomic_condition<int>* c) {
        coopsync_tbb::atomic_notify_all<int>(c);
    });
}

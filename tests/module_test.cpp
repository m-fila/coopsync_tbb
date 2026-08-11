// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

import coopsync_tbb;
#include "coopsync_tbb/feature_test.hpp"

TEST(ModuleImport, Classes) {
    static_assert(sizeof(coopsync_tbb::atomic_condition<int>) > 0);
    static_assert(sizeof(coopsync_tbb::barrier<>) > 0);
    static_assert(sizeof(coopsync_tbb::condition_variable) > 0);
    static_assert(sizeof(coopsync_tbb::future<void>) > 0);
    static_assert(sizeof(coopsync_tbb::future<int>) > 0);
    static_assert(sizeof(coopsync_tbb::future<int&>) > 0);
    static_assert(sizeof(coopsync_tbb::future_error) > 0);
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
#if defined(COOPSYNC_TBB_HAS_ATOMIC_FLAG) && COOPSYNC_TBB_HAS_ATOMIC_FLAG == 1
    static_assert(sizeof(coopsync_tbb::atomic_flag) > 0);
#endif
#if defined(COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION) && \
    COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION == 1
    static_assert(sizeof(coopsync_tbb::atomic_ref_condition<int>) > 0);
#endif
}

TEST(ModuleImport, FreeFunctions) {
    static_assert(requires(coopsync_tbb::atomic_condition<int>* c, int v,
                           std::memory_order order) {
        coopsync_tbb::atomic_wait_explicit<int>(c, v, order);
    });
    static_assert(requires(coopsync_tbb::atomic_condition<int>* c, int v) {
        coopsync_tbb::atomic_wait<int>(c, v);
    });

    static_assert(requires(coopsync_tbb::atomic_condition<int>* c) {
        coopsync_tbb::atomic_notify_one<int>(c);
    });

    static_assert(requires(coopsync_tbb::atomic_condition<int>* c) {
        coopsync_tbb::atomic_notify_all<int>(c);
    });
#if defined(COOPSYNC_TBB_HAS_ATOMIC_FLAG) && COOPSYNC_TBB_HAS_ATOMIC_FLAG == 1
    static_assert(
        requires(coopsync_tbb::atomic_flag* c, std::memory_order order) {
            coopsync_tbb::atomic_flag_test_and_set_explicit(c, order);
        });
    static_assert(requires(coopsync_tbb::atomic_flag* c) {
        coopsync_tbb::atomic_flag_test_and_set(c);
    });
    static_assert(
        requires(coopsync_tbb::atomic_flag* c, std::memory_order order) {
            coopsync_tbb::atomic_flag_test_explicit(c, order);
        });
    static_assert(requires(coopsync_tbb::atomic_flag* c) {
        coopsync_tbb::atomic_flag_test(c);
    });
    static_assert(
        requires(coopsync_tbb::atomic_flag* c, std::memory_order order) {
            coopsync_tbb::atomic_flag_clear_explicit(c, order);
        });
    static_assert(requires(coopsync_tbb::atomic_flag* c) {
        coopsync_tbb::atomic_flag_clear(c);
    });
    static_assert(requires(coopsync_tbb::atomic_flag* c) {
        coopsync_tbb::atomic_flag_notify_one(c);
    });
    static_assert(requires(coopsync_tbb::atomic_flag* c) {
        coopsync_tbb::atomic_flag_notify_all(c);
    });
    static_assert(requires(coopsync_tbb::atomic_flag* c, bool v,
                           std::memory_order order) {
        coopsync_tbb::atomic_flag_wait_explicit(c, v, order);
    });
    static_assert(requires(coopsync_tbb::atomic_flag* c, bool v) {
        coopsync_tbb::atomic_flag_wait(c, v);
    });
#endif
}

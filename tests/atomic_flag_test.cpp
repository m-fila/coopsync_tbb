// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/feature_test.hpp"

#if defined(COOPSYNC_TBB_HAS_ATOMIC_FLAG) && COOPSYNC_TBB_HAS_ATOMIC_FLAG == 1

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>

#include "coopsync_tbb/atomic_flag_condition.hpp"

TEST(AtomicFlag, BasicOperations) {
    auto flag = coopsync_tbb::atomic_flag();

    ASSERT_FALSE(flag.test());
    ASSERT_FALSE(flag.test_and_set());
    ASSERT_TRUE(flag.test());
    flag.clear();
    ASSERT_FALSE(flag.test());
}

TEST(AtomicFlag, BasicOperationsFreeFunctions) {
    auto flag = coopsync_tbb::atomic_flag();

    ASSERT_FALSE(coopsync_tbb::atomic_flag_test(&flag));
    ASSERT_FALSE(coopsync_tbb::atomic_flag_test_and_set(&flag));
    ASSERT_TRUE(coopsync_tbb::atomic_flag_test(&flag));
    coopsync_tbb::atomic_flag_clear(&flag);
    ASSERT_FALSE(coopsync_tbb::atomic_flag_test(&flag));
}

TEST(AtomicFlag, Accessors) {
    auto flag = coopsync_tbb::atomic_flag();

    auto& atomic_ref = flag.atomic();
    auto& deref_ref = *flag;
    auto* atomic_ptr = flag.operator->();

    ASSERT_EQ(&atomic_ref, &deref_ref);
    ASSERT_EQ(&atomic_ref, atomic_ptr);

    atomic_ref.test_and_set(std::memory_order_relaxed);
    ASSERT_TRUE(deref_ref.test(std::memory_order_relaxed));

    (*flag).clear(std::memory_order_relaxed);
    ASSERT_FALSE(flag->test(std::memory_order_relaxed));

    flag->test_and_set(std::memory_order_relaxed);
    ASSERT_TRUE(flag.atomic().test(std::memory_order_relaxed));

    const auto& cflag = flag;

    const auto& const_atomic_ref = cflag.atomic();
    const auto& const_deref_ref = *cflag;
    const auto* const_atomic_ptr = cflag.operator->();

    ASSERT_EQ(&const_atomic_ref, &const_deref_ref);
    ASSERT_EQ(&const_atomic_ref, const_atomic_ptr);
    ASSERT_TRUE(const_atomic_ptr->test(std::memory_order_relaxed));
}

TEST(AtomicFlag, WaitReturnsImmediatelyWhenNotEqual) {
    auto flag = coopsync_tbb::atomic_flag();
    // Should not suspend or block.
    flag.wait(true);
}

TEST(AtomicFlag, NotifyWithNoWaiters) {
    auto flag = coopsync_tbb::atomic_flag();
    // Should not crash or do anything.
    flag.notify_one();
    flag.notify_all();
}

TEST(AtomicFlag, NotifyOneUnblocksOne) {
    auto flag = coopsync_tbb::atomic_flag();

    auto done = std::atomic<int>{0};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            flag.wait(false);
            done.store(1, std::memory_order_relaxed);
        } else {
            flag.test_and_set(std::memory_order_relaxed);
            flag.notify_one();
        }
    });

    ASSERT_EQ(done.load(std::memory_order_relaxed), 1);
}

TEST(AtomicFlag, NotifyAllUnblocksAll) {
    auto flag = coopsync_tbb::atomic_flag();

    auto done = std::atomic<int>{0};

    tbb::parallel_for(0, 3, [&](int i) {
        if (i == 0) {
            flag.wait(false);
            done.fetch_add(1, std::memory_order_relaxed);
        } else if (i == 1) {
            flag.wait(false);
            done.fetch_add(1, std::memory_order_relaxed);
        } else {
            flag.test_and_set(std::memory_order_relaxed);
            flag.notify_all();
        }
    });

    ASSERT_EQ(done.load(std::memory_order_relaxed), 2);
}

TEST(AtomicFlag, WaitFreeFunctions) {

    auto flag = coopsync_tbb::atomic_flag();
    auto done = std::atomic<int>{0};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            coopsync_tbb::atomic_wait(&flag, false);
            done.store(1, std::memory_order_relaxed);
        } else {
            coopsync_tbb::atomic_flag_test_and_set(&flag);
            coopsync_tbb::atomic_notify_one(&flag);
        }
    });

    ASSERT_EQ(done.load(std::memory_order_relaxed), 1);
}

#endif

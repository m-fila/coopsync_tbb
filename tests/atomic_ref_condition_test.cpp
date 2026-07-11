// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/feature_test.hpp"

#if defined(COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION) && \
    COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION == 1

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>

#include "coopsync_tbb/atomic_ref_condition.hpp"

TEST(AtomicRefCondition, Accessors) {
    auto value = 0;
    auto cond = coopsync_tbb::atomic_ref_condition<int>(value);

    auto& atomic_ref = cond.atomic();
    auto& deref_ref = *cond;
    auto* atomic_ptr = cond.operator->();

    ASSERT_EQ(&atomic_ref, &deref_ref);
    ASSERT_EQ(&atomic_ref, atomic_ptr);

    atomic_ref.store(1, std::memory_order_relaxed);
    ASSERT_EQ(deref_ref.load(std::memory_order_relaxed), 1);

    (*cond).store(2, std::memory_order_relaxed);
    ASSERT_EQ(cond->load(std::memory_order_relaxed), 2);

    cond->store(3, std::memory_order_relaxed);
    ASSERT_EQ(cond.atomic().load(std::memory_order_relaxed), 3);

    const auto& ccond = cond;

    const auto& const_atomic_ref = ccond.atomic();
    const auto& const_deref_ref = *ccond;
    const auto* const_atomic_ptr = ccond.operator->();

    ASSERT_EQ(&const_atomic_ref, &const_deref_ref);
    ASSERT_EQ(&const_atomic_ref, const_atomic_ptr);
    ASSERT_EQ(const_atomic_ptr->load(std::memory_order_relaxed), 3);
}

TEST(AtomicRefCondition, WaitReturnsImmediatelyWhenNotEqual) {
    auto value = 1;
    auto cond = coopsync_tbb::atomic_ref_condition<int>(value);
    // Should not suspend or block.
    cond.wait(0);
}

TEST(AtomicRefCondition, NotifyWithNoWaiters) {
    auto value = 0;
    auto cond = coopsync_tbb::atomic_ref_condition<int>(value);
    // Should not crash or do anything.
    cond.notify_one();
    cond.notify_all();
}

TEST(AtomicRefCondition, NotifyOneUnblocksOne) {
    auto value = 0;
    auto cond = coopsync_tbb::atomic_ref_condition<int>(value);

    auto done = std::atomic<int>{0};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            cond.wait(0);
            done.store(-1, std::memory_order_relaxed);
        } else {
            cond.atomic().store(1, std::memory_order_relaxed);
            cond.notify_one();
        }
    });

    ASSERT_EQ(done.load(std::memory_order_relaxed), -1);
}

TEST(AtomicRefCondition, NotifyAllUnblocksAllWaiters) {
    auto value = 0;
    auto cond = coopsync_tbb::atomic_ref_condition<int>(value);

    auto woken = std::atomic<int>{0};

    tbb::parallel_for(0, 3, [&](int i) {
        if (i == 0) {
            cond.atomic().store(1, std::memory_order_relaxed);
            cond.notify_all();
        } else {
            cond.wait(0);
            woken.fetch_add(1, std::memory_order_relaxed);
        }
    });

    ASSERT_EQ(woken.load(std::memory_order_relaxed), 2);
}

struct always_equal {
    int value;
};

inline bool operator==(const always_equal&, const always_equal&) {
    return true;
}

TEST(AtomicRefCondition, WaitUsesBitwiseComparison) {

    auto old = always_equal{0};
    const auto new_value = always_equal{1};
    ASSERT_EQ(old, new_value);

    auto cond = coopsync_tbb::atomic_ref_condition<always_equal>(old);
    // should not suspend or hang since the the comparison is bitwise
    // and the values are different
    cond.wait(new_value);
}

#endif

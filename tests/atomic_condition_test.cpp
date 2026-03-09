#include "coopsync_tbb/atomic_condition.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>

TEST(AtomicCondition, Accessors) {
    auto cond = coopsync_tbb::atomic_condition<int>(0);

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

TEST(AtomicCondition, WaitReturnsImmediatelyWhenNotEqual) {
    auto cond = coopsync_tbb::atomic_condition<int>(1);
    // Should not suspend or block.
    cond.wait(0);
}

TEST(AtomicCondition, NotifyWithNoWaiters) {
    auto cond = coopsync_tbb::atomic_condition<int>(0);
    // Should not crash or do anything.
    cond.notify_one();
    cond.notify_all();
}

TEST(AtomicCondition, NotifyOneUnblocksOne) {
    auto cond = coopsync_tbb::atomic_condition<int>(0);

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

TEST(AtomicCondition, NotifyAllUnblocksAllWaiters) {
    auto cond = coopsync_tbb::atomic_condition<int>(0);

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

TEST(AtomicCondition, FreeFunctions) {
    {
        auto cond = coopsync_tbb::atomic_condition<int>(0);
        auto done = std::atomic<int>{0};

        tbb::parallel_for(0, 2, [&](int i) {
            if (i == 0) {
                coopsync_tbb::atomic_wait_explicit(cond, 0,
                                                   std::memory_order_seq_cst);
                done.store(1, std::memory_order_relaxed);
            } else {
                cond.atomic().store(1, std::memory_order_relaxed);
                coopsync_tbb::atomic_notify_one(cond);
            }
        });

        ASSERT_EQ(done.load(std::memory_order_relaxed), 1);
    }

    {
        auto cond = coopsync_tbb::atomic_condition<int>(0);
        auto done = std::atomic<int>{0};

        tbb::parallel_for(0, 2, [&](int i) {
            if (i == 0) {
                coopsync_tbb::atomic_wait(cond, 0);
                done.store(1, std::memory_order_relaxed);
            } else {
                cond.atomic().store(1, std::memory_order_relaxed);
                coopsync_tbb::atomic_notify_one(cond);
            }
        });

        ASSERT_EQ(done.load(std::memory_order_relaxed), 1);
    }
}

struct always_equal {
    int value;
};

inline bool operator==(const always_equal&, const always_equal&) noexcept {
    return true;
}

TEST(AtomicCondition, WaitUsesBitwiseComparison) {

    const auto old = always_equal{0};
    const auto new_value = always_equal{1};
    ASSERT_EQ(old, new_value);

    auto cond = coopsync_tbb::atomic_condition<always_equal>(old);
    // should not suspend or hang since the the comparison is bitwise
    // and the values are different
    cond.wait(new_value);
}

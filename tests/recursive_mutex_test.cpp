#include "coopsync_tbb/recursive_mutex.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include "mutex_traits.hpp"

TEST(RecursiveMutex, TBBMutexRequirement) {
    using mutex = coopsync_tbb::recursive_mutex;

    static_assert(coopsync_tbb::traits::has_scoped_lock_v<mutex>);
    static_assert(
        std::is_default_constructible<typename mutex::scoped_lock>::value);
    static_assert(
        std::is_constructible<typename mutex::scoped_lock, mutex&>::value);
    static_assert(std::is_destructible<typename mutex::scoped_lock>::value);

    static_assert(
        coopsync_tbb::traits::has_acquire_v<typename mutex::scoped_lock,
                                            mutex>);
    static_assert(
        coopsync_tbb::traits::has_try_acquire_v<typename mutex::scoped_lock,
                                                mutex>);
    static_assert(
        coopsync_tbb::traits::has_release_v<typename mutex::scoped_lock>);

    static_assert(coopsync_tbb::traits::has_is_rw_mutex_v<mutex>);
    static_assert(coopsync_tbb::traits::has_is_fair_mutex_v<mutex>);
    static_assert(coopsync_tbb::traits::has_is_recursive_mutex_v<mutex>);
}

TEST(RecursiveMutex, StdRequirements) {
    using mutex = coopsync_tbb::recursive_mutex;
    // BasicLockable
    static_assert(coopsync_tbb::traits::has_lock_v<mutex>);
    static_assert(coopsync_tbb::traits::has_try_lock_v<mutex>);
    // Lockable
    static_assert(coopsync_tbb::traits::has_unlock_v<mutex>);
}

TEST(RecursiveMutex, ExceptionOutsideTaskTryLock) {
    auto m = coopsync_tbb::recursive_mutex{};
    EXPECT_THROW(m.try_lock(), std::runtime_error);
}

TEST(RecursiveMutex, ExceptionOutsideTaskLock) {
    auto m = coopsync_tbb::recursive_mutex{};
    EXPECT_THROW(m.lock(), std::runtime_error);
}

TEST(RecursiveMutex, NoContentionTryLock) {
    tbb::parallel_for(0, 1, [](int) {
        auto m = coopsync_tbb::recursive_mutex{};
        ASSERT_TRUE(m.try_lock());
        ASSERT_TRUE(m.try_lock());
        m.unlock();
        m.unlock();
        ASSERT_TRUE(m.try_lock());
        m.unlock();
    });
}

TEST(RecursiveMutex, NoContentionLock) {
    tbb::parallel_for(0, 1, [](int) {
        auto m = coopsync_tbb::recursive_mutex{};
        m.lock();
        ASSERT_TRUE(m.try_lock());
        m.unlock();
        m.unlock();
        ASSERT_TRUE(m.try_lock());
        m.unlock();
    });
}

TEST(RecursiveMutex, MutualExclusionParallelIncrement) {
    auto m = coopsync_tbb::recursive_mutex{};
    auto counter = 0;
    const auto n = 20000;

    tbb::parallel_for(0, n, [&](int) {
        std::scoped_lock lock(m);
        ++counter;
    });

    ASSERT_EQ(counter, n);
}

TEST(RecursiveMutex, RecursiveLocking) {
    auto m = coopsync_tbb::recursive_mutex{};
    tbb::parallel_for(0, 10, [&](int) {
        {
            std::scoped_lock lock1(m);
            {
                std::scoped_lock lock2(m);
                { std::scoped_lock lock3(m); }
            }
        }
    });
}

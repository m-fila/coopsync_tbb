#include "coopsync_tbb/mutex.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <type_traits>

#include "mutex_traits.hpp"

TEST(Mutex, TBBMutexRequirement) {

    using mutex = coopsync_tbb::mutex;

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

TEST(Mutex, StdRequirements) {
    using mutex = coopsync_tbb::mutex;
    // BasicLockable
    static_assert(coopsync_tbb::traits::has_lock_v<mutex>);
    static_assert(coopsync_tbb::traits::has_try_lock_v<mutex>);
    // Lockable
    static_assert(coopsync_tbb::traits::has_unlock_v<mutex>);
}

TEST(Mutex, NoContentionTryLock) {
    auto m = coopsync_tbb::mutex{};
    ASSERT_TRUE(m.try_lock());
    ASSERT_FALSE(m.try_lock());
    m.unlock();
    ASSERT_TRUE(m.try_lock());
    m.unlock();
}

TEST(Mutex, NoContentionLock) {
    auto m = coopsync_tbb::mutex{};
    m.lock();
    ASSERT_FALSE(m.try_lock());
    m.unlock();
    ASSERT_TRUE(m.try_lock());
    m.unlock();
}

TEST(Mutex, MutualExclusionParallelIncrement) {
    auto m = coopsync_tbb::mutex{};
    auto counter = 0;
    const auto n = 16;

    tbb::parallel_for(0, n, [&](int) {
        auto lock = coopsync_tbb::mutex::scoped_lock(m);
        ++counter;
    });

    ASSERT_EQ(counter, n);
}

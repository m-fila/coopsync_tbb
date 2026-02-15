#include "coopsync_tbb/mutex.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include "mutex_traits.hpp"

TEST(Mutex, Traits) {
    ASSERT_TRUE(coopsync_tbb::has_is_rw_mutex_v<coopsync_tbb::mutex>);
    ASSERT_TRUE(coopsync_tbb::has_is_fair_mutex_v<coopsync_tbb::mutex>);
    ASSERT_TRUE(coopsync_tbb::has_is_recursive_mutex_v<coopsync_tbb::mutex>);
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
    const auto n = 20000;

    tbb::parallel_for(0, n, [&](int) {
        coopsync_tbb::scoped_lock lock(m);
        ++counter;
    });

    ASSERT_EQ(counter, n);
}

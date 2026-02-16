#include "coopsync_tbb/scoped_lock.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/spin_mutex.h>

#include <mutex>

struct MutexMock {
    void lock() { ++lock_count; }

    void unlock() { ++unlock_count; }

    int lock_count = 0;
    int unlock_count = 0;
};

TEST(ScopedLock, MutexMock) {
    auto m = MutexMock{};
    {
        auto lock = coopsync_tbb::scoped_lock(m);
        ASSERT_EQ(m.lock_count, 1);
        ASSERT_EQ(m.unlock_count, 0);
    }
    ASSERT_EQ(m.lock_count, 1);
    ASSERT_EQ(m.unlock_count, 1);
}

TEST(ScopedLock, StdMutex) {
    auto m = std::mutex{};
    {
        auto lock = coopsync_tbb::scoped_lock(m);
        ASSERT_FALSE(m.try_lock());
    }
}

TEST(ScopedLock, TBBSpinMutex) {
    auto m = tbb::spin_mutex{};
    {
        auto lock = coopsync_tbb::scoped_lock(m);
        ASSERT_FALSE(m.try_lock());
    }
}

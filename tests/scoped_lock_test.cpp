#include "coopsync_tbb/scoped_lock.hpp"

#include <gtest/gtest.h>

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

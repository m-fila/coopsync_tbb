#include "coopsync_tbb/detail/unique_scoped_lock.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/spin_mutex.h>

#include <mutex>

class MutexMock {
    public:
    void lock() { ++m_lock_count; }
    void unlock() { ++m_unlock_count; }
    bool try_lock() {
        ++m_try_lock_count;
        return true;
    }
    int get_lock_count() const { return m_lock_count; }
    int get_unlock_count() const { return m_unlock_count; }
    int get_try_lock_count() const { return m_try_lock_count; }

    private:
    int m_lock_count = 0;
    int m_unlock_count = 0;
    int m_try_lock_count = 0;
};

TEST(UniqueScopedLock, MutexMock) {
    auto m = MutexMock{};
    {
        auto lock = coopsync_tbb::detail::unique_scoped_lock<MutexMock>();
        ASSERT_EQ(m.get_lock_count(), 0);
        ASSERT_EQ(m.get_unlock_count(), 0);
        ASSERT_EQ(m.get_try_lock_count(), 0);
    }
    ASSERT_EQ(m.get_lock_count(), 0);
    ASSERT_EQ(m.get_unlock_count(), 0);
    ASSERT_EQ(m.get_try_lock_count(), 0);

    {
        auto lock = coopsync_tbb::detail::unique_scoped_lock(m);
        ASSERT_EQ(m.get_lock_count(), 1);
        ASSERT_EQ(m.get_unlock_count(), 0);
        ASSERT_EQ(m.get_try_lock_count(), 0);
    }
    ASSERT_EQ(m.get_lock_count(), 1);
    ASSERT_EQ(m.get_unlock_count(), 1);
    ASSERT_EQ(m.get_try_lock_count(), 0);
    {
        auto lock = coopsync_tbb::detail::unique_scoped_lock<MutexMock>();
        lock.release();
        ASSERT_EQ(m.get_lock_count(), 1);
        ASSERT_EQ(m.get_unlock_count(), 1);
        ASSERT_EQ(m.get_try_lock_count(), 0);
        lock.acquire(m);
        ASSERT_EQ(m.get_lock_count(), 2);
        ASSERT_EQ(m.get_unlock_count(), 1);
        ASSERT_EQ(m.get_try_lock_count(), 0);
        EXPECT_THROW(lock.acquire(m), std::system_error);
        ASSERT_EQ(m.get_lock_count(), 2);
        ASSERT_EQ(m.get_unlock_count(), 1);
        ASSERT_EQ(m.get_try_lock_count(), 0);
        EXPECT_THROW(lock.try_acquire(m), std::system_error);
        ASSERT_EQ(m.get_lock_count(), 2);
        ASSERT_EQ(m.get_unlock_count(), 1);
        ASSERT_EQ(m.get_try_lock_count(), 0);
        lock.release();
        ASSERT_EQ(m.get_lock_count(), 2);
        ASSERT_EQ(m.get_unlock_count(), 2);
        ASSERT_EQ(m.get_try_lock_count(), 0);
        lock.try_acquire(m);
        ASSERT_EQ(m.get_lock_count(), 2);
        ASSERT_EQ(m.get_unlock_count(), 2);
        ASSERT_EQ(m.get_try_lock_count(), 1);
        lock.release();
        ASSERT_EQ(m.get_lock_count(), 2);
        ASSERT_EQ(m.get_unlock_count(), 3);
        ASSERT_EQ(m.get_try_lock_count(), 1);
    }
    ASSERT_EQ(m.get_lock_count(), 2);
    ASSERT_EQ(m.get_unlock_count(), 3);
    ASSERT_EQ(m.get_try_lock_count(), 1);
}

TEST(ScopedLock, StdMutex) {
    auto m = std::mutex{};
    {
        auto lock = coopsync_tbb::detail::unique_scoped_lock(m);
        ASSERT_FALSE(m.try_lock());
    }
}

TEST(ScopedLock, TBBSpinMutex) {
    auto m = tbb::spin_mutex{};
    {
        auto lock = coopsync_tbb::detail::unique_scoped_lock(m);
        ASSERT_FALSE(m.try_lock());
    }
}

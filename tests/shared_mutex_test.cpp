// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/shared_mutex.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
#include <type_traits>

#include "mutex_traits.hpp"

TEST(SharedMutex, TBBMutexRequirement) {
    using mutex = coopsync_tbb::shared_mutex;

    static_assert(coopsync_tbb::traits::has_scoped_lock_v<mutex>);

    static_assert(
        std::is_default_constructible<typename mutex::scoped_lock>::value);
    static_assert(
        std::is_constructible<typename mutex::scoped_lock, mutex&>::value);
    static_assert(std::is_destructible<typename mutex::scoped_lock>::value);

    static_assert(coopsync_tbb::traits::has_scoped_lock_acquire_v<mutex>);
    static_assert(coopsync_tbb::traits::has_scoped_lock_try_acquire_v<mutex>);
    static_assert(coopsync_tbb::traits::has_scoped_lock_release_v<mutex>);

    static_assert(coopsync_tbb::traits::has_is_rw_mutex_v<mutex>);
    static_assert(coopsync_tbb::traits::has_is_fair_mutex_v<mutex>);
    static_assert(coopsync_tbb::traits::has_is_recursive_mutex_v<mutex>);
}

TEST(SharedMutex, TBBSharedMutexRequirement) {
    using mutex = coopsync_tbb::shared_mutex;

    static_assert(
        std::is_constructible_v<typename mutex::scoped_lock, mutex&, bool>);

    static_assert(coopsync_tbb::traits::has_scoped_lock_v<mutex>);

    static_assert(
        coopsync_tbb::traits::has_scoped_lock_acquire_with_mode_v<mutex>);
    static_assert(
        coopsync_tbb::traits::has_scoped_lock_try_acquire_with_mode_v<mutex>);
    static_assert(
        coopsync_tbb::traits::has_scoped_lock_upgrade_to_writer_v<mutex>);
    static_assert(
        coopsync_tbb::traits::has_scoped_lock_downgrade_to_reader_v<mutex>);
}

TEST(SharedMutex, StdMutexRequirements) {
    using mutex = coopsync_tbb::shared_mutex;
    // BasicLockable
    static_assert(coopsync_tbb::traits::has_lock_v<mutex>);
    static_assert(coopsync_tbb::traits::has_try_lock_v<mutex>);
    // Lockable
    static_assert(coopsync_tbb::traits::has_unlock_v<mutex>);
}

TEST(SharedMutex, StdSharedMutexRequirements) {
    using mutex = coopsync_tbb::shared_mutex;
    // SharedLockable
    static_assert(coopsync_tbb::traits::has_lock_shared_v<mutex>);
    static_assert(coopsync_tbb::traits::has_try_lock_shared_v<mutex>);
    static_assert(coopsync_tbb::traits::has_unlock_shared_v<mutex>);
}

TEST(SharedMutex, NoContentionTryLockWriterOnly) {
    auto m = coopsync_tbb::shared_mutex{};
    ASSERT_TRUE(m.try_lock());
    ASSERT_FALSE(m.try_lock());
    m.unlock();
    ASSERT_TRUE(m.try_lock());
    m.unlock();
}

TEST(SharedMutex, NoContentionLockWriterOnly) {
    auto m = coopsync_tbb::shared_mutex{};
    m.lock();
    ASSERT_FALSE(m.try_lock());
    m.unlock();
    ASSERT_TRUE(m.try_lock());
    m.unlock();
}

TEST(SharedMutex, MutualExclusionParallelIncrementWriterOnly) {
    auto m = coopsync_tbb::shared_mutex{};
    auto counter = 0;
    const auto n = 16;

    tbb::parallel_for(0, n, [&](int) {
        auto lock = coopsync_tbb::shared_mutex::scoped_lock(m);
        ++counter;
    });

    ASSERT_EQ(counter, n);
}

TEST(SharedMutex, NoContentionTryLockAndTryLockShared) {
    auto m = coopsync_tbb::shared_mutex{};

    ASSERT_TRUE(m.try_lock_shared());
    ASSERT_TRUE(m.try_lock_shared());
    ASSERT_FALSE(m.try_lock());
    m.unlock_shared();
    m.unlock_shared();

    ASSERT_TRUE(m.try_lock());
    ASSERT_FALSE(m.try_lock_shared());
    m.unlock();

    ASSERT_TRUE(m.try_lock_shared());
    m.unlock_shared();
}

TEST(SharedMutex, ParallelReadersWritersInvariant) {
    struct data_t {
        int a = 0;
        int b = 0;
    } data;

    auto m = coopsync_tbb::shared_mutex{};
    std::atomic<bool> failed = false;

    constexpr int tasks = 64;
    constexpr int iters = 2000;

    tbb::parallel_for(0, tasks, [&](int tid) {
        for (int i = 0; i < iters; ++i) {
            const bool do_write = ((tid + i) % 16) == 0;
            if (do_write) {
                m.lock();
                ++data.a;
                ++data.b;
                m.unlock();
            } else {
                m.lock_shared();
                if (data.a != data.b) {
                    failed.store(true, std::memory_order_relaxed);
                }
                m.unlock_shared();
            }
        }
    });

    ASSERT_FALSE(failed.load(std::memory_order_relaxed));
}

TEST(SharedMutex, ScopedLockAcquireRelease) {
    {
        auto m = coopsync_tbb::shared_mutex{};
        auto lock = coopsync_tbb::shared_mutex::scoped_lock{};
        lock.acquire(m);
        ASSERT_FALSE(m.try_lock());
        lock.release();
        ASSERT_TRUE(m.try_lock());
        m.unlock();
    }
    {
        auto m = coopsync_tbb::shared_mutex{};
        auto lock = coopsync_tbb::shared_mutex::scoped_lock{m};
        ASSERT_FALSE(m.try_lock());
        lock.release();
        ASSERT_TRUE(m.try_lock());
        m.unlock();
    }
}

TEST(SharedMutex, ScopedLockRAII) {
    auto m = coopsync_tbb::shared_mutex{};
    {
        auto lock = coopsync_tbb::shared_mutex::scoped_lock{m};
        ASSERT_FALSE(m.try_lock());
    }
    ASSERT_TRUE(m.try_lock());
    m.unlock();
}

TEST(SharedMutex, ScopedLockAcquireReleaseShared) {
    {
        auto m = coopsync_tbb::shared_mutex{};
        auto lock = coopsync_tbb::shared_mutex::scoped_lock{};
        lock.acquire(m, false);
        ASSERT_FALSE(m.try_lock());
        lock.release();
        ASSERT_TRUE(m.try_lock());
        m.unlock();
    }
    {
        auto m = coopsync_tbb::shared_mutex{};
        auto lock = coopsync_tbb::shared_mutex::scoped_lock{m, false};
        ASSERT_FALSE(m.try_lock());
        lock.release();
        ASSERT_TRUE(m.try_lock());
        m.unlock();
    }
}

TEST(SharedMutex, ScopedLockRAIIShared) {
    auto m = coopsync_tbb::shared_mutex{};
    {
        auto lock = coopsync_tbb::shared_mutex::scoped_lock{m, false};
        ASSERT_FALSE(m.try_lock());
    }
    ASSERT_TRUE(m.try_lock());
    m.unlock();
}

TEST(SharedMutex, ScopedLockUpgradeDowngrade) {
    auto m = coopsync_tbb::shared_mutex{};
    auto lock = coopsync_tbb::shared_mutex::scoped_lock{m, false};
    ASSERT_FALSE(m.try_lock());
    ASSERT_TRUE(lock.downgrade_to_reader());
    lock.upgrade_to_writer();
    ASSERT_TRUE(lock.upgrade_to_writer());
    ASSERT_FALSE(m.try_lock_shared());
    lock.downgrade_to_reader();
    ASSERT_TRUE(lock.downgrade_to_reader());
    ASSERT_FALSE(m.try_lock());
}

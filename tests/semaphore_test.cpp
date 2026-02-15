#include "coopsync_tbb/semaphore.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

TEST(Semaphore, NoContentionTryAcquireRelease) {
    {
        auto sem = coopsync_tbb::binary_semaphore(1);
        ASSERT_TRUE(sem.try_acquire());
        ASSERT_FALSE(sem.try_acquire());
        sem.release();
        ASSERT_TRUE(sem.try_acquire());
    }
    {
        auto sem = coopsync_tbb::binary_semaphore(0);
        ASSERT_FALSE(sem.try_acquire());
        sem.release();
        ASSERT_TRUE(sem.try_acquire());
    }

    {
        auto sem = coopsync_tbb::counting_semaphore<5>(1);
        ASSERT_TRUE(sem.try_acquire());
        ASSERT_FALSE(sem.try_acquire());
        sem.release();
        ASSERT_TRUE(sem.try_acquire());
    }
    {
        auto sem = coopsync_tbb::counting_semaphore<5>(0);
        ASSERT_FALSE(sem.try_acquire());
        sem.release();
        ASSERT_TRUE(sem.try_acquire());
    }
}

TEST(Semaphore, ReleaseAccumulatesPermits) {
    auto sem = coopsync_tbb::counting_semaphore<5>(0);
    sem.release(3);
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_FALSE(sem.try_acquire());
}

TEST(Semaphore, ContentionAquire) {
    auto sem = coopsync_tbb::counting_semaphore<5>(5);
    tbb::parallel_for(0, 5, [&](int) { sem.acquire(); });
}

TEST(Semaphore, ContentionAcquireRelease) {
    auto sem = coopsync_tbb::binary_semaphore(1);
    auto counter = 0;
    const auto n = 20'000;
    tbb::parallel_for(0, n, [&](int) {
        sem.acquire();
        ++counter;
        sem.release();
    });
    ASSERT_EQ(counter, n);
}

TEST(Semaphore, ContentionAcquireReleaseManyPermits) {
    auto sem = coopsync_tbb::counting_semaphore(1'000);
    auto counter = 0;
    const auto n = 20'000;
    tbb::parallel_for(0, n, [&](int i) {
        if (i % 2 == 0) {
            sem.acquire();
        } else {
            sem.release();
        }
    });
}

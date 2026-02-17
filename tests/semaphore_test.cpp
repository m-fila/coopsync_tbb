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
        const auto lmax = 5;
        auto sem = coopsync_tbb::counting_semaphore<lmax>(1);
        ASSERT_TRUE(sem.try_acquire());
        ASSERT_FALSE(sem.try_acquire());
        sem.release();
        ASSERT_TRUE(sem.try_acquire());
    }
    {
        const auto lmax = 5;
        auto sem = coopsync_tbb::counting_semaphore<lmax>(0);
        ASSERT_FALSE(sem.try_acquire());
        sem.release();
        ASSERT_TRUE(sem.try_acquire());
    }
}

TEST(Semaphore, ReleaseAccumulatesPermits) {
    const auto lmax = 5;
    auto sem = coopsync_tbb::counting_semaphore<lmax>(0);
    sem.release(3);
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_FALSE(sem.try_acquire());
}

TEST(Semaphore, ContentionAquire) {
    const auto lmax = 5;
    const auto permits = 5;
    auto sem = coopsync_tbb::counting_semaphore<lmax>(permits);
    tbb::parallel_for(0, permits, [&](int) { sem.acquire(); });
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
    const auto starting_permits = 1'000;
    auto sem = coopsync_tbb::counting_semaphore(starting_permits);
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

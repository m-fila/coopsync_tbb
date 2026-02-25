#include "coopsync_tbb/semaphore.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

TEST(CountingSemaphore, StorageWidth) {
    // given a value we deduce a type large enough to hold it, and that type's
    // max value is indeed large enough.
    EXPECT_GE(
        std::numeric_limits<
            typename coopsync_tbb::detail::int_max_value_t<1>::type>::max(),
        1);
    EXPECT_GE(
        std::numeric_limits<
            typename coopsync_tbb::detail::int_max_value_t<1000>::type>::max(),
        1000);

    EXPECT_GE(
        std::numeric_limits<typename coopsync_tbb::detail::int_max_value_t<
            1000000000>::type>::max(),
        1000000000);
}

TEST(CountingSemaphore, NoContentionTryAcquireRelease) {
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
        auto sem = coopsync_tbb::counting_semaphore<lmax>(3);
        ASSERT_TRUE(sem.try_acquire());
        ASSERT_TRUE(sem.try_acquire());
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

TEST(BinarySemaphore, NoContentionTryAcquireRelease) {
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
}

TEST(CountingSemaphore, ReleaseAccumulatesPermits) {
    const auto lmax = 5;
    auto sem = coopsync_tbb::counting_semaphore<lmax>(0);
    sem.release(3);
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_TRUE(sem.try_acquire());
    ASSERT_FALSE(sem.try_acquire());
}

TEST(CountingSemaphore, ContentionAcquire) {
    const auto lmax = 5;
    const auto permits = 5;
    auto sem = coopsync_tbb::counting_semaphore<lmax>(permits);
    tbb::parallel_for(0, permits, [&](int) { sem.acquire(); });
}

TEST(CountingSemaphore, ContentionAcquireRelease) {
    const auto lmax = 5;
    const auto permits = 5;
    const auto workers = 10;
    auto sem = coopsync_tbb::counting_semaphore<lmax>(permits);
    tbb::parallel_for(0, workers, [&](int) {
        sem.acquire();
        sem.release();
    });
}

TEST(BinarySemaphore, ContentionAcquireRelease) {
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

TEST(CountingSemaphore, ContentionAcquireReleaseManyPermits) {
    const auto starting_permits = 1'000;
    const auto n = 20'000;
    auto sem = coopsync_tbb::counting_semaphore<n>(starting_permits);

    tbb::parallel_for(0, n, [&](int i) {
        if (i % 2 == 0) {
            sem.acquire();
        } else {
            sem.release();
        }
    });
}

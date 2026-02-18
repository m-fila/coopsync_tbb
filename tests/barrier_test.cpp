#include "coopsync_tbb/barrier.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>

TEST(Barrier, NoContentionArriveThenWait) {
    auto b = coopsync_tbb::barrier(1);
    auto token1 = b.arrive();
    b.wait(std::move(token1));
    auto token2 = b.arrive();
    b.wait(std::move(token2));
    auto token3 = b.arrive();
    b.wait(std::move(token3));
}

TEST(Barrier, NoContentionArriveThenWaitWithCompletion) {
    auto completions = 0;
    auto b = coopsync_tbb::barrier(1, [&] { ++completions; });
    EXPECT_EQ(completions, 0);
    auto token1 = b.arrive();
    b.wait(std::move(token1));
    EXPECT_EQ(completions, 1);
    auto token2 = b.arrive();
    b.wait(std::move(token2));
    EXPECT_EQ(completions, 2);
    auto token3 = b.arrive();
    b.wait(std::move(token3));
    EXPECT_EQ(completions, 3);
}

TEST(Barrier, NoContentionArriveAndWait) {
    auto barrier = coopsync_tbb::barrier(1);
    barrier.arrive_and_wait();
    barrier.arrive_and_wait();
    barrier.arrive_and_wait();
}

TEST(Barrier, NoContentionArriveAndWaitWithCompletion) {
    auto completions = 0;
    auto barrier = coopsync_tbb::barrier(1, [&] { ++completions; });
    EXPECT_EQ(completions, 0);
    barrier.arrive_and_wait();
    EXPECT_EQ(completions, 1);
    barrier.arrive_and_wait();
    EXPECT_EQ(completions, 2);
    barrier.arrive_and_wait();
    EXPECT_EQ(completions, 3);
}

TEST(Barrier, ContentionArriveAndWait) {
    auto barrier = coopsync_tbb::barrier(4);
    auto results = std::array{false, false, false, false};

    tbb::parallel_for(std::size_t{0}, results.size(), [&](std::size_t i) {
        results.at(i) = true;
        barrier.arrive_and_wait();
        for (auto r : results) {
            EXPECT_TRUE(r);
        }
    });
    for (auto r : results) {
        EXPECT_TRUE(r);
    }
}

TEST(Barrier, ContentionArriveAndWaitWithCompletion) {
    const int arrivals_per_phase = 3;
    const int iterations = 6;
    const int multiplier = 2;
    const int expected_completions =
        (iterations * multiplier) / arrivals_per_phase;

    auto completions = std::atomic<int>(0);
    auto barrier =
        coopsync_tbb::barrier(arrivals_per_phase, [&] { ++completions; });
    tbb::parallel_for(0, arrivals_per_phase, [&](int) {
        for (int i = 0; i < iterations * multiplier / arrivals_per_phase; ++i) {
            barrier.arrive_and_wait();
        }
    });
    EXPECT_EQ(completions, expected_completions);
}

TEST(Barrier, ContentionArriveThenDrop) {
    auto completions = std::atomic<int>(0);
    auto barrier = coopsync_tbb::barrier(3, [&] { ++completions; });
    const auto iterations = 5;
    tbb::parallel_for(0, iterations, [&](int i) {
        if (i == 0) {
            barrier.arrive_and_drop();
        } else {
            barrier.arrive_and_wait();
        }
    });
    // 5 iterations, 1 drop, 4 waits, 2 completions
    EXPECT_EQ(completions, 2);
}

#include "coopsync_tbb/latch.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_group.h>

TEST(Latch, TryWait) {
    ASSERT_TRUE(coopsync_tbb::latch(0).try_wait());
    ASSERT_FALSE(coopsync_tbb::latch(1).try_wait());
}

TEST(Latch, NoContentionCountDown) {
    const auto max = 10;
    {
        auto latch = coopsync_tbb::latch(max);
        for (int j = 0; j < max; ++j) {
            ASSERT_FALSE(latch.try_wait());
            latch.count_down();
        }
        ASSERT_TRUE(latch.try_wait());
    }
    {
        auto latch = coopsync_tbb::latch(max);
        ASSERT_FALSE(latch.try_wait());
        latch.count_down(max);
        ASSERT_TRUE(latch.try_wait());
    }
}

TEST(Latch, NoContentionWait) {
    tbb::parallel_for(0, 1, [](int) { coopsync_tbb::latch(0).wait(); });
}

TEST(Latch, ContentionWait) {
    auto arena = tbb::task_arena();
    auto group = tbb::task_group();
    const auto max = 1;
    auto latch = coopsync_tbb::latch(max);
    arena.execute([&] { group.run([&] { latch.wait(); }); });
    arena.execute([&] { group.run([&] { latch.wait(); }); });
    arena.execute([&] { group.run([&] { latch.wait(); }); });
    arena.execute([&] { group.run([&] { latch.count_down(); }); });
    group.wait();
    ASSERT_TRUE(latch.try_wait());
}

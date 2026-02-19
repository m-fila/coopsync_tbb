#include "coopsync_tbb/condition_variable.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
#include <mutex>

#include "coopsync_tbb/mutex.hpp"

TEST(ConditionVariable, NotifyOneWakesWaiter) {
    auto m = coopsync_tbb::mutex{};
    auto cv = coopsync_tbb::condition_variable{};

    auto ready = false;
    auto result = std::atomic<int>{-1};
    const auto expected = 123;

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            auto lock = std::unique_lock<coopsync_tbb::mutex>(m);
            cv.wait(lock, [&]() { return ready; });
            result.store(expected, std::memory_order_relaxed);
        } else {
            {
                auto lock = std::unique_lock<coopsync_tbb::mutex>(m);
                ready = true;
            }
            cv.notify_one();
        }
    });

    ASSERT_EQ(result.load(std::memory_order_relaxed), expected);
}

TEST(ConditionVariable, NotifyAllWakesAllWaiters) {
    auto m = coopsync_tbb::mutex{};
    auto cv = coopsync_tbb::condition_variable{};

    auto ready = false;
    auto woken = std::atomic<int>{0};

    tbb::parallel_for(0, 3, [&](int i) {
        if (i == 0) {
            {
                auto lock = std::unique_lock<coopsync_tbb::mutex>(m);
                ready = true;
            }
            cv.notify_all();
        } else {
            auto lock = std::unique_lock<coopsync_tbb::mutex>(m);
            cv.wait(lock, [&]() { return ready; });
            woken.fetch_add(1, std::memory_order_relaxed);
        }
    });

    ASSERT_EQ(woken.load(std::memory_order_relaxed), 2);
}

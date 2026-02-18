#include "coopsync_tbb/future.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
#include <memory>
#include <stdexcept>

TEST(Future, GetReturnsValue) {
    auto p = std::make_shared<coopsync_tbb::promise<int>>();
    auto f = p->get_future();
    auto fptr = std::make_shared<coopsync_tbb::future<int>>(std::move(f));

    std::atomic<int> result{-1};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            result.store(fptr->get(), std::memory_order_relaxed);
        } else {
            p->set_value(123);
        }
    });

    ASSERT_EQ(result.load(std::memory_order_relaxed), 123);
}

TEST(Future, BrokenPromiseThrows) {
    coopsync_tbb::future<int> f;
    {
        auto p = coopsync_tbb::promise<int>{};
        f = p.get_future();
        // p is destroyed without set_value
    }

    EXPECT_THROW((void)f.get(), coopsync_tbb::future_error);
}

TEST(Future, ExceptionPropagates) {
    auto p = coopsync_tbb::promise<int>{};
    auto f = p.get_future();

    p.set_exception(std::make_exception_ptr(std::runtime_error("boom")));

    EXPECT_THROW((void)f.get(), std::runtime_error);
}

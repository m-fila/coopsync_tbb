#include "coopsync_tbb/future.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
#include <cstddef>
#include <stdexcept>

TEST(Future, NoStateThrows) {

    auto f = coopsync_tbb::future<int>{};
    EXPECT_FALSE(f.valid());
    EXPECT_THROW((void)f.get(), coopsync_tbb::future_error);
    EXPECT_THROW(f.wait(), coopsync_tbb::future_error);
}

TEST(Future, FutureAlreadyRetrievedThrows) {
    auto p = coopsync_tbb::promise<int>{};
    auto f1 = p.get_future();
    EXPECT_THROW((void)p.get_future(), coopsync_tbb::future_error);
}

TEST(Future, PromiseAlreadySatisfiedThrows) {
    auto p = coopsync_tbb::promise<int>{};
    auto f = p.get_future();
    p.set_value(-1);
    EXPECT_THROW(p.set_value(1), coopsync_tbb::future_error);
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

TEST(Future, GetReturnsValue) {
    auto p = coopsync_tbb::promise<int>();
    auto f = p.get_future();

    auto result = std::atomic<int>{-1};
    const auto expected = 123;

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            result.store(f.get(), std::memory_order_relaxed);
        } else {
            p.set_value(expected);
        }
    });

    ASSERT_EQ(result.load(std::memory_order_relaxed), expected);
}

TEST(FutureVoid, GetUnblocksAfterSetValue) {
    auto p = coopsync_tbb::promise<void>();
    auto f = p.get_future();

    std::atomic<bool> done{false};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            f.get();
            done.store(true, std::memory_order_relaxed);
        } else {
            p.set_value();
        }
    });

    ASSERT_TRUE(done.load(std::memory_order_relaxed));
}

TEST(FutureRef, GetReturnsReference) {
    auto p = coopsync_tbb::promise<int&>();
    auto f = p.get_future();

    auto x = -1;
    const auto expected = 42;
    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            int& r = f.get();
            r = expected;
        } else {
            p.set_value(x);
        }
    });

    ASSERT_EQ(x, expected);
}

TEST(SharedFuture, ShareInvalidatesFutureAndAllowsMultipleGets) {
    auto p = coopsync_tbb::promise<int>();
    auto f = p.get_future();
    auto sf = f.share();

    ASSERT_FALSE(f.valid());
    ASSERT_TRUE(sf.valid());

    auto sf2 = sf;

    std::atomic<int> r1{-1};
    std::atomic<int> r2{-1};
    const auto expected = 123;

    tbb::parallel_for(0, 3, [&](int i) {
        if (i == 0) {
            r1.store(sf.get(), std::memory_order_relaxed);
        } else if (i == 1) {
            r2.store(sf2.get(), std::memory_order_relaxed);
        } else {
            p.set_value(expected);
        }
    });

    ASSERT_EQ(r1.load(std::memory_order_relaxed), expected);
    ASSERT_EQ(r2.load(std::memory_order_relaxed), expected);
}

TEST(SharedFutureVoid, MultipleGetsDoNotConsume) {
    auto p = coopsync_tbb::promise<void>();
    auto f = p.get_future();
    auto sf = f.share();
    auto sf2 = sf;

    std::atomic<int> done{0};
    tbb::parallel_for(0, 3, [&](int i) {
        if (i == 0) {
            sf.get();
            done.fetch_add(1, std::memory_order_relaxed);
        } else if (i == 1) {
            sf2.get();
            done.fetch_add(1, std::memory_order_relaxed);
        } else {
            p.set_value();
        }
    });

    ASSERT_EQ(done.load(std::memory_order_relaxed), 2);
}

TEST(SharedFutureRef, GetReturnsSameReference) {
    auto p = coopsync_tbb::promise<int&>();
    auto f = p.get_future();
    auto sf = f.share();
    auto shared_futures = std::array{sf, sf, sf, sf};
    int x = 0;

    tbb::parallel_for(size_t{0}, shared_futures.size() + 1, [&](size_t i) {
        if (shared_futures.size() == i) {
            p.set_value(x);
        } else {
            auto& r = shared_futures.at(i).get();
            EXPECT_EQ(&r, &x);
        }
    });
}

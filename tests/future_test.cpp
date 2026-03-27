#include "coopsync_tbb/future.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>

TEST(Future, NoStateThrows) {

    auto f = coopsync_tbb::future<int>{};
    EXPECT_FALSE(f.valid());
    EXPECT_THROW(f.get(), coopsync_tbb::future_error);
    EXPECT_THROW(f.wait(), coopsync_tbb::future_error);
}

TEST(Future, FutureAlreadyRetrievedThrows) {
    auto p = coopsync_tbb::promise<int>{};
    auto f1 = p.get_future();
    EXPECT_THROW(std::ignore = p.get_future(), coopsync_tbb::future_error);
}

TEST(Future, PromiseAlreadySatisfiedThrows) {
    auto p = coopsync_tbb::promise<int>{};
    auto f = p.get_future();
    p.set_value(-1);
    EXPECT_THROW(p.set_value(1), coopsync_tbb::future_error);
    EXPECT_THROW(
        p.set_exception(std::make_exception_ptr(std::runtime_error("boom"))),
        coopsync_tbb::future_error);
}

TEST(Future, BrokenPromiseThrows) {
    coopsync_tbb::future<int> f;
    {
        auto p = coopsync_tbb::promise<int>{};
        f = p.get_future();
        // p is destroyed without set_value
    }

    EXPECT_THROW(f.get(), coopsync_tbb::future_error);
}

TEST(Future, ExceptionPropagates) {
    auto p = coopsync_tbb::promise<int>{};
    auto f = p.get_future();

    p.set_exception(std::make_exception_ptr(std::runtime_error("boom")));

    EXPECT_THROW(f.get(), std::runtime_error);
}

TEST(Future, GetReturnsValue) {
    auto p = coopsync_tbb::promise<int>();
    auto f = p.get_future();

    auto result = std::atomic<int>{-1};
    const auto expected = 123;

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            result.store(f.get(), std::memory_order_relaxed);
            EXPECT_FALSE(f.valid());  // get invalidates the future
        } else {
            p.set_value(expected);
        }
    });

    ASSERT_EQ(result.load(std::memory_order_relaxed), expected);
}

TEST(Future, NoContentionWaitGet) {
    {
        auto p = coopsync_tbb::promise<int>();
        auto f = p.get_future();
        p.set_value(1);
        f.wait();
        EXPECT_TRUE(f.valid());  // wait does not invalidate the future
        ASSERT_EQ(f.get(), 1);
        EXPECT_FALSE(f.valid());  // get invalidates the future
    }
}

TEST(Future, WaitUnblocksAfterSetValue) {
    auto p = coopsync_tbb::promise<int>();
    auto f = p.get_future();

    std::atomic<bool> done{false};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            f.wait();
            EXPECT_TRUE(f.valid());  // wait does not invalidate the future
            done.store(true, std::memory_order_relaxed);
        } else {
            p.set_value(1);
        }
    });

    ASSERT_TRUE(done.load(std::memory_order_relaxed));
}

TEST(FutureVoid, PromiseAlreadySatisfiedThrows) {
    auto p = coopsync_tbb::promise<void>{};
    auto f = p.get_future();
    p.set_value();
    EXPECT_THROW(p.set_value(), coopsync_tbb::future_error);
    EXPECT_THROW(
        p.set_exception(std::make_exception_ptr(std::runtime_error("boom"))),
        coopsync_tbb::future_error);
}

TEST(FutureVoid, GetUnblocksAfterSetValue) {
    auto p = coopsync_tbb::promise<void>();
    auto f = p.get_future();

    std::atomic<bool> done{false};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            f.get();
            EXPECT_FALSE(f.valid());  // get invalidates the future
            done.store(true, std::memory_order_relaxed);
        } else {
            p.set_value();
        }
    });

    ASSERT_TRUE(done.load(std::memory_order_relaxed));
}

TEST(FutureRef, PromiseAlreadySatisfiedThrows) {
    auto p = coopsync_tbb::promise<int&>{};
    auto f = p.get_future();
    int x = 0;
    p.set_value(x);
    EXPECT_THROW(p.set_value(x), coopsync_tbb::future_error);
    EXPECT_THROW(
        p.set_exception(std::make_exception_ptr(std::runtime_error("boom"))),
        coopsync_tbb::future_error);
}

TEST(FutureRef, GetReturnsReference) {
    auto p = coopsync_tbb::promise<int&>();
    auto f = p.get_future();

    auto x = -1;
    const auto expected = 42;
    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            int& r = f.get();
            EXPECT_FALSE(f.valid());  // get invalidates the future
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

TEST(PackagedTask, DefaultConstructedInvalid) {
    auto task = coopsync_tbb::packaged_task<int()>();
    EXPECT_FALSE(task.valid());
    EXPECT_THROW(std::ignore = task.get_future(), coopsync_tbb::future_error);
    EXPECT_THROW(task(), coopsync_tbb::future_error);
}

TEST(PackagedTask, EmptyStdFunctionStillAllocatesState) {
    auto fn = std::function<int()>{};
    auto task = coopsync_tbb::packaged_task<int()>(fn);

    EXPECT_TRUE(task.valid());
    auto f = task.get_future();

    task();
    EXPECT_THROW(f.get(), std::bad_function_call);
}

TEST(PackagedTask, GetFutureOnlyOnce) {
    const auto expected = 123;
    auto task = coopsync_tbb::packaged_task<int()>([] { return expected; });
    auto f = task.get_future();
    EXPECT_THROW(std::ignore = task.get_future(), coopsync_tbb::future_error);
}

TEST(PackagedTask, RunsAndSetsValue) {
    const auto input = 41;
    const auto expected = 42;
    auto task =
        coopsync_tbb::packaged_task<int(int)>([](int x) { return x + 1; });
    auto f = task.get_future();

    std::atomic<int> result{-1};
    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            result.store(f.get(), std::memory_order_relaxed);
        } else {
            task(input);
        }
    });

    ASSERT_EQ(result.load(std::memory_order_relaxed), expected);
}

TEST(PackagedTask, RunsAndUnblocksFuture) {
    auto x = std::atomic<int>{0};
    auto task = coopsync_tbb::packaged_task<void()>([&] { x.fetch_add(1); });
    auto f = task.get_future();

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            f.get();
        } else {
            task();
        }
    });

    ASSERT_EQ(x.load(), 1);
}

TEST(PackagedTask, ExceptionIsStoredInFuture) {
    auto task = coopsync_tbb::packaged_task<int()>(
        []() -> int { throw std::runtime_error("boom"); });
    auto f = task.get_future();

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            EXPECT_THROW(f.get(), std::runtime_error);
        } else {
            EXPECT_NO_THROW(task());
        }
    });
}

TEST(PackagedTask, ResetCreatesNewSharedState) {
    auto counter = 0;
    auto task = coopsync_tbb::packaged_task<int()>([&] { return ++counter; });

    auto f1 = task.get_future();
    task();
    ASSERT_EQ(f1.get(), 1);

    task.reset();
    auto f2 = task.get_future();
    task();
    ASSERT_EQ(f2.get(), 2);
}

TEST(PackagedTask, ResetWithNoStateThrows) {
    auto task = coopsync_tbb::packaged_task<int()>();
    EXPECT_THROW(task.reset(), coopsync_tbb::future_error);
}

TEST(PackagedTask, MoveTransfersValidity) {
    const auto expected = 7;
    auto task1 = coopsync_tbb::packaged_task<int()>([] { return expected; });
    auto f = task1.get_future();

    auto task2 = std::move(task1);
    EXPECT_TRUE(task2.valid());

    task2();
    ASSERT_EQ(f.get(), expected);
}

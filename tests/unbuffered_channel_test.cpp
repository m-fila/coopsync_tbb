// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/unbuffered_channel.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
#include <string>
#include <tuple>

using channel_op_status =
    coopsync_tbb::unbuffered_channel<int>::channel_op_status;

TEST(UnbufferedChannel, CloseWithNoWaiters) {
    auto channel = coopsync_tbb::unbuffered_channel<int>{};
    channel.close();

    auto value = 0;
    ASSERT_EQ(channel.push(1), channel_op_status::closed);
    ASSERT_EQ(channel.pop(value), channel_op_status::closed);
}

TEST(UnbufferedChannel, PopThrowsOnClosedChannel) {
    auto channel = coopsync_tbb::unbuffered_channel<int>{};
    channel.close();
    ASSERT_THROW(std::ignore = channel.pop(), std::runtime_error);
}

TEST(UnbufferedChannel, RendezvousPushPop) {
    auto channel = coopsync_tbb::unbuffered_channel<int>{};
    const auto expected = 42;
    auto received = std::atomic<int>{-1};
    auto push_status = channel_op_status::closed;
    auto pop_status = channel_op_status::closed;

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            push_status = channel.push(expected);
        } else {
            auto value = 0;
            pop_status = channel.pop(value);
            received.store(value, std::memory_order_relaxed);
        }
    });

    ASSERT_EQ(push_status, channel_op_status::success);
    ASSERT_EQ(pop_status, channel_op_status::success);
    ASSERT_EQ(received.load(std::memory_order_relaxed), expected);
}

TEST(UnbufferedChannel, RendezvousPushRvaluePop) {
    auto channel = coopsync_tbb::unbuffered_channel<std::string>{};
    const auto expected = std::string("hello");
    auto received = std::string{};
    auto push_status = coopsync_tbb::unbuffered_channel<
        std::string>::channel_op_status::closed;
    auto pop_status = push_status;

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            push_status = channel.push(std::string(expected));
        } else {
            pop_status = channel.pop(received);
        }
    });

    ASSERT_EQ(push_status, decltype(push_status)::success);
    ASSERT_EQ(pop_status, decltype(pop_status)::success);
    ASSERT_EQ(received, expected);
}

TEST(UnbufferedChannel, PopReturnsThrownValue) {
    auto channel = coopsync_tbb::unbuffered_channel<int>{};
    const auto expected = 7;
    auto received = std::atomic<int>{-1};

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            std::ignore = channel.push(expected);
        } else {
            received.store(channel.pop(), std::memory_order_relaxed);
        }
    });

    ASSERT_EQ(received.load(std::memory_order_relaxed), expected);
}

TEST(UnbufferedChannel, CloseWakesPendingPush) {
    auto channel = coopsync_tbb::unbuffered_channel<int>{};
    auto push_status = channel_op_status::success;

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            push_status = channel.push(1);
        } else {
            channel.close();
        }
    });

    ASSERT_EQ(push_status, channel_op_status::closed);
}

TEST(UnbufferedChannel, CloseWakesPendingPop) {
    auto channel = coopsync_tbb::unbuffered_channel<int>{};
    auto pop_status = channel_op_status::success;

    tbb::parallel_for(0, 2, [&](int i) {
        if (i == 0) {
            auto value = 0;
            pop_status = channel.pop(value);
        } else {
            channel.close();
        }
    });

    ASSERT_EQ(pop_status, channel_op_status::closed);
}

TEST(UnbufferedChannel, ContentionMultipleSendersReceivers) {
    auto channel = coopsync_tbb::unbuffered_channel<int>{};
    const auto pairs = 8;
    auto sent = std::atomic<int>{0};
    auto received_sum = std::atomic<int>{0};

    tbb::parallel_for(0, pairs * 2, [&](int i) {
        if (i < pairs) {
            ASSERT_EQ(channel.push(i), channel_op_status::success);
            sent.fetch_add(1, std::memory_order_relaxed);
        } else {
            auto value = 0;
            ASSERT_EQ(channel.pop(value), channel_op_status::success);
            received_sum.fetch_add(value, std::memory_order_relaxed);
        }
    });

    ASSERT_EQ(sent.load(std::memory_order_relaxed), pairs);
    const auto expected_sum = pairs * (pairs - 1) / 2;
    ASSERT_EQ(received_sum.load(std::memory_order_relaxed), expected_sum);
}

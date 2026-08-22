// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/alpaka/wait_for.hpp"

#include <gtest/gtest.h>

#include <alpaka/alpaka.hpp>
#include <cstdio>

template <typename T>
class AlpakaTest : public ::testing::Test {
    protected:
    using Tag = T;
    using Dim = alpaka::DimInt<1u>;
    using Idx = std::size_t;
    using Acc = alpaka::TagToAcc<Tag, Dim, Idx>;
    using Queue = alpaka::Queue<Acc, alpaka::NonBlocking>;

    static auto device() {
        auto const platform = alpaka::Platform<Acc>{};
        return alpaka::getDevByIdx(platform, 0);
    }
};

template <typename Tuple>
struct MakeTestQueueTypes;

template <typename... Ts>
struct MakeTestQueueTypes<std::tuple<Ts...>> {
    using type = ::testing::Types<Ts...>;
};

using test_queue_types = MakeTestQueueTypes<alpaka::EnabledAccTags>::type;
TYPED_TEST_SUITE(AlpakaTest, test_queue_types);

TYPED_TEST(AlpakaTest, MemoryOperations) {
    using Acc = typename TestFixture::Acc;
    using Queue = typename TestFixture::Queue;
    using Dim = typename TestFixture::Dim;
    using Idx = typename TestFixture::Idx;

    using Data = int;
    constexpr auto size = Idx(1000);

    const auto devAcc = TestFixture::device();

    using DevAcc = alpaka::Dev<Acc>;
    using DevHost = alpaka::DevCpu;

    const auto platformHost = alpaka::PlatformCpu{};
    const auto devHost = alpaka::getDevByIdx(platformHost, 0);

    auto queue = Queue(devAcc);

    const auto extent = alpaka::Vec<Dim, Idx>{size};

    using BufHost = alpaka::Buf<DevHost, Data, Dim, Idx>;
    using BufAcc = alpaka::Buf<DevAcc, Data, Dim, Idx>;

    auto host = alpaka::allocBuf<Data, Idx>(devHost, extent);
    auto device = alpaka::allocBuf<Data, Idx>(devAcc, extent);

    for (auto i = Idx(0); i < size; ++i) {
        host[i] = static_cast<Data>(i);
    }

    alpaka::memcpy(queue, device, host);
    alpaka::memset(queue, device, 0);
    alpaka::memcpy(queue, host, device);

    auto event = alpaka::Event<Queue>(devAcc);
    alpaka::enqueue(queue, event);
    coopsync_tbb::alpaka::wait_for(queue);
    ASSERT_TRUE(alpaka::isComplete(event));

    for (auto i = Idx(0); i < size; ++i) {
        ASSERT_EQ(host[i], 0);
    }
}

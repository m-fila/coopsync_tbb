// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/alpaka/wait_for.hpp"

#include <gtest/gtest.h>

#include <alpaka/alpaka.hpp>

template <typename Tuple>
struct MakeTestBackendTypes;

template <typename... Backends>
struct MakeTestBackendTypes<std::tuple<Backends...>> {
    using type = ::testing::Types<Backends...>;
};

template <typename... Backends>
struct MakeTestBackendTypes<std::tuple<Backends...> const> {
    using type = ::testing::Types<Backends...>;
};

using test_backend_types =
    MakeTestBackendTypes<decltype(alpaka::onHost::enabledDeviceSpecs)>::type;

template <typename T>
class AlpakaTest : public ::testing::Test {
    protected:
    using Backend = T;
};

TYPED_TEST_SUITE(AlpakaTest, test_backend_types);

TYPED_TEST(AlpakaTest, MemoryOperations) {
    using Idx = std::size_t;
    using Data = int;
    constexpr auto size = Idx(1000);

    auto selector =
        alpaka::onHost::makeDeviceSelector(typename TestFixture::Backend{});
    if (!selector.isAvailable()) {
        GTEST_SKIP() << "Backend is unavailable";
    }

    auto device = selector.makeDevice(0);
    auto queue = device.makeQueue();
    auto const extent = alpaka::Vec<Idx, 1u>{size};
    auto host = alpaka::onHost::allocHost<Data>(extent);
    auto deviceBuffer = alpaka::onHost::allocLike(device, host);

    for (auto i = Idx(0); i < size; ++i) {
        host[i] = static_cast<Data>(i);
    }

    alpaka::onHost::memcpy(queue, deviceBuffer, host);
    alpaka::onHost::memset(queue, deviceBuffer, uint8_t{0});
    alpaka::onHost::memcpy(queue, host, deviceBuffer);
    coopsync_tbb::alpaka::wait_for(queue);

    for (auto i = Idx(0); i < size; ++i) {
        EXPECT_EQ(host[i], 0);
    }
}

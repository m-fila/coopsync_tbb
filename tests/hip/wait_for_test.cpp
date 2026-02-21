#include "coopsync_tbb/hip/wait_for.hpp"

#include <gtest/gtest.h>
#include <hip/hip_runtime_api.h>

#include <cstddef>

#include "nanospin.hpp"

TEST(HIPWaitFor, KernelCompletion) {
    ASSERT_EQ(hipSetDevice(0), hipSuccess);
    // Create a HIP stream and event
    auto stream = hipStream_t{};
    ASSERT_EQ(hipStreamCreate(&stream), hipSuccess);
    auto event = hipEvent_t{};
    ASSERT_EQ(hipEventCreate(&event), hipSuccess);
    // Spin for some time and record event
    const auto ns = 1'000'000;
    launch_nanospin(ns, stream);
    ASSERT_EQ(hipGetLastError(), hipSuccess);
    ASSERT_EQ(hipEventRecord(event, stream), hipSuccess);
    // Wait for the event to complete
    ASSERT_EQ(coopsync_tbb::hip::wait_for(stream), hipSuccess);
    ASSERT_EQ(hipEventQuery(event), hipSuccess);
    ASSERT_EQ(hipStreamSynchronize(stream),
              hipSuccess);  // Ensure all the errors are propagated
    ASSERT_EQ(hipGetLastError(), hipSuccess);
    // Clean up
    ASSERT_EQ(hipEventDestroy(event), hipSuccess);
    ASSERT_EQ(hipStreamDestroy(stream), hipSuccess);
}

TEST(HIPWaitFor, MemoryOperations) {
    const std::size_t size = 100;

    ASSERT_EQ(hipSetDevice(0), hipSuccess);
    auto stream = hipStream_t{};
    ASSERT_EQ(hipStreamCreate(&stream), hipSuccess);
    auto event = hipEvent_t{};
    ASSERT_EQ(hipEventCreate(&event), hipSuccess);

    std::uint32_t* host = nullptr;
    ASSERT_EQ(
        hipHostMalloc(
            reinterpret_cast<  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                void**>(&host),
            size * sizeof(int)),
        hipSuccess);

    std::uint32_t* device = nullptr;
    ASSERT_EQ(
        hipMalloc(
            reinterpret_cast<  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                void**>(&device),
            size * sizeof(int)),
        hipSuccess);

    for (std::size_t i = 0; i < size; ++i) {
        host[i] =  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            static_cast<int>(i);
    }

    ASSERT_EQ(hipMemcpyAsync(device, host, size * sizeof(int),
                             hipMemcpyHostToDevice, stream),
              hipSuccess);
    // Mutate device buffer so the DtH copy must complete and change host.
    ASSERT_EQ(hipMemsetAsync(device, 0, size * sizeof(int), stream),
              hipSuccess);
    ASSERT_EQ(hipMemcpyAsync(host, device, size * sizeof(int),
                             hipMemcpyDeviceToHost, stream),
              hipSuccess);
    ASSERT_EQ(hipEventRecord(event, stream), hipSuccess);
    ASSERT_EQ(coopsync_tbb::hip::wait_for(stream), hipSuccess);
    ASSERT_EQ(hipEventQuery(event), hipSuccess);
    ASSERT_EQ(hipStreamSynchronize(stream),
              hipSuccess);  // Ensure all the errors are propagated
    ASSERT_EQ(hipGetLastError(), hipSuccess);
    for (std::size_t i = 0; i < size; ++i) {
        ASSERT_EQ(
            host[i],  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            0);
    }

    ASSERT_EQ(hipFree(device), hipSuccess);
    ASSERT_EQ(hipFreeHost(host), hipSuccess);
    ASSERT_EQ(hipEventDestroy(event), hipSuccess);
    ASSERT_EQ(hipStreamDestroy(stream), hipSuccess);
}

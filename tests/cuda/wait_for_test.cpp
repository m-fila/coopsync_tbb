#include "coopsync_tbb/cuda/wait_for.hpp"

#include <cuda_runtime_api.h>
#include <gtest/gtest.h>

#include <cstddef>

#include "nanospin.hpp"

TEST(CUDAWaitFor, KernelCompletion) {
    ASSERT_EQ(cudaSetDevice(0), cudaSuccess);
    // Create a CUDA stream and event
    auto stream = cudaStream_t{};
    ASSERT_EQ(cudaStreamCreate(&stream), cudaSuccess);
    auto event = cudaEvent_t{};
    ASSERT_EQ(cudaEventCreate(&event), cudaSuccess);
    // Spin for some time and record event
    const auto ns = 1'000'000;
    launch_nanospin(ns, stream);
    ASSERT_EQ(cudaGetLastError(), cudaSuccess);
    ASSERT_EQ(cudaEventRecord(event, stream), cudaSuccess);
    // Wait for the event to complete
    ASSERT_EQ(coopsync_tbb::cuda::wait_for(stream), cudaSuccess);
    ASSERT_EQ(cudaEventQuery(event), cudaSuccess);
    ASSERT_EQ(cudaStreamSynchronize(stream),
              cudaSuccess);  // Ensure all the errors are propagated
    ASSERT_EQ(cudaGetLastError(), cudaSuccess);
    // Clean up
    ASSERT_EQ(cudaEventDestroy(event), cudaSuccess);
    ASSERT_EQ(cudaStreamDestroy(stream), cudaSuccess);
}

TEST(CUDAWaitFor, MemoryOperations) {
    const std::size_t size = 100;

    ASSERT_EQ(cudaSetDevice(0), cudaSuccess);
    auto stream = cudaStream_t{};
    ASSERT_EQ(cudaStreamCreate(&stream), cudaSuccess);
    auto event = cudaEvent_t{};
    ASSERT_EQ(cudaEventCreate(&event), cudaSuccess);

    std::uint32_t* host = nullptr;
    ASSERT_EQ(
        cudaMallocHost(
            reinterpret_cast<  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                void**>(&host),
            size * sizeof(int)),
        cudaSuccess);

    std::uint32_t* device = nullptr;
    ASSERT_EQ(
        cudaMalloc(
            reinterpret_cast<  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                void**>(&device),
            size * sizeof(int)),
        cudaSuccess);

    for (std::size_t i = 0; i < size; ++i) {
        host[i] =  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            static_cast<int>(i);
    }

    ASSERT_EQ(cudaMemcpyAsync(device, host, size * sizeof(int),
                              cudaMemcpyHostToDevice, stream),
              cudaSuccess);
    // Mutate device buffer so the DtH copy must complete and change host.
    ASSERT_EQ(cudaMemsetAsync(device, 0, size * sizeof(int), stream),
              cudaSuccess);
    ASSERT_EQ(cudaMemcpyAsync(host, device, size * sizeof(int),
                              cudaMemcpyDeviceToHost, stream),
              cudaSuccess);
    ASSERT_EQ(cudaEventRecord(event, stream), cudaSuccess);
    ASSERT_EQ(coopsync_tbb::cuda::wait_for(stream), cudaSuccess);
    ASSERT_EQ(cudaEventQuery(event), cudaSuccess);
    ASSERT_EQ(cudaStreamSynchronize(stream),
              cudaSuccess);  // Ensure all the errors are propagated
    ASSERT_EQ(cudaGetLastError(), cudaSuccess);
    for (std::size_t i = 0; i < size; ++i) {
        ASSERT_EQ(
            host[i],  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            0);
    }

    ASSERT_EQ(cudaFree(device), cudaSuccess);
    ASSERT_EQ(cudaFreeHost(host), cudaSuccess);
    ASSERT_EQ(cudaEventDestroy(event), cudaSuccess);
    ASSERT_EQ(cudaStreamDestroy(stream), cudaSuccess);
}

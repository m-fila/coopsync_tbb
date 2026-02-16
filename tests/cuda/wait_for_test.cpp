#include "coopsync_tbb/cuda/wait_for.hpp"

#include <cuda_runtime_api.h>
#include <gtest/gtest.h>

#include "nanospin.hpp"

TEST(CUDAWaitFor, BasicTest) {
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
    ASSERT_EQ(cudaStreamSynchronize(stream), cudaSuccess);
    // Clean up
    ASSERT_EQ(cudaEventDestroy(event), cudaSuccess);
    ASSERT_EQ(cudaStreamDestroy(stream), cudaSuccess);
}

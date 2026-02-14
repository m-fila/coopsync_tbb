#include "coopsync_tbb/cuda/wait_for.hpp"

#include <cuda_runtime_api.h>
#include <gtest/gtest.h>

#include "nanospin.hpp"

TEST(CUDAWaitFor, BasicTest) {
    ASSERT_EQ(cudaSetDevice(0), cudaSuccess);
    // Create a CUDA stream and event
    cudaStream_t stream;
    ASSERT_EQ(cudaStreamCreate(&stream), cudaSuccess);
    cudaEvent_t event;
    ASSERT_EQ(cudaEventCreate(&event), cudaSuccess);
    // Spin for some time and record event
    launch_nanospin(1'000'000, stream);
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

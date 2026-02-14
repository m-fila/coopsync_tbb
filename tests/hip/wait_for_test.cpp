#include "coopsync_tbb/hip/wait_for.hpp"

#include <gtest/gtest.h>
#include <hip/hip_runtime_api.h>

#include "nanospin.hpp"

TEST(HIPWaitFor, BasicTest) {
    ASSERT_EQ(hipSetDevice(0), hipSuccess);
    // Create a HIP stream and event
    hipStream_t stream;
    ASSERT_EQ(hipStreamCreate(&stream), hipSuccess);
    hipEvent_t event;
    ASSERT_EQ(hipEventCreate(&event), hipSuccess);
    // Spin for some time and record event
    launch_nanospin(1'000'000, stream);
    ASSERT_EQ(hipGetLastError(), hipSuccess);
    ASSERT_EQ(hipEventRecord(event, stream), hipSuccess);
    // Wait for the event to complete
    ASSERT_EQ(coopsync_tbb::hip::wait_for(stream), hipSuccess);
    ASSERT_EQ(hipEventQuery(event), hipSuccess);
    ASSERT_EQ(hipStreamSynchronize(stream), hipSuccess);
    // Clean up
    ASSERT_EQ(hipEventDestroy(event), hipSuccess);
    ASSERT_EQ(hipStreamDestroy(stream), hipSuccess);
}

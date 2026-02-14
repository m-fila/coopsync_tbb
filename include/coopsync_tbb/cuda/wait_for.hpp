#pragma once
#include <cuda_runtime_api.h>
#include <oneapi/tbb/task.h>

/// @brief CUDA integration.
namespace coopsync_tbb::cuda {

namespace detail {
/// @brief CUDA callback to resume a suspended TBB task.
/// @param tag Pointer to the suspend point of the task to resume.
///
static inline void resumption_callback(void *tag) {
    tbb::task::resume(*static_cast<tbb::task::suspend_point *>(tag));
}
}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a CUDA stream
/// completes. Internally a CUDA callback is used to resume the task.
/// @param stream CUDA stream.
/// @return The CUDA error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
///
static inline cudaError_t wait_for(cudaStream_t stream) {
    tbb::task::suspend_point suspend_point;
    cudaError_t err;
    tbb::task::suspend([stream, &err, &suspend_point](auto tag) {
        suspend_point = tag;
        err = cudaLaunchHostFunc(stream, detail::resumption_callback,
                                 &suspend_point);
        if (err != cudaSuccess) {
            detail::resumption_callback(&suspend_point);
        }
    });
    return err;
}

}  // namespace coopsync_tbb::cuda

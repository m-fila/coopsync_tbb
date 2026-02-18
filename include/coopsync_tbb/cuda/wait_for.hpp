#pragma once
#include <cuda_runtime_api.h>
#include <oneapi/tbb/task.h>

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define COOPSYNC_TOOLS_CUDA_NODISCARD [[nodiscard]]
#endif
#else
#if __cplusplus > 201603L
#define COOPSYNC_TOOLS_CUDA_NODISCARD [[nodiscard]]
#endif
#endif
#ifndef COOPSYNC_TOOLS_CUDA_NODISCARD
#define COOPSYNC_TOOLS_CUDA_NODISCARD
#endif

/// @brief CUDA integration.
namespace coopsync_tbb::cuda {

namespace detail {
/// @brief CUDA callback to resume a suspended TBB task.
/// @param tag Pointer to the suspend point of the task to resume.
///
static inline void resumption_callback(void* tag) {
    tbb::task::resume(*static_cast<tbb::task::suspend_point*>(tag));
}
}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a CUDA stream
/// completes. Internally a CUDA callback is used to resume the task.
/// @param stream CUDA stream.
/// @return The CUDA error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
///
COOPSYNC_TOOLS_CUDA_NODISCARD static inline cudaError_t wait_for(
    cudaStream_t stream) {
    auto suspend_point = tbb::task::suspend_point{};
    auto err = cudaSuccess;
    tbb::task::suspend(
        [stream, &err, &suspend_point](tbb::task::suspend_point tag) {
            suspend_point = tag;
            // Note: cudaStreamAddCallback is pending for deprecation, using
            // cudaLaunchHostFunc instead
            err = cudaLaunchHostFunc(stream, detail::resumption_callback,
                                     &suspend_point);
            if (err != cudaSuccess) {
                detail::resumption_callback(&suspend_point);
            }
        });
    return err;
}

}  // namespace coopsync_tbb::cuda

#undef COOPSYNC_TOOLS_CUDA_NODISCARD

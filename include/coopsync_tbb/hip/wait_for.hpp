#pragma once
#include <hip/hip_runtime_api.h>
#include <oneapi/tbb/task.h>

/// @brief HIP integration.
namespace coopsync_tbb::hip {

namespace detail {

/// @brief Context for HIP callback to resume a suspended TBB task.
struct context {
    tbb::task::suspend_point suspend_point;
    hipError_t err;
};

/// @brief HIP callback to resume a suspended TBB task.
/// @param stream HIP stream.
/// @param err HIP error code of the completed stream work.
/// @param data Pointer to the context of the task to resume.
///
static inline void resumption_callback(hipStream_t, hipError_t err,
                                       void* data) {
    detail::context* ctx = static_cast<detail::context*>(data);
    ctx->err = err;
    tbb::task::resume(ctx->suspend_point);
}

}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a HIP stream
/// completes. Internally a HIP callback is used to resume the task.
/// @param stream HIP stream.
/// @return The HIP error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
///
static inline hipError_t wait_for(hipStream_t stream) {
    detail::context ctx;
    tbb::task::suspend([stream, &ctx](tbb::task::suspend_point tag) {
        ctx.suspend_point = tag;
        // Note: hipLaunchHostFunc is beta, using hipStreamAddCallback for now
        auto err =
            hipStreamAddCallback(stream, detail::resumption_callback, &ctx, 0);
        if (err != hipSuccess) {
            detail::resumption_callback(stream, err, &ctx);
        }
    });
    return ctx.err;
}

}  // namespace coopsync_tbb::hip

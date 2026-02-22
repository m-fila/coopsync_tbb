#pragma once
#include <hip/hip_runtime_api.h>
#include <oneapi/tbb/task.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

// clang-format off
#ifndef COOPSYNC_TBB_HIP_NODISCARD
  #if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
      #define COOPSYNC_TBB_HIP_NODISCARD [[nodiscard]]
    #endif
  #elif defined(__cplusplus) && __cplusplus >= 201703L
    #define COOPSYNC_TBB_HIP_NODISCARD [[nodiscard]]
  #endif
  #ifndef COOPSYNC_TBB_HIP_NODISCARD
    #define COOPSYNC_TBB_HIP_NODISCARD
  #endif
#endif
// clang-format on

/// @brief HIP integration.
namespace coopsync_tbb::hip {

namespace detail {

/// @brief Context for HIP callback to resume a suspended TBB task.
struct context {
    tbb::task::suspend_point suspend_point{};
    hipError_t err{};
};

/// @brief HIP callback to resume a suspended TBB task.
/// @param stream HIP stream.
/// @param err HIP error code of the completed stream work.
/// @param data Pointer to the context of the task to resume.
///
static inline void resumption_callback(hipStream_t, hipError_t err,
                                       void* data) {
    auto* ctx = static_cast<detail::context*>(data);
    ctx->err = err;
    tbb::task::resume(ctx->suspend_point);
}

struct wait_for_all_context {
    explicit wait_for_all_context(std::size_t pending_streams)
        : pending(pending_streams) {}
    std::atomic<std::size_t> pending{0};
    tbb::task::suspend_point suspend_point{};
};

struct wait_for_all_payload {
    wait_for_all_context* context{nullptr};
    hipError_t* out_status{nullptr};
};

template <typename T>
struct is_hip_stream : std::is_same<typename std::decay<T>::type, hipStream_t> {
};

template <bool...>
struct bool_pack;

template <bool... Bs>
struct all_true
    : std::is_same<bool_pack<Bs..., true>, bool_pack<true, Bs...> > {};

template <typename... Ts>
struct all_hip_stream : all_true<is_hip_stream<Ts>::value...> {};

static inline void wait_for_all_status_callback(hipStream_t, hipError_t status,
                                                void* data) {
    if (data == nullptr) {
        return;
    }
    auto* payload = static_cast<wait_for_all_payload*>(data);
    if (payload->out_status != nullptr) {
        *payload->out_status = status;
    }
    if (payload->context == nullptr) {
        return;
    }
    if (payload->context->pending.fetch_sub(1, std::memory_order_acq_rel) ==
        1) {
        tbb::task::resume(payload->context->suspend_point);
    }
}

}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a HIP stream
/// completes. Internally a HIP callback is used to resume the task.
/// @param stream HIP stream.
/// @return The HIP error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
COOPSYNC_TBB_HIP_NODISCARD static inline hipError_t wait_for(
    hipStream_t stream) {
    auto ctx = detail::context{};
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

/// @brief Suspends the current TBB task until all the work in all the provided
/// HIP streams completes.
/// A HIP callback is enqueued into every stream. The calling task resumes once
/// all callbacks that were successfully enqueued have executed.
/// @return Array of HIP error codes. Element i corresponds to stream i.
template <typename... StreamTs>
COOPSYNC_TBB_HIP_NODISCARD static inline std::array<hipError_t,
                                                    sizeof...(StreamTs)>
wait_for_all(StreamTs... streams) {
    static_assert(detail::all_hip_stream<StreamTs...>::value,
                  "wait_for_all(streams...) requires hipStream_t arguments");

    const std::size_t N = sizeof...(StreamTs);
    std::array<hipError_t, sizeof...(StreamTs)> errs = {{}};
    if (N == 0) {
        return errs;
    }

    const std::array<hipStream_t, sizeof...(StreamTs)> stream_array = {
        {streams...}};

    auto state = detail::wait_for_all_context(N);
    std::array<detail::wait_for_all_payload, sizeof...(StreamTs)> payloads = {
        {}};

    tbb::task::suspend([&](tbb::task::suspend_point tag) {
        state.suspend_point = tag;
        for (std::size_t i = 0; i < N; ++i) {
            payloads.at(i).context = &state;
            payloads.at(i).out_status = &errs.at(i);
            const auto add_err = hipStreamAddCallback(
                stream_array.at(i), detail::wait_for_all_status_callback,
                &payloads.at(i), 0);

            if (add_err != hipSuccess) {
                errs.at(i) = add_err;
                // Callback won't run; exclude it from the pending count.
                if (state.pending.fetch_sub(1, std::memory_order_acq_rel) ==
                    1) {
                    tbb::task::resume(state.suspend_point);
                }
            }
        }
    });

    return errs;
}

}  // namespace coopsync_tbb::hip

#undef COOPSYNC_TBB_HIP_NODISCARD

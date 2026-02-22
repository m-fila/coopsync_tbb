#pragma once

#include <cuda_runtime_api.h>
#include <oneapi/tbb/task.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <iterator>
#include <type_traits>

// clang-format off
#ifndef COOPSYNC_TBB_CUDA_NODISCARD
  #if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
      #define COOPSYNC_TBB_CUDA_NODISCARD [[nodiscard]]
    #endif
  #elif defined(__cplusplus) && __cplusplus >= 201703L
    #define COOPSYNC_TBB_CUDA_NODISCARD [[nodiscard]]
  #endif
  #ifndef COOPSYNC_TBB_CUDA_NODISCARD
    #define COOPSYNC_TBB_CUDA_NODISCARD
  #endif
#endif
// clang-format on

/// @brief CUDA integration.
namespace coopsync_tbb::cuda {

namespace detail {

/// @brief CUDA callback to resume a suspended TBB task.
/// @param tag Pointer to the suspend point of the task to resume.
static inline void resumption_callback(void* tag) {
    if (tag == nullptr) {
        return;
    }
    tbb::task::resume(*static_cast<tbb::task::suspend_point*>(tag));
}

struct wait_for_all_context {
    explicit wait_for_all_context(size_t pending_streams = 0)
        : pending(pending_streams) {}
    std::atomic<std::size_t> pending{0};
    tbb::task::suspend_point suspend_point{};
};

template <typename T>
struct is_cuda_stream
    : std::is_same<typename std::decay<T>::type, cudaStream_t> {};

template <bool...>
struct bool_pack;

template <bool... Bs>
struct all_true
    : std::is_same<bool_pack<Bs..., true>, bool_pack<true, Bs...> > {};

template <typename... Ts>
struct all_cuda_stream : all_true<is_cuda_stream<Ts>::value...> {};

static inline void wait_for_all_callback(void* context) {
    if (context == nullptr) {
        return;
    }
    auto* wait_context = static_cast<wait_for_all_context*>(context);
    if (wait_context->pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        tbb::task::resume(wait_context->suspend_point);
    }
}

}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a CUDA stream
/// completes. Internally a CUDA callback is used to resume the task.
/// @param stream CUDA stream.
/// @return The CUDA error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
COOPSYNC_TBB_CUDA_NODISCARD static inline cudaError_t wait_for(
    cudaStream_t stream) {
    auto suspend_point = tbb::task::suspend_point{};
    auto err = cudaSuccess;
    tbb::task::suspend(
        [stream, &err, &suspend_point](tbb::task::suspend_point tag) {
            suspend_point = tag;
            // Note: cudaStreamAddCallback is pending for deprecation, using
            // cudaLaunchHostFunc instead.
            err = cudaLaunchHostFunc(stream, detail::resumption_callback,
                                     &suspend_point);
            if (err != cudaSuccess) {
                detail::resumption_callback(&suspend_point);
            }
        });
    return err;
}

/// @brief Suspends the current TBB task until all the work in all the provided
/// CUDA streams completes.
/// A CUDA host callback is enqueued into every stream. The calling task resumes
/// once all callbacks that were successfully enqueued have executed.
/// @return Array of CUDA error codes. Element i corresponds to stream i.
template <typename... StreamTs>
COOPSYNC_TBB_CUDA_NODISCARD static inline std::array<cudaError_t,
                                                     sizeof...(StreamTs)>
wait_for_all(StreamTs... streams) {
    static_assert(detail::all_cuda_stream<StreamTs...>::value,
                  "wait_for_all(streams...) requires cudaStream_t arguments");

    const std::size_t N = sizeof...(StreamTs);
    std::array<cudaError_t, sizeof...(StreamTs)> errs = {{}};
    if (N == 0) {
        return errs;
    }

    const std::array<cudaStream_t, sizeof...(StreamTs)> stream_array = {
        {streams...}};

    auto state = detail::wait_for_all_context(N);

    tbb::task::suspend([&](tbb::task::suspend_point tag) {
        state.suspend_point = tag;
        for (std::size_t i = 0; i < N; ++i) {
            const auto launch_err = cudaLaunchHostFunc(
                stream_array.at(i), detail::wait_for_all_callback, &state);
            errs.at(i) = launch_err;

            if (launch_err != cudaSuccess) {
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

/// @brief Suspends the current TBB task until all the work in all the provided
/// CUDA streams completes.
/// A CUDA host callback is enqueued into every stream. The calling task resumes
/// once all callbacks that were successfully enqueued have executed.
/// @param first Iterator to first CUDA stream. Must be a LegacyForwardIterator.
/// @param last Iterator past the last CUDA stream. Must be a
/// LegacyForwardIterator.
/// @param out Iterator receiving CUDA error codes in the same order. Must be a
/// LegacyOutputIterator.
/// @return Iterator past the last written error code.
template <typename InputIt, typename OutputIt>
static inline OutputIt wait_for_all(InputIt first, InputIt last, OutputIt out) {
    static_assert(
        detail::is_cuda_stream<
            typename std::iterator_traits<InputIt>::value_type>::value,
        "wait_for_all(first,last,out) requires cudaStream_t values");

    if (first == last) {
        return out;
    }

    std::size_t n = 0;
    for (InputIt it = first; it != last; ++it) {
        ++n;
    }

    auto state = detail::wait_for_all_context(n);

    tbb::task::suspend([&](tbb::task::suspend_point tag) {
        state.suspend_point = tag;
        for (InputIt it = first; it != last; ++it) {
            const auto launch_err =
                cudaLaunchHostFunc(*it, detail::wait_for_all_callback, &state);
            *out = launch_err;
            ++out;

            if (launch_err != cudaSuccess) {
                // Callback won't run; exclude it from the pending count.
                if (state.pending.fetch_sub(1, std::memory_order_acq_rel) ==
                    1) {
                    tbb::task::resume(state.suspend_point);
                }
            }
        }
    });
    return out;
}
}  // namespace coopsync_tbb::cuda

#undef COOPSYNC_TBB_CUDA_NODISCARD

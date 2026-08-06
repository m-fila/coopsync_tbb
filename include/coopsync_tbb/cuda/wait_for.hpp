// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

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

#ifndef COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
    #if defined(CUDART_VERSION) && CUDART_VERSION >= 10'000
        #define COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_ 1
    #else
        #define COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_ 0
    #endif
#endif
// clang-format on

/// @brief CUDA integration.
namespace coopsync_tbb::cuda {

namespace detail {

struct single_stream_context {
    ::tbb::task::suspend_point suspend_point{};
    ::cudaError_t err{::cudaSuccess};
};

#if COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
static inline void CUDART_CB resumption_single_host_func(void* context) {
    if (context == nullptr) {
        return;
    }
    auto* single_context = static_cast<single_stream_context*>(context);
    ::tbb::task::resume(single_context->suspend_point);
}
#else
static inline void CUDART_CB resumption_single_callback(::cudaStream_t,
                                                        ::cudaError_t err,
                                                        void* context) {
    if (context == nullptr) {
        return;
    }
    auto* single_context = static_cast<single_stream_context*>(context);
    single_context->err = err;
    ::tbb::task::resume(single_context->suspend_point);
}
#endif

template <typename T>
struct is_cuda_stream
    : std::is_same<typename std::decay<T>::type, ::cudaStream_t> {};

template <bool...>
struct bool_pack;

template <bool... Bs>
struct all_true : std::is_same<bool_pack<Bs..., true>, bool_pack<true, Bs...>> {
};

template <typename... Ts>
struct all_cuda_stream : all_true<is_cuda_stream<Ts>::value...> {};
struct multiple_stream_context {
    explicit multiple_stream_context(std::size_t pending_streams = 0)
        : pending(pending_streams) {}
    std::atomic<std::size_t> pending{0};
    ::tbb::task::suspend_point suspend_point{};
};

#if COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
static inline void CUDART_CB resumption_multiple_host_func(void* context) {
    if (context == nullptr) {
        return;
    }
    auto* wait_context = static_cast<multiple_stream_context*>(context);
    if (wait_context->pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        ::tbb::task::resume(wait_context->suspend_point);
    }
}
#else
struct multiple_stream_payload {
    multiple_stream_context* context{nullptr};
    ::cudaError_t* out_status{nullptr};
};

static inline void CUDART_CB resumption_multiple_callback(::cudaStream_t,
                                                          ::cudaError_t status,
                                                          void* payload) {
    if (payload == nullptr) {
        return;
    }
    auto* stream_payload = static_cast<multiple_stream_payload*>(payload);
    if (stream_payload->out_status != nullptr) {
        *stream_payload->out_status = status;
    }
    if (stream_payload->context == nullptr) {
        return;
    }
    if (stream_payload->context->pending.fetch_sub(
            1, std::memory_order_acq_rel) == 1) {
        ::tbb::task::resume(stream_payload->context->suspend_point);
    }
}

static inline void resumption_multiple_iter_callback(::cudaStream_t,
                                                     ::cudaError_t,
                                                     void* context) {
    if (context == nullptr) {
        return;
    }
    auto* wait_context = static_cast<multiple_stream_context*>(context);
    if (wait_context->pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        ::tbb::task::resume(wait_context->suspend_point);
    }
}

#endif

}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a CUDA stream
/// completes. Internally a CUDA callback is used to resume the task.
/// @param stream CUDA stream.
/// @return The CUDA error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
COOPSYNC_TBB_CUDA_NODISCARD static inline ::cudaError_t wait_for(
    ::cudaStream_t stream) {
    auto context = detail::single_stream_context{};
    ::tbb::task::suspend([stream, &context](::tbb::task::suspend_point tag) {
        context.suspend_point = tag;
#if COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
        context.err = ::cudaLaunchHostFunc(
            stream, detail::resumption_single_host_func, &context);
        if (context.err != ::cudaSuccess) {
            detail::resumption_single_host_func(&context);
        }
#else
        context.err = ::cudaStreamAddCallback(
            stream, detail::resumption_single_callback, &context, 0);
        if (context.err != ::cudaSuccess) {
            detail::resumption_single_callback(stream, context.err, &context);
        }
#endif
    });
    return context.err;
}

/// @brief Suspends the current TBB task until all the work in all the provided
/// CUDA streams completes.
/// A CUDA host callback is enqueued into every stream. The calling task resumes
/// once all callbacks that were successfully enqueued have executed.
/// @return Array of CUDA error codes. Element i corresponds to stream i.
template <typename... StreamTs>
COOPSYNC_TBB_CUDA_NODISCARD static inline std::array<::cudaError_t,
                                                     sizeof...(StreamTs)>
wait_for_all(StreamTs... streams) {
    static_assert(detail::all_cuda_stream<StreamTs...>::value,
                  "wait_for_all(streams...) requires cudaStream_t arguments");

    const std::size_t N = sizeof...(StreamTs);
    std::array<::cudaError_t, sizeof...(StreamTs)> errs = {{}};
    if (N == 0) {
        return errs;
    }

    const auto stream_array =
        std::array<::cudaStream_t, sizeof...(StreamTs)>{{streams...}};

    auto state = detail::multiple_stream_context(N);

#if !COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
    auto payloads =
        std::array<detail::multiple_stream_payload, sizeof...(StreamTs)>{{}};
#endif

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        state.suspend_point = tag;
        for (std::size_t i = 0; i < N; ++i) {
#if COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
            const auto launch_err = ::cudaLaunchHostFunc(
                stream_array.at(i), detail::resumption_multiple_host_func,
                &state);

            if (launch_err != ::cudaSuccess) {
                errs.at(i) = launch_err;
                detail::resumption_multiple_host_func(&state);
            }
#else
            auto& payload = payloads.at(i);
            payload.context = &state;
            payload.out_status = &errs.at(i);
            const auto launch_err = ::cudaStreamAddCallback(
                stream_array.at(i), detail::resumption_multiple_callback,
                &payload, 0);

            if (launch_err != ::cudaSuccess) {
                detail::resumption_multiple_callback(stream_array.at(i),
                                                     launch_err, &payload);
            }
#endif
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
template <typename ForwardIt, typename OutputIt>
static inline OutputIt wait_for_all(ForwardIt first, ForwardIt last,
                                    OutputIt out) {
    static_assert(
        detail::is_cuda_stream<
            typename std::iterator_traits<ForwardIt>::value_type>::value,
        "wait_for_all(first,last,out) requires cudaStream_t values");

    if (first == last) {
        return out;
    }

    auto n = std::distance(first, last);

    auto state = detail::multiple_stream_context(n);

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        state.suspend_point = tag;
        for (auto it = first; it != last; ++it) {
#if COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
            const auto launch_err = ::cudaLaunchHostFunc(
                *it, detail::resumption_multiple_host_func, &state);
#else
            const auto launch_err = ::cudaStreamAddCallback(
                *it, detail::resumption_multiple_iter_callback, &state, 0);
#endif

            if (launch_err != ::cudaSuccess) {
#if COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
                detail::resumption_multiple_host_func(&state);
#else
                detail::resumption_multiple_iter_callback(*it, launch_err,
                                                          &state);
#endif
            }
            *out = launch_err;
            ++out;
        }
    });
    return out;
}
}  // namespace coopsync_tbb::cuda

#undef COOPSYNC_TBB_CUDA_NODISCARD

// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cuda_runtime_api.h>
#include <oneapi/tbb/task.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <vector>

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

// cudaStreamAddCallback is pending deprecation as of CUDA 10.0; the
// replacement is cudaLaunchHostFunc().
#ifndef COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
    #if defined(CUDART_VERSION) && CUDART_VERSION >= 10000
        #define COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_ 1
    #else
        #define COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_ 0
    #endif
#endif
// clang-format on

/// @brief CUDA integration.
namespace coopsync_tbb::cuda {

namespace detail {

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

template <typename Context>
static void CUDART_CB resumption_host_func(void* self) {
    static_cast<Context*>(self)->notify(::cudaSuccess);
}

template <typename Context>
static void CUDART_CB resumption_callback(::cudaStream_t, ::cudaError_t status,
                                          void* self) {
    static_cast<Context*>(self)->notify(status);
}

template <typename Context>
static inline void register_callback(::cudaStream_t stream, Context& context) {
#if COOPSYNC_TBB_CUDA_HAS_HOST_FUNC_
    const auto err =
        ::cudaLaunchHostFunc(stream, &resumption_host_func<Context>, &context);
#else
    const auto err = ::cudaStreamAddCallback(
        stream, &resumption_callback<Context>, &context, 0);
#endif
    if (err != ::cudaSuccess) {
        context.notify(err);
    }
}

struct single_stream_context {
    void notify(::cudaError_t status) {
        err = status;
        ::tbb::task::resume(suspend_point);
    }

    ::tbb::task::suspend_point suspend_point{};
    ::cudaError_t err{::cudaSuccess};
};

struct multi_stream_context {
    explicit multi_stream_context(std::size_t n) : pending(n) {}

    void notify_one() {
        if (pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ::tbb::task::resume(suspend_point);
        }
    }

    ::tbb::task::suspend_point suspend_point{};
    std::atomic<std::size_t> pending;
};

struct multi_stream_item {
    multi_stream_context* context{nullptr};
    ::cudaError_t* out{nullptr};

    void notify(::cudaError_t status) {
        *out = status;
        context->notify_one();
    }
};

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
    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        context.suspend_point = tag;
        detail::register_callback(stream, context);
    });
    return context.err;
}

/// @brief Suspends the current TBB task until all the work in all the provided
/// CUDA streams is completed.
/// A CUDA host callback is enqueued into every stream. The calling task resumes
/// once all callbacks that were successfully enqueued have executed.
/// @param streams Variadic list of CUDA streams.
/// @return Array of CUDA error codes. Element i corresponds to stream i.
template <typename... StreamTs>
COOPSYNC_TBB_CUDA_NODISCARD static inline std::array<::cudaError_t,
                                                     sizeof...(StreamTs)>
wait_for_all(StreamTs... streams) {
    static_assert(detail::all_cuda_stream<StreamTs...>::value,
                  "wait_for_all(streams...) requires cudaStream_t arguments");

    const std::size_t N = sizeof...(StreamTs);
    auto errs = std::array<::cudaError_t, sizeof...(StreamTs)>{};
    errs.fill(::cudaSuccess);

    if (N == 0) {
        return errs;
    }

    const auto stream_array =
        std::array<::cudaStream_t, sizeof...(StreamTs)>{{streams...}};
    auto context = detail::multi_stream_context(N);
    auto items = std::array<detail::multi_stream_item, N>{};

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        context.suspend_point = tag;
        for (std::size_t i = 0; i < N; ++i) {
            items.at(i) = {&context, &errs.at(i)};
            detail::register_callback(stream_array.at(i), items.at(i));
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

    const auto n = static_cast<std::size_t>(std::distance(first, last));
    auto context = detail::multi_stream_context(n);
    auto errs = std::vector<::cudaError_t>(n, ::cudaSuccess);
    auto items = std::vector<detail::multi_stream_item>(n);

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        context.suspend_point = tag;
        std::size_t i = 0;
        for (auto it = first; it != last; ++it, ++i) {
            items[i] = {&context, &errs[i]};
            detail::register_callback(*it, items[i]);
        }
    });

    return std::copy(errs.begin(), errs.end(), out);
}
}  // namespace coopsync_tbb::cuda

#undef COOPSYNC_TBB_CUDA_NODISCARD

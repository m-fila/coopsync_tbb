// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <hip/hip_runtime_api.h>
#include <oneapi/tbb/task.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <vector>

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

#ifndef COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
    #if defined(HIP_VERSION) && HIP_VERSION >= 50200000
        #define COOPSYNC_TBB_HIP_HAS_HOST_FUNC_ 1 // NOLINT(cppcoreguidelines-macro-to-enum,cppcoreguidelines-macro-usage)
    #else
        #define COOPSYNC_TBB_HIP_HAS_HOST_FUNC_ 0 // NOLINT(cppcoreguidelines-macro-to-enum,cppcoreguidelines-macro-usage)
    #endif
#endif
// clang-format on

/// @brief HIP integration.
namespace coopsync_tbb::hip {

namespace detail {

template <typename T>
struct is_hip_stream
    : std::is_same<typename std::decay<T>::type, ::hipStream_t> {};

template <bool...>
struct bool_pack;

template <bool... Bs>
struct all_true : std::is_same<bool_pack<Bs..., true>, bool_pack<true, Bs...>> {
};

template <typename... Ts>
struct all_hip_stream : all_true<is_hip_stream<Ts>::value...> {};

template <typename Context>
static void resumption_host_func(void* self) {
    static_cast<Context*>(self)->notify(::hipSuccess);
}

template <typename Context>
static void resumption_callback(::hipStream_t, ::hipError_t status,
                                void* self) {
    static_cast<Context*>(self)->notify(status);
}

template <typename Context>
static inline void register_callback(::hipStream_t stream, Context& context) {
#if COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
    const auto err =
        ::hipLaunchHostFunc(stream, &resumption_host_func<Context>, &context);
#else
    const auto err = ::hipStreamAddCallback(
        stream, &resumption_callback<Context>, &context, 0);
#endif
    if (err != ::hipSuccess) {
        context.notify(err);
    }
}

struct single_stream_context {
    void notify(::hipError_t status) {
        err = status;
        ::tbb::task::resume(suspend_point);
    }

    ::tbb::task::suspend_point suspend_point{};
    ::hipError_t err{::hipSuccess};
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
    ::hipError_t* out{nullptr};

    void notify(::hipError_t status) {
        *out = status;
        context->notify_one();
    }
};

}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a HIP stream
/// completes. Internally a HIP callback is used to resume the task.
/// @param stream HIP stream.
/// @return The HIP error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
COOPSYNC_TBB_HIP_NODISCARD static inline ::hipError_t wait_for(
    ::hipStream_t stream) {
    auto context = detail::single_stream_context{};
    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        context.suspend_point = tag;
        detail::register_callback(stream, context);
    });
    return context.err;
}

/// @brief Suspends the current TBB task until all the work in all the provided
/// HIP streams is completed.
/// A HIP host callback is enqueued into every stream. The calling task resumes
/// once all callbacks that were successfully enqueued have executed.
/// @param streams Variadic list of HIP streams.
/// @return Array of HIP error codes. Element i corresponds to stream i.
template <typename... StreamTs>
COOPSYNC_TBB_HIP_NODISCARD static inline std::array<::hipError_t,
                                                    sizeof...(StreamTs)>
wait_for_all(StreamTs... streams) {
    static_assert(detail::all_hip_stream<StreamTs...>::value,
                  "wait_for_all(streams...) requires hipStream_t arguments");

    const std::size_t N = sizeof...(StreamTs);
    auto errs = std::array<::hipError_t, sizeof...(StreamTs)>{};
    errs.fill(::hipSuccess);

    if (N == 0) {
        return errs;
    }

    const auto stream_array =
        std::array<::hipStream_t, sizeof...(StreamTs)>{{streams...}};
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
/// HIP streams completes.
/// A HIP host callback is enqueued into every stream. The calling task resumes
/// once all callbacks that were successfully enqueued have executed.
/// @param first Iterator to first HIP stream. Must be a LegacyForwardIterator.
/// @param last Iterator past the last HIP stream. Must be a
/// LegacyForwardIterator.
/// @param out Iterator receiving HIP error codes in the same order. Must be a
/// LegacyOutputIterator.
/// @return Iterator past the last written error code.
template <typename ForwardIt, typename OutputIt>
static inline OutputIt wait_for_range(ForwardIt first, ForwardIt last,
                                      OutputIt out) {
    static_assert(
        detail::is_hip_stream<
            typename std::iterator_traits<ForwardIt>::value_type>::value,
        "wait_for_range(first,last,out) requires hipStream_t values");

    if (first == last) {
        return out;
    }

    const auto n = static_cast<std::size_t>(std::distance(first, last));
    auto context = detail::multi_stream_context(n);
    auto errs = std::vector<::hipError_t>(n, ::hipSuccess);
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
}  // namespace coopsync_tbb::hip

#undef COOPSYNC_TBB_HIP_NODISCARD

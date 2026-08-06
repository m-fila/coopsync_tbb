// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <hip/hip_runtime_api.h>
#include <oneapi/tbb/task.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <iterator>
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

#ifndef COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
  #if defined(HIP_VERSION) && HIP_VERSION >= 50'200'000
    #define COOPSYNC_TBB_HIP_HAS_HOST_FUNC_ 1
  #else
    #define COOPSYNC_TBB_HIP_HAS_HOST_FUNC_ 0
  #endif
#endif
// clang-format on

/// @brief HIP integration.
namespace coopsync_tbb::hip {

namespace detail {

struct single_stream_context {
    ::tbb::task::suspend_point suspend_point{};
    ::hipError_t err{::hipSuccess};
};

#if COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
static inline void resumption_single_host_func(void* context) {
    if (context == nullptr) {
        return;
    }
    auto* single_context = static_cast<single_stream_context*>(context);
    ::tbb::task::resume(single_context->suspend_point);
}
#else
static inline void resumption_single_callback(::hipStream_t, ::hipError_t err,
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
struct is_hip_stream
    : std::is_same<typename std::decay<T>::type, ::hipStream_t> {};

template <bool...>
struct bool_pack;

template <bool... Bs>
struct all_true : std::is_same<bool_pack<Bs..., true>, bool_pack<true, Bs...>> {
};

template <typename... Ts>
struct all_hip_stream : all_true<is_hip_stream<Ts>::value...> {};

struct multiple_stream_context {
    explicit multiple_stream_context(std::size_t pending_streams)
        : pending(pending_streams) {}
    std::atomic<std::size_t> pending{0};
    ::tbb::task::suspend_point suspend_point{};
};

#if COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
static inline void resumption_multiple_host_func(void* context) {
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
    ::hipError_t* out_status{nullptr};
};

static inline void resumption_multiple_callback(::hipStream_t,
                                                ::hipError_t status,
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

static inline void resumption_multiple_iter_callback(::hipStream_t,
                                                     ::hipError_t,
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

/// @brief Suspends the current TBB task until all the work in a HIP stream
/// completes. Internally a HIP callback is used to resume the task.
/// @param stream HIP stream.
/// @return The HIP error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
COOPSYNC_TBB_HIP_NODISCARD static inline ::hipError_t wait_for(
    ::hipStream_t stream) {
    auto context = detail::single_stream_context{};
    ::tbb::task::suspend([stream, &context](::tbb::task::suspend_point tag) {
        context.suspend_point = tag;
#if COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
        context.err = ::hipLaunchHostFunc(
            stream, detail::resumption_single_host_func, &context);
        if (context.err != ::hipSuccess) {
            detail::resumption_single_host_func(&context);
        }
#else
        // Note: hipLaunchHostFunc was beta; use hipStreamAddCallback on older
        // runtimes.
        context.err = ::hipStreamAddCallback(
            stream, detail::resumption_single_callback, &context, 0);
        if (context.err != ::hipSuccess) {
            detail::resumption_single_callback(stream, context.err, &context);
        }
#endif
    });
    return context.err;
}

/// @brief Suspends the current TBB task until all the work in all the provided
/// HIP streams completes.
/// A HIP callback is enqueued into every stream. The calling task resumes once
/// all callbacks that were successfully enqueued have executed.
/// @return Array of HIP error codes. Element i corresponds to stream i.
template <typename... StreamTs>
COOPSYNC_TBB_HIP_NODISCARD static inline std::array<::hipError_t,
                                                    sizeof...(StreamTs)>
wait_for_all(StreamTs... streams) {
    static_assert(detail::all_hip_stream<StreamTs...>::value,
                  "wait_for_all(streams...) requires hipStream_t arguments");

    const std::size_t N = sizeof...(StreamTs);
    std::array<::hipError_t, sizeof...(StreamTs)> errs = {{}};
    if (N == 0) {
        return errs;
    }

    const std::array<::hipStream_t, sizeof...(StreamTs)> stream_array = {
        {streams...}};

    auto state = detail::multiple_stream_context(N);
#if !COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
    std::array<detail::multiple_stream_payload, sizeof...(StreamTs)> payloads =
        {{}};
#endif

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        state.suspend_point = tag;
        for (std::size_t i = 0; i < N; ++i) {
#if COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
            const auto add_err = ::hipLaunchHostFunc(
                stream_array.at(i), detail::resumption_multiple_host_func,
                &state);
            errs.at(i) = add_err;

            if (add_err != ::hipSuccess) {
                // Callback won't run; exclude it from the pending count.
                detail::resumption_multiple_host_func(&state);
            }
#else
            payloads.at(i).context = &state;
            payloads.at(i).out_status = &errs.at(i);
            const auto add_err = ::hipStreamAddCallback(
                stream_array.at(i), detail::resumption_multiple_callback,
                &payloads.at(i), 0);

            if (add_err != ::hipSuccess) {
                detail::resumption_multiple_callback(stream_array.at(i),
                                                     add_err, &payloads.at(i));
            }
#endif
        }
    });

    return errs;
}

/// @brief Suspends the current TBB task until all the work in all the provided
/// HIP streams completes.
/// A HIP callback is enqueued into every stream. The calling task resumes once
/// all callbacks that were successfully enqueued have executed.
/// @param first Iterator to first HIP stream. Must be a LegacyForwardIterator.
/// @param last Iterator past the last HIP stream. Must be a
/// LegacyForwardIterator.
/// @param out Iterator receiving HIP error codes in the same order. Must be a
/// LegacyOutputIterator.
/// @return Iterator past the last written error code.
template <typename ForwardIt, typename OutputIt>
static inline OutputIt wait_for_all(ForwardIt first, ForwardIt last,
                                    OutputIt out) {
    static_assert(
        detail::is_hip_stream<
            typename std::iterator_traits<ForwardIt>::value_type>::value,
        "wait_for_all(first,last,out) requires hipStream_t values");

    if (first == last) {
        return out;
    }

    auto n = std::distance(first, last);

    auto state = detail::multiple_stream_context(n);

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        state.suspend_point = tag;
        for (auto it = first; it != last; ++it) {
#if COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
            const auto launch_err = ::hipLaunchHostFunc(
                *it, detail::resumption_multiple_host_func, &state);
#else
            const auto launch_err = ::hipStreamAddCallback(
                *it, detail::resumption_multiple_iter_callback, &state, 0);
#endif

            if (launch_err != ::hipSuccess) {
#if COOPSYNC_TBB_HIP_HAS_HOST_FUNC_
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

}  // namespace coopsync_tbb::hip

#undef COOPSYNC_TBB_HIP_NODISCARD

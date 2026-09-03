// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <oneapi/tbb/task.h>
#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>

#include <cassert>
#include <cstddef>

// clang-format off
#ifndef COOPSYNC_TBB_ONNX_NODISCARD
  #if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard) >= 201603L
      #define COOPSYNC_TBB_ONNX_NODISCARD [[nodiscard]]
    #endif
  #elif defined(__cplusplus) && __cplusplus >= 201703L
    #define COOPSYNC_TBB_ONNX_NODISCARD [[nodiscard]]
  #endif
  #ifndef COOPSYNC_TBB_ONNX_NODISCARD
    #define COOPSYNC_TBB_ONNX_NODISCARD
  #endif
#endif
// clang-format on

namespace coopsync_tbb {
/// @brief ONNX Runtime integration.
namespace onnx {

namespace detail {

struct context {
    ::tbb::task::suspend_point suspend_point{};
    ::OrtStatus* status = nullptr;
};

static inline void resumption_callback(void* user_data, ::OrtValue**,
                                       std::size_t, ::OrtStatus* status) {
    auto* ctx = static_cast<context*>(user_data);
    if (ctx == nullptr) {
        return;
    }
    ctx->status = status;
    ::tbb::task::resume(ctx->suspend_point);
}

}  // namespace detail

/// @brief Suspends the current TBB task until an ORT session RunAsync is
/// complete. Accepts ONXX Runtime C API arguments.
///
/// @param[in] api The \c OrtApi to use for the RunAsync call.
/// @param[in] session The \c OrtSession to run.
/// @param[in] run_options If nullptr, will use a default \c OrtRunOptions.
/// @param[in] input_names Array of null terminated UTF8 encoded strings of the
/// input names.
/// @param[in] input_values Array of \c OrtValue of the input values
/// @param[in] input_count Number of elements in the \c input_names and inputs
/// arrays.
/// @param[in] output_names Array of null terminated UTF8 encoded strings of the
/// output names.
/// @param[in] output_count Number of elements in the \c output_names and
/// outputs array.
/// @param[out] output_values \c OrtValue* array of size output_count. On
/// calling, \c output_values[i] could either be a null or a pointer to a
/// preallocated \c OrtValue. It is caller's duty to finally release the output
/// array and each of its member, regardless of whether the member (\c
/// OrtValue*) is allocated by onnxruntime or preallocated by the caller.
/// @return The \c OrtStatus* passed by ORT to the callback. On success, this is
/// nullptr. The caller owns the returned status and must release it via
/// \c OrtApi::ReleaseStatus when non-null.
///
/// Calls the \c api->RunAsync with the provided arguments and a callback
/// that resumes the current TBB task. The task is suspended until the callback
/// is invoked. The arguments follow the same semantics as the \c
/// OrtApi::RunAsync.
///
/// @note In case of error during callback setup, the task is resumed
/// immediately.
COOPSYNC_TBB_ONNX_NODISCARD static inline ::OrtStatus* wait_for(
    const ::OrtApi* api, ::OrtSession* session,
    const ::OrtRunOptions* run_options, const char* const* input_names,
    const ::OrtValue* const* input_values, std::size_t input_count,
    const char* const* output_names, std::size_t output_count,
    ::OrtValue** output_values) {

    assert(api != nullptr);                                 // LCOV_EXCL_LINE
    assert(session != nullptr);                             // LCOV_EXCL_LINE
    assert(input_names != nullptr || input_count == 0);     // LCOV_EXCL_LINE
    assert(input_values != nullptr || input_count == 0);    // LCOV_EXCL_LINE
    assert(output_names != nullptr || output_count == 0);   // LCOV_EXCL_LINE
    assert(output_values != nullptr || output_count == 0);  // LCOV_EXCL_LINE

    auto ctx = detail::context{};

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        ctx.suspend_point = tag;
        auto* submit_status =
            api->RunAsync(session, run_options, input_names, input_values,
                          input_count, output_names, output_count,
                          output_values, &detail::resumption_callback, &ctx);
        if (submit_status != nullptr) {
            // Callback won't run; resume immediately with submission error.
            detail::resumption_callback(&ctx, nullptr, 0, submit_status);
        }
    });

    return ctx.status;
}

/// @brief Suspends the current TBB task until an ORT session RunAsync is
/// complete. Accepts ONXX Runtime C++ API arguments.
///
/// @param[in] session The \c Ort::Session to run.
/// @param[in] run_options The \c Ort::RunOptions to use for the RunAsync call.
/// @param[in] input_names Array of null terminated UTF8 encoded strings of the
/// input names.
/// @param[in] input_values Array of \c Ort::Value%s of the input values.
/// @param[in] input_count Number of elements in the input_names and inputs
/// arrays.
/// @param[in] output_names Array of null terminated UTF8 encoded strings of the
/// output names.
/// @param[out] output_values Array of provided Values to be filled with
/// outputs. On calling, output_values[i] could either be initialized
/// by a null pointer or a preallocated Ort::Value*. It is caller's duty to
/// finally release output_values and each of its member, regardless
/// of whether the member (\c Ort::Value) is allocated by onnxruntime or
/// preallocated by the caller.
/// @param[in] output_count Number of elements in the output_names and outputs
/// array.
/// @throws Ort::Exception on error. The exception will contain the status
/// message from the ORT callback.
///
/// Calls the \c session.RunAsync with the provided arguments and a callback
/// that resumes the current TBB task. The task is suspended until the callback
/// is invoked, at which point the status from the callback is checked and an
/// exception is thrown if it indicates an error. The arguments follow the same
/// semantics as the \c Ort::Session::RunAsync.
///
/// @note In case of error during callback setup, the task is resumed
/// immediately.
static inline void wait_for(
    ::Ort::Session& session, const ::Ort::RunOptions& run_options,
    const char* const* input_names, const ::Ort::Value* input_values,
    std::size_t input_count, const char* const* output_names,
    ::Ort::Value* output_values, std::size_t output_count) {

    assert(input_names != nullptr || input_count == 0);     // LCOV_EXCL_LINE
    assert(input_values != nullptr || input_count == 0);    // LCOV_EXCL_LINE
    assert(output_names != nullptr || output_count == 0);   // LCOV_EXCL_LINE
    assert(output_values != nullptr || output_count == 0);  // LCOV_EXCL_LINE

    auto ctx = detail::context{};

    ::tbb::task::suspend([&](::tbb::task::suspend_point tag) {
        ctx.suspend_point = tag;
        session.RunAsync(run_options, input_names, input_values, input_count,
                         output_names, output_values, output_count,
                         detail::resumption_callback, &ctx);
    });
    ::Ort::ThrowOnError(ctx.status);
}
}  // namespace onnx
}  // namespace coopsync_tbb

#undef COOPSYNC_TBB_ONNX_NODISCARD

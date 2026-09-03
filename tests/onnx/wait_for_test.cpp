// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/onnx/wait_for.hpp"

#include <gtest/gtest.h>
#include <oneapi/tbb/parallel_for.h>
#include <onnxruntime_cxx_api.h>

#include <vector>

TEST(OnnxRuntimeTest, CXXWaitForIdentity1x1) {
    const auto expected = 42.0f;

    auto env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "test");
    auto opts = Ort::SessionOptions{};

    auto session = Ort::Session(env, "identity_1x1.onnx", opts);

    auto allocator = Ort::AllocatorWithDefaultOptions();

    auto input_name = session.GetInputNameAllocated(0, allocator);
    auto output_name = session.GetOutputNameAllocated(0, allocator);

    auto input = std::vector<float>{expected};
    auto shape = std::vector<int64_t>{1, 1};

    auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    auto input_tensor = Ort::Value::CreateTensor<float>(
        mem, input.data(), input.size(), shape.data(), shape.size());

    auto const input_names = std::vector<const char*>{input_name.get()};
    auto const output_names = std::vector<const char*>{output_name.get()};

    auto outputs = std::vector<Ort::Value>(1);

    tbb::parallel_for(0, 1, [&](int _) {
        coopsync_tbb::onnx::wait_for(session, Ort::RunOptions{nullptr},
                                     input_names.data(), &input_tensor,
                                     input_names.size(), output_names.data(),
                                     outputs.data(), output_names.size());
    });
    const auto* out = outputs.at(0).GetTensorData<float>();

    EXPECT_FLOAT_EQ(*out, expected);
}

TEST(OnnxRuntimeTest, CWaitForIdentity1x1) {
    const float expected = 42.0f;

    const auto* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);

    OrtEnv* env = nullptr;
    ASSERT_EQ(api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &env), nullptr);

    OrtSessionOptions* opts = nullptr;
    ASSERT_EQ(api->CreateSessionOptions(&opts), nullptr);

    OrtSession* session = nullptr;
    ASSERT_EQ(api->CreateSession(env, "identity_1x1.onnx", opts, &session),
              nullptr);

    OrtAllocator* allocator = nullptr;
    ASSERT_EQ(api->GetAllocatorWithDefaultOptions(&allocator), nullptr);

    char* input_name = nullptr;
    char* output_name = nullptr;

    ASSERT_EQ(api->SessionGetInputName(session, 0, allocator, &input_name),
              nullptr);
    ASSERT_EQ(api->SessionGetOutputName(session, 0, allocator, &output_name),
              nullptr);

    auto input = std::vector<float>{expected};
    auto shape = std::vector<int64_t>{1, 1};

    OrtMemoryInfo* memory_info = nullptr;
    ASSERT_EQ(api->CreateCpuMemoryInfo(OrtDeviceAllocator, OrtMemTypeDefault,
                                       &memory_info),
              nullptr);

    OrtValue* input_tensor = nullptr;

    OrtStatus* status = api->CreateTensorWithDataAsOrtValue(
        memory_info, input.data(), input.size() * sizeof(float), shape.data(),
        shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensor);

    if (status != nullptr) {
        const char* message = api->GetErrorMessage(status);
        ADD_FAILURE() << "CreateTensorWithDataAsOrtValue failed: "
                      << (message != nullptr ? message : "<no error message>");
        api->ReleaseStatus(status);
    }

    ASSERT_NE(input_tensor, nullptr);

    auto const input_names = std::vector<const char*>{input_name};
    auto const output_names = std::vector<const char*>{output_name};
    auto const input_values = std::vector<const OrtValue*>{input_tensor};
    auto outputs = std::vector<OrtValue*>(1, nullptr);

    OrtRunOptions* run_options = nullptr;
    ASSERT_EQ(api->CreateRunOptions(&run_options), nullptr);

    OrtStatus* run_status = nullptr;

    tbb::parallel_for(0, 1, [&](int) {
        run_status = coopsync_tbb::onnx::wait_for(
            api, session, run_options, input_names.data(), input_values.data(),
            input_names.size(), output_names.data(), output_names.size(),
            outputs.data());
    });

    if (run_status != nullptr) {
        const char* message = api->GetErrorMessage(run_status);
        ADD_FAILURE() << "wait_for failed: "
                      << (message != nullptr ? message : "<no error message>");
        api->ReleaseStatus(run_status);
    }

    ASSERT_NE(outputs.at(0), nullptr);

    float* output_data = nullptr;

    // clang-format off
    ASSERT_EQ(
        api->GetTensorMutableData(
            outputs.at(0),
            reinterpret_cast<void**>(&output_data)),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        nullptr);
    // clang-format on

    ASSERT_NE(output_data, nullptr);

    EXPECT_FLOAT_EQ(*output_data, expected);

    api->ReleaseValue(outputs.at(0));
    api->ReleaseValue(input_tensor);
    api->ReleaseMemoryInfo(memory_info);
    api->ReleaseRunOptions(run_options);

    allocator->Free(allocator, input_name);
    allocator->Free(allocator, output_name);

    api->ReleaseSession(session);
    api->ReleaseSessionOptions(opts);
    api->ReleaseEnv(env);
}

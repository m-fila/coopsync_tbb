// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include <hip/hip_runtime_api.h>

#include <cstdint>

/// Launch a GPU spin for the specified duration in ns on the given HIP stream.
void launch_nanospin(std::uint64_t ns, hipStream_t stream);

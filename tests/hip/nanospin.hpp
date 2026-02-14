#include <hip/hip_runtime_api.h>

#include <cstdint>

/// Launch a GPU spin for the specified duration in ns on the given HIP stream.
void launch_nanospin(std::uint64_t ns, hipStream_t stream);

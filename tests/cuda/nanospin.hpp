#include <cuda_runtime_api.h>  // NOLINT(clang-diagnostic-error)

#include <cstdint>

/// Launch a GPU spin for the specified duration in ns on the given CUDA stream.
void launch_nanospin(std::uint64_t ns, cudaStream_t stream);

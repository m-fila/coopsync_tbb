#include <hip/hip_runtime.h>

#include "nanospin.hpp"

static __global__ void nanospin(std::uint64_t ns) {
    uint64_t start = clock64();
    constexpr uint64_t cps = 1'400'000'000;  // Assuming 1.4 GHz clock rate
    uint64_t end = start + ((ns * cps) / 1'000'000'000);
    while (clock64() < end) {
    }
}

void launch_nanospin(std::uint64_t ns, hipStream_t stream) {
    nanospin<<<1, 32, 0, stream>>>(ns);
}

#include <oneapi/tbb/parallel_for.h>

#include <coopsync_tbb/latch.hpp>
#include <iostream>

int main() {
    // This is just a smoke-test that headers link and run.
    coopsync_tbb::latch latch(0);
    tbb::parallel_for(0, 1, [&](int) { latch.wait(); });

    std::cout << "CoopSync_TBB downstream example: OK\n";
    return 0;
}

// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include <oneapi/tbb/parallel_for.h>

#ifdef USE_COOPSYNC_TBB_MODULE
import coopsync_tbb;
#else
#include <coopsync_tbb/latch.hpp>
#endif
#include <coopsync_tbb/version.hpp>
#include <iostream>

int main() {
    // This is just a smoke-test that headers link and run.
    coopsync_tbb::latch latch(0);
    tbb::parallel_for(0, 1, [&](int) { latch.wait(); });

    std::cout << "CoopSync_TBB downstream example: OK\n";

    std::cout << "CoopSync_TBB version: " << COOPSYNC_TBB_VERSION << "\n";
    return 0;
}

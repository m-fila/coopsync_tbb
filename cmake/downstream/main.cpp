// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for.h>

#include <coopsync_tbb/coopsync_tbb.hpp>
#include <coopsync_tbb/version.hpp>
#include <iostream>

int main() {
    // This is just a smoke-test that headers link and run.
    std::cout << "CoopSync_TBB version: " << COOPSYNC_TBB_VERSION << "\n";
    // Limit the number of threads to 1
    auto control =
        tbb::global_control(tbb::global_control::max_allowed_parallelism, 1);
    auto latch = coopsync_tbb::latch(1);
    auto task = coopsync_tbb::packaged_task<int()>([&]() {
        latch.count_down();
        return 42;
    });
    auto fut = task.get_future();
    // Run 3 tasks concurrently
    tbb::parallel_for(0, 3, [&](int i) {
        if (i == 0) {
            latch.wait();
        } else if (i == 1) {
            task();
        } else {
            auto v = fut.get();
            if (v != 42) {
                std::cerr << "Unexpected value from future: " << v << "\n";
                std::exit(1);
            }
        }
    });

    std::cout << "CoopSync_TBB downstream example: OK\n";

    return 0;
}

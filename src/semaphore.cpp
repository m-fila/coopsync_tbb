#include "coopsync_tbb/semaphore.hpp"

namespace coopsync_tbb {

counting_semaphore<1>::counting_semaphore(std::ptrdiff_t desired)
    : m_available(desired) {
    assert(desired == 0 || desired == 1);
}

bool counting_semaphore<1>::try_acquire() {
    auto expected = true;
    const auto desired = false;
    return m_available.compare_exchange_strong(expected, desired,
                                               std::memory_order_acquire,
                                               std::memory_order_relaxed);
}

void counting_semaphore<1>::acquire() {
    while (!try_acquire()) {
        m_waiters.wait_if(
            [this] { return !m_available.load(std::memory_order_acquire); });
    }
}

void counting_semaphore<1>::release(std::ptrdiff_t update) {
    assert(update >= 0);

    // No permits returned
    if (update == 0) {
        return;
    }

    assert(m_available.load(std::memory_order_acquire) == false);
    m_available.store(true, std::memory_order_release);
    m_waiters.resume_one();
}

}  // namespace coopsync_tbb

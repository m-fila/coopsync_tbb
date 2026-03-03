#include "coopsync_tbb/mutex.hpp"

namespace coopsync_tbb {

bool mutex::try_lock() noexcept {
    bool expected = false;
    const bool desired = true;
    return m_locked.compare_exchange_strong(expected, desired,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed);
}

void mutex::lock() {
    while (!try_lock()) {
        m_wait_queue.wait_if(
            [this] { return m_locked.load(std::memory_order_acquire); });
    }
}

void mutex::unlock() {
    assert(m_locked.load(std::memory_order_acquire));
    m_locked.store(false, std::memory_order_release);

    // Wake a single waiter (if any). The woken task will retry try_lock().
    m_wait_queue.resume_one();
}

}  // namespace coopsync_tbb

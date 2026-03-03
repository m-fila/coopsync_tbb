#include "coopsync_tbb/latch.hpp"

namespace coopsync_tbb {

latch::latch(std::ptrdiff_t expected) : m_counter(expected) {
    assert(expected >= 0);
    assert(expected <= max());
}

bool latch::try_wait() const noexcept {
    return m_counter.load(std::memory_order_acquire) == 0;
}

void latch::arrive_and_wait(std::ptrdiff_t update) {
    count_down(update);
    wait();
}

void latch::count_down(std::ptrdiff_t update) {
    assert(update >= 0);
    auto prev = m_counter.fetch_sub(update, std::memory_order_acq_rel);
    assert(prev >= update);
    if (prev == update) {
        m_waiters.resume_all();
    }
}

void latch::wait() {
    // Fast path
    if (!try_wait()) {
        m_waiters.wait_if([this] { return !try_wait(); });
    }

    // Post resumption, the latch counter has reached zero.
    assert(m_counter.load(std::memory_order_acquire) == 0);
}

}  // namespace coopsync_tbb

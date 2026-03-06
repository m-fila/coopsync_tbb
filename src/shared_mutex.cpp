#include "coopsync_tbb/shared_mutex.hpp"

namespace coopsync_tbb {

shared_mutex::~shared_mutex() {
    assert(m_state.load(std::memory_order_acquire) == 0);  // LCOV_EXCL_LINE
    assert(m_writer_waiters.empty());                      // LCOV_EXCL_LINE
    assert(m_reader_waiters.empty());                      // LCOV_EXCL_LINE
}

bool shared_mutex::try_lock() noexcept {
    int expected = 0;
    const int desired = k_writer_locked;
    return m_state.compare_exchange_strong(expected, desired,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed);
}

void shared_mutex::lock() {
    if (try_lock()) {
        return;
    }
    m_writer_waiters.wait_if([this] { return !try_lock(); });

    // Post direct handoff, the state should be already locked on the writer's
    // behalf.
    assert(m_state.load(std::memory_order_acquire) ==
           k_writer_locked);  // LCOV_EXCL_LINE
}

void shared_mutex::unlock() {
    assert(m_state.load(std::memory_order_acquire) ==
           k_writer_locked);  // LCOV_EXCL_LINE

    // Direct handoff to a waiting writer if there is any.
    if (m_writer_waiters.resume_one()) {
        return;
    }
    // Otherwise, unlock and resume all waiting readers.
    m_state.store(0, std::memory_order_release);
    m_reader_waiters.resume_all();
}

bool shared_mutex::try_lock_shared() noexcept {
    int state = m_state.load(std::memory_order_relaxed);
    while (state >= 0) {
        if (m_state.compare_exchange_weak(state, state + 1,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void shared_mutex::lock_shared() {
    while (!try_lock_shared()) {
        m_reader_waiters.wait_if(
            [this] { return m_state.load(std::memory_order_acquire) < 0; });
    }
}

void shared_mutex::unlock_shared() {
    // Decrement readers. The last reader performs the handoff.
    int state = m_state.load(std::memory_order_acquire);
    while (true) {
        assert(state >= 1);  // LCOV_EXCL_LINE

        if (state > 1) {
            if (m_state.compare_exchange_weak(state, state - 1,
                                              std::memory_order_release,
                                              std::memory_order_acquire)) {
                return;
            }
            continue;
        }

        // state == 1: transition to a temporary negative state so no new
        // owners can acquire while we decide who to wake up.
        if (m_state.compare_exchange_weak(state, k_transition,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
            break;
        }
    }

    // Direct handoff to a waiting writer if there is any.
    m_state.store(k_writer_locked, std::memory_order_release);
    if (m_writer_waiters.resume_one()) {
        return;
    }
    // Otherwise, unlock and resume all waiting readers.
    m_state.store(0, std::memory_order_release);
    m_reader_waiters.resume_all();
}

}  // namespace coopsync_tbb

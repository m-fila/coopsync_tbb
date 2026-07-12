// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

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
    assert(m_state.load(std::memory_order_acquire) ==  // LCOV_EXCL_LINE
           k_writer_locked);
}

void shared_mutex::unlock() {
    assert(m_state.load(std::memory_order_acquire) ==  // LCOV_EXCL_LINE
           k_writer_locked);

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

shared_mutex::scoped_lock::scoped_lock(shared_mutex& m, bool write)
    : m_mutex(&m), m_is_writer_lock(write) {
    if (write) {
        m_mutex->lock();
    } else {
        m_mutex->lock_shared();
    }
}

shared_mutex::scoped_lock::scoped_lock()
    : m_mutex(nullptr), m_is_writer_lock(false) {}

void shared_mutex::scoped_lock::acquire(shared_mutex& m, bool write) {
    if (m_mutex != nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }
    m_mutex = &m;
    if (write) {
        m_mutex->lock();
    } else {
        m_mutex->lock_shared();
    }
    m_is_writer_lock = write;
}

shared_mutex::scoped_lock::~scoped_lock() {
    release();
}

bool shared_mutex::scoped_lock::try_acquire(shared_mutex& m, bool write) {
    if (m_mutex != nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }
    auto success = write ? m.try_lock() : m.try_lock_shared();
    if (success) {
        m_mutex = &m;
        m_is_writer_lock = write;
    }
    return success;
}

void shared_mutex::scoped_lock::release() {
    if (m_mutex == nullptr) {
        return;
    }
    m_mutex->unlock();
    m_mutex = nullptr;
}

bool shared_mutex::scoped_lock::upgrade_to_writer() {
    if (m_mutex == nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }
    if (m_is_writer_lock) {
        return true;
    }
    // Release the reader lock and acquire a writer lock.
    m_mutex->unlock_shared();
    m_mutex->lock();
    m_is_writer_lock = true;
    return false;
}

bool shared_mutex::scoped_lock::downgrade_to_reader() {
    if (m_mutex == nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }

    if (!m_is_writer_lock) {
        return true;
    }
    // Release the writer lock and acquire a reader lock.
    m_mutex->unlock();
    m_mutex->lock_shared();
    m_is_writer_lock = false;
    return false;
}
}  // namespace coopsync_tbb

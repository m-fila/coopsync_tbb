// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/mutex.hpp"

namespace coopsync_tbb {

mutex::~mutex() {
    assert(m_wait_queue.empty());  // LCOV_EXCL_LINE
}

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
    assert(m_locked.load(std::memory_order_acquire));  // LCOV_EXCL_LINE
    m_locked.store(false, std::memory_order_release);

    // Wake a single waiter (if any). The woken task will retry try_lock().
    m_wait_queue.resume_one();
}

mutex::scoped_lock::scoped_lock(mutex& m) : m_mutex(&m) {
    m_mutex->lock();
}

mutex::scoped_lock::scoped_lock() : m_mutex(nullptr) {}

void mutex::scoped_lock::acquire(mutex& m) {
    if (m_mutex != nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }
    m_mutex = &m;
    m_mutex->lock();
}

mutex::scoped_lock::~scoped_lock() {
    release();
}

bool mutex::scoped_lock::try_acquire(mutex& m) {
    if (m_mutex != nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }
    auto success = m.try_lock();
    if (success) {
        m_mutex = &m;
    }
    return success;
}

void mutex::scoped_lock::release() {
    if (m_mutex == nullptr) {
        return;
    }
    m_mutex->unlock();
    m_mutex = nullptr;
}

}  // namespace coopsync_tbb

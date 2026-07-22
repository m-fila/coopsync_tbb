// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/feature_test.hpp"

#if defined(COOPSYNC_TBB_HAS_ATOMIC_FLAG) && COOPSYNC_TBB_HAS_ATOMIC_FLAG == 1

#include <atomic>
#include <cassert>

#include "coopsync_tbb/atomic_flag.hpp"

namespace coopsync_tbb {

atomic_flag::~atomic_flag() {
    assert(m_waiters.empty());  // LCOV_EXCL_LINE
}

void atomic_flag::clear(std::memory_order order) noexcept {
    assert(order != std::memory_order_consume);  // LCOV_EXCL_LINE
    assert(order != std::memory_order_acquire);  // LCOV_EXCL_LINE
    assert(order != std::memory_order_acq_rel);  // LCOV_EXCL_LINE

    m_value.clear(order);
}

bool atomic_flag::test_and_set(std::memory_order order) noexcept {
    return m_value.test_and_set(order);
}

bool atomic_flag::test(std::memory_order order) const noexcept {
    assert(order != std::memory_order_release);  // LCOV_EXCL_LINE
    assert(order != std::memory_order_acq_rel);  // LCOV_EXCL_LINE

    return m_value.test(order);
}

void atomic_flag::wait(bool old, std::memory_order order) {
    assert(order != std::memory_order_release);  // LCOV_EXCL_LINE
    assert(order != std::memory_order_acq_rel);  // LCOV_EXCL_LINE

    while (true) {
        if (old != m_value.test(order)) {
            return;
        }
        m_waiters.wait_if(
            [this, old, order] { return old == m_value.test(order); });
    }
}

void atomic_flag::notify_one() {
    m_waiters.resume_one();
}

void atomic_flag::notify_all() {
    m_waiters.resume_all();
}

std::atomic_flag& atomic_flag::atomic() noexcept {
    return m_value;
}

std::atomic_flag const& atomic_flag::atomic() const noexcept {
    return m_value;
}

std::atomic_flag& atomic_flag::operator*() noexcept {
    return m_value;
}

const std::atomic_flag& atomic_flag::operator*() const noexcept {
    return m_value;
}

std::atomic_flag* atomic_flag::operator->() noexcept {
    return &m_value;
}

std::atomic_flag const* atomic_flag::operator->() const noexcept {
    return &m_value;
}

void atomic_flag_clear(atomic_flag* object) noexcept {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    object->clear(std::memory_order_seq_cst);
}

void atomic_flag_clear_explicit(atomic_flag* object,
                                std::memory_order order) noexcept {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    object->clear(order);
}

bool atomic_flag_test_and_set(atomic_flag* object) noexcept {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    return object->test_and_set(std::memory_order_seq_cst);
}

bool atomic_flag_test_and_set_explicit(atomic_flag* object,
                                       std::memory_order order) noexcept {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    return object->test_and_set(order);
}

bool atomic_flag_test(atomic_flag* object) noexcept {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    return object->test(std::memory_order_seq_cst);
}

bool atomic_flag_test_explicit(atomic_flag* object,
                               std::memory_order order) noexcept {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    return object->test(order);
}

void atomic_wait(atomic_flag* object, bool old) {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    object->wait(old, std::memory_order_seq_cst);
}

void atomic_wait_explicit(atomic_flag* object, bool old,
                          std::memory_order order) {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    object->wait(old, order);
}

void atomic_notify_one(atomic_flag* object) {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    object->notify_one();
}

void atomic_notify_all(atomic_flag* object) {
    assert(object != nullptr);  // LCOV_EXCL_LINE
    object->notify_all();
}

}  // namespace coopsync_tbb

#endif

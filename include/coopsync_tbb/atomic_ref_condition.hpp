// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "coopsync_tbb/feature_test.hpp"

#if defined(DOXYGEN) || (defined(COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION) && \
                         COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION == 1)

#include <atomic>
#include <cstring>
#include <type_traits>

#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

template <typename T>
class atomic_ref_condition {
    static_assert(std::is_trivially_copyable<T>::value,
                  "atomic_ref_condition requires trivially copyable type");
    static_assert(
        std::atomic_ref<T>::is_always_lock_free || !std::is_volatile<T>::value,
        "If std::atomic<T> is not always lock-free, T must not be volatile");

    public:
    /// @brief The value type of the atomic_ref_condition, same as the value
    /// type of the internal atomic_ref.
    using value_type = typename std::atomic_ref<T>::value_type;

    explicit atomic_ref_condition(T& value);

    /// @brief The atomic_ref_condition is copy-constructible. The copy
    /// constructor creates a new atomic_ref_condition that refers to the same
    /// internal atomic object as the original but does not share the waiting
    /// queue.
    /// @param other The atomic_ref_condition to share internal state with.
    atomic_ref_condition(const atomic_ref_condition& other);

    /// @brief The atomic_ref_condition is not copy-assignable.
    atomic_ref_condition& operator=(const atomic_ref_condition&) = delete;

    /// @brief The atomic_ref_condition is not move-constructible.
    atomic_ref_condition(atomic_ref_condition&&) = delete;

    /// @brief The atomic_ref_condition is not move-assignable.
    atomic_ref_condition& operator=(atomic_ref_condition&&) = delete;

    /// @brief Destroys the atomic_ref_condition.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the atomic_ref_condition. The destructor does not notify or
    /// resume any waiting tasks.
    ~atomic_ref_condition();

    /// @brief Suspends the calling task until the internal atomic_ref value is
    /// not equal to specified value. If the internal atomic_ref value is not
    /// equal to old, the function returns immediately. Otherwise, the task is
    /// suspended. The task will be resumed once associated notify function is
    /// called and the internal atomic_ref value is not equal to old.
    /// @param old The value to compare the internal atomic_ref value to. The
    /// task will be suspended while the internal atomic_ref value is equal to
    /// old.
    /// @param order The memory order to use for loading the internal atomic_ref
    /// value. Must not be \c std::memory_order_release or \c
    /// std::memory_order_acq_rel.
    /// @note Due to ABA problem, transitions from old to a different value and
    /// back to old may be missed.
    /// @note The comparison is done by bitwise comparison of the internal
    /// atomic_ref value and old, not by \c operator==. The comparison may be
    /// affected by padding bytes.
    void wait(value_type old,
              std::memory_order order = std::memory_order_seq_cst);

    /// @brief Resumes one task suspended waiting on this atomic_ref_condition,
    /// if there is any.
    void notify_one();

    /// @brief Resumes all tasks suspended waiting on this atomic_ref_condition.
    void notify_all();

    /// @brief Access to the internal atomic_ref value.
    /// @return Reference to the internal atomic_ref value. Remains valid for
    /// the lifetime of the atomic_ref_condition.
    std::atomic_ref<T>& atomic() noexcept;

    /// @brief Access to the internal atomic_ref value.
    /// @return Const reference to the internal atomic_ref value. Remains valid
    /// for the lifetime of the atomic_ref_condition.
    const std::atomic_ref<T>& atomic() const noexcept;

    /// @brief Access to the internal atomic_ref value.
    /// @return Reference to the internal atomic_ref value. Remains valid for
    /// the lifetime of the atomic_ref_condition.
    std::atomic_ref<T>& operator*() noexcept;

    /// @brief Access to the internal atomic_ref value.
    /// @return Const reference to the internal atomic_ref value. Remains valid
    /// for the lifetime of the atomic_ref_condition.
    const std::atomic_ref<T>& operator*() const noexcept;

    /// @brief Access to the internal atomic_ref value.
    /// @return Pointer to the internal atomic_ref value. Remains valid for the
    /// lifetime of the atomic_ref_condition.
    std::atomic_ref<T>* operator->() noexcept;

    /// @brief Access to the internal atomic_ref value.
    /// @return Const pointer to the internal atomic_ref value. Remains valid
    /// for the lifetime of the atomic_ref_condition.
    const std::atomic_ref<T>* operator->() const noexcept;

    private:
    std::atomic_ref<T> m_value;
    detail::wait_queue m_waiters;
};

template <typename T>
atomic_ref_condition<T>::atomic_ref_condition(T& value) : m_value(value) {}

template <typename T>
atomic_ref_condition<T>::atomic_ref_condition(const atomic_ref_condition& other)
    : m_value(other.m_value) {}

template <typename T>
atomic_ref_condition<T>::~atomic_ref_condition() {
    assert(m_waiters.empty());  // LCOV_EXCL_LINE
}

template <typename T>
void atomic_ref_condition<T>::wait(value_type old, std::memory_order order) {
    assert(order != std::memory_order_release);  // LCOV_EXCL_LINE
    assert(order != std::memory_order_acq_rel);  // LCOV_EXCL_LINE

    while (true) {
        // bitwise comparison instead of operator==
        // TODO make this ignore padding bytes
        const auto current = m_value.load(order);
        if (std::memcmp(&current, &old, sizeof(value_type)) != 0) {
            return;
        }
        m_waiters.wait_if([this, &old, order] {
            const auto current = m_value.load(order);
            return std::memcmp(&current, &old, sizeof(value_type)) == 0;
        });
    }
}

template <typename T>
void atomic_ref_condition<T>::notify_one() {
    m_waiters.resume_one();
}

template <typename T>
void atomic_ref_condition<T>::notify_all() {
    m_waiters.resume_all();
}

template <typename T>
std::atomic_ref<T>& atomic_ref_condition<T>::atomic() noexcept {
    return m_value;
}

template <typename T>
const std::atomic_ref<T>& atomic_ref_condition<T>::atomic() const noexcept {
    return m_value;
}

template <typename T>
std::atomic_ref<T>& atomic_ref_condition<T>::operator*() noexcept {
    return m_value;
}

template <typename T>
const std::atomic_ref<T>& atomic_ref_condition<T>::operator*() const noexcept {
    return m_value;
}

template <typename T>
std::atomic_ref<T>* atomic_ref_condition<T>::operator->() noexcept {
    return &m_value;
}

template <typename T>
const std::atomic_ref<T>* atomic_ref_condition<T>::operator->() const noexcept {
    return &m_value;
}

}  // namespace coopsync_tbb
#else
#error \
    "atomic_ref_condition is not supported on this platform. Please check the COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION macro."
#endif

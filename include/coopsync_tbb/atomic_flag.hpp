// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "coopsync_tbb/feature_test.hpp"

#if defined(DOXYGEN) || (defined(COOPSYNC_TBB_HAS_ATOMIC_FLAG) && \
                         COOPSYNC_TBB_HAS_ATOMIC_FLAG == 1)

#include <atomic>

#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

/// @brief Wrapper around std::atomic_flag that provides atomic waiting which
/// suspends the calling task instead of blocking it.
class atomic_flag {

    public:
    /// @brief Constructs a new atomic_flag. Initializes to clear state.
    atomic_flag() = default;

    /// @brief The atomic_flag is not copy-constructible.
    atomic_flag(const atomic_flag&) = delete;

    /// @brief The atomic_flag is not copy-assignable.
    atomic_flag& operator=(const atomic_flag&) = delete;

    /// @brief The atomic_flag is not move-constructible.
    atomic_flag(atomic_flag&&) = delete;

    /// @brief The atomic_flag is not move-assignable.
    atomic_flag& operator=(atomic_flag&&) = delete;

    /// @brief Destroys the atomic_flag.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the atomic_flag. The destructor does not notify or
    /// resume any waiting tasks.
    ~atomic_flag();

    /// @brief Atomically sets the state to clear.
    /// @param order The memory order to use for clearing the flag. Must not be
    /// \c std::memory_order_consume, \c std::memory_order_acquire or \c
    /// std::memory_order_acq_rel.
    void clear(std::memory_order order = std::memory_order_seq_cst) noexcept;

    /// @brief Atomically sets the state to set and returns the previous state.
    /// @param order The memory order to use for setting the flag.
    bool test_and_set(
        std::memory_order order = std::memory_order_seq_cst) noexcept;

    /// @brief Atomically reads the current state and returns it.
    /// @param order The memory order to use for reading the flag. Must not be
    /// \c std::memory_order_release, \c std::memory_order_acq_rel.
    bool test(
        std::memory_order order = std::memory_order_seq_cst) const noexcept;

    /// @brief Suspends the calling task until the state is not
    /// equal to specified value. If the state is not equal to
    /// old, the function returns immediately. Otherwise, the task is suspended.
    /// The task will be resumed once notify_one or notify_all is called and the
    /// state is not equal to old.
    /// @param old The value to compare the internal atomic value to. The task
    /// will be suspended while the internal atomic value is equal to old.
    /// @param order The memory order to use for loading the internal atomic
    /// value. Must not be \c std::memory_order_release or \c
    /// std::memory_order_acq_rel.
    /// @note Due to ABA problem, transitions from old to a different value and
    /// back to old may be missed.
    void wait(bool old, std::memory_order order = std::memory_order_seq_cst);

    /// @brief Resumes one task suspended waiting on this atomic_flag, if
    /// there is any.
    void notify_one();

    /// @brief Resumes all tasks suspended waiting on this atomic_flag.
    void notify_all();

    /// @brief Access to the internal atomic value.
    /// @return Reference to the internal atomic value. Remains valid for the
    /// lifetime of the atomic_flag.
    std::atomic_flag& atomic() noexcept;

    /// @brief Access to the internal atomic value.
    /// @return Const reference to the internal atomic value. Remains valid for
    /// the lifetime of the atomic_flag.
    const std::atomic_flag& atomic() const noexcept;

    /// @brief Access to the internal atomic value.
    /// @return Reference to the internal atomic value. Remains valid for the
    /// lifetime of the atomic_flag.
    std::atomic_flag& operator*() noexcept;

    /// @brief Access to the internal atomic value.
    /// @return Const reference to the internal atomic value. Remains valid for
    /// the lifetime of the atomic_flag.
    const std::atomic_flag& operator*() const noexcept;

    /// @brief Access to the internal atomic value.
    /// @return Pointer to the internal atomic value. Remains valid for the
    /// lifetime of the atomic_flag.
    std::atomic_flag* operator->() noexcept;

    /// @brief Access to the internal atomic value.
    /// @return Const pointer to the internal atomic value. Remains valid for
    /// the lifetime of the atomic_flag.
    const std::atomic_flag* operator->() const noexcept;

    private:
    std::atomic_flag m_value{};
    detail::wait_queue m_waiters;
};

void atomic_flag_clear(atomic_flag* object) noexcept;

void atomic_flag_clear_explicit(atomic_flag* object,
                                std::memory_order order) noexcept;

bool atomic_flag_test_and_set(atomic_flag* object) noexcept;

bool atomic_flag_test_and_set_explicit(atomic_flag* object,
                                       std::memory_order order) noexcept;

bool atomic_flag_test(atomic_flag* object) noexcept;

bool atomic_flag_test_explicit(atomic_flag* object,
                               std::memory_order order) noexcept;

/// @brief Suspends the calling task until the state is not equal to specified
/// value. If the internal atomic the state is not equal to old, the function
/// returns immediately. Otherwise, the task is suspended. The task will be
/// resumed once associated notify function is called and the state is not equal
/// to old. Same as \c object->wait(old).
/// @param object Pointer to the atomic_flag to wait on.
/// @param old The value to compare the state to. The task will
/// be suspended while the state is equal to old.
/// @note Due to ABA problem, transitions from old to a different value and
/// back to old may be missed.
void atomic_wait(atomic_flag* object, bool old);

/// @brief Suspends the calling task until the state is not equal to specified
/// value. If the internal atomic the state is not equal to old, the function
/// returns immediately. Otherwise, the task is suspended. The task will be
/// resumed once associated notify function is called and the state is not equal
/// to old. Same as \c object->wait(old, order).
/// @param object Pointer to the atomic_flag to wait on.
/// @param old The value to compare the state to. The task will
/// be suspended while the state is equal to old.
/// @param order The memory order to use for loading the internal atomic value.
/// Must not be \c std::memory_order_release or \c std::memory_order_acq_rel.
/// @note Due to ABA problem, transitions from old to a different value and
/// back to old may be missed.
void atomic_wait_explicit(atomic_flag* object, bool old,
                          std::memory_order order);

/// @brief Resumes one task suspended waiting on the atomic_flag, if there
/// is any.
/// Same as \c object.notify_one().
/// @param object Pointer to the atomic_flag to notify on.
void atomic_notify_one(atomic_flag* object);

/// @brief Resumes all tasks suspended waiting on the atomic_flag.
/// Same as \c object.notify_all().
/// @param object Pointer to the atomic_flag to notify on.
void atomic_notify_all(atomic_flag* object);

}  // namespace coopsync_tbb

#else
#error "Atomic flag condition variable is not supported on this platform."
#endif

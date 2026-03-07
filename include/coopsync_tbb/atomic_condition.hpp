#pragma once

#include <atomic>
#include <type_traits>

#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

/// @brief Wrapper around std::atomic that provides atomic waiting which
/// suspends the calling task instead of blocking it.
/// @tparam T The type of the value contained in the atomic_condition. Must be
/// \c TriviallyCopyable, \c CopyConstructible and \c CopyAssignable.
template <typename T>
class atomic_condition {
    static_assert(std::is_trivially_copyable<T>::value,
                  "atomic_condition requires trivially copyable type");
    static_assert(std::is_copy_constructible<T>::value,
                  "atomic_condition requires copy constructible type");
    static_assert(std::is_move_constructible<T>::value,
                  "atomic_condition requires move constructible type");
    static_assert(std::is_copy_assignable<T>::value,
                  "atomic_condition requires copy assignable type");
    static_assert(std::is_move_assignable<T>::value,
                  "atomic_condition requires move assignable type");
    static_assert(std::is_same<T, typename std::remove_cv<T>::type>::value,
                  "atomic_condition does not support const or volatile types");

    public:
    /// @brief The value type of the atomic condition, same as the value type of
    /// the internal atomic.
    using value_type = typename std::atomic<T>::value_type;

    /// @brief Constructs a new atomic_condition. No initialization of the
    /// atomic value takes place.
    atomic_condition() = default;

    /// @brief Constructs a new atomic_condition with the given initial value.
    /// This operation is not atomic.
    /// @param value A value to move-construct the internal atomic from.
    explicit atomic_condition(value_type value);

    /// @brief The atomic_condition is not copy-constructible.
    atomic_condition(const atomic_condition&) = delete;

    /// @brief The atomic_condition is not copy-assignable.
    atomic_condition& operator=(const atomic_condition&) = delete;

    /// @brief The atomic_condition is not move-constructible.
    atomic_condition(atomic_condition&&) = delete;

    /// @brief The atomic_condition is not move-assignable.
    atomic_condition& operator=(atomic_condition&&) = delete;

    /// @brief Destroys the atomic_condition.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the atomic_condition. The destructor does not notify or
    /// resume any waiting tasks.
    ~atomic_condition();

    /// @brief Suspends the calling task until the internal atomic value is not
    /// equal to specified value. If the internal atomic value is not equal to
    /// old, the function returns immediately. Otherwise, the task is suspended.
    /// The task will be resumed once notify_one or notify_all is called and the
    /// internal atomic value is not equal to old.
    /// @param old The value to compare the internal atomic value to. The task
    /// will be suspended while the internal atomic value is equal to old.
    /// @param order The memory order to use for loading the internal atomic
    /// value. Must not be \c std::memory_order_release or
    /// \c std::memory_order_acq
    /// @note Due to ABA problem, transitions from old to a different value and
    /// back to old may be missed.
    void wait(value_type old,
              std::memory_order order = std::memory_order_seq_cst);

    /// @brief Resumes one task suspended waiting on this atomic_condition, if
    /// there is any.
    void notify_one();

    /// @brief Resumes all tasks suspended waiting on this atomic_condition.
    void notify_all();

    std::atomic<T>& atomic() noexcept;
    const std::atomic<T>& atomic() const noexcept;
    std::atomic<T>& operator*() noexcept;
    const std::atomic<T>& operator*() const noexcept;
    std::atomic<T>* operator->() noexcept;
    const std::atomic<T>* operator->() const noexcept;

    private:
    std::atomic<T> m_value;
    detail::wait_queue m_waiters;
};

/// @brief Suspends the calling task until the internal atomic value of the
/// atomic_condition is not equal to specified value. If the internal atomic
/// value is not equal to old, the function returns immediately. Otherwise, the
/// task is suspended. The task will be resumed once associated notify function
/// is called and the internal atomic value is not equal to old.
/// Same as \c object.wait(old).
/// @tparam T The type of the value contained in the atomic_condition.
/// @param object The atomic_condition to wait on.
/// @param old The value to compare the internal atomic value to. The task will
/// be suspended while the internal atomic value is equal to old.
/// @param order The memory order to use for loading the internal atomic value.
/// Must not be \c std::memory_order_release or \c std
template <typename T>
void atomic_wait(atomic_condition<T>& object,
                 typename atomic_condition<T>::value_type old);

/// @brief Suspends the calling task until the internal atomic value of the
/// atomic_condition is not equal to specified value. If the internal atomic
/// value is not equal to old, the function returns immediately. Otherwise, the
/// task is suspended. The task will be resumed once associated notify function
/// is called and the internal atomic value is not equal to old. Same as \c
/// object.wait(old, order).
/// @tparam T The type of the value contained in the atomic_condition.
/// @param object The atomic_condition to wait on.
/// @param old The value to compare the internal atomic value to. The task will
/// be suspended while the internal atomic value is equal to old.
/// @param order The memory order to use for loading the internal atomic value.
/// Must not be \c std::memory_order_release or \c std::memory_order_acq_rel.
template <typename T>
void atomic_wait(atomic_condition<T>& object,
                 typename atomic_condition<T>::value_type old,
                 std::memory_order order);

/// @brief Resumes one task suspended waiting on the atomic_condition, if there
/// is any.
/// @tparam T The type of the value contained in the atomic_condition.
/// @param object The atomic_condition to notify on.
template <typename T>
void atomic_notify_one(atomic_condition<T>& object);

/// @brief Resumes all tasks suspended waiting on the atomic_condition.
/// @tparam T The type of the value contained in the atomic_condition.
/// @param object The atomic_condition to notify on.
template <typename T>
void atomic_notify_all(atomic_condition<T>& object);

}  // namespace coopsync_tbb

namespace coopsync_tbb {

template <typename T>
atomic_condition<T>::atomic_condition(value_type value)
    : m_value(std::move(value)) {}

template <typename T>
atomic_condition<T>::~atomic_condition() {
    assert(m_waiters.empty());  // LC_EXCL_LINE
}

template <typename T>
void atomic_condition<T>::wait(value_type old, std::memory_order order) {
    assert(order != std::memory_order_release);  // LCOV_EXCL_LINE
    assert(order != std::memory_order_acq_rel);  // LCOV_EXCL_LINE

    while (m_value.load(order) == old) {
        m_waiters.wait_if([] { return true; });
    }
}

template <typename T>
void atomic_condition<T>::notify_one() {
    m_waiters.resume_one();
}

template <typename T>
void atomic_condition<T>::notify_all() {
    m_waiters.resume_all();
}

template <typename T>
std::atomic<T>& atomic_condition<T>::atomic() noexcept {
    return m_value;
}

template <typename T>
const std::atomic<T>& atomic_condition<T>::atomic() const noexcept {
    return m_value;
}

template <typename T>
std::atomic<T>& atomic_condition<T>::operator*() noexcept {
    return m_value;
}

template <typename T>
const std::atomic<T>& atomic_condition<T>::operator*() const noexcept {
    return m_value;
}

template <typename T>
std::atomic<T>* atomic_condition<T>::operator->() noexcept {
    return &m_value;
}

template <typename T>
const std::atomic<T>* atomic_condition<T>::operator->() const noexcept {
    return &m_value;
}

template <typename T>
inline void atomic_wait(atomic_condition<T>& object,
                        typename atomic_condition<T>::value_type old) {
    object.wait(old);
}

template <typename T>
inline void atomic_wait(atomic_condition<T>& object,
                        typename atomic_condition<T>::value_type old,
                        std::memory_order order) {
    object.wait(old, order);
}

template <typename T>
inline void atomic_notify_one(atomic_condition<T>& object) {
    object.notify_one();
}

template <typename T>
inline void atomic_notify_all(atomic_condition<T>& object) {
    object.notify_all();
}

}  // namespace coopsync_tbb

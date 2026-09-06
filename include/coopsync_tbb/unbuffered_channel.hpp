// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <utility>

#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

/// @brief An unbuffered (synchronous) channel used to hand off values between
/// tasks. A push() does not complete until a matching pop() has claimed the
/// value, and vice-versa. Concurrent invocations of the member functions,
/// except for the destructor, are safe.
/// @tparam T The type of value transferred through the channel.
template <typename T>
class COOPSYNC_TBB_EXPORT unbuffered_channel {
    public:
    /// @brief Status of a channel operation.
    enum class COOPSYNC_TBB_NODISCARD channel_op_status {
        success,  ///< The operation was successful.
        closed    ///< The channel is closed and the operation could not be
                  ///< completed.
    };

    using value_type = T;

    unbuffered_channel() = default;
    unbuffered_channel(const unbuffered_channel&) = delete;
    unbuffered_channel& operator=(const unbuffered_channel&) = delete;
    unbuffered_channel(unbuffered_channel&&) = delete;
    unbuffered_channel& operator=(unbuffered_channel&&) = delete;

    ~unbuffered_channel();

    /// @brief Closes the channel. Any push() or pop() suspended on the channel
    /// is resumed and returns channel_op_status::closed. Any subsequent call
    /// to push() or pop() returns channel_op_status::closed immediately.
    /// @note Does not affect a handoff that has already been claimed by a
    /// matching pop().
    void close();

    /// @brief Returns whether the channel is closed.
    /// @return true if the channel is closed, false otherwise.
    bool is_closed() const noexcept;

    /// @brief Hands off a copy of value to a matching pop(). Suspends the
    /// calling task until a matching pop() claims the value or the channel is
    /// closed.
    /// @param value The value to hand off.
    /// @return channel_op_status::success if the value was claimed by a
    /// pop(), channel_op_status::closed if the channel was closed before the
    /// value could be claimed.
    channel_op_status push(const value_type& value);

    /// @brief Hands off value, moved-from on success, to a matching pop().
    /// Suspends the calling task until a matching pop() claims the value or
    /// the channel is closed.
    /// @param value The value to hand off.
    /// @return channel_op_status::success if the value was claimed by a
    /// pop(), channel_op_status::closed if the channel was closed before the
    /// value could be claimed.
    channel_op_status push(value_type&& value);

    /// @brief Claims a value handed off by a matching push(). Suspends the
    /// calling task until a matching push() provides a value or the channel is
    /// closed.
    /// @param value Set to the claimed value on success.
    /// @return channel_op_status::success if a value was claimed,
    /// channel_op_status::closed if the channel was closed before a value
    /// became available.
    channel_op_status pop(value_type& value);

    /// @brief Claims a value handed off by a matching push().
    /// @return The claimed value.
    /// @throws std::runtime_error if the channel is closed before a value
    /// becomes available.
    value_type pop();

    private:
    channel_op_status push_impl(value_type& value);

    std::atomic<value_type*> m_slot = {nullptr};
    std::atomic<bool> m_closed = {false};

    // Senders waiting to claim the slot.
    detail::wait_queue m_sender_waiters;
    // The current owner of the slot waiting for its value to be claimed.
    detail::wait_queue m_ack_waiters;
    // Receivers waiting for a value to become available.
    detail::wait_queue m_receiver_waiters;
};

template <typename T>
unbuffered_channel<T>::~unbuffered_channel() {
    assert(m_sender_waiters.empty());    // LCOV_EXCL_LINE
    assert(m_ack_waiters.empty());       // LCOV_EXCL_LINE
    assert(m_receiver_waiters.empty());  // LCOV_EXCL_LINE
}

template <typename T>
void unbuffered_channel<T>::close() {
    m_closed.store(true, std::memory_order_release);
    m_sender_waiters.resume_all();
    m_ack_waiters.resume_all();
    m_receiver_waiters.resume_all();
}
template <typename T>
bool unbuffered_channel<T>::is_closed() const noexcept {
    return m_closed.load(std::memory_order_acquire);
}

template <typename T>
typename unbuffered_channel<T>::channel_op_status
unbuffered_channel<T>::push_impl(value_type& value) {
    if (is_closed()) {
        return channel_op_status::closed;
    }

    value_type* expected = nullptr;
    while (!m_slot.compare_exchange_strong(expected, &value,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
        if (is_closed()) {
            return channel_op_status::closed;
        }
        expected = nullptr;
        m_sender_waiters.wait_if([this] {
            return m_slot.load(std::memory_order_acquire) != nullptr &&
                   !is_closed();
        });
    }

    // The slot is claimed. If the channel was closed concurrently, give up
    // the slot without publishing the value.
    if (is_closed()) {
        m_slot.store(nullptr, std::memory_order_release);
        m_sender_waiters.resume_one();
        return channel_op_status::closed;
    }

    m_receiver_waiters.resume_one();

    m_ack_waiters.wait_if([this, &value] {
        return m_slot.load(std::memory_order_acquire) == &value && !is_closed();
    });

    // The slot still holds our value only if it was never claimed by a
    // pop(), in which case we reclaim it ourselves.
    value_type* mine = &value;
    const auto reclaimed = m_slot.compare_exchange_strong(
        mine, nullptr, std::memory_order_acq_rel, std::memory_order_acquire);
    m_sender_waiters.resume_one();

    return reclaimed ? channel_op_status::closed : channel_op_status::success;
}

template <typename T>
typename unbuffered_channel<T>::channel_op_status unbuffered_channel<T>::push(
    const value_type& value) {
    auto local = value;
    return push_impl(local);
}

template <typename T>
typename unbuffered_channel<T>::channel_op_status unbuffered_channel<T>::push(
    value_type&& value) {
    auto local = std::move(value);
    return push_impl(local);
}

template <typename T>
typename unbuffered_channel<T>::channel_op_status unbuffered_channel<T>::pop(
    value_type& value) {
    for (;;) {
        auto* slot = m_slot.exchange(nullptr, std::memory_order_acq_rel);
        if (slot != nullptr) {
            value = std::move(*slot);
            m_ack_waiters.resume_one();
            return channel_op_status::success;
        }

        if (is_closed()) {
            return channel_op_status::closed;
        }

        m_receiver_waiters.wait_if([this] {
            return m_slot.load(std::memory_order_acquire) == nullptr &&
                   !is_closed();
        });
    }
}

template <typename T>
typename unbuffered_channel<T>::value_type unbuffered_channel<T>::pop() {
    value_type value;
    if (pop(value) != channel_op_status::success) {
        throw std::runtime_error("Failed to pop value from channel");
    }
    return value;
}
}  // namespace coopsync_tbb

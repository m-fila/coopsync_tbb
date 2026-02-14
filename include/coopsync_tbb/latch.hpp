#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>

#include "coopsync_tbb/detail/intrusive_list.hpp"

namespace coopsync_tbb {
/// @brief A latch is a down-ward counter that allows tasks to wait until the
/// counter reaches zero. The latch is not reusable, i.e., once the counter
/// reaches zero, it cannot be incremented again.
///
class latch {
    public:
    /// @brief Constructs a latch with the specified initial count. The count
    /// must be non-negative.
    /// @param expected The initial count for the latch.
    latch(std::ptrdiff_t expected);
    /// @brief Latch is not copy-constructible.
    latch(const latch&) = delete;
    /// @brief Latch is not copy-assignable.
    latch& operator=(const latch&) = delete;
    /// @brief Latch is not move-constructible.
    latch(latch&&) = delete;
    /// @brief Latch is not move-assignable.
    latch& operator=(latch&&) = delete;
    /// @brief Destructor.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the latch.
    ~latch() = default;

    /// @brief Returns the maximum value for the latch counter.
    /// @return The maximum value for the latch counter.
    static constexpr std::ptrdiff_t max() noexcept;

    /// @brief Checks if the latch has reached zero without suspending the
    /// calling task.
    /// @return true if the latch has reached zero, false otherwise.
    bool try_wait() const noexcept;

    /// @brief Decrements the latch counter by the specified update. The update
    /// must be non-negative and less than or equal to the current counter
    /// value. If the counter reaches zero, all suspended tasks are resumed.
    /// @param update The value to decrement the counter by.
    void count_down(std::ptrdiff_t update = 1);

    /// @brief Suspends the calling task until the latch counter reaches zero.
    /// If the counter is already zero, the function returns immediately.
    /// @note The suspended task must remain valid until it is resumed.
    /// @note The suspended task must be resumed before the latch is destroyed.
    void wait();

    /// @brief Decrements the latch counter by the specified update and suspends
    /// until the counter reaches zero. The update must be non-negative and less
    /// than or equal to the current counter value.
    /// @param update The value to decrement the counter by.
    void arrive_and_wait(std::ptrdiff_t update = 1);

    private:
    std::atomic<std::ptrdiff_t> m_counter;
    tbb::spin_mutex m_waiters_mutex;
    detail::intrusive_list<tbb::task::suspend_point> m_waiters;
};

inline latch::latch(std::ptrdiff_t expected) : m_counter(expected) {
    assert(expected >= 0);
}

inline constexpr std::ptrdiff_t latch::max() noexcept {
    return std::numeric_limits<std::ptrdiff_t>::max();
}

inline bool latch::try_wait() const noexcept {
    return m_counter.load(std::memory_order_acquire) == 0;
}

inline void latch::arrive_and_wait(std::ptrdiff_t update) {
    count_down(update);
    wait();
}

inline void latch::count_down(std::ptrdiff_t update) {
    assert(update >= 0);
    if (m_counter.fetch_sub(update, std::memory_order_acq_rel) == update) {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        while (auto* item = m_waiters.pop_front()) {
            tbb::task::resume(item->value);
        }
    }
}

inline void latch::wait() {
    // Fast path
    if (try_wait()) {
        return;
    }
    // Slow path
    auto node = detail::intrusive_list<tbb::task::suspend_point>::node{};
    m_waiters_mutex.lock();
    // Guaranteed that the suspend lambda will be executed on the same
    // thread so capturing locked mutex is fine.
    tbb::task::suspend([this, &node](tbb::task::suspend_point sp) {
        node.value = sp;
        m_waiters.push_back(node);
        m_waiters_mutex.unlock();
    });
}

}  // namespace coopsync_tbb

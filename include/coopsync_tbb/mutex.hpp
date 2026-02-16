#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>

#include "coopsync_tbb/detail/intrusive_list.hpp"
#include "coopsync_tbb/scoped_lock.hpp"

namespace coopsync_tbb {

/// @brief A mutex that can be used to synchronize access to shared resources.
/// The mutex is non-recursive and provides no fairness guarantees.
/// @note This mutex does not satisfy the standard named requirements
/// (BasicLockable, Lockable, Mutex) because it never blocks the calling
/// thread, even though it exposes the same interface. Concurrent invocations of
/// the member functions. except for destructor, are safe.
class mutex {
    public:
    /// @brief Associated RAII wrapper type for this mutex.
    using scoped_lock = coopsync_tbb::scoped_lock<mutex>;

    /// @brief Constructs a new mutex. The mutex is initially unlocked.
    mutex() = default;

    /// @brief The mutex is not copy-constructible.
    mutex(const mutex&) = delete;

    /// @brief The mutex is not copy-assignable.
    mutex& operator=(const mutex&) = delete;
    /// @brief The mutex is not move-constructible.

    mutex(mutex&&) = delete;

    /// @brief Mutex is not move-assignable.
    mutex& operator=(mutex&&) = delete;

    /// @brief Destroys the mutex.
    /// @note The destructor must not be called while the mutex is still locked
    /// or while there are tasks suspended on it. The destructor does not notify
    /// or resume any waiting tasks.
    ~mutex() = default;

    /// @brief Attempts to acquire the mutex without suspending.
    /// @return true if the mutex was successfully acquired, false otherwise.
    bool try_lock() noexcept;

    /// @brief Acquires the mutex, suspending the calling task if necessary
    /// until the mutex becomes available.
    /// @note The suspended task must remain valid until it is resumed.
    /// @note The suspended task must be resumed before the mutex is destroyed.
    void lock();

    /// @brief Releases the mutex. If there are tasks suspended on the mutex,
    /// exactly one of them is resumed and acquires the mutex.
    void unlock();

    /// @brief The mutex is not a reader-writer (shared) mutex.
    static inline constexpr bool is_rw_mutex = false;
    /// @brief The mutex is not recursive.
    static inline constexpr bool is_recursive_mutex = false;
    /// @brief The mutex does not provide any fairness guarantees.
    static inline constexpr bool is_fair_mutex = false;

    private:
    using waiter_t = tbb::task::suspend_point;
    std::atomic<bool> m_locked = false;
    detail::intrusive_list<waiter_t> m_waiters;
    tbb::spin_mutex m_waiters_mutex;
};

inline bool mutex::try_lock() noexcept {
    bool expected = false;
    const bool desired = true;
    return m_locked.compare_exchange_strong(expected, desired,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed);
}

inline void mutex::lock() {
    // Fast path
    if (try_lock()) {
        return;
    }
    // Slow path
    auto node = detail::intrusive_list<waiter_t>::node{};
    m_waiters_mutex.lock();

    // Re-check while holding the lock to avoid racing with unlock()
    if (try_lock()) {
        m_waiters_mutex.unlock();
        return;
    }

    // Guaranteed that the suspend lambda will be executed on the same thread
    // so capturing locked mutex is fine.
    tbb::task::suspend([this, &node](tbb::task::suspend_point sp) {
        node.value = sp;
        m_waiters.push_back(node);
        m_waiters_mutex.unlock();
    });

    // Post resumption, the was given the ownership of the mutex so just return.
}

inline void mutex::unlock() {
    assert(m_locked.load(std::memory_order_acquire));
    tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
    if (const auto* waiter = m_waiters.pop_front()) {
        // Direct handoff: keep the mutex locked and resume exactly one waiter.
        tbb::task::resume(waiter->value);
        return;
    }
    m_locked.store(0, std::memory_order_release);
}

}  // namespace coopsync_tbb

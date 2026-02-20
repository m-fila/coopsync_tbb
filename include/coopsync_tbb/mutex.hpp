#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>

#include "coopsync_tbb/detail/intrusive_list.hpp"
#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/unique_scoped_lock.hpp"

namespace coopsync_tbb {

/// @brief A mutex that can be used to synchronize access to shared resources.
/// The mutex is non-recursive and provides no fairness guarantees. An attempt
/// to acquire a mutex that is already in acquired state suspends the calling
/// task until the mutex can be acquired.
/// @note This mutex does satisfy the standard named requirements for
/// BasicLockable and Lockable but does not meet the requirements for Mutex
/// because it never blocks the calling thread, even though it exposes the same
/// interface. Concurrent invocations of the member functions. except for
/// destructor, are safe.
class mutex {
    public:
    /// @brief Associated RAII wrapper type for this mutex.
    using scoped_lock = coopsync_tbb::detail::unique_scoped_lock<mutex>;

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
    COOPSYNC_TBB_NODISCARD bool try_lock() noexcept;

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
    // node must remain valid until the task is resumed. It's a local variable
    // on a stack of suspended task which is preserved during suspension so it
    // isn't an issue.
    tbb::task::suspend([this, &node](tbb::task::suspend_point sp) {
        {
            // Re-check while holding the lock to avoid racing with unlock()
            tbb::spin_mutex::scoped_lock lock_(m_waiters_mutex);
            if (!try_lock()) {
                node.value = sp;
                m_waiters.push_back(node);
                return;
            }
        }
        // Resume immediately in case re-check succeeded
        tbb::task::resume(sp);
    });

    // Post resumption, the ownership was transferred to the resumed task.
    assert(m_locked.load(std::memory_order_acquire));
}

inline void mutex::unlock() {
    assert(m_locked.load(std::memory_order_acquire));

    typename detail::intrusive_list<waiter_t>::node* waiter = nullptr;

    {
        tbb::spin_mutex::scoped_lock lock_(m_waiters_mutex);
        waiter = m_waiters.pop_front();
    }

    if (waiter) {
        // Direct handoff: keep the mutex locked and resume exactly one waiter.
        tbb::task::resume(waiter->value);
        return;
    }
    // No waiters, unlock the mutex.
    m_locked.store(false, std::memory_order_release);
}

}  // namespace coopsync_tbb

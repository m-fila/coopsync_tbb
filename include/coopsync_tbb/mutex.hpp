#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>

#include "coopsync_tbb/detail/intrusive_list.hpp"

namespace coopsync_tbb {

/// @brief A mutex that can be used to synchronize access to shared resources.
/// The mutex is non-recursive and provides no fairness guarantees.
// @note This mutex does not satisfy the standard named requirements
// (BasicLockable, Lockable, Mutex) because it never blocks the calling
// thread, even though it exposes the same interface. Concurrent invocations of
// the member functions. except for destructor, are safe.
class mutex {
    public:
    /// @brief Constructs a new mutex. The mutex is initially unlocked.
    mutex() = default;

    /// @brief Mutex is not copy-constructible.
    mutex(const mutex&) = delete;

    /// @brief Mutex is not copy-assignable.
    mutex& operator=(const mutex&) = delete;
    /// @brief Mutex is not move-constructible.

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

    private:
    using waiter_t = tbb::task::suspend_point;
    std::atomic<bool> m_locked = false;
    detail::intrusive_list<waiter_t> m_waiters;
    tbb::spin_mutex m_waiters_mutex;
};

/// @brief A scoped_lock is a RAII wrapper for mutex that acquires the mutex on
/// construction and releases it on destruction.
class scoped_lock {
    public:
    /// @brief Constructs a scoped_lock and acquires the given mutex.
    /// @param m The mutex to acquire.
    explicit scoped_lock(mutex& m);
    /// @brief Destroys the scoped_lock and releases the mutex.
    ~scoped_lock();

    /// @brief scoped_lock is not copy-constructible.
    scoped_lock(const scoped_lock&) = delete;
    /// @brief scoped_lock is not copy-assignable.
    scoped_lock& operator=(const scoped_lock&) = delete;
    /// @brief scoped_lock is not move-constructible.
    scoped_lock(scoped_lock&&) = delete;
    /// @brief scoped_lock is not move-assignable.
    scoped_lock& operator=(scoped_lock&&) = delete;

    private:
    mutex& m_mutex;
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

inline scoped_lock::scoped_lock(mutex& m) : m_mutex(m) {
    m_mutex.lock();
}

inline scoped_lock::~scoped_lock() {
    m_mutex.unlock();
}

}  // namespace coopsync_tbb

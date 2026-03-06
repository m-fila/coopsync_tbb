#pragma once

#include <atomic>
#include <cassert>

#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/unique_scoped_lock.hpp"
#include "coopsync_tbb/detail/wait_queue.hpp"

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
    ~mutex();

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
    static constexpr bool is_rw_mutex = false;
    /// @brief The mutex is not recursive.
    static constexpr bool is_recursive_mutex = false;
    /// @brief The mutex does not provide any fairness guarantees.
    static constexpr bool is_fair_mutex = false;

    private:
    std::atomic<bool> m_locked = {false};
    detail::wait_queue m_wait_queue;
};

}  // namespace coopsync_tbb

// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>

#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

/// @brief A shared mutex that can be used to synchronize access to shared
/// resources. The mutex can be acquired in exclusive (writer) mode (\ref lock)
/// or shared (reader) mode
/// (\ref lock_shared). The mutex is non-recursive and provides no fairness
/// guarantees.
/// @note This mutex does satisfy the standard named requirements for
/// BasicLockable and Lockable but does not meet the requirements for Mutex
/// because it never blocks the calling thread, even though it exposes the same
/// interface. Concurrent invocations of the member functions, except for
/// destructor, are safe.
class shared_mutex {
    public:
    /// @brief Associated RAII wrapper type.
    class scoped_lock;

    /// @brief Constructs a new shared_mutex. The shared_mutex is initially
    /// unlocked.
    shared_mutex() = default;

    /// @brief The shared_mutex is not copy-constructible.
    shared_mutex(const shared_mutex&) = delete;

    /// @brief The shared_mutex is not copy-assignable.
    shared_mutex& operator=(const shared_mutex&) = delete;
    /// @brief The shared_mutex is not move-constructible.

    shared_mutex(shared_mutex&&) = delete;

    /// @brief shared_mutex is not move-assignable.
    shared_mutex& operator=(shared_mutex&&) = delete;

    /// @brief Destroys the shared_mutex.
    /// @note The destructor must not be called while the shared_mutex is still
    /// locked or while there are tasks suspended on it. The destructor does not
    /// notify or resume any waiting tasks.
    ~shared_mutex();

    /// @brief Attempts to acquire the shared_mutex without suspending.
    /// @return true if the shared_mutex was successfully acquired, false
    /// otherwise.
    COOPSYNC_TBB_NODISCARD bool try_lock() noexcept;

    /// @brief Acquires the shared_mutex, suspending the calling task if
    /// necessary until the shared_mutex becomes available.
    /// @note The suspended task must remain valid until it is resumed.
    /// @note The suspended task must be resumed before the shared_mutex is
    /// destroyed.
    void lock();

    /// @brief Releases the shared_mutex. If there are tasks suspended on the
    /// shared_mutex, either one of the exclusive waiters is resumed and
    /// acquires the shared_mutex, or a batch of shared waiters is resumed and
    /// acquires the shared_mutex.
    void unlock();

    /// @brief Attempts to acquire the shared_mutex in a shared mode without
    /// suspending.
    /// @return true if the shared_mutex was successfully acquired in shared
    /// mode, false otherwise.
    COOPSYNC_TBB_NODISCARD bool try_lock_shared() noexcept;

    /// @brief Acquires the shared_mutex in a shared mode, suspending the
    /// calling task if necessary until the shared_mutex becomes available in
    /// shared mode.
    /// @note The suspended task must remain valid until it is resumed.
    /// @note The suspended task must be resumed before the shared_mutex is
    /// destroyed.
    void lock_shared();

    /// @brief Releases the shared_mutex from a shared mode. If there are
    /// tasks suspended on the shared_mutex, either one of the exclusive waiters
    /// is resumed and acquires the shared_mutex, or a batch of shared waiters
    /// is resumed and acquires the shared_mutex.
    void unlock_shared();

    /// @brief The shared_mutex is a reader-writer (shared) mutex.
    static constexpr bool is_rw_mutex = true;
    /// @brief The shared_mutex is not recursive.
    static constexpr bool is_recursive_mutex = false;
    /// @brief The shared_mutex does not provide any fairness guarantees.
    static constexpr bool is_fair_mutex = false;

    private:
    // m_state meanings:
    static constexpr int k_transition =
        -2;  // transition state during last-reader handoff
    static constexpr int k_writer_locked = -1;  // exclusive (writer) locked
    //  >=0 : number of shared (reader) owners
    std::atomic<int> m_state = {0};

    detail::wait_queue m_writer_waiters;
    detail::wait_queue m_reader_waiters;
};

/// @brief RAII wrapper for mutex that acquires the
/// mutex on construction and releases it on destruction.
class COOPSYNC_TBB_NODISCARD shared_mutex::scoped_lock {
    public:
    /// @brief Constructs a scoped_lock without acquiring a mutex.
    scoped_lock();

    /// @brief Constructs a scoped_lock and acquires the given mutex.
    /// @param m The mutex to acquire.
    /// @param write If true, acquires the mutex in exclusive (writer) mode. If
    /// false, acquires the mutex in shared (reader) mode.
    explicit scoped_lock(shared_mutex& m, bool write = true);

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

    /// @brief Acquires the mutex.
    /// @param m Mutex to acquire.
    /// @param write If true, acquires the mutex in exclusive (writer) mode. If
    /// false, acquires the mutex in shared (reader) mode.
    /// @throws std::system_error if another mutex is already acquired.
    void acquire(shared_mutex& m, bool write = true);

    /// @brief Attempts to acquire the mutex without blocking.
    /// @param m Mutex to acquire.
    /// @param write If true, attempts to acquire the mutex in exclusive
    /// (writer) mode. If false, attempts to acquire the mutex in shared
    /// (reader) mode.
    /// @return true if the mutex was successfully acquired, false otherwise.
    /// @throws std::system_error if another mutex is already acquired.
    COOPSYNC_TBB_NODISCARD bool try_acquire(shared_mutex& m, bool write = true);

    /// @brief Releases the mutex. Does nothing if no mutex was previously
    /// acquired.
    void release();

    /// @brief Changes a writer lock to a reader lock.
    /// @return false if the lock was released and reacquired. Otherwise, return
    /// true.
    /// @throws std::system_error if no mutex was previously acquired.
    bool upgrade_to_writer();

    /// @brief Changes a reader lock to a writer lock.
    /// @return false if the lock was released and reacquired. Otherwise, return
    /// true.
    /// @throws std::system_error if no mutex was previously acquired.
    bool downgrade_to_reader();

    private:
    shared_mutex* m_mutex;
    bool m_is_writer_lock;
};

/// @brief Alias for shared_mutex, following the TBB convention of naming
/// reader-writer mutexes.
using rw_mutex = shared_mutex;

}  // namespace coopsync_tbb

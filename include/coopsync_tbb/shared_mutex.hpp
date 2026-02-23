#pragma once

#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>

#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/unique_scoped_lock.hpp"
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
/// interface. Concurrent invocations of the member functions. except for
/// destructor, are safe.
class shared_mutex {
    public:
    /// @brief Associated RAII wrapper type for this mutex.
    using scoped_lock = coopsync_tbb::detail::unique_scoped_lock<shared_mutex>;

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
    bool try_lock() noexcept;

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
    bool try_lock_shared() noexcept;

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
    static inline constexpr bool is_rw_mutex = true;
    /// @brief The shared_mutex is not recursive.
    static inline constexpr bool is_recursive_mutex = false;
    /// @brief The shared_mutex does not provide any fairness guarantees.
    static inline constexpr bool is_fair_mutex = false;

    private:
    // m_state meanings:
    static inline constexpr int k_transition =
        -2;  // transition state during last-reader handoff
    static inline constexpr int k_writer_locked =
        -1;  // exclusive (writer) locked
    //  >=0 : number of shared (reader) owners
    std::atomic<int> m_state = 0;

    detail::wait_queue m_writer_waiters;
    detail::wait_queue m_reader_waiters;
};

/// @brief Alias for shared_mutex, following the TBB convention of naming
/// reader-writer mutexes.
using rw_mutex = shared_mutex;

shared_mutex::~shared_mutex() {
    assert(m_state.load(std::memory_order_acquire) == 0);
    assert(m_writer_waiters.empty());
    assert(m_reader_waiters.empty());
}

COOPSYNC_TBB_NODISCARD bool shared_mutex::try_lock() noexcept {
    int expected = 0;
    const int desired = k_writer_locked;
    return m_state.compare_exchange_strong(expected, desired,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed);
}

inline void shared_mutex::lock() {
    if (try_lock()) {
        return;
    }
    m_writer_waiters.wait_if([this] { return !try_lock(); });

    // Post direct handoff, the state should be already locked on the writer's
    // behalf.
    assert(m_state.load(std::memory_order_acquire) == k_writer_locked);
}

inline void shared_mutex::unlock() {
    assert(m_state.load(std::memory_order_acquire) == k_writer_locked);

    // Direct handoff to a waiting writer if there is any.
    if (m_writer_waiters.resume_one()) {
        return;
    }
    // Otherwise, unlock and resume all waiting readers.
    m_state.store(0, std::memory_order_release);
    m_reader_waiters.resume_all();
}

COOPSYNC_TBB_NODISCARD inline bool shared_mutex::try_lock_shared() noexcept {
    int state = m_state.load(std::memory_order_relaxed);
    while (state >= 0) {
        if (m_state.compare_exchange_weak(state, state + 1,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

inline void shared_mutex::lock_shared() {
    while (!try_lock_shared()) {
        m_reader_waiters.wait_if(
            [this] { return m_state.load(std::memory_order_acquire) < 0; });
    }
}

inline void shared_mutex::unlock_shared() {
    // Decrement readers. The last reader performs the handoff.
    int state = m_state.load(std::memory_order_acquire);
    while (true) {
        assert(state >= 1);

        if (state > 1) {
            if (m_state.compare_exchange_weak(state, state - 1,
                                              std::memory_order_release,
                                              std::memory_order_acquire)) {
                return;
            }
            continue;
        }

        // state == 1: transition to a temporary negative state so no new
        // owners can acquire while we decide who to wake up.
        if (m_state.compare_exchange_weak(state, k_transition,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
            break;
        }
    }

    // Direct handoff to a waiting writer if there is any.
    m_state.store(k_writer_locked, std::memory_order_release);
    if (m_writer_waiters.resume_one()) {
        return;
    }
    // Otherwise, unlock and resume all waiting readers.
    m_state.store(0, std::memory_order_release);
    m_reader_waiters.resume_all();
}

}  // namespace coopsync_tbb

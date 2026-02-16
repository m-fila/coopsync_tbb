#pragma once

#include <oneapi/tbb/task.h>

#include <cassert>
#include <cstddef>
#include <atomic>
#include <cstring>
#include <stdexcept>

#include "coopsync_tbb/detail/unique_scoped_lock.hpp"
#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

/// @brief A mutex that can be recursively locked to synchronize access to
/// shared resources. The recursive mutex is recursive, is not shared, and
/// provides no fairness guarantees.
/// @note This mutex does not satisfy the standard named requirements
/// (BasicLockable, Lockable, Mutex) because it never blocks the calling
/// thread, even though it exposes the same interface. Concurrent invocations of
/// the member functions. except for destructor, are safe.
/// @note The recursive_mutex can only be used in the context of TBB resumable
/// tasks. Attempting to use it outside of a task will result in a
/// std::runtime_error being thrown.
class recursive_mutex {
    public:
    /// @brief Associated RAII wrapper type for this mutex.
    using scoped_lock =
        coopsync_tbb::detail::unique_scoped_lock<recursive_mutex>;

    /// @brief Constructs a new recursive_mutex. The mutex is initially
    /// unlocked.
    recursive_mutex() = default;

    /// @brief The recursive_mutex is not copy-constructible.
    recursive_mutex(const recursive_mutex&) = delete;

    /// @brief The recursive_mutex is not copy-assignable.
    recursive_mutex& operator=(const recursive_mutex&) = delete;
    /// @brief The recursive_mutex is not move-constructible.

    recursive_mutex(recursive_mutex&&) = delete;

    /// @brief The recursive_mutex is not move-assignable.
    recursive_mutex& operator=(recursive_mutex&&) = delete;

    /// @brief Destroys the recursive_mutex.
    /// @note The destructor must not be called while the recursive_mutex is
    /// still locked or while there are tasks suspended on it. The destructor
    /// does not notify or resume any waiting tasks.
    ~recursive_mutex();

    /// @brief Attempts to acquire the recursive_mutex without suspending.
    /// @return true if the recursive_mutex was successfully acquired, false
    /// otherwise.
    /// @throws std::runtime_error if called outside of a TBB resumable task.
    bool try_lock();

    /// @brief Acquires the recursive_mutex, suspending the calling task if
    /// necessary until the recursive_mutex becomes available.
    /// @throws std::runtime_error if called outside of a TBB resumable task.
    /// @note The suspended task must remain valid until it is resumed.
    /// @note The suspended task must be resumed before the recursive_mutex is
    /// destroyed.
    void lock();

    /// @brief Releases the recursive_mutex. If there are tasks suspended on the
    /// recursive_mutex, exactly one of them is resumed and acquires the
    /// recursive_mutex.
    void unlock();

    /// @brief The recursive_mutex is not a reader-writer (shared) mutex.
    static inline constexpr bool is_rw_mutex = false;
    /// @brief The recursive_mutex is recursive.
    static inline constexpr bool is_recursive_mutex = true;
    /// @brief The recursive_mutex does not provide any fairness guarantees.
    static inline constexpr bool is_fair_mutex = false;

    private:
    static inline void* current_owner_token() {
        auto* task_ptr = tbb::detail::d1::current_task_ptr();
        if (task_ptr == nullptr) {
            throw std::runtime_error(
                "recursive_mutex can only be used in the context of a TBB "
                "resumable task");
        }

        return task_ptr;
    }

    std::atomic<void*> m_owner = nullptr;
    std::atomic<std::size_t> m_recursion = 0;
    detail::wait_queue m_wait_queue;
};

inline recursive_mutex::~recursive_mutex() {
    assert(m_owner.load(std::memory_order_acquire) == 0);
    assert(m_recursion.load(std::memory_order_relaxed) == 0);
    assert(m_wait_queue.empty());
}

inline bool recursive_mutex::try_lock() {
    const auto self = current_owner_token();

    // Recursive acquisition by the current owner.
    if (m_owner.load(std::memory_order_acquire) == self) {
        m_recursion.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // First acquisition: claim ownership.
    void* expected = nullptr;
    if (m_owner.compare_exchange_strong(expected, self,
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
        m_recursion.store(1, std::memory_order_relaxed);
        return true;
    }

    return false;
}

inline void recursive_mutex::lock() {
    const auto* self = current_owner_token();

    while (!try_lock()) {
        m_wait_queue.wait_if([this, self] {
            const auto* owner = m_owner.load(std::memory_order_acquire);
            return owner != nullptr && owner != self;
        });
    }
}

inline void recursive_mutex::unlock() {
    const auto* self = current_owner_token();
    assert(m_owner.load(std::memory_order_acquire) == self);
    auto prev = m_recursion.load(std::memory_order_relaxed);
    assert(prev > 0);

    prev = m_recursion.fetch_sub(1, std::memory_order_release);
    if (prev > 1) {
        return;
    }

    // Fully releasing the mutex.
    m_recursion.store(0, std::memory_order_relaxed);
    m_owner.store(nullptr, std::memory_order_release);

    // Wake a single waiter (if any). The woken task will retry try_lock().
    m_wait_queue.resume_one();
}

}  // namespace coopsync_tbb

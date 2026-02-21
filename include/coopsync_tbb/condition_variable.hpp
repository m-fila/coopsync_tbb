#pragma once

#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

/// @brief Condition variable used to synchronize tasks. The tasks can be
/// suspended on this condition variable and then later notify to resume if
/// given condition is met.
class condition_variable {
    public:
    /// @brief Constructs a new condition variable.
    condition_variable() = default;

    /// @brief The condition_variable is not copy-constructible.
    condition_variable(const condition_variable&) = delete;

    /// @brief The condition_variable is not copy-assignable.
    condition_variable& operator=(const condition_variable&) = delete;

    /// @brief THe condition_variable is not move-constructible.
    condition_variable(condition_variable&&) = delete;

    /// @brief The condition_variable is not move-assignable.
    condition_variable& operator=(condition_variable&&) = delete;

    /// @brief Destroys the condition variable.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the condition variable. The destructor does not notify or
    /// resume any waiting tasks.
    ~condition_variable();

    /// @brief Resumes one task suspended waiting on this condition variable, if
    /// there is any.
    void notify_one();

    /// @brief Resumes all tasks suspended waiting on this condition variable.
    void notify_all();

    /// @brief Suspends the calling task until it is notified.
    /// @tparam Lock A lock type providing lock() and unlock().
    /// @param lock The lock to be released while waiting and reacquired after
    /// resumption. The lock must be in an locked state before calling this
    /// function.
    template <typename Lock>
    void wait(Lock& lock);

    /// @brief Suspends the calling task until pred() becomes true.
    /// @tparam Lock A lock type providing lock() and unlock().
    /// @tparam Pred Callable returning bool. Must be invocable without
    /// arguments.
    /// @param lock The lock to be released while waiting and reacquired after
    /// resumption. The lock must be in an locked state before calling this
    /// function.
    /// @param pred The predicate to be evaluated after each resumption, if it
    /// returns false the task is suspended again.
    /// @throws any exception thrown by pred().
    template <typename Lock, typename Pred>
    void wait(Lock& lock, Pred pred);

    private:
    detail::wait_queue m_waiters;
};

inline condition_variable::~condition_variable() {
    assert(m_waiters.empty());
}

inline void condition_variable::notify_one() {
    m_waiters.resume_one();
}

inline void condition_variable::notify_all() {
    m_waiters.resume_all();
}

template <typename Lock>
inline void condition_variable::wait(Lock& lock) {
    lock.unlock();
    m_waiters.wait_if([] { return true; });

    // Post-resumption, reacquire the lock.
    lock.lock();
}

template <typename Lock, typename Pred>
inline void condition_variable::wait(Lock& lock, Pred pred) {
    while (!pred()) {
        wait(lock);
    }
}

}  // namespace coopsync_tbb

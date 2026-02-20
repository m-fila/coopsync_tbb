#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <cassert>

#include "coopsync_tbb/detail/intrusive_list.hpp"

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
    using waiter_t = tbb::task::suspend_point;

    tbb::spin_mutex m_waiters_mutex;
    detail::intrusive_list<waiter_t> m_waiters;
};

inline condition_variable::~condition_variable() {
    assert(m_waiters.empty());
}

inline void condition_variable::notify_one() {

    typename detail::intrusive_list<waiter_t>::node* waiter = nullptr;
    {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        waiter = m_waiters.pop_front();
    }

    if (waiter) {
        tbb::task::resume(waiter->value);
    }
}

inline void condition_variable::notify_all() {

    auto waiters_to_resume = detail::intrusive_list<waiter_t>{};

    {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        while (auto* waiter = m_waiters.pop_front()) {
            waiters_to_resume.push_back(*waiter);
        }
    }

    while (const auto* waiter = waiters_to_resume.pop_front()) {
        tbb::task::resume(waiter->value);
    }
}

template <typename Lock>
inline void condition_variable::wait(Lock& lock) {

    auto node = typename detail::intrusive_list<waiter_t>::node{};
    // node must remain valid until the task is
    // resumed. It's a local variable on a stack of suspended task which is
    // preserved during suspension so it isn't an issue.
    tbb::task::suspend([this, &node, &lock](tbb::task::suspend_point sp) {
        node.value = sp;
        {
            tbb::spin_mutex::scoped_lock waiters_lock(m_waiters_mutex);
            m_waiters.push_back(node);
        }
        lock.unlock();
    });

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

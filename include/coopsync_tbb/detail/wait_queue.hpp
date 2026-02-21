#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <cassert>
#include <cstddef>
#include <utility>

#include "coopsync_tbb/detail/intrusive_list.hpp"

namespace coopsync_tbb::detail {
class wait_queue {
    public:
    /// @brief Constructs an empty wait queue.
    wait_queue() = default;

    /// @brief The wait_queue is not copy-constructible.
    wait_queue(const wait_queue&) = delete;

    /// @brief The wait_queue is not copy-assignable.
    wait_queue& operator=(const wait_queue&) = delete;

    /// @brief The wait_queue is not move-constructible.
    wait_queue(wait_queue&&) = delete;

    /// @brief The wait_queue is not move-assignable.
    wait_queue& operator=(wait_queue&&) = delete;

    /// @brief Destroys the wait queue. The destructor does not resume any
    /// waiting tasks, so it must not be called while it is not empty.
    ~wait_queue();

    /// @brief Resumes one of the waiting tasks, if there is any.
    void resume_one();

    /// @brief Resumes all waiting tasks.
    void resume_all();

    /// @brief Resumes up to n waiting tasks. If there are fewer than n waiting
    /// tasks, resumes all of them.
    /// @param n The maximum number of tasks to resume. Must be non-negative.
    /// @return The number of tasks that were resumed.
    std::ptrdiff_t resume_n(std::ptrdiff_t n);

    /// @brief Checks if there are are no waiting tasks.
    /// @return true if there are no waiting tasks, false otherwise.
    bool empty() const;

    /// @brief Suspends the calling task if predicate returns true.
    /// @tparam Pred A callable type that returns a bool and is invocable
    /// without arguments.
    /// @param pred The predicate to be evaluated. If it returns true, the task
    /// is suspended and added to the wait queue. If it returns false, the
    /// function returns immediately. The predicate may be evaluated multiple
    /// times, it should not throw or perform any blocking operations.
    template <typename Pred>
    void wait_if(Pred pred);

    private:
    using waiter_t = tbb::task::suspend_point;

    mutable tbb::spin_mutex m_waiters_mutex;
    intrusive_list<waiter_t> m_waiters;

    void do_resume_all(intrusive_list<waiter_t>& waiters_to_resume);
};

inline wait_queue::~wait_queue() {
    assert(m_waiters.empty());
}

inline void wait_queue::resume_one() {
    typename detail::intrusive_list<waiter_t>::node* waiter = nullptr;
    {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        waiter = m_waiters.pop_front();
    }
    if (waiter) {
        tbb::task::resume(waiter->value);
    }
}

inline void wait_queue::resume_all() {
    auto waiters_to_resume = detail::intrusive_list<waiter_t>{};
    {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        waiters_to_resume.swap(m_waiters);
        assert(m_waiters.empty());
    }
    do_resume_all(waiters_to_resume);
}

inline std::ptrdiff_t wait_queue::resume_n(std::ptrdiff_t n) {
    assert(n >= 0);
    if (n == 0) {
        return 0;
    }
    ptrdiff_t resumed = 0;
    auto waiters_to_resume = detail::intrusive_list<waiter_t>{};
    {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        while (resumed < n) {
            auto* waiter = m_waiters.pop_front();
            if (!waiter) {
                break;
            }
            waiters_to_resume.push_back(*waiter);
            ++resumed;
        }
    }
    do_resume_all(waiters_to_resume);
    return resumed;
}

inline bool wait_queue::empty() const {
    tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
    return m_waiters.empty();
}

template <typename Pred>
void wait_queue::wait_if(Pred pred) {

    // Fast path
    if (!pred()) {
        return;
    }

    // Slow path
    auto node = typename detail::intrusive_list<waiter_t>::node{};
    // node must remain valid until the task is resumed. It's a local
    // variable on a stack of suspended task which is preserved during
    // suspension so it isn't an issue.
    tbb::task::suspend([this, &node, pred = std::move(pred)](
                           tbb::task::suspend_point sp) mutable {
        {
            // Re-check while holding the lock to avoid racing with a
            // resume_* call.
            tbb::spin_mutex::scoped_lock waiters_lock(m_waiters_mutex);
            if (pred()) {
                node.value = sp;
                m_waiters.push_back(node);
                return;
            }
        }

        // Resume immediately in case the re-check succeeded.
        tbb::task::resume(sp);
    });
}

inline void wait_queue::do_resume_all(
    intrusive_list<waiter_t>& waiters_to_resume) {
    while (const auto* waiter = waiters_to_resume.pop_front()) {
        tbb::task::resume(waiter->value);
    }
}
}  // namespace coopsync_tbb::detail

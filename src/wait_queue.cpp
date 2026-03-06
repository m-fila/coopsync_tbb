#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb::detail {

wait_queue::~wait_queue() {
    assert(m_waiters.empty());  // LCOV_EXCL_LINE
}

bool wait_queue::resume_one() {
    typename detail::intrusive_list<waiter_t>::node* waiter = nullptr;
    {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        waiter = m_waiters.pop_front();
    }
    if (waiter) {
        tbb::task::resume(waiter->value);
        return true;
    }
    return false;
}

void wait_queue::resume_all() {
    auto waiters_to_resume = detail::intrusive_list<waiter_t>{};
    {
        tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
        waiters_to_resume.swap(m_waiters);
        assert(m_waiters.empty());  // LCOV_EXCL_LINE
    }
    do_resume_all(waiters_to_resume);
}

std::ptrdiff_t wait_queue::resume_n(std::ptrdiff_t n) {
    assert(n >= 0);  // LCOV_EXCL_LINE
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

bool wait_queue::empty() const {
    tbb::spin_mutex::scoped_lock lock(m_waiters_mutex);
    return m_waiters.empty();
}

void wait_queue::do_resume_all(intrusive_list<waiter_t>& waiters_to_resume) {
    while (const auto* waiter = waiters_to_resume.pop_front()) {
        tbb::task::resume(waiter->value);
    }
}

}  // namespace coopsync_tbb::detail

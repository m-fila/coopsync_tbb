#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>

#include "coopsync_tbb/detail/intrusive_list.hpp"

namespace coopsync_tbb {

class latch {
    public:
    latch(std::ptrdiff_t expected) : m_counter(expected) {
        assert(expected >= 0);
    }

    latch(const latch &) = delete;
    latch &operator=(const latch &) = delete;
    latch(latch &&) = delete;
    latch &operator=(latch &&) = delete;
    ~latch() = default;

    static constexpr std::ptrdiff_t max() noexcept {
        return std::numeric_limits<std::ptrdiff_t>::max();
    }

    bool try_wait() const noexcept {
        return m_counter.load(std::memory_order_acquire) == 0;
    }

    void arrive_and_wait(std::ptrdiff_t update = 1) noexcept {
        count_down(update);
        wait();
    }

    void count_down(std::ptrdiff_t update = 1) noexcept {
        assert(update >= 0);
        if (m_counter.fetch_sub(update, std::memory_order_acq_rel) == update) {
            tbb::spin_mutex::scoped_lock lock(m_mutex);
            while (auto *item = m_queue.pop_front()) {
                tbb::task::resume(item->value);
            }
        }
    }

    void wait() noexcept {
        // Fast path
        if (try_wait()) {
            return;
        }
        // Slow path
        auto node = detail::intrusive_list<tbb::task::suspend_point>::node{};
        m_mutex.lock();
        // Guaranteed that the suspend lambda will be executed on the same
        // thread so capturing locked mutex is fine.
        tbb::task::suspend([this, &node](tbb::task::suspend_point sp) {
            node.value = sp;
            m_queue.push_back(node);
            m_mutex.unlock();
        });
    }

    private:
    std::atomic<std::ptrdiff_t> m_counter;
    // tbb::concurrent_queue<tbb::task::suspend_point> m_queue;
    tbb::spin_mutex m_mutex;
    detail::intrusive_list<tbb::task::suspend_point> m_queue;
};

}  // namespace coopsync_tbb

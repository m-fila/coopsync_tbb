#pragma once

#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/task.h>

#include <atomic>
#include <cassert>
#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

#include "coopsync_tbb/detail/intrusive_list.hpp"
#include "coopsync_tbb/detail/macros.hpp"

namespace coopsync_tbb {

class future_error : public std::logic_error {
    public:
    explicit future_error(std::future_errc errc)
        : future_error(make_error_code(errc)) {}

    const std::error_code& code() const noexcept { return m_errc; }

    private:
    explicit future_error(std::error_code errc)
        : logic_error(errc.message()), m_errc(std::move(errc)) {}
    std::error_code m_errc;
};

namespace detail::future {

/// @brief Enumeration encoding internal status of a shared state.
enum class status : unsigned char {
    empty = 0,      ///< No value or exception has been set.
    value = 1,      ///< A value has been set and is ready to be consumed.
    exception = 2,  ///< An exception has been set and is ready to be rethrown.
    broken =
        3,  ///< The promise was destroyed without setting a value or exception.
};

///
template <typename T>
struct shared_state {
    static_assert(!std::is_void_v<T>,
                  "coopsync_tbb::future<void> is not implemented");
    static_assert(!std::is_reference_v<T>,
                  "coopsync_tbb::future<T&> is not implemented");

    using waiter_t = tbb::task::suspend_point;

    tbb::spin_mutex waiters_mutex;
    intrusive_list<waiter_t> waiters;

    std::atomic<status> state{status::empty};
    std::atomic<bool> future_obtained{false};
    std::atomic<bool> value_consumed{false};

    std::optional<T> value;
    std::exception_ptr exception;

    bool ready() const noexcept {
        return state.load(std::memory_order_acquire) != status::empty;
    }

    void resume_all_waiters() {
        auto lock = tbb::spin_mutex::scoped_lock(waiters_mutex);
        while (const auto* waiter = waiters.pop_front()) {
            tbb::task::resume(waiter->value);
        }
    }

    template <typename R>
    void set_value(R&& v) {
        const auto s = state.load(std::memory_order_acquire);
        if (s != status::empty) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        value.emplace(std::forward<R>(v));
        state.store(status::value, std::memory_order_release);
        resume_all_waiters();
    }

    void set_exception(std::exception_ptr p) {
        const auto s = state.load(std::memory_order_acquire);
        if (s != status::empty) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        exception = std::move(p);
        state.store(status::exception, std::memory_order_release);
        resume_all_waiters();
    }

    void break_promise() noexcept {
        const auto s = state.load(std::memory_order_acquire);
        if (s != status::empty) {
            return;
        }
        state.store(status::broken, std::memory_order_release);
        resume_all_waiters();
    }

    void wait() {
        if (ready()) {
            return;
        }

        auto node = intrusive_list<tbb::task::suspend_point>::node{};
        waiters_mutex.lock();

        if (ready()) {
            waiters_mutex.unlock();
            return;
        }

        tbb::task::suspend([this, &node](tbb::task::suspend_point sp) {
            node.value = sp;
            waiters.push_back(node);
            waiters_mutex.unlock();
        });

        assert(ready());
    }
};

}  // namespace detail::future

template <typename T>
class promise;

template <typename T>
class future {
    public:
    future() noexcept = default;
    future(const future&) = delete;
    future& operator=(const future&) = delete;
    future(future&&) noexcept = default;
    future& operator=(future&&) noexcept = default;
    ~future() = default;

    bool valid() const noexcept { return static_cast<bool>(m_state); }

    void wait() const {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }
        m_state->wait();
    }

    T get() {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }

        m_state->wait();
        const auto st = m_state->state.load(std::memory_order_acquire);
        if (st == detail::future::status::exception) {
            std::rethrow_exception(m_state->exception);
        }
        if (st == detail::future::status::broken) {
            throw future_error(std::future_errc::broken_promise);
        }
        assert(st == detail::future::status::value);

        bool expected = false;
        if (!m_state->value_consumed.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            throw future_error(std::future_errc::future_already_retrieved);
        }

        auto out = std::move(*m_state->value);
        m_state.reset();
        return out;
    }

    private:
    friend class promise<T>;
    explicit future(std::shared_ptr<detail::future::shared_state<T>> state)
        : m_state(std::move(state)) {}

    std::shared_ptr<detail::future::shared_state<T>> m_state;
};

template <typename T>
class promise {
    public:
    promise() : m_state(std::make_shared<detail::future::shared_state<T>>()) {}
    promise(const promise&) = delete;
    promise& operator=(const promise&) = delete;
    promise(promise&&) noexcept = default;
    promise& operator=(promise&&) noexcept = default;

    ~promise() {
        if (m_state) {
            m_state->break_promise();
        }
    }

    void swap(promise& other) noexcept { m_state.swap(other.m_state); }

    COOPSYNC_TBB_NODISCARD future<T> get_future() {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }
        bool expected = false;
        if (!m_state->future_obtained.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            throw future_error(std::future_errc::future_already_retrieved);
        }
        return future<T>(m_state);
    }

    void set_value(const T& v) {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }
        m_state->set_value(v);
    }

    void set_value(T&& v) {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }
        m_state->set_value(std::move(v));
    }

    void set_exception(std::exception_ptr p) {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }
        m_state->set_exception(std::move(p));
    }

    private:
    std::shared_ptr<detail::future::shared_state<T>> m_state;
};

}  // namespace coopsync_tbb

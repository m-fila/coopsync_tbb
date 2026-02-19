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

/// @brief Exception type thrown by future and promise operations.
class future_error : public std::logic_error {
    public:
    /// @brief Constructs a future_error with the given error code.
    /// @param errc The error code to associate with this future_error.
    explicit future_error(std::future_errc errc)
        : future_error(make_error_code(errc)) {}

    /// @brief Obtains the error code associated with this future_error.
    /// @return The error code associated with this future_error.
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

struct shared_state_base {

    using waiter_t = tbb::task::suspend_point;

    tbb::spin_mutex waiters_mutex;
    intrusive_list<waiter_t> waiters;

    std::atomic<status> state{status::empty};
    std::atomic<bool> future_obtained{false};
    std::atomic<bool> value_consumed{false};

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

template <typename T>
struct shared_state : public shared_state_base {

    std::optional<T> value;

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
};

template <>
struct shared_state<void> : public shared_state_base {
    void set_value() {
        const auto s = state.load(std::memory_order_acquire);
        if (s != status::empty) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        state.store(status::value, std::memory_order_release);
        resume_all_waiters();
    }
};

template <typename T>
struct shared_state<T&> : public shared_state_base {
    T* value = nullptr;

    void set_value(T& v) {
        const auto s = state.load(std::memory_order_acquire);
        if (s != status::empty) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        value = std::addressof(v);
        state.store(status::value, std::memory_order_release);
        resume_all_waiters();
    }
};

template <typename T>
class future_base {
    protected:
    std::shared_ptr<shared_state<T>>
        m_state;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

    future_base() noexcept = default;
    explicit future_base(std::shared_ptr<shared_state<T>> state)
        : m_state(std::move(state)) {}

    /// @brief Throws if this future has no shared state.
    /// @throws future_error with \c std::future_errc::no_state.
    void ensure_valid() const {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }
    }

    /// @brief Rethrows a stored exception or throws broken_promise.
    ///
    /// Assumes the shared state is ready.
    /// @throws Any exception stored in the shared state.
    /// @throws future_error with \c std::future_errc::broken_promise.
    void throw_if_exception_or_broken() const {
        const auto st = m_state->state.load(std::memory_order_acquire);
        if (st == status::exception) {
            std::rethrow_exception(m_state->exception);
        }
        if (st == status::broken) {
            throw future_error(std::future_errc::broken_promise);
        }
        assert(st == status::value);
    }

    /// @brief Marks the shared state as consumed.
    /// @throws future_error with \c std::future_errc::future_already_retrieved.
    void mark_consumed() {
        bool expected = false;
        if (!m_state->value_consumed.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            throw future_error(std::future_errc::future_already_retrieved);
        }
    }

    /// @brief Releases the reference to the shared state.
    void reset() noexcept { m_state.reset(); }

    /// @brief Releases the shared state pointer and leaves this object invalid.
    COOPSYNC_TBB_NODISCARD std::shared_ptr<shared_state<T>> release_state() noexcept {
        return std::exchange(m_state, std::shared_ptr<shared_state<T>>{});
    }

    public:
    /// @brief Tests whether the future is valid and refers to a shared state.
    /// @return true if the future is valid, false otherwise.
    bool valid() const noexcept { return static_cast<bool>(m_state); }

    /// @brief Suspends the current task until the shared state is ready.
    ///
    /// If the shared state is ready this function returns immediately.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    void wait() const {
        ensure_valid();
        m_state->wait();
    }
};

template <typename T>
class promise_base {
    protected:
    std::shared_ptr<shared_state<T>>
        m_state;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

    /// @brief Constructs a new promise with an empty shared state.
    promise_base() : m_state(std::make_shared<shared_state<T>>()) {}

    /// @brief Constructs a new promise using the given allocator.
    /// @param alloc Allocator used to allocate the shared state.
    template <typename Alloc>
    explicit promise_base(const Alloc& alloc)
        : m_state(std::allocate_shared<shared_state<T>>(alloc)) {}

    /// @brief Throws if this promise has no shared state.
    /// @throws future_error with \c std::future_errc::no_state.
    void ensure_valid() const {
        if (!m_state) {
            throw future_error(std::future_errc::no_state);
        }
    }

    /// @brief Marks that the future has been obtained.
    /// @throws future_error with \c std::future_errc::future_already_retrieved
    /// if the future was already retrieved.
    void mark_future_obtained() {
        bool expected = false;
        if (!m_state->future_obtained.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            throw future_error(std::future_errc::future_already_retrieved);
        }
    }

    public:
    promise_base(const promise_base&) = delete;
    promise_base& operator=(const promise_base&) = delete;
    promise_base(promise_base&&) noexcept = default;
    promise_base& operator=(promise_base&&) noexcept = default;

    /// @brief Destroys the promise.
    /// If the shared state is not ready, it is marked as broken.
    ~promise_base() {
        if (m_state) {
            m_state->break_promise();
        }
    }

    /// @brief Exchanges the shared states between two promises.
    /// @param other The promise to exchange shared state with.
    void swap(promise_base& other) noexcept { m_state.swap(other.m_state); }

    /// @brief Stores an exception into the shared state and makes it ready.
    /// @param p The exception to store in the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    void set_exception(std::exception_ptr p) {
        ensure_valid();
        m_state->set_exception(std::move(p));
    }
};

}  // namespace detail::future

template <typename T>
class future;

template <typename T>
class promise;

template <typename T>
class shared_future;

/// @brief Future holding a shared state with a value type. Waiting or
/// getting the value from a not ready state suspends the current task until the
/// state becomes ready. After the value is retrieved, the future is left in not
/// valid state and cannot be used again.
/// @tparam T The type of the value that will be stored in the shared state.
template <typename T>
class future : private detail::future::future_base<T> {
    public:
    /// @brief Constructs a new future. After construction the future has no
    /// shared state and is not valid.
    future() noexcept = default;

    /// @brief The future is not copy-constructible.
    future(const future&) = delete;

    /// @brief The future is not copy-assignable.
    future& operator=(const future&) = delete;

    /// @brief The future is move-constructible, after construction has no
    /// shared state and is not valid.
    future(future&&) noexcept = default;

    /// @brief The future is move-assignable.
    /// @param other The future to move from, after assignment has no
    /// shared state and is not valid.
    future& operator=(future&&) noexcept = default;

    /// @brief Destroys the future releasing any shared state.
    ~future() = default;

    /// @brief Tests whether the future is valid and refers to a shared state.
    /// @return true if the future is valid, false otherwise.
    bool valid() const noexcept {
        return detail::future::future_base<T>::valid();
    }

    /// @brief Suspends the current task until the shared state is ready.
    ///
    /// If the shared state is ready this function returns immediately.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    void wait() const { detail::future::future_base<T>::wait(); }

    /// @brief Retrieves the value stored in the shared state.
    ///
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready.
    /// After retrieval the future is left in not valid state.
    ///
    /// @return The value stored in the shared state.
    /// @throws future_error if the future is not valid, if the promise was
    /// broken, or if the value was already retrieved.
    /// @throws Any exception stored in the shared state.
    typename std::remove_reference<T>::type get() {
        this->ensure_valid();
        this->m_state->wait();
        this->throw_if_exception_or_broken();
        this->mark_consumed();

        assert(this->m_state->value.has_value());
        auto out = std::move(*this->m_state->value);
        this->reset();
        return out;
    }

    /// @brief Converts this future into a shared_future. After this call, the
    /// current future is left in not valid state.
    /// @return A shared_future referring to the same shared state.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    COOPSYNC_TBB_NODISCARD shared_future<T> share();

    private:
    friend class promise<T>;
    explicit future(std::shared_ptr<detail::future::shared_state<T>> state)
        : detail::future::future_base<T>(std::move(state)) {}
};

/// @brief Specialization of future for void type.
template <>
class future<void> : private detail::future::future_base<void> {
    public:
    /// @brief Constructs a new future. After construction the future has no
    /// shared state and is not valid.
    future() noexcept = default;

    /// @brief The future is not copy-constructible.
    future(const future&) = delete;

    /// @brief The future is not copy-assignable.
    future& operator=(const future&) = delete;

    /// @brief The future is move-constructible, after construction has no
    /// shared state and is not valid.
    future(future&&) noexcept = default;

    /// @brief The future is move-assignable.
    /// @param other The future to move from, after assignment has no
    /// shared state and is not valid.
    future& operator=(future&&) noexcept = default;

    /// @brief Destroys the future releasing any shared state.
    ~future() = default;

    /// @brief Tests whether the future is valid and refers to a shared state.
    /// @return true if the future is valid, false otherwise.
    bool valid() const noexcept {
        return detail::future::future_base<void>::valid();
    }

    /// @brief Suspends the current task until the shared state is ready.
    ///
    /// If the shared state is ready this function returns immediately.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    void wait() const { detail::future::future_base<void>::wait(); }

    /// @brief Waits for readiness and consumes the shared state.
    ///
    /// After this call the future is left in not valid state.
    ///
    /// @throws future_error if the future is not valid, if the promise was
    /// broken, or if the value was already retrieved.
    /// @throws Any exception stored in the shared state.
    void get() {
        this->ensure_valid();
        this->m_state->wait();
        this->throw_if_exception_or_broken();
        this->mark_consumed();
        this->reset();
    }

    /// @brief Converts this future into a shared_future. After this call, the
    /// current future is left in not valid state.
    /// @return A shared_future referring to the same shared state.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    COOPSYNC_TBB_NODISCARD shared_future<void> share();

    private:
    friend class promise<void>;
    explicit future(std::shared_ptr<detail::future::shared_state<void>> state)
        : detail::future::future_base<void>(std::move(state)) {}
};

/// @brief Specialization of future for reference types.
// @tparam T The type of the value that will be stored in the shared state.
template <typename T>
class future<T&> : private detail::future::future_base<T&> {
    public:
    /// @brief Constructs a new future. After construction the future has no
    /// shared state and is not valid.
    future() noexcept = default;

    /// @brief The future is not copy-constructible.
    future(const future&) = delete;

    /// @brief The future is not copy-assignable.
    future& operator=(const future&) = delete;

    /// @brief The future is move-constructible, after construction has no
    /// shared state and is not valid.
    future(future&&) noexcept =  // cppcheck-suppress noExplicitConstructor
        default;

    /// @brief The future is move-assignable.
    /// @param other The future to move from, after assignment has no
    /// shared state and is not valid.
    future& operator=(future&&) noexcept = default;

    /// @brief Destroys the future releasing any shared state.
    ~future() = default;

    /// @brief Tests whether the future is valid and refers to a shared state.
    /// @return true if the future is valid, false otherwise.
    bool valid() const noexcept {
        return detail::future::future_base<T&>::valid();
    }

    /// @brief Suspends the current task until the shared state is ready.
    ///
    /// If the shared state is ready this function returns immediately.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    void wait() const { detail::future::future_base<T&>::wait(); }

    /// @brief Retrieves the reference stored in the shared state.
    ///
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready.
    /// After retrieval the future is left in not valid state.
    ///
    /// @return Reference stored in the shared state.
    /// @throws future_error if the future is not valid, if the promise was
    /// broken, or if the value was already retrieved.
    /// @throws Any exception stored in the shared state.
    T& get() {
        this->ensure_valid();
        this->m_state->wait();
        this->throw_if_exception_or_broken();
        this->mark_consumed();

        assert(this->m_state->value != nullptr);
        T* out = this->m_state->value;
        this->reset();
        return *out;
    }

    /// @brief Converts this future into a shared_future. After this call, the
    /// current future is left in not valid state.
    /// @return A shared_future referring to the same shared state.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    COOPSYNC_TBB_NODISCARD shared_future<T&> share();

    private:
    friend class promise<T&>;
    explicit future(std::shared_ptr<detail::future::shared_state<T&>> state)
        : detail::future::future_base<T&>(std::move(state)) {}
};

/// @brief Copyable future that allows retrieving the result multiple times.
///
/// Unlike future, shared_future does not consume the shared state when get()
/// is called.
///
/// @tparam T The type of the value stored in the shared state.
template <typename T>
class shared_future : private detail::future::future_base<T> {
    public:
    /// @brief Constructs an invalid shared_future.
    shared_future() noexcept = default;

    /// @brief The shared_future is copyable.
    shared_future(const shared_future&) noexcept = default;
    shared_future& operator=(const shared_future&) noexcept = default;

    /// @brief The shared_future is moveable.
    shared_future(shared_future&&) noexcept = default;
    shared_future& operator=(shared_future&&) noexcept = default;

    /// @brief Destroys the shared_future releasing any shared state.
    ~shared_future() = default;

    /// @brief Tests whether the shared_future is valid and refers to a shared
    /// state.
    /// @return true if the shared_future is valid, false otherwise.
    bool valid() const noexcept {
        return detail::future::future_base<T>::valid();
    }

    /// @brief Suspends the current task until the shared state is ready.
    /// @throws future_error with \c std::future_errc::no_state if not valid.
    void wait() const { detail::future::future_base<T>::wait(); }

    /// @brief Retrieves the stored value.
    ///
    /// This function may be called multiple times.
    ///
    /// @return A const reference to the stored value.
    /// @throws future_error if not valid or if the promise is broken.
    /// @throws Any exception stored in the shared state.
    const T& get() const {
        this->ensure_valid();
        this->m_state->wait();
        this->throw_if_exception_or_broken();
        assert(this->m_state->value.has_value());
        return *this->m_state->value;
    }

    private:
    template <typename U>
    friend class future;
    explicit shared_future(
        std::shared_ptr<detail::future::shared_state<T>> state)
        : detail::future::future_base<T>(std::move(state)) {}
};

/// @brief Specialization of shared_future for void type.
template <>
class shared_future<void> : private detail::future::future_base<void> {
    public:
    shared_future() noexcept = default;
    shared_future(const shared_future&) noexcept = default;
    shared_future& operator=(const shared_future&) noexcept = default;
    shared_future(shared_future&&) noexcept = default;
    shared_future& operator=(shared_future&&) noexcept = default;
    ~shared_future() = default;

    bool valid() const noexcept {
        return detail::future::future_base<void>::valid();
    }

    void wait() const { detail::future::future_base<void>::wait(); }

    /// @brief Waits for readiness and checks for exception/broken promise.
    void get() const {
        this->ensure_valid();
        this->m_state->wait();
        this->throw_if_exception_or_broken();
    }

    private:
    template <typename U>
    friend class future;
    explicit shared_future(
        std::shared_ptr<detail::future::shared_state<void>> state)
        : detail::future::future_base<void>(std::move(state)) {}
};

/// @brief Specialization of shared_future for reference types.
template <typename T>
class shared_future<T&> : private detail::future::future_base<T&> {
    public:
    shared_future() noexcept = default;
    shared_future(const shared_future&) noexcept = default;
    shared_future& operator=(const shared_future&) noexcept = default;
    shared_future(shared_future&&) noexcept = default;
    shared_future& operator=(shared_future&&) noexcept = default;
    ~shared_future() = default;

    bool valid() const noexcept {
        return detail::future::future_base<T&>::valid();
    }

    void wait() const { detail::future::future_base<T&>::wait(); }

    /// @brief Retrieves the stored reference.
    ///
    /// This function may be called multiple times.
    T& get() const {
        this->ensure_valid();
        this->m_state->wait();
        this->throw_if_exception_or_broken();
        assert(this->m_state->value != nullptr);
        return *this->m_state->value;
    }

    private:
    template <typename U>
    friend class future;
    explicit shared_future(
        std::shared_ptr<detail::future::shared_state<T&>> state)
        : detail::future::future_base<T&>(std::move(state)) {}
};

template <typename T>
shared_future<T> future<T>::share() {
    this->ensure_valid();
    return shared_future<T>(this->release_state());
}

inline shared_future<void> future<void>::share() {
    this->ensure_valid();
    return shared_future<void>(this->release_state());
}

template <typename T>
shared_future<T&> future<T&>::share() {
    this->ensure_valid();
    return shared_future<T&>(this->release_state());
}

/// @brief Promise holding a shared state with a value type. The promise is
/// used to set the value in the shared state that can be retrieved by a future
/// sharing the same state.
/// @tparam T The type of the value that will be stored in the shared state.
template <typename T>
class promise : private detail::future::promise_base<T> {
    public:
    /// @brief Constructs a new promise with an empty shared state.
    promise() = default;

    /// @brief Constructs a new promise with a shared state allocated using the
    /// given allocator.
    /// @param alloc The allocator to use for allocating the shared state.
    /// Must meet the standard requirement of Allocator.
    template <typename Alloc>
    explicit promise(const Alloc& alloc)
        : detail::future::promise_base<T>(alloc) {}

    /// @brief The promise is not copy-constructible.
    promise(const promise&) = delete;

    /// @brief The promise is not copy-assignable.
    promise& operator=(const promise&) = delete;

    /// @brief The promise is move-constructible.
    /// @param other The promise to move from, after construction has empty
    /// shared state.
    promise(promise&& other) noexcept = default;

    /// @brief The promise is move-assignable.
    /// @param other The promise to move from, after assignment has empty shared
    /// state.
    promise& operator=(promise&& other) noexcept = default;

    /// @brief Destroys the promise.
    /// If the shared state is not ready, it is marked as broken.
    ~promise() = default;

    /// @brief Exchanges the shared states between two promises.
    /// @param other The promise to exchange shared state with.
    void swap(promise& other) noexcept {
        detail::future::promise_base<T>::swap(other);
    }

    /// @brief Returns a future associated with the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    /// @throws future_error with \c std::future_errc::future_already_retrieved
    /// if the future was already retrieved.
    COOPSYNC_TBB_NODISCARD future<T> get_future() {
        this->ensure_valid();
        this->mark_future_obtained();
        return future<T>(this->m_state);
    }

    /// @brief Stores a value into the shared state and makes it ready.
    /// @param v The value to store in the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    /// @throws future_error with \c std::future_errc::promise_already_satisfied
    /// if the shared state already stores a value or an exception.
    void set_value(const T& v) {
        this->ensure_valid();
        this->m_state->set_value(v);
    }

    /// @brief Stores a value into the shared state and makes it ready.
    /// @param v The value to store in the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    /// @throws future_error with \c std::future_errc::promise_already_satisfied
    /// if the shared state already stores a value or an exception.
    void set_value(T&& v) {
        this->ensure_valid();
        this->m_state->set_value(std::move(v));
    }

    /// @brief Stores an exception into the shared state and makes it ready.
    /// @param p The exception to store in the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    void set_exception(std::exception_ptr p) {
        detail::future::promise_base<T>::set_exception(std::move(p));
    }
};

/// @brief Specialization of promise for void type. The promise<void> does not
/// store a value, but only the state of readiness and any exception.
template <>
class promise<void> : private detail::future::promise_base<void> {
    public:
    /// @brief Constructs a new promise with an empty shared state.
    promise() = default;

    /// @brief Constructs a new promise using the given allocator.
    /// @param alloc Allocator used to allocate the shared state.
    template <typename Alloc>
    explicit promise(const Alloc& alloc)
        : detail::future::promise_base<void>(alloc) {}

    /// @brief The promise is not copy-constructible.
    promise(const promise&) = delete;

    /// @brief The promise is not copy-assignable.
    promise& operator=(const promise&) = delete;

    /// @brief The promise is move-constructible.
    /// @param other The promise to move from, after construction has empty
    /// shared state.
    promise(promise&& other) noexcept = default;

    /// @brief The promise is move-assignable.
    /// @param other The promise to move from, after assignment has empty shared
    /// state.
    promise& operator=(promise&& other) noexcept = default;

    /// @brief Destroys the promise.
    /// If the shared state is not ready, it is marked as broken.
    ~promise() = default;

    /// @brief Exchanges the shared states between two promises.
    /// @param other The promise to exchange shared state with.
    void swap(promise& other) noexcept {
        detail::future::promise_base<void>::swap(other);
    }

    /// @brief Returns a future associated with the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    /// @throws future_error with \c std::future_errc::future_already_retrieved
    /// if the future was already retrieved.
    COOPSYNC_TBB_NODISCARD future<void> get_future() {
        this->ensure_valid();
        this->mark_future_obtained();
        return future<void>(this->m_state);
    }

    /// @brief Marks the shared state as ready without storing a value.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    /// @throws future_error with \c std::future_errc::promise_already_satisfied
    /// if the shared state already stores a value or an exception.
    void set_value() {
        this->ensure_valid();
        this->m_state->set_value();
    }

    /// @brief Stores an exception into the shared state and makes it ready.
    /// @param p The exception to store in the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    void set_exception(std::exception_ptr p) {
        detail::future::promise_base<void>::set_exception(std::move(p));
    }
};

/// @brief Specialization of promise for reference types. The promise<T&> stores
/// a reference to a value of type T in the shared state.
template <typename T>
class promise<T&> : private detail::future::promise_base<T&> {
    public:
    /// @brief Constructs a new promise with an empty shared state.
    promise() = default;

    /// @brief Constructs a new promise using the given allocator.
    /// @param alloc Allocator used to allocate the shared state.
    template <typename Alloc>
    explicit promise(const Alloc& alloc)
        : detail::future::promise_base<T&>(alloc) {}

    /// @brief The promise is not copy-constructible.
    promise(const promise&) = delete;

    /// @brief The promise is not copy-assignable.
    promise& operator=(const promise&) = delete;

    /// @brief The promise is move-constructible.
    /// @param other The promise to move from, after construction has empty
    /// shared state.
    promise(  // cppcheck-suppress noExplicitConstructor
        promise&& other) noexcept = default;

    /// @brief The promise is move-assignable.
    /// @param other The promise to move from, after assignment has empty shared
    /// state.
    promise& operator=(promise&& other) noexcept = default;

    /// @brief Destroys the promise.
    /// If the shared state is not ready, it is marked as broken.
    ~promise() = default;

    /// @brief Exchanges the shared states between two promises.
    /// @param other The promise to exchange shared state with.
    void swap(promise& other) noexcept {
        detail::future::promise_base<T&>::swap(other);
    }

    /// @brief Returns a future associated with the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    /// @throws future_error with \c std::future_errc::future_already_retrieved
    /// if the future was already retrieved.
    COOPSYNC_TBB_NODISCARD future<T&> get_future() {
        this->ensure_valid();
        this->mark_future_obtained();
        return future<T&>(this->m_state);
    }

    /// @brief Stores a reference into the shared state and makes it ready.
    /// @param v The reference to store in the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    /// @throws future_error with \c std::future_errc::promise_already_satisfied
    /// if the shared state already stores a value or an exception.
    /// @note The reference must remain valid for the entire lifetime of the
    /// shared state, which is until the future is retrieved and consumed.
    void set_value(T& v) {
        this->ensure_valid();
        this->m_state->set_value(v);
    }

    /// @brief Stores an exception into the shared state and makes it ready.
    /// @param p The exception to store in the shared state.
    /// @throws future_error with \c std::future_errc::no_state if the promise
    /// is not valid.
    void set_exception(std::exception_ptr p) {
        detail::future::promise_base<T&>::set_exception(std::move(p));
    }
};

}  // namespace coopsync_tbb

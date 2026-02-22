#pragma once

#include <atomic>
#include <cassert>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/wait_queue.hpp"

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
    broken = 3,     ///< The promise was destroyed without setting a value or an
                    ///< exception.
    transition = 4  ///< The shared state is in a transient state while setting
                    ///< a value or an exception
};

struct shared_state_base {

    detail::wait_queue waiters;

    std::atomic<status> state{status::empty};
    std::atomic<bool> future_obtained{false};
    std::atomic<bool> value_consumed{false};

    std::exception_ptr exception;

    bool ready() const noexcept {
        auto status = state.load(std::memory_order_acquire);
        return (status == status::value) || (status == status::exception) ||
               (status == status::broken);
    }

    void set_exception(std::exception_ptr p) {
        auto expected = status::empty;
        const auto desired = status::transition;
        if (!state.compare_exchange_strong(expected, desired,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        exception = std::move(p);
        state.store(status::exception, std::memory_order_release);
        waiters.resume_all();
    }

    void break_promise() noexcept {
        auto expected = status::empty;
        const auto desired = status::broken;
        if (state.compare_exchange_strong(expected, desired,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
            waiters.resume_all();
        }
    }

    void wait() {
        waiters.wait_if([this] { return !ready(); });
        assert(ready());
    }
};

template <typename T>
struct shared_state : public shared_state_base {

    std::optional<T> value;

    template <typename R>
    void set_value(R&& v) {
        auto expected = status::empty;
        const auto desired = status::transition;
        if (!state.compare_exchange_strong(expected, desired,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        try {
            value.emplace(std::forward<R>(v));
            state.store(status::value, std::memory_order_release);
        } catch (...) {
            exception = std::current_exception();
            state.store(status::exception, std::memory_order_release);
        }
        waiters.resume_all();
    }
};

template <>
struct shared_state<void> : public shared_state_base {
    void set_value() {

        auto expected = status::empty;
        const auto desired = status::value;
        if (!state.compare_exchange_strong(expected, desired,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        waiters.resume_all();
    }
};

template <typename T>
struct shared_state<T&> : public shared_state_base {
    T* value = nullptr;

    void set_value(T& v) {
        auto expected = status::empty;
        const auto desired = status::transition;
        if (!state.compare_exchange_strong(expected, desired,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
            throw future_error(std::future_errc::promise_already_satisfied);
        }
        try {
            value = std::addressof(v);
            state.store(status::value, std::memory_order_release);
        } catch (...) {
            exception = std::current_exception();
            state.store(status::exception, std::memory_order_release);
        }
        waiters.resume_all();
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
    COOPSYNC_TBB_NODISCARD std::shared_ptr<shared_state<T>>
    release_state() noexcept {
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

template <typename>
class packaged_task;

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
    /// If the shared state is ready this function returns immediately.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    void wait() const { detail::future::future_base<T>::wait(); }

    /// @brief Retrieves the value stored in the shared state.
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready.
    /// After retrieval the future is left in not valid state.
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
    template <typename>
    friend class packaged_task;
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
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready. After
    /// this call the future is left in not valid state.
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
    template <typename>
    friend class packaged_task;
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
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready.
    /// After retrieval the future is left in not valid state.
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
    template <typename>
    friend class packaged_task;
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

    /// @brief Obtains a reference to the value stored in the shared state.
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready.
    /// This function can be called multiple times without invalidating the
    /// shared state.
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
    /// @brief Constructs the shared_future in a not valid state.
    shared_future() noexcept = default;

    /// @brief Copy-constructs the shared_future, after copying both objects
    /// refer to the same shared state.
    /// @param other The shared_future to copy from.
    shared_future(const shared_future& other) noexcept = default;

    /// @brief Copy-assigns the shared_future, after assignment both objects
    /// refer to the same shared state.
    /// @param other The shared_future to copy from.
    shared_future& operator=(const shared_future& other) noexcept = default;

    /// @brief Move-constructs the shared_future, after moving the current
    /// object refers to the shared state of the moved-from object and the
    /// moved-from object is left in not valid state.
    /// @param other The shared_future to move from.
    shared_future(shared_future&& other) noexcept = default;

    /// @brief Move-assigns the shared_future, after moving the current object
    /// refers to the shared state of the moved-from object and the moved-from
    /// object is left in not valid state.
    /// @param other The shared_future to move from.
    shared_future& operator=(shared_future&& other) noexcept = default;

    /// @brief Destroys the shared_future. If it was the last shared_future
    /// referring to the shared state, the shared state is released.
    ~shared_future() = default;

    /// @brief Tests whether the shared_future is valid and refers to a shared
    /// state.
    /// @return true if the shared_future is valid, false otherwise.
    bool valid() const noexcept {
        return detail::future::future_base<void>::valid();
    }

    /// @brief Suspends the current task until the shared state is ready.
    /// If the shared state is ready this function returns immediately.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    void wait() const { detail::future::future_base<void>::wait(); }

    /// @brief Waits for readiness and checks for exception/broken promise.
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready. This
    /// function can be called multiple times without invalidating the shared
    /// state.
    /// @throws future_error if the future is not valid or if the promise was
    /// broken.
    /// @throws Any exception stored in the shared state.
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
    /// @brief Constructs the shared_future in a not valid state.
    shared_future() noexcept = default;

    /// @brief Copy-constructs the shared_future, after copying both objects
    /// refer to the same shared state.
    /// @param other The shared_future to copy from.
    shared_future(const shared_future& other) noexcept = default;

    /// @brief Copy-assigns the shared_future, after assignment both objects
    /// refer to the same shared state.
    /// @param other The shared_future to copy from.
    shared_future& operator=(const shared_future& other) noexcept = default;

    /// @brief Move-constructs the shared_future, after moving the current
    /// object refers to the shared state of the moved-from object and the
    /// moved-from object is left in not valid state.
    /// @param other The shared_future to move from.
    shared_future(shared_future&& other) noexcept = default;

    /// @brief Move-assigns the shared_future, after moving the current object
    /// refers to the shared state of the moved-from object and the moved-from
    /// object is left in not valid state.
    /// @param other The shared_future to move from.
    shared_future& operator=(shared_future&& other) noexcept = default;

    /// @brief Destroys the shared_future. If it was the last shared_future
    /// referring to the shared state, the shared state is released.
    ~shared_future() = default;

    /// @brief Tests whether the shared_future is valid and refers to a shared
    /// state.
    /// @return true if the shared_future is valid, false otherwise.
    bool valid() const noexcept {
        return detail::future::future_base<T&>::valid();
    }

    /// @brief Suspends the current task until the shared state is ready.
    /// If the shared state is ready this function returns immediately.
    /// @throws future_error with \c std::future_errc::no_state if the future is
    /// not valid.
    void wait() const { detail::future::future_base<T&>::wait(); }

    /// @brief Obtains a reference stored in the shared state.
    /// If the shared state is ready, this function returns immediately.
    /// Otherwise, the task is suspended until the state becomes ready.
    /// This function can be called multiple times without invalidating the
    /// shared state.
    /// @return A reference stored in the shared state.
    /// @throws future_error if the future is not valid or if the promise was
    /// broken.
    /// @throws Any exception stored in the shared state.
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

template <typename>
class packaged_task;

template <typename R, typename... Args>
class packaged_task<R(Args...)> {
    public:
    packaged_task() = default;
    template <typename F>
    explicit packaged_task(F f)
        : m_function(std::move(f)),
          m_state(m_function
                      ? std::make_shared<detail::future::shared_state<R>>()
                      : nullptr) {}

    /// @brief The packaged_task is not copy-constructible.
    packaged_task(const packaged_task&) = delete;

    /// @brief The packaged_task is not copy-assignable.
    packaged_task& operator=(const packaged_task&) = delete;

    /// @brief The packaged_task is move-constructible.
    /// @param other The packaged_task to move from, after construction has no
    /// shared state and is not valid.
    packaged_task(packaged_task&& other) noexcept
        : m_function(std::move(other.m_function)),
          m_state(std::move(other.m_state)) {
        other.m_function = {};
        other.m_state.reset();
    }

    /// @brief The packaged_task is move-assignable.
    /// @param other The packaged_task to move from, after assignment has no
    /// shared state and is not valid.
    packaged_task& operator=(packaged_task&& other) noexcept {
        if (this != &other) {
            // Ensure any previously held shared state is released and marked as
            // broken if it wasn't made ready.
            if (m_state) {
                m_state->break_promise();
            }
            m_state.reset();

            m_function = std::move(other.m_function);
            m_state = std::move(other.m_state);

            other.m_function = {};
            other.m_state.reset();
        }
        return *this;
    }

    /// @brief Destroys the packaged_task. If it has a shared state that is not
    /// ready, the shared state is marked as broken.
    ~packaged_task() {
        if (m_state) {
            m_state->break_promise();
        }
    }

    /// @brief Tests whether the packaged_task has a valid function and an
    /// associated shared state.
    /// @return true if the packaged_task is valid, false otherwise.
    bool valid() const noexcept {
        return static_cast<bool>(m_function) && static_cast<bool>(m_state);
    }

    /// @brief Exchanges the shared states with another packaged_task.
    /// @param other The packaged_task to exchange state with.
    void swap(packaged_task& other) noexcept {
        using std::swap;
        swap(m_function, other.m_function);
        swap(m_state, other.m_state);
    }

    future<R> get_future() {
        if (!valid()) {
            throw future_error(std::future_errc::no_state);
        }

        bool expected = false;
        if (!m_state->future_obtained.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            throw future_error(std::future_errc::future_already_retrieved);
        }
        return future<R>(m_state);
    }

    void operator()(Args... args) {
        if (!valid()) {
            throw future_error(std::future_errc::no_state);
        }

        try {
            if constexpr (std::is_void_v<R>) {
                std::invoke(m_function, std::forward<Args>(args)...);
                m_state->set_value();
            } else if constexpr (std::is_reference_v<R>) {
                auto& r = std::invoke(m_function, std::forward<Args>(args)...);
                m_state->set_value(r);
            } else {
                m_state->set_value(
                    std::invoke(m_function, std::forward<Args>(args)...));
            }
        } catch (...) {
            m_state->set_exception(std::current_exception());
        }
    }

    /// @brief Resets the packaged_task to a valid state with an empty shared
    /// state. After this call, the packaged_task can be invoked again to set a
    /// new value in the shared state.
    void reset() {
        if (!valid()) {
            throw future_error(std::future_errc::no_state);
        }

        auto new_state = std::make_shared<detail::future::shared_state<R>>();

        // Mark the old shared state as broken if it wasn't made ready yet.
        m_state->break_promise();
        m_state = std::move(new_state);
    }

    private:
    std::function<R(Args...)> m_function;
    std::shared_ptr<detail::future::shared_state<R>> m_state;
};

/// @brief Exchanges the states between two packaged_tasks.
/// @param lhs The first packaged_task to exchange state with.
/// @param rhs The second packaged_task to exchange state with.
template <typename R, typename... Args>
void swap(packaged_task<R(Args...)>& lhs,
          packaged_task<R(Args...)>& rhs) noexcept {
    lhs.swap(rhs);
}

}  // namespace coopsync_tbb

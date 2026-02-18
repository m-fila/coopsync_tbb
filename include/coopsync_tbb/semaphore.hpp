#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

namespace detail {

/// @brief A type trait that deduces the smallest signed integer type that can
/// hold a given positive value N. The type is defined as
/// int_max_value_t<N>::type.
/// @tparam N The value for which to deduce the integer type. Must be positive.
template <std::ptrdiff_t N>
class int_max_value_t {
    static_assert(N > 0, "N must be positive");

    private:
    static const bool fits8 =
        N <= static_cast<std::ptrdiff_t>(
                 std::numeric_limits<std::int_least8_t>::max());

    static const bool fits16 =
        N <= static_cast<std::ptrdiff_t>(
                 std::numeric_limits<std::int_least16_t>::max());

    static const bool fits32 =
        N <= static_cast<std::ptrdiff_t>(
                 std::numeric_limits<std::int_least32_t>::max());

    static const bool fits64 =
        N <= static_cast<std::ptrdiff_t>(
                 std::numeric_limits<std::int_least64_t>::max());

    public:
    // clang-format off
    using type =
    typename std::conditional<
        fits8,
        std::int_least8_t,
    typename std::conditional<
        fits16,
        std::int_least16_t,
    typename std::conditional<
        fits32,
        std::int_least32_t,
    typename std::conditional<
        fits64,
        std::int_least64_t,
        std::ptrdiff_t
    >::type>::type>::type>::type;
    // clang-format on
};
}  // namespace detail

/// @brief A counting semaphore that allows tasks to synchronize based on a
/// counter. The semaphore is initialized with a non-negative count, and tasks
/// can acquire the semaphore by decrementing the count or release the semaphore
/// by incrementing the count. If a task tries to acquire the semaphore when the
/// count is zero, it is suspended until the count becomes greater than zero.
/// Concurrent invocations of the member functions. except for destructor, are
/// safe.
// @tparam LeastMaxValue The least maximum value for the counter of the
// counting_semaphore. Must be non-negative and less than or equal to
// max().
template <std::ptrdiff_t LeastMaxValue =
              std::numeric_limits<std::ptrdiff_t>::max()>
class counting_semaphore {
    public:
    static_assert(LeastMaxValue >= 0, "LeastMaxValue must be non-negative");
    static_assert(LeastMaxValue <= std::numeric_limits<std::ptrdiff_t>::max(),
                  "LeastMaxValue must be less than or equal to "
                  "std::numeric_limits<std::ptrdiff_t>::max()");

    /// @brief Constructs a counting_semaphore with the specified initial count.
    /// @parm initial_count The initial count for the semaphore. Must be
    /// non-negative and less than or equal to max().
    explicit counting_semaphore(std::ptrdiff_t desired);

    /// @brief counting_semaphore is not copy-constructible.
    counting_semaphore(const counting_semaphore&) = delete;

    /// @brief counting_semaphore is not copy-assignable.
    counting_semaphore& operator=(const counting_semaphore&) = delete;

    /// @brief counting_semaphore is not move-constructible.
    counting_semaphore(counting_semaphore&&) = delete;

    /// @brief counting_semaphore is not move-assignable.
    counting_semaphore& operator=(counting_semaphore&&) = delete;

    /// @brief Destroys the counting_semaphore.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the counting_semaphore. The destructor does not notify or
    /// resume any waiting tasks.
    ~counting_semaphore() = default;

    /// @brief Releases the semaphore, incrementing the counter by the specified
    /// update. If there are tasks suspended on the semaphore and the counter
    /// becomes greater than zero, at least one of them is resumed.
    /// @param update The value to increment the counter by. Must be
    /// non-negative and less than or equal to max() - current counter value.
    void release(std::ptrdiff_t update = 1);

    /// @brief Acquires the semaphore, decrementing the counter by 1. If the
    /// counter is zero, the calling task is suspended until the counter becomes
    /// greater than zero.
    /// @note The suspended task must remain valid until it is resumed.
    /// @note The suspended task must be resumed before the counting_semaphore
    /// is destroyed.
    void acquire();

    /// @brief Attempts to acquire the semaphore without suspending.
    /// @return true if the semaphore was successfully acquired, false
    /// otherwise.
    COOPSYNC_TBB_NODISCARD bool try_acquire();

    /// @brief Returns the maximum value for the counter of the
    /// counting_semaphore.
    /// @return The maximum value for the counting_semaphore, bigger than or
    /// equal to the @ref LeastMaxValue.
    constexpr static std::ptrdiff_t max() noexcept;

    private:
    using storage_t = typename detail::int_max_value_t<LeastMaxValue>::type;

    std::atomic<storage_t> m_counter;
    detail::wait_queue m_waiters;
};

/// @brief A specialization of a counting_semaphore handling only a single
/// permit. Same as @c binary_semaphore.
///
template <>
class counting_semaphore<1> {
    public:
    /// @brief Constructs a counting_semaphore with the specified initial count.
    /// @param desired The initial count. Must be 0 or 1.
    explicit counting_semaphore(std::ptrdiff_t desired);

    /// @brief counting_semaphore is not copy-constructible.
    counting_semaphore(const counting_semaphore&) = delete;

    /// @brief counting_semaphore is not copy-assignable.
    counting_semaphore& operator=(const counting_semaphore&) = delete;

    /// @brief counting_semaphore is not move-constructible.
    counting_semaphore(counting_semaphore&&) = delete;

    /// @brief counting_semaphore is not move-assignable.
    counting_semaphore& operator=(counting_semaphore&&) = delete;

    /// @brief Destroys the counting_semaphore.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the counting_semaphore. The destructor does not notify or
    /// resume any waiting tasks.
    ~counting_semaphore() = default;

    /// @brief Releases the semaphore incrementing the counter by the specified
    /// update. If there are tasks suspended on the semaphore and the counter
    /// becomes greater than zero, one of them is resumed.
    /// @param update The value to increment the counter by. Must be
    /// non-negative,  all positive values are treated as 1.
    void release(std::ptrdiff_t update = 1);

    /// @brief Acquires the semaphore. If the semaphore is already acquired the
    /// calling task is suspended until the semaphore is released.
    /// @note The suspended task must remain valid until it is resumed.
    /// @note The suspended task must be resumed before the binary_semaphore
    /// is destroyed.
    void acquire();

    /// @brief Attempts to acquire the semaphore without suspending.
    /// @return true if the semaphore was successfully acquired, false
    /// otherwise.
    COOPSYNC_TBB_NODISCARD bool try_acquire();

    /// @brief Returns the maximum value for the counter of the binary
    /// semaphore.
    /// @return Always returns 1.
    constexpr static std::ptrdiff_t max() noexcept;

    private:
    using waiter_t = tbb::task::suspend_point;

    std::atomic<bool> m_available;
    tbb::spin_mutex m_waiters_mutex;
    detail::intrusive_list<waiter_t> m_waiters;
};

/// @brief An alias for @c counting_semaphore<1>.
using binary_semaphore = counting_semaphore<1>;

template <std::ptrdiff_t LeastMaxValue>
inline counting_semaphore<LeastMaxValue>::counting_semaphore(
    std::ptrdiff_t desired)
    : m_counter(desired) {
    assert(desired >= 0);
    assert(desired <= max());
}

template <std::ptrdiff_t LeastMaxValue>
inline constexpr std::ptrdiff_t
counting_semaphore<LeastMaxValue>::max() noexcept {
    return static_cast<std::ptrdiff_t>(std::numeric_limits<storage_t>::max());
}

template <std::ptrdiff_t LeastMaxValue>
inline bool counting_semaphore<LeastMaxValue>::try_acquire() {
    auto current = m_counter.load(std::memory_order_acquire);
    while (current > 0) {
        if (m_counter.compare_exchange_weak(current, current - 1,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

template <std::ptrdiff_t LeastMaxValue>
inline void counting_semaphore<LeastMaxValue>::acquire() {
    while (!try_acquire()) {
        m_waiters.wait_if(
            [this] { return m_counter.load(std::memory_order_acquire) == 0; });
    }
}

template <std::ptrdiff_t LeastMaxValue>
inline void counting_semaphore<LeastMaxValue>::release(std::ptrdiff_t update) {
    assert(update >= 0);
    if (update == 0) {
        return;
    }

    const auto prev = m_counter.fetch_add(static_cast<storage_t>(update),
                                          std::memory_order_release);
    assert(update <= max() - static_cast<std::ptrdiff_t>(prev));

    // Wake up to `update` waiters; any unused permits stay in m_counter.
    m_waiters.resume_n(update);
}

inline counting_semaphore<1>::counting_semaphore(std::ptrdiff_t desired)
    : m_available(desired) {
    assert(desired == 0 || desired == 1);
}

inline constexpr std::ptrdiff_t counting_semaphore<1>::max() noexcept {
    return 1;
}

inline bool counting_semaphore<1>::try_acquire() {
    auto expected = true;
    return m_available.compare_exchange_strong(
        expected, false, std::memory_order_acquire, std::memory_order_relaxed);
}

inline void counting_semaphore<1>::acquire() {
    // Fast path
    if (try_acquire()) {
        return;
    }

    // Slow path
    auto node = detail::intrusive_list<waiter_t>::node{};
    m_waiters_mutex.lock();

    // Re-check under lock to avoid race with release().
    if (try_acquire()) {
        m_waiters_mutex.unlock();
        return;
    }

    // Guaranteed that the suspend lambda will be executed on the same thread
    // so capturing locked mutex is fine.
    tbb::task::suspend([this, &node](tbb::task::suspend_point sp) {
        node.value = sp;
        m_waiters.push_back(node);
        m_waiters_mutex.unlock();
    });

    // Post resumption, release() has handed a permit to us.
}

inline void counting_semaphore<1>::release(std::ptrdiff_t update) {
    assert(update >= 0);

    // No permits returned
    if (update == 0) {
        return;
    }

    assert(m_available.load(std::memory_order_acquire) == false);

    auto lock = tbb::spin_mutex::scoped_lock(m_waiters_mutex);

    // Direct handoff: resume a waiter if there is any.
    if (const auto* waiter = m_waiters.pop_front()) {
        m_available.store(false, std::memory_order_relaxed);
        tbb::task::resume(waiter->value);
        return;
    }

    // No waiters: store a single permit.
    m_available.exchange(true, std::memory_order_release);
}

}  // namespace coopsync_tbb

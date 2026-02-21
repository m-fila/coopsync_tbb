#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>

#include "coopsync_tbb/detail/macros.hpp"
#include "coopsync_tbb/detail/wait_queue.hpp"

namespace coopsync_tbb {

namespace detail::barrier {
/// @brief An empty completion function that does nothing. Used as the default
/// completion function for the barrier.
inline void completion() noexcept {}

using default_completion_t = void (*)() noexcept;
}  // namespace detail::barrier

/// @brief A barrier is a reusable synchronization primitive that allows tasks
/// to suspend until a certain number of arrivals have occurred. The barrier is
/// reusable, meaning that once the expected number of arrivals has been reached
/// and the completion function has been called, the barrier can be used again
/// for the next phase. Concurrent invocations of the member functions. except
/// for destructor, are safe.
template <typename CompletionFunction = detail::barrier::default_completion_t>
class barrier {
    public:
    class COOPSYNC_TBB_NODISCARD arrival_token;

    /// @brief Constructs a barrier with the specified initial expected count
    /// and completion function.
    /// @param expected The initial expected count for the barrier. Must be
    /// non-negative and not greater than max().
    /// @param completion The completion function to be called when a phase
    /// completes.

    explicit barrier(std::ptrdiff_t expected, CompletionFunction completion =
                                                  detail::barrier::completion);

    /// @brief Barrier is not copy-constructible.
    barrier(const barrier&) = delete;

    /// @brief Barrier is not copy-assignable.
    barrier& operator=(const barrier&) = delete;

    /// @brief Barrier is not move-constructible.
    barrier(barrier&&) = delete;

    /// @brief Barrier is not move-assignable.
    barrier& operator=(barrier&&) = delete;

    /// @brief Destroys the mutex.
    /// @note The destructor must not be called while there are still tasks
    /// waiting on the barrier. The destructor doesn not notify or resume any
    /// waiting tasks.
    ~barrier() = default;

    /// @brief Returns the maximum value for the barrier counter.
    /// @return The maximum value for the barrier counter.
    static constexpr std::ptrdiff_t max() noexcept;

    /// @brief Arrives at the barrier with the specified update. Constructs an
    /// arrival token associated with the current phase and decrement the
    /// expected count by the update. If the expected count reaches zero, the
    /// completion function is called and all waiting tasks are resumed.
    /// @param update The value to decrement the expected count by. Must be
    /// non-negative and less than or equal to the current expected count.
    /// @return An arrival token associated with the current phase.
    COOPSYNC_TBB_NODISCARD arrival_token arrive(std::ptrdiff_t update = 1);

    /// @brief Waits for the completion of the phase associated with the given
    /// arrival token. If the token is associated with the current phase, the
    /// task is suspended until the phase is complete. If the token is associate
    /// with the immediately previous phase, the function returns immediately.
    /// Token associated with any other phase must not be used.
    /// @param arrival The arrival token associated with the current phase or
    /// the immediately previous phase.
    void wait(arrival_token arrival);

    /// @brief Decrements the expected counter by 1 and waits
    /// for the completion of the current phase. If the expected counter for the
    /// current phase reaches zero, the completion function is called and all
    /// waiting tasks are resumed. Equivalent to wait(arrive()).
    void arrive_and_wait();

    /// @brief Decrements the initial expected count for the following phases by
    /// 1 and decrements the expect count for the current phase by 1. If the
    /// expected count for the current phase reaches zero, the completion
    /// function is called and all waiting tasks are resumed.
    void arrive_and_drop();

    private:
    using token_base_t = unsigned char;
    CompletionFunction m_completion;
    std::atomic<std::ptrdiff_t> m_initial_expected;
    std::atomic<std::ptrdiff_t> m_expected;
    std::atomic<token_base_t> m_phase;
    detail::wait_queue m_waiters;
};

/// @brief An opaque value representing task arrival at the barrier. It is used
/// to wait for the completion of a phase.
template <typename CompletionFunction>
class barrier<CompletionFunction>::arrival_token {
    private:
    friend class barrier;
    explicit arrival_token(token_base_t phase) noexcept : m_phase(phase) {}
    token_base_t m_phase;
};

template <typename CompletionFunction>
inline barrier<CompletionFunction>::barrier(std::ptrdiff_t expected,
                                            CompletionFunction completion)
    : m_completion(std::move(completion)),
      m_initial_expected(expected),
      m_expected(expected),
      m_phase(0) {
    assert(expected >= 0);
    assert(expected <= max());
}

template <typename CompletionFunction>
inline constexpr std::ptrdiff_t barrier<CompletionFunction>::max() noexcept {
    return std::numeric_limits<std::ptrdiff_t>::max();
}

template <typename CompletionFunction>
inline typename barrier<CompletionFunction>::arrival_token
barrier<CompletionFunction>::arrive(std::ptrdiff_t update) {
    assert(update >= 0);

    const auto phase = m_phase.load(std::memory_order_relaxed);
    const auto expected =
        m_expected.fetch_sub(update, std::memory_order_acq_rel);
    assert(expected >= 0);

    // Last arrival for this phase.
    if (expected == update) {

        m_completion();

        const auto next_expected =
            m_initial_expected.load(std::memory_order_acquire);
        m_expected.store(next_expected, std::memory_order_release);

        const auto next_phase = static_cast<token_base_t>(phase + 1);
        m_phase.store(next_phase, std::memory_order_release);

        m_waiters.resume_all();
    }

    return arrival_token(phase);
}

template <typename CompletionFunction>
inline void barrier<CompletionFunction>::wait(arrival_token arrival) {
    // Fast path
    {
        const auto current_phase = m_phase.load(std::memory_order_acquire);
        const auto previous_phase = current_phase - 1;
        if (arrival.m_phase == previous_phase) {
            return;
        }
        assert(arrival.m_phase == current_phase);
    }

    const auto arrival_phase = arrival.m_phase;
    m_waiters.wait_if([this, arrival_phase]() {
        return m_phase.load(std::memory_order_acquire) == arrival_phase;
    });
}

template <typename CompletionFunction>
inline void barrier<CompletionFunction>::arrive_and_wait() {
    wait(arrive());
}

template <typename CompletionFunction>
inline void barrier<CompletionFunction>::arrive_and_drop() {
    const auto prev =
        m_initial_expected.fetch_sub(1, std::memory_order_acq_rel);
    assert(prev > 0);
    std::ignore = arrive(1);
}

}  // namespace coopsync_tbb

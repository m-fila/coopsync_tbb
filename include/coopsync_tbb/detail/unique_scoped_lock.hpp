#pragma once

#include <system_error>
namespace coopsync_tbb::detail {

template <typename Mutex>
/// @brief A unique_scoped_lock is a RAII wrapper for mutex that acquires the
/// mutex on construction and releases it on destruction.
/// @tparam Mutex The type of the mutex to be locked. Must meet the *Lockable*
/// requirements.
class unique_scoped_lock {
    public:
    /// @brief Constructs a unique_scoped_lock without acquiring a mutex.
    unique_scoped_lock();

    /// @brief Constructs a unique_scoped_lock and acquires the given mutex.
    /// @param m The mutex to acquire.
    explicit unique_scoped_lock(Mutex& m);

    /// @brief Destroys the unique_scoped_lock and releases the mutex.
    ~unique_scoped_lock();

    /// @brief unique_scoped_lock is not copy-constructible.
    unique_scoped_lock(const unique_scoped_lock&) = delete;

    /// @brief unique_scoped_lock is not copy-assignable.
    unique_scoped_lock& operator=(const unique_scoped_lock&) = delete;

    /// @brief unique_scoped_lock is not move-constructible.
    unique_scoped_lock(unique_scoped_lock&&) = delete;

    /// @brief unique_scoped_lock is not move-assignable.
    unique_scoped_lock& operator=(unique_scoped_lock&&) = delete;
    /// @brief Acquires the mutex.
    /// @param m Mutex to acquire.
    /// @throws std::system_error if another mutex is already acquired.
    void acquire(Mutex& m);

    /// @brief Attempts to acquire the mutex without blocking.
    /// @param m Mutex to acquire.
    /// @return true if the mutex was successfully acquired, false otherwise.
    /// @throws std::system_error if another mutex is already acquired.
    bool try_acquire(Mutex& m);

    /// @brief Releases the mutex. Does nothing if no mutex was previously
    /// acquired.
    void release();

    private:
    Mutex* m_mutex = nullptr;
};

template <typename Mutex>
inline unique_scoped_lock<Mutex>::unique_scoped_lock(Mutex& m) : m_mutex(&m) {
    m_mutex->lock();
}

template <typename Mutex>
inline unique_scoped_lock<Mutex>::unique_scoped_lock() : m_mutex(nullptr) {}

template <typename Mutex>
inline void unique_scoped_lock<Mutex>::acquire(Mutex& m) {
    if (m_mutex != nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }
    m_mutex = &m;
    m.lock();
}

template <typename Mutex>
inline unique_scoped_lock<Mutex>::~unique_scoped_lock() {
    release();
}

template <typename Mutex>
inline bool unique_scoped_lock<Mutex>::try_acquire(Mutex& m) {
    if (m_mutex != nullptr) {
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_permitted));
    }
    auto success = m.try_lock();
    if (success) {
        m_mutex = &m;
    }
    return success;
}

template <typename Mutex>
inline void unique_scoped_lock<Mutex>::release() {
    if (m_mutex == nullptr) {
        return;
    }
    m_mutex->unlock();
    m_mutex = nullptr;
}

}  // namespace coopsync_tbb::detail

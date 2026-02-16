#pragma once

namespace coopsync_tbb {

template <typename Mutex>
/// @brief A scoped_lock is a RAII wrapper for mutex that acquires the mutex on
/// construction and releases it on destruction.
/// @tparam Mutex The type of the mutex to be locked. Must provide lock() and
/// unlock() member functions.
class scoped_lock {
    public:
    /// @brief Constructs a scoped_lock and acquires the given mutex.
    /// @param m The mutex to acquire.
    explicit scoped_lock(Mutex& m);
    /// @brief Destroys the scoped_lock and releases the mutex.
    ~scoped_lock();

    /// @brief scoped_lock is not copy-constructible.
    scoped_lock(const scoped_lock&) = delete;
    /// @brief scoped_lock is not copy-assignable.
    scoped_lock& operator=(const scoped_lock&) = delete;
    /// @brief scoped_lock is not move-constructible.
    scoped_lock(scoped_lock&&) = delete;
    /// @brief scoped_lock is not move-assignable.
    scoped_lock& operator=(scoped_lock&&) = delete;

    private:
    Mutex& m_mutex;
};

template <typename Mutex>
inline scoped_lock<Mutex>::scoped_lock(Mutex& m) : m_mutex(m) {
    m_mutex.lock();
}

template <typename Mutex>
inline scoped_lock<Mutex>::~scoped_lock() {
    m_mutex.unlock();
}
}  // namespace coopsync_tbb

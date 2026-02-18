#pragma once

#include <type_traits>

// A few type traits checking presence of static members as in TBB.
// No constrains and concepts for compatibility with pre C++20.

namespace coopsync_tbb::traits {

// void_t for C++11
template <typename...>
using void_t = void;

// enable_if_t for C++11
template <bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

// is_rw_mutex

template <typename T, typename = void>
struct has_is_rw_mutex : std::false_type {};

template <typename T>
struct has_is_rw_mutex<
    T,
    enable_if_t<std::is_same<
        typename std::remove_cv<decltype(T::is_rw_mutex)>::type, bool>::value>>
    : std::true_type {};

template <typename T>
constexpr bool has_is_rw_mutex_v = has_is_rw_mutex<T>::value;

// has static bool is_fair_mutex

template <typename T, typename = void>
struct has_is_fair_mutex : std::false_type {};

template <typename T>
struct has_is_fair_mutex<
    T, enable_if_t<std::is_same<
           typename std::remove_cv<decltype(T::is_fair_mutex)>::type,
           bool>::value>> : std::true_type {};

template <typename T>
constexpr bool has_is_fair_mutex_v = has_is_fair_mutex<T>::value;

// has static bool  is_recursive_mutex

template <typename T, typename = void>
struct has_is_recursive_mutex : std::false_type {};

template <typename T>
struct has_is_recursive_mutex<
    T, enable_if_t<std::is_same<
           typename std::remove_cv<decltype(T::is_recursive_mutex)>::type,
           bool>::value>> : std::true_type {};

template <typename T>
constexpr bool has_is_recursive_mutex_v = has_is_recursive_mutex<T>::value;

// has typedef scoped_lock

template <typename T, typename = void>
struct has_scoped_lock : std::false_type {};

template <typename T>
struct has_scoped_lock<T, void_t<typename T::scoped_lock>> : std::true_type {};

template <typename T>
inline constexpr bool has_scoped_lock_v = has_scoped_lock<T>::value;

// has void release()

template <typename T, typename = void>
struct has_release : std::false_type {};

template <typename T>
struct has_release<
    T, void_t<enable_if_t<
           std::is_same<decltype(std::declval<T&>().release()), void>::value>>>
    : std::true_type {};

template <typename T>
constexpr bool has_release_v = has_release<T>::value;

// has void acquire(M&)

template <typename T, typename M, typename = void>
struct has_acquire : std::false_type {};

template <typename T, typename M>
struct has_acquire<T, M,
                   void_t<enable_if_t<std::is_same<
                       decltype(std::declval<T&>().acquire(std::declval<M&>())),
                       void>::value>>> : std::true_type {};

template <typename T, typename M>
constexpr bool has_acquire_v = has_acquire<T, M>::value;

// has bool try_acquire(M&)

template <typename T, typename M, typename = void>
struct has_try_acquire : std::false_type {};

template <typename T, typename M>
struct has_try_acquire<
    T, M,
    void_t<enable_if_t<std::is_same<decltype(std::declval<T&>().try_acquire(
                                        std::declval<M&>())),
                                    bool>::value>>> : std::true_type {};

template <typename T, typename M>
constexpr bool has_try_acquire_v = has_try_acquire<T, M>::value;

// has void lock()

template <typename T, typename = void>
struct has_lock : std::false_type {};

template <typename T>
struct has_lock<T, void_t<enable_if_t<std::is_same<
                       decltype(std::declval<T&>().lock()), void>::value>>>
    : std::true_type {};

template <typename T>
constexpr bool has_lock_v = has_lock<T>::value;

// has bool try_lock()

template <typename T, typename = void>
struct has_try_lock : std::false_type {};

template <typename T>
struct has_try_lock<
    T, void_t<enable_if_t<
           std::is_same<decltype(std::declval<T&>().try_lock()), bool>::value>>>
    : std::true_type {};

template <typename T>
constexpr bool has_try_lock_v = has_try_lock<T>::value;

// has void unlock()

template <typename T, typename = void>
struct has_unlock : std::false_type {};

template <typename T>
struct has_unlock<T, void_t<enable_if_t<std::is_same<
                         decltype(std::declval<T&>().unlock()), void>::value>>>
    : std::true_type {};

template <typename T>
constexpr bool has_unlock_v = has_unlock<T>::value;

}  // namespace coopsync_tbb::traits

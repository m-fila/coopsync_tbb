#pragma once

#include <type_traits>

// A few type traits checking presence of static members as in TBB.
// No constrains and concepts for compatibility with pre C++20.

namespace coopsync_tbb {

// void_t for C++11
template <typename...>
using void_t = void;

// is_rw_mutex

template <typename T, typename = void>
struct has_is_rw_mutex : std::false_type {};

template <typename T>
struct has_is_rw_mutex<T, void_t<decltype(T::is_rw_mutex)>> : std::true_type {};

template <typename T>
inline constexpr bool has_is_rw_mutex_v = has_is_rw_mutex<T>::value;

// is_fair_mutex

template <typename T, typename = void>
struct has_is_fair_mutex : std::false_type {};

template <typename T>
struct has_is_fair_mutex<T, void_t<decltype(T::is_fair_mutex)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_is_fair_mutex_v = has_is_fair_mutex<T>::value;

// is_recursive_mutex

template <typename T, typename = void>
struct has_is_recursive_mutex : std::false_type {};

template <typename T>
struct has_is_recursive_mutex<T, void_t<decltype(T::is_recursive_mutex)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_is_recursive_mutex_v =
    has_is_recursive_mutex<T>::value;

}  // namespace coopsync_tbb

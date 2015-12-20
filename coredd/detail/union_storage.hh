/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <type_traits> // aligned_storage

#include "coredd/packed.hh"

namespace coredd { namespace detail {

/*------------------------------------------------------------------------------------------------*/

template <typename T, typename... Types>
struct largest_alignment
{
  static constexpr std::size_t tail  = largest_alignment<Types...>::value;
  static constexpr std::size_t head  = std::alignment_of<T>::value;
  static constexpr std::size_t value = tail > head ? tail : head;
};

template <typename T>
struct largest_alignment<T>
{
  static constexpr std::size_t value = std::alignment_of<T>::value;
};

/*------------------------------------------------------------------------------------------------*/

template <typename T, typename... Types>
struct largest_size
{
  static constexpr std::size_t tail  = largest_size<Types...>::value;
  static constexpr std::size_t head  = sizeof(T);
  static constexpr std::size_t value = tail > head ? tail : head;
};

template <typename T>
struct largest_size<T>
{
  static constexpr std::size_t value = sizeof(T);
};

/*------------------------------------------------------------------------------------------------*/

/// @brief A storage large enough for the biggest T in Types.
///
/// When the packed mode is required (at compilation time), we don't care about alignement to save
/// on memory.
template <std::size_t Len, typename... Types>
struct union_storage
{
  static constexpr std::size_t max_size = largest_size<Types...>::value;
#ifdef COREDD_PACKED
  static constexpr std::size_t alignment_value = 1;
#else
  static constexpr std::size_t alignment_value = largest_alignment<Types...>::value;
#endif
  using type = typename std::aligned_storage< (Len > max_size ? Len : max_size)
                                            , alignment_value
                                            >::type;
};

/*------------------------------------------------------------------------------------------------*/

}} // namespace coredd::detail

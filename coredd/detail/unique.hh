/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <cassert>
#include <cstdint>     // uint32_t
#include <functional>  // hash
#include <limits>      // numeric_limits
#include <type_traits> // is_nothrow_constructible
#include <utility>     // forward

#include "coredd/detail/intrusive_member_hook.hh"
#include "coredd//packed.hh"

namespace coredd { namespace detail {

/*------------------------------------------------------------------------------------------------*/

/// @brief A wrapper to associate a reference counter to a unified data.
///
/// This type is meant to be used by ptr, which takes care of incrementing and decrementing
/// the reference counter, as well as the deletion of the held data.
template <typename T>
class
#ifdef __clang__
COREDD_ATTRIBUTE_PACKED
#endif
unique
{
public:

  unique(const unique&) = delete;
  unique& operator=(const unique&) = delete;
  unique(unique&&) = delete;
  unique& operator=(unique&&) = delete;

  template <typename... Args>
  unique(Args&&... args)
  noexcept(std::is_nothrow_constructible<T, Args...>::value)
    : m_hook(), m_ref_count(0), m_data(std::forward<Args>(args)...)
  {}

  /// @brief Get a reference of the unified data.
  const T&
  data()
  const noexcept
  {
    return m_data;
  }

  /// @brief Tell if the unified data is no longer referenced.
  bool
  is_not_referenced()
  const noexcept
  {
    return m_ref_count == 0;
  }

  /// @brief Equality.
  friend
  bool
  operator==(const unique& lhs, const unique& rhs)
  noexcept
  {
    return lhs.m_data == rhs.m_data;
  }

  /// @brief A ptr references that unified data.
  void
  increment_reference_counter()
  noexcept
  {
    assert(m_ref_count < std::numeric_limits<uint32_t>::max());
    ++m_ref_count;
  }

  /// @brief A ptr no longer references that unified data.
  void
  decrement_reference_counter()
  noexcept
  {
    assert(m_ref_count > 0);
    --m_ref_count;
  }

  intrusive_member_hook<unique>&
  hook()
  noexcept
  {
    return m_hook;
  }

private:

  /// @brief Used by mem::hash_table to store some informations.
  intrusive_member_hook<unique> m_hook;

  /// @brief The number of time the encapsulated data is referenced
  ///
  /// Implements a reference-counting garbage collection.
  std::uint32_t m_ref_count;

  /// @brief The garbage collected data.
  /// @note This field must be the last one of this class to enable variable-length data
  ///
  /// The ptr class is responsible for the detection of dereferenced data and for
  /// instructing the unicity table to erase it.
  const T m_data;
};

/*------------------------------------------------------------------------------------------------*/

}} // namespace coredd::detail

namespace std {

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @brief Hash specialization for coredd:unique
template <typename T>
struct hash<coredd::detail::unique<T>>
{
  std::size_t
  operator()(const coredd::detail::unique<T>& x)
  const noexcept(noexcept(hash<T>()(x.data())))
  {
    return hash<T>()(x.data());
  }
};

/*------------------------------------------------------------------------------------------------*/

} // namespace std

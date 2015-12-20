/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <functional> // hash
#include <list>
#include <utility>    // forward

#include "coredd/detail/hash_table.hh"
#include "coredd/detail/lru_list.hh"
#include "coredd/hash.hh"

namespace coredd { namespace detail {

/*------------------------------------------------------------------------------------------------*/

/// @brief Associate an operation to its result into the cache.
///
/// The operation acts as a key and the associated result is the value counterpart.
template <typename Operation, typename Result>
class cache_entry
{
public:

  // Can't copy a cache_entry.
  cache_entry(const cache_entry&) = delete;
  cache_entry& operator=(const cache_entry&) = delete;

  /// @brief Constructor.
  template <typename... Args>
  cache_entry(Operation&& op, Args&&... args)
    : m_hook()
    , m_operation(std::move(op))
    , m_result(std::forward<Args>(args)...)
    , m_lru_cit()
  {}

  intrusive_member_hook<cache_entry>&
  hook()
  noexcept
  {
    return m_hook;
  }

  const Operation&
  operation()
  const noexcept
  {
    return m_operation;
  }

  const Result&
  result()
  const noexcept
  {
    return m_result;
  }

  auto
  lru_cit()
  const noexcept
  {
    return m_lru_cit;
  }

  auto&
  lru_cit()
  noexcept
  {
    return m_lru_cit;
  }

  /// @brief Cache entries are only compared using their operations.
  friend
  bool
  operator==(const cache_entry& lhs, const cache_entry& rhs)
  noexcept
  {
    return lhs.m_operation == rhs.m_operation;
  }

private:

  /// @brief
  intrusive_member_hook<cache_entry> m_hook;

  /// @brief The cached operation.
  const Operation m_operation;

  /// @brief The result of the evaluation of operation.
  const Result m_result;

  /// @brief Where this cache entry is stored in the LRU list.
  typename lru_list<cache_entry>::const_iterator m_lru_cit;
};

/*------------------------------------------------------------------------------------------------*/

}} // namespace coredd::detail

namespace std {

/*------------------------------------------------------------------------------------------------*/

/// @internal
template <typename Operation, typename Result>
struct hash<coredd::detail::cache_entry<Operation, Result>>
{
  std::size_t
  operator()(const coredd::detail::cache_entry<Operation, Result>& x)
  {
    using namespace coredd;
    // A cache entry must have the same hash as its contained operation. Otherwise, cache::erase()
    // and cache::insert_check()/cache::insert_commit() won't use the same position in buckets.
    return seed(x.operation());
  }
};

/*------------------------------------------------------------------------------------------------*/

} // namespace std

/*------------------------------------------------------------------------------------------------*/

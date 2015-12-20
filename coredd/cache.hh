/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <memory> // unique_ptr
#include <tuple>

#include "coredd/detail/apply_filters.hh"
#include "coredd/detail/cache_entry.hh"
#include "coredd/detail/hash_table.hh"
#include "coredd/detail/lru_list.hh"
#include "coredd/detail/pool.hh"
#include "coredd/hash.hh"

namespace coredd {

/*------------------------------------------------------------------------------------------------*/

/// @brief The statistics of a cache.
struct cache_statistics
{
  /// @brief The number of entries.
  std::size_t size;

  /// @brief The number of hits.
  std::size_t hits;

  /// @brief The number of misses.
  std::size_t misses;

  /// @brief The number of filtered entries.
  std::size_t filtered;

  /// @brief The number of entries discarded by the LRU policy.
  std::size_t discarded;

  /// @brief The number of buckets with more than one element in the underlying hash table.
  std::size_t collisions;

  /// @brief The number of buckets with only one element in the underlying hash table.
  std::size_t alone;

  /// @brief The number of empty buckets in the underlying hash table.
  std::size_t empty;

  /// @brief The number of buckets in the underlying hash table.
  std::size_t buckets;

  /// @brief The load factor of the underlying hash table.
  double load_factor;
};

/*------------------------------------------------------------------------------------------------*/

/// @brief  A generic cache.
/// @tparam Operation is the operation type.
/// @tparam Filters is a list of filters that reject some operations.
///
/// It uses the LRU strategy to cleanup old entries.
template <typename Context, typename Operation, typename... Filters>
class cache
{
  // Can't copy a cache.
  cache(const cache&) = delete;
  cache* operator=(const cache&) = delete;

private:

  /// @brief The type of the context of this cache.
  using context_type = Context;

  /// @brief The type of the result of an operation stored in the cache.
  using result_type = std::result_of_t<Operation(context_type&)>;

  /// @brief The of an entry that stores an operation and its result.
  using cache_entry_type = detail::cache_entry<Operation, result_type>;

  /// @brief An intrusive hash table.
  using set_type = detail::hash_table<cache_entry_type, false /* no rehash */>;

public:

  /// @brief Construct a cache.
  /// @param context This cache's context.
  /// @param size How many cache entries are kept, should be greater than the order height.
  ///
  /// When the maximal size is reached, a cleanup is launched: half of the cache is removed,
  /// using a LRU strategy. This cache will never perform a rehash, therefore it allocates
  /// all the memory it needs at its construction.
  cache(context_type& context, std::size_t size)
    : m_cxt(context)
    , m_set(size, max_load_factor)
    , m_lru_list()
    , m_max_size(m_set.bucket_count() * max_load_factor)
    , m_stats()
    , m_pool(m_max_size)
  {}

  /// @brief Destructor.
  ~cache()
  {
    clear();
  }

  /// @brief Cache lookup.
  result_type
  operator()(Operation&& op)
  {
    // Check if the current operation should be cached or not.
    if (not detail::apply_filters<Operation, Filters...>()(op))
    {
      ++m_stats.filtered;
      return op(m_cxt);
    }

    // Lookup for op.
    typename set_type::insert_commit_data commit_data;
    auto insertion = m_set.insert_check( op
                                      , [](auto&& lhs, auto&& rhs){return lhs == rhs.operation();}
                                      , commit_data);

    // Check if op has already been computed.
    if (not insertion.second)
    {
      ++m_stats.hits;
      // Move cache entry to the end of the LRU list.
      m_lru_list.splice(m_lru_list.end(), m_lru_list, insertion.first->lru_cit());
      return insertion.first->result();
    }

    ++m_stats.misses;

    cache_entry_type* entry;
    auto res = op(m_cxt); // evaluation may throw

    // Clean up the cache, if necessary.
    if (m_set.size() == m_max_size)
    {
      auto oldest = m_lru_list.front();
      m_set.erase(oldest);
      oldest->~cache_entry_type();
      m_pool.deallocate(oldest);
      m_lru_list.pop_front();
      ++m_stats.discarded;
    }

    entry = new (m_pool.allocate()) cache_entry_type(std::move(op), std::move(res));

    // Add the new cache entry to the end of the LRU list.
    entry->lru_cit() = m_lru_list.insert(m_lru_list.end(), entry);

    // Finally, set the result associated to op.
    m_set.insert_commit(entry, commit_data); // doesn't throw

    return entry->result();
  }

  /// @brief Remove all entries of the cache.
  void
  clear()
  noexcept
  {
    m_set.clear_and_dispose([&](cache_entry_type* x)
                              {
                                x->~cache_entry_type();
                                m_pool.deallocate(x);
                              });
  }

  /// @brief Get the number of cached operations.
  std::size_t
  size()
  const noexcept
  {
    return m_set.size();
  }

  /// @brief Get the statistics of this cache.
  const cache_statistics&
  statistics()
  const noexcept
  {
    m_stats.size = size();
    std::tie(m_stats.collisions, m_stats.alone, m_stats.empty) = m_set.collisions();
    m_stats.buckets = m_set.bucket_count();
    m_stats.load_factor = m_set.load_factor();
    return m_stats;
  }

private:

  /// @brief This cache's context.
  context_type& m_cxt;

  /// @brief The wanted load factor for the underlying hash table.
  static constexpr double max_load_factor = 0.85;

  /// @brief The actual storage of caches entries.
  set_type m_set;

  /// @brief The the container that sorts cache entries by last access date.
  detail::lru_list<cache_entry_type> m_lru_list;

  /// @brief The maximum size this cache is authorized to grow to.
  std::size_t m_max_size;

  /// @brief The statistics of this cache
  mutable cache_statistics m_stats;

  /// @brief Fixed-size pool allocator for cache entries
  detail::pool<cache_entry_type> m_pool;
};

/*------------------------------------------------------------------------------------------------*/

} // namespace coredd

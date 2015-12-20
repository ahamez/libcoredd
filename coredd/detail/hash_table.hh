/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <algorithm>   // fill
#include <cassert>
#include <functional>  // hash
#include <memory>      // unique_ptr
#include <tuple>
#include <type_traits> // enable_if
#include <utility>     // make_pair, pair

#include "coredd/detail/intrusive_member_hook.hh"
#include "coredd/detail/next_power.hh"
#include "coredd/packed.hh"

namespace coredd { namespace detail {

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @brief An intrusive hash table.
///
/// It's modeled after boost::intrusive. It uses chaining to handle collisions.
template <typename Data, bool Rehash = true>
class hash_table
{
public:

  /// @brief Used by insert_check
  struct insert_commit_data
  {
    Data** bucket;
  };

public:

  hash_table(std::size_t size, double max_load_factor = 0.75)
    : m_nb_buckets(next_power_of_2(size))
    , m_size(0)
    , m_buckets(std::make_unique<Data*[]>(m_nb_buckets))
    , m_max_load_factor(max_load_factor)
    , m_nb_rehash(0)
  {
    std::fill(m_buckets.get(), m_buckets.get() + m_nb_buckets, nullptr);
  }

  template <typename T, typename EqT>
  std::pair<Data*, bool>
  insert_check(const T& x, EqT eq, insert_commit_data& commit_data)
  const noexcept(noexcept(std::hash<T>()(x)))
  {
    static_assert(not Rehash, "Use with fixed-size hash table only");

    const std::size_t pos = std::hash<T>()(x) & (m_nb_buckets - 1);

    Data* current = m_buckets[pos];
    commit_data.bucket = m_buckets.get() + pos;

    while (current != nullptr)
    {
      if (eq(x, *current))
      {
        return {current, false};
      }
      current = current->hook().next;
    }

    return {current, true};
  }

  void
  insert_commit(Data* x, insert_commit_data& commit_data)
  noexcept
  {
    static_assert(not Rehash, "Use with fixed-size hash table only");
    assert(x != nullptr);

    Data* previous = nullptr;
    Data* current = *commit_data.bucket;

    // We append x at the end of the bucket, it seems to be faster than appending it directly
    // in front.
    while (current != nullptr)
    {
      previous = current;
      current = current->hook().next;
    }

    if (previous != nullptr)
    {
      previous->hook().next = x;
    }
    else
    {
      *commit_data.bucket = x;
    }

    ++m_size;
  }

  /// @brief Insert an element.
  std::pair<Data*, bool>
  insert(Data* x)
  {
    auto res = insert_impl(x, m_buckets.get(), m_nb_buckets);
    rehash();
    return res;
  }

  /// @brief Return the number of elements.
  std::size_t
  size()
  const noexcept
  {
    return m_size;
  }

  /// @brief Return the number of buckets.
  std::size_t
  bucket_count()
  const noexcept
  {
    return m_nb_buckets;
  }

  /// @brief Remove an element given its value.
  void
  erase(const Data* x)
  noexcept
  {
    const std::size_t pos = std::hash<Data>()(*x) & (m_nb_buckets - 1);
    Data* previous = nullptr;
    Data* current = m_buckets[pos];
    while (current != nullptr)
    {
      if (*x == *current)
      {
        if (previous == nullptr) // first element in bucket
        {
          m_buckets[pos] = current->hook().next;
        }
        else
        {
          previous->hook().next = current->hook().next;
        }
        --m_size;
        return;
      }
      previous = current;
      current = current->hook().next;
    }
    assert(false && "Data to erase not found");
  }

  /// @brief Clear the whole table.
  template <typename Disposer>
  void
  clear_and_dispose(Disposer disposer)
  {
    for (auto i = 0ul; i < m_nb_buckets; ++i)
    {
      Data* current = m_buckets[i];
      while (current != nullptr)
      {
        const auto to_erase = current;
        current = current->hook().next;
        disposer(to_erase);
      }
      m_buckets[i] = nullptr;
    }
    m_size = 0;
  }

  /// @brief Get the load factor of the internal hash table.
  double
  load_factor()
  const noexcept
  {
    return static_cast<double>(size()) / static_cast<double>(bucket_count());
  }

  /// @brief The number of times this hash table has been rehashed.
  std::size_t
  nb_rehash()
  const noexcept
  {
    return m_nb_rehash;
  }

  /// @brief The number of collisions.
  std::tuple<std::size_t /* collisions */, std::size_t /* alone */, std::size_t /* empty */>
  collisions()
  const noexcept
  {
    std::size_t col = 0;
    std::size_t alone = 0;
    std::size_t empty = 0;
    for (auto i = 0ul; i <m_nb_buckets; ++i)
    {
      std::size_t nb = 0;
      auto current = m_buckets[i];
      while (current != nullptr)
      {
        ++nb;
        current = current->hook().next;
      }
      if      (nb == 0) ++empty;
      else if (nb == 1) ++alone;
      else if (nb > 1)  ++col;
    }
    return std::make_tuple(col, alone, empty);
  }

private:

  void
  rehash()
  {
    if (load_factor() < m_max_load_factor) // no need to rehash
    {
      return;
    }
    ++m_nb_rehash;
    auto new_nb_buckets = m_nb_buckets * 2;
    auto new_buckets = new Data*[new_nb_buckets];
    std::fill(new_buckets, new_buckets + new_nb_buckets, nullptr);
    m_size = 0;
    for (auto i = 0ul; i < m_nb_buckets; ++i)
    {
      Data* data_ptr = m_buckets[i];
      while (data_ptr)
      {
        Data* next = data_ptr->hook().next;
        data_ptr->hook().next = nullptr;
        insert_impl(data_ptr, new_buckets, new_nb_buckets);
        data_ptr = next;
      }
      // else empty bucket
    }
    m_buckets.reset(new_buckets);
    m_nb_buckets = new_nb_buckets;
  }

  /// @brief Insert an element.
  std::pair<Data*, bool>
  insert_impl(Data* x, Data** buckets, std::size_t nb_buckets)
  noexcept(noexcept(std::hash<Data>()(*x)))
  {
    const std::size_t pos = std::hash<Data>()(*x) & (nb_buckets - 1);

    Data* current = buckets[pos];

    while (current != nullptr)
    {
      if (*x == *current)
      {
        return {current, false /* no insertion */};
      }
      current = current->hook().next;
    }

    // Push in front of the list.
    x->hook().next = buckets[pos];
    buckets[pos] = x;

    ++m_size;
    return {x, true /* insertion */};
  }

private:

  /// @brief
  std::size_t m_nb_buckets;

  /// @brief
  std::size_t m_size;

  /// @brief
  std::unique_ptr<Data*[]> m_buckets;

  /// @brief The maximal allowed load factor.
  const double m_max_load_factor;

  /// @brief The number of times this hash table has been rehashed.
  std::size_t m_nb_rehash;
};

/*------------------------------------------------------------------------------------------------*/

}} // namespace coredd::detail

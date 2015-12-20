/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <cassert>
#include <functional>  // hash, function
#include <type_traits> // remove_const

#include "coredd/detail/unique_table.hh"
#include "coredd/detail/variant.hh"
#include "coredd/hash.hh"

namespace coredd {

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @brief Define the type of a deletion handler.
///
/// A deletion handler is called by ptr whenever a data is no longer referenced.
template <typename Unique>
using handler_type = std::function<void(const Unique*)>;

/// @internal
/// @brief Get the deletion handler for a given Unique type.
template <typename Unique>
handler_type<Unique>&
deletion_handler()
{
  static handler_type<Unique> handler = [](const Unique*)
  {assert(false && "Unset handler");};
  return handler;
}

/// @internal
/// @brief Reset the deletion handler for a given Unique type.
template <typename Unique>
void
reset_deletion_handler()
{
  deletion_handler<Unique>() = [](const Unique*)
  {assert(false && "Reset handler");};
}

/// @internal
/// @brief Set the deletion handler for a given Unique type.
template <typename Unique>
void
set_deletion_handler(const handler_type<Unique>& h)
{
  deletion_handler<Unique>() = h;
}

/*------------------------------------------------------------------------------------------------*/

/// @brief A smart pointer to manage unified ressources.
/// @tparam Unique the type of the unified ressource.
///
/// Unified ressources are ref_counted elements constructed with an unique_table.
template <typename Unique>
class ptr {
  // Can't default construct a ptr.
  ptr() = delete;

public:

  /// @brief Constructor with a unified data.
  explicit
  ptr(Unique* p)
  noexcept
    : m_x(p)
  {
    m_x->increment_reference_counter();
  }

  /// @brief Copy constructor.
  ptr(const ptr& other)
  noexcept
    : m_x(other.m_x)
  {
    assert(other.m_x != nullptr);
    m_x->increment_reference_counter();
  }

  /// @brief Copy operator.
  ptr&
  operator=(const ptr& other)
  noexcept
  {
    assert(other.m_x != nullptr); // Don't copy from an already moved ptr.
    if (m_x != nullptr)
    {
      m_x->decrement_reference_counter();
      if (m_x->is_not_referenced())
      {
        deletion_handler<Unique>()(m_x);
      }
    }
    m_x = other.m_x;
    m_x->increment_reference_counter();
    return *this;
  }

  /// @brief Move constructor.
  ptr(ptr&& other)
  noexcept
    : m_x(other.m_x)
  {
    other.m_x = nullptr;
  }

  /// @brief Move operator.
  ptr&
  operator=(ptr&& other)
  noexcept
  {
    if (m_x != nullptr)
    {
      m_x->decrement_reference_counter();
      if (m_x->is_not_referenced())
      {
        deletion_handler<Unique>()(m_x);
      }
    }
    m_x = other.m_x;
    other.m_x = nullptr;
    return *this;
  }

  /// @brief Destructor.
  ~ptr()
  {
    if (m_x != nullptr)
    {
      m_x->decrement_reference_counter();
      if (m_x->is_not_referenced())
      {
        deletion_handler<Unique>()(m_x);
      }
    }
  }

  /// @internal
  /// @brief Get a pointer to the unified data.
  const Unique*
  operator->()
  const noexcept
  {
    return m_x;
  }

  ///
  template <typename T>
  bool
  is()
  const noexcept
  {
    return detail::is<T>(m_x->data());
  }

  ///
  template <typename T>
  const T&
  get()
  const noexcept
  {
    return detail::variant_cast<T>(m_x->data());
  }

  /// @brief Swap.
  friend void
  swap(ptr& lhs, ptr& rhs)
  noexcept
  {
    using std::swap;
    swap(lhs.m_x, rhs.m_x);
  }

  /// @brief Swap.
  friend void
  swap(ptr&& lhs, ptr& rhs)
  noexcept
  {
    using std::swap;
    swap(lhs.m_x, rhs.m_x);
  }

  /// @brief Swap.
  friend void
  swap(ptr& lhs, ptr&& rhs)
  noexcept
  {
    using std::swap;
    swap(lhs.m_x, rhs.m_x);
  }

  friend
  bool
  operator==(const ptr& lhs, const ptr& rhs)
  noexcept
  {
    return lhs.m_x == rhs.m_x;
  }

  friend
  bool
  operator<(const ptr& lhs, const ptr& rhs)
  noexcept
  {
    return lhs.m_x < rhs.m_x;
  }

private:

  /// @brief Pointer to the managed ressource, a unified data.
  Unique* m_x;
};

/*------------------------------------------------------------------------------------------------*/

} // namespace coredd

namespace std {

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @brief Hash specialization for coredd::ptr
template <typename Unique>
struct hash<coredd::ptr<Unique>>
{
  std::size_t
  operator()(const coredd::ptr<Unique>& x)
  const noexcept
  {
    return coredd::seed(x.operator->());
  }
};

/*------------------------------------------------------------------------------------------------*/

} // namespace std

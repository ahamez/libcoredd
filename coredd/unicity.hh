/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <cassert>
#include <memory> // unique_ptr

#include "coredd/detail/unique.hh"
#include "coredd/detail/unique_table.hh"
#include "coredd/detail/variant.hh"
#include "coredd/ptr.hh"

namespace coredd {

/*------------------------------------------------------------------------------------------------*/

template <typename... Ts>
class unicity
{
private:

  using definition_type = detail::variant<Ts...>;
  using unique_type = detail::unique<definition_type>;
  using unique_table_type = detail::unique_table<unique_type>;

public:

  using ptr_type = ptr<unique_type>;

public:

  unicity(std::size_t ut_size)
    : m_ut{std::make_unique<unique_table_type>(ut_size)}
  {
    set_deletion_handler<unique_type>([this](const auto* u){m_ut->erase(u);});
  }

  template <typename T, typename... Args>
  ptr_type
  make(Args&&... args)
  {
    return make_sized<T>(sizeof(T), std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  ptr_type
  make_sized(std::size_t size, Args&&... args)
  {
    assert(size >= sizeof(T));
    auto* addr = m_ut->allocate(size);
    auto* u = new (addr) unique_type{detail::construct<T>{}, std::forward<Args>(args)...};
    return ptr_type{&(*m_ut)(u, size)};
  }

  auto
  unique_table_stats()
  const noexcept
  {
    return m_ut->stats();
  }

private:

  std::unique_ptr<unique_table_type> m_ut;
};

/*------------------------------------------------------------------------------------------------*/

} // namespace coredd

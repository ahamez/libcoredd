/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include "coredd/detail/variant.hh"

namespace coredd {

/*------------------------------------------------------------------------------------------------*/

template <typename Visitor, typename X, typename... Args>
auto
visit(Visitor&& v, const X& x, Args&&... args)
noexcept(noexcept(apply_visitor(std::forward<Visitor>(v), x->data(), std::forward<Args>(args)...)))
{
  return apply_visitor(std::forward<Visitor>(v), x->data(), std::forward<Args>(args)...);
}

/*------------------------------------------------------------------------------------------------*/

template <typename Visitor, typename X, typename Y, typename... Args>
auto
binary_visit(Visitor&& v, const X& x, const Y& y, Args&&... args)
noexcept(noexcept(apply_binary_visitor( std::forward<Visitor>(v), x->data(), y->data()
                                      , std::forward<Args>(args)...)))
{
  return apply_binary_visitor( std::forward<Visitor>(v), x->data(), y->data()
                             , std::forward<Args>(args)...);
}

/*------------------------------------------------------------------------------------------------*/

} // namespace coredd

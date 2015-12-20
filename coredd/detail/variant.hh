/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#include <cstdint>     // uint8_t
#include <functional>  // hash
#include <iosfwd>
#include <limits>      // numeric_limits
#include <type_traits> // is_nothrow_constructible, result_of
#include <utility>     // forward

#include "coredd/detail/construct.hh"
#include "coredd/detail/typelist.hh"
#include "coredd/detail/union_storage.hh"
#include "coredd/hash.hh"
#include "coredd/packed.hh"

namespace coredd { namespace detail {

/*------------------------------------------------------------------------------------------------*/

/// @brief Dispatch the destructor to the contained type in the visited variant.
struct dtor_visitor
{
  template <typename T>
  void
  operator()(const T& x)
  const noexcept(noexcept(x.~T()))
  {
    x.~T();
  }
};

/*------------------------------------------------------------------------------------------------*/

struct hash_visitor
{
  template <typename T>
  std::size_t
  operator()(const T& x)
  const noexcept(noexcept(std::hash<T>()(x)))
  {
    return std::hash<T>()(x);
  }
};

/*------------------------------------------------------------------------------------------------*/

/// @internal
struct eq_visitor
{
  template <typename T>
  bool
  operator()(const T& lhs, const T& rhs)
  const noexcept
  {
    static_assert(noexcept(lhs == rhs), "Comparison should be noexcept");
    return lhs == rhs;
  }

  template <typename T, typename U>
  bool
  operator()(const T&, const U&)
  const noexcept
  {
    assert(false);
    __builtin_unreachable();
  }
};

/*------------------------------------------------------------------------------------------------*/

template <typename... Types>
class variant;

/*------------------------------------------------------------------------------------------------*/

template <typename Visitor, typename X, typename... Args>
auto
call(Visitor&& v, const void* x, Args&&... args)
noexcept(noexcept(v(*static_cast<const X*>(x), std::forward<Args>(args)...)))
-> decltype(auto)
{
  return v(*static_cast<const X*>(x), std::forward<Args>(args)...);
}

/*------------------------------------------------------------------------------------------------*/

template <typename Visitor, typename X, typename Y, typename... Args>
auto
binary_call(Visitor&& v, const void* x, const void* y, Args&&... args)
-> decltype(auto)
{
  return v(*static_cast<const X*>(x), *static_cast<const Y*>(y), std::forward<Args>(args)...);
}

/*------------------------------------------------------------------------------------------------*/

/// @brief Expands a list of pairs of types into pointers to binary_call.
/// @see http://stackoverflow.com/a/25661431/21584
template<typename Visitor, typename T, std::size_t N, typename... Args>
struct binary_jump_table
{
    template<class... Xs, class... Ys>
    constexpr binary_jump_table(list<pair<Xs, Ys>...>)
      : table{&binary_call<Visitor, Xs, Ys, Args...>...}
    {}

    T table[N];
};

/*------------------------------------------------------------------------------------------------*/

template <typename Visitor, typename... Xs, typename... Args>
auto
apply_visitor(Visitor&& v, const variant<Xs...>& x, Args&&... args)
{
  using fun_ptr_type
    = std::result_of_t<Visitor(nth_t<0, Xs...>, Args&&...)>
      (*) (Visitor&&, const void*, Args&&...);

  static constexpr fun_ptr_type table[] = {&call<Visitor, Xs, Args&&...>...};

  return table[x.index](std::forward<Visitor>(v), &x.storage, std::forward<Args>(args)...);
}

/*------------------------------------------------------------------------------------------------*/

template <typename Visitor, typename... Xs, typename... Ys, typename... Args>
auto
apply_binary_visitor(Visitor&& v, const variant<Xs...>& x, const variant<Ys...>& y, Args&&... args)
{
  using fun_ptr_type
    = std::result_of_t<Visitor(nth_t<0, Xs...>, nth_t<0, Ys...>, Args&&...)>
      (*) (Visitor&&, const void*, const void*, Args&&...);

  static constexpr binary_jump_table<Visitor, fun_ptr_type, sizeof...(Xs) * sizeof...(Ys), Args...>
    table = {typename join<list<Xs...>, list<Ys...>>::type()};

  return table.table[x.index * sizeof...(Ys) + y.index]
    (std::forward<Visitor>(v), &x.storage, &y.storage, std::forward<Args>(args)...);
}

/*------------------------------------------------------------------------------------------------*/

/// @brief A type-safe discriminated union.
/// @tparam Types The list of possible types.
///
/// This type is meant to be stored in a unique_table, wrapped in a ref_counted.
/// Once constructed, it can never be assigned an other data.
template <typename... Types>
class COREDD_ATTRIBUTE_PACKED variant
{
private:

  /// @brief A type large enough to contain all variant's types, with the correct alignement.
  using storage_type = typename union_storage<0, Types...>::type;

public:

  // Can't copy a variant.
  const variant& operator=(const variant&) = delete;
  variant(const variant&) = delete;

  // Can't move a variant.
  variant& operator=(variant&&) = delete;
  variant(variant&&) = delete;

  static_assert( sizeof...(Types) >= 1
               , "A variant should contain at least one type.");

  static_assert( sizeof...(Types) <= std::numeric_limits<uint8_t>::max()
               , "A variant can't hold more than UCHAR_MAX types.");

  /// @brief In place construction of an held type.
  ///
  /// Unlike boost::variant, we don't need the never empty guaranty, so we don't need to check
  /// if held types can throw exceptions upon construction.
  template <typename T, typename... Args>
  variant(construct<T>, Args&&... args)
  noexcept(std::is_nothrow_constructible<T, Args...>::value)
    : index(index_of<T, Types...>::value)
  	, storage()
  {
    new (const_cast<storage_type*>(&storage)) T{std::forward<Args>(args)...};
  }

  /// @brief Destructor.
  ~variant()
  {
    apply_visitor(dtor_visitor(), *this);
  }

  friend
  std::ostream&
  operator<<(std::ostream& os, const variant& v)
  {
    return apply_visitor([&](const auto& x) -> std::ostream& {return os << x;}, v);
  }

  friend
  bool
  operator==(const variant& lhs, const variant& rhs)
  noexcept
  {
    return lhs.index == rhs.index and apply_binary_visitor(eq_visitor(), lhs, rhs);
  }

//private:

  /// @brief Index of the held type in the list of all possible types.
  const uint8_t index;

  /// @brief Memory storage suitable for all Types.
  const storage_type storage;
};

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @related variant
template <typename T, typename... Types>
bool
is(const variant<Types...>& v)
noexcept
{
  return v.index == index_of<typename std::decay<T>::type, Types...>::value;
}

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @related variant
template <typename T, typename... Types>
const T&
variant_cast(const variant<Types...>& v)
noexcept
{
  assert(is<T>(v));
  return *reinterpret_cast<const T*>(&v.storage);
}

/*------------------------------------------------------------------------------------------------*/

}} // namespace coredd::detail

namespace std {

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @brief Hash specialization for coredd::variant
template <typename... Types>
struct hash<coredd::detail::variant<Types...>>
{
  std::size_t
  operator()(const coredd::detail::variant<Types...>& x)
  const
  {
    using namespace coredd;
    return seed(apply_visitor(coredd::detail::hash_visitor{}, x)) (val(x.index));
  }
};

/*------------------------------------------------------------------------------------------------*/

} // namespace std

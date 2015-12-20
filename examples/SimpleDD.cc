/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#include <cassert>
#include <memory> // unique_ptr
#include <unordered_map>

#include "coredd/cache.hh"
#include "coredd/ptr.hh"
#include "coredd/unicity.hh"
#include "coredd/visit.hh"

/*------------------------------------------------------------------------------------------------*/

struct Zero;
struct One;
struct Node;

/*------------------------------------------------------------------------------------------------*/

using Unicity = coredd::unicity<Zero, One, Node>;

/*------------------------------------------------------------------------------------------------*/

using SimpleDD = Unicity::ptr_type;

/*------------------------------------------------------------------------------------------------*/

struct Zero
{
  friend
  constexpr bool
  operator==(const Zero&, const Zero&)
  noexcept
  {
    return true;
  }
};

/*------------------------------------------------------------------------------------------------*/

struct One
{
  friend
  constexpr bool
  operator==(const One&, const One&)
  noexcept
  {
    return true;
  }
};

/*------------------------------------------------------------------------------------------------*/

struct Node
{
  int variable;
  SimpleDD lo;
  SimpleDD hi;

  friend
  bool
  operator==(const Node& lhs, const Node& rhs)
  noexcept
  {
    return lhs.variable == rhs.variable and lhs.lo == rhs.lo and rhs.hi == rhs.hi;
  }
};

/*------------------------------------------------------------------------------------------------*/

namespace std {

template <> struct hash<Zero> {std::size_t operator()(const Zero&) const noexcept {return 0;}};
template <> struct hash<One>  {std::size_t operator()(const One&)  const noexcept {return 1;}};

template <>
struct hash<Node>
{
  std::size_t
  operator()(const Node& n)
  const noexcept
  {
    using namespace coredd;
    return seed(n.variable) (val(n.lo)) (val(n.hi));
  }
};

} // namespace std

/*------------------------------------------------------------------------------------------------*/

struct NbPathsVisitor
{
  std::size_t
  operator()(const Zero&, std::unordered_map<const char*, std::size_t>&)
  const noexcept
  {
    return 0ul;
  }

  std::size_t
  operator()(const One&, std::unordered_map<const char*, std::size_t>&)
  const noexcept
  {
    return 1ul;
  }

  std::size_t
  operator()(const Node& n, std::unordered_map<const char*, std::size_t>& cache)
  const noexcept
  {
    auto insertion = cache.emplace(reinterpret_cast<const char*>(&n), 0ul);
    if (insertion.second)
    // First time
    {
      insertion.first->second =  visit(*this, n.lo, cache) + visit(*this, n.hi, cache);
    }
    return insertion.first->second;
  }
};

/*------------------------------------------------------------------------------------------------*/

auto unicity = Unicity{2048};
const auto one = unicity.make<One>();
const auto zero = unicity.make<Zero>();

/*------------------------------------------------------------------------------------------------*/

template <typename Operation>
class Context
{
private:

  using cache_type = coredd::cache<Context, Operation>;

public:

  Context(std::size_t cache_size)
    : m_cache{std::make_shared<cache_type>(*this, cache_size)}
  {}

  cache_type&
  cache()
  {
    return *m_cache;
  }

private:

  std::shared_ptr<cache_type> m_cache;
};

/*------------------------------------------------------------------------------------------------*/

template <typename Operation>
struct SumVisitor
{
  SimpleDD lhs_ptr;
  SimpleDD rhs_ptr;

  template <typename T1, typename T2>
  SimpleDD
  operator()(const T1&, const T2&, Context<Operation>&)
  const
  {
    throw std::runtime_error{"Incompatible SimpleDD"};
  }

  SimpleDD
  operator()(const One&, const One&, Context<Operation>&)
  const noexcept
  {
    return one;
  }

  SimpleDD
  operator()(const Node& lhs, const Node& rhs, Context<Operation>& cxt)
  const
  {
    if (lhs.variable != rhs.variable)
    {
      throw std::runtime_error{"Incompatible SimpleDD"};
    }
    const auto lo = cxt.cache()(Operation{lhs.lo, rhs.lo});
    const auto hi = cxt.cache()(Operation{lhs.hi, rhs.hi});
    return unicity.make<Node>(lhs.variable, lo, hi);
  }
};

/*------------------------------------------------------------------------------------------------*/

struct SumOperation
{
  SimpleDD lhs;
  SimpleDD rhs;

  SimpleDD
  operator()(Context<SumOperation>& cxt)
  const
  {
    if (lhs.is<Zero>())
    {
      return rhs;
    }
    else if (rhs.is<Zero>())
    {
      return lhs;
    }
    return binary_visit(SumVisitor<SumOperation>{lhs, rhs}, lhs, rhs, cxt);
  }

  friend
  bool
  operator==(const SumOperation& lhs, const SumOperation& rhs)
  noexcept
  {
    return lhs.lhs == rhs.lhs and lhs.rhs == rhs.rhs;
  }
};

/*------------------------------------------------------------------------------------------------*/

namespace std {

template <>
struct hash<SumOperation>
{
  std::size_t
  operator()(const SumOperation& op)
  const noexcept
  {
    using namespace coredd;
    return seed() (val(op.lhs)) (val(op.rhs));
  }
};

} // namespace std

/*------------------------------------------------------------------------------------------------*/

auto context = Context<SumOperation>{8192};

/*------------------------------------------------------------------------------------------------*/

SimpleDD
operator+(const SimpleDD& lhs, const SimpleDD& rhs)
{
  return context.cache()(SumOperation{lhs, rhs});
}

/*------------------------------------------------------------------------------------------------*/

int
main()
{
  // Unicity
  assert(unicity.unique_table_stats().size == 2);
  {
    const auto n0 = unicity.make<Node>(0, one, one);
    const auto n1_1 = unicity.make<Node>(1, n0, zero);
    const auto n1_2 = unicity.make<Node>(1, zero, n0);
    const auto n2 = unicity.make<Node>(2, n1_1, n1_2);
    assert(unicity.unique_table_stats().size == 6);
  }
  assert(unicity.unique_table_stats().size == 2);

  // Visitor
  {
    const auto n0 = unicity.make<Node>(0, one, one);
    const auto n1 = unicity.make<Node>(1, n0, zero);
    const auto n2 = unicity.make<Node>(2, n1, n1);

    auto cache = std::unordered_map<const char*, std::size_t>{};

    assert(visit(NbPathsVisitor{}, n0, cache) == 2);
    assert(visit(NbPathsVisitor{}, n1, cache) == 2);
    assert(visit(NbPathsVisitor{}, n2, cache) == 4);
  }

  // Cache
  {
    const auto n0_1 = unicity.make<Node>(0, one, zero);
    const auto n0_2 = unicity.make<Node>(0, zero, one);

    const auto n0 = n0_1 + n0_2;
    assert(context.cache().statistics().hits == 0);
    const auto n0_bis = n0_1 + n0_2;
    assert(context.cache().statistics().hits == 1);

    assert(n0.get<Node>().variable == 0);
    assert(n0.get<Node>().lo == one);
    assert(n0.get<Node>().hi == one);
  }
}

/*------------------------------------------------------------------------------------------------*/

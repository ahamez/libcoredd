/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

namespace coredd { namespace detail {

/*------------------------------------------------------------------------------------------------*/

/// @brief It must be stored by the hash table's data as a member named 'hook'.
template <typename Data>
struct intrusive_member_hook
{
  /// @brief Store the next data in a bucket.
  mutable Data* next = nullptr;
};

/*------------------------------------------------------------------------------------------------*/

}} // namespace coredd::detail

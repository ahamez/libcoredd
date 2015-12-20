/// @file
/// @copyright The code is licensed under the BSD License
///            <http://opensource.org/licenses/BSD-2-Clause>,
///            Copyright (c) 2012-2015 Alexandre Hamez.
/// @author Alexandre Hamez

#pragma once

#if defined COREDD_PACKED
#  define COREDD_ATTRIBUTE_PACKED __attribute__ ((__packed__))
#else
#  define COREDD_ATTRIBUTE_PACKED
#endif

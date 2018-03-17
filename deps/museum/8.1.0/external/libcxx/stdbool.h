// -*- C++ -*-
//===--------------------------- stdbool.h --------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef _MUSEUM_LIBCPP_STDBOOL_H
#define _MUSEUM_LIBCPP_STDBOOL_H


/*
    stdbool.h synopsis

Macros:

    __bool_true_false_are_defined

*/

#include <museum/8.1.0/external/libcxx/__config>

#if !defined(_MUSEUM_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

#include_next <stdbool.h>

#ifdef __cplusplus
#undef bool
#undef true
#undef false
#undef __bool_true_false_are_defined
#define __bool_true_false_are_defined 1
#endif

#endif  // _MUSEUM_LIBCPP_STDBOOL_H

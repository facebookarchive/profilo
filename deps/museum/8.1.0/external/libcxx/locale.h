// -*- C++ -*-
//===---------------------------- locale.h --------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _MUSEUM_LIBCPP_LOCALE_H
#define _MUSEUM_LIBCPP_LOCALE_H

/*
    locale.h synopsis

Macros:

    LC_ALL
    LC_COLLATE
    LC_CTYPE
    LC_MONETARY
    LC_NUMERIC
    LC_TIME

Types:

    lconv

Functions:

   setlocale
   localeconv

*/

#include <museum/8.1.0/external/libcxx/__config>

#if !defined(_MUSEUM_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

#include <museum/8.1.0/bionic/libc/locale.h>

#endif  // _MUSEUM_LIBCPP_LOCALE_H

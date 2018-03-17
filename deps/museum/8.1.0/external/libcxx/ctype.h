// -*- C++ -*-
//===---------------------------- ctype.h ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _MUSEUM_LIBCPP_CTYPE_H
#define _MUSEUM_LIBCPP_CTYPE_H

/*
    ctype.h synopsis

int isalnum(int c);
int isalpha(int c);
int isblank(int c);  // C99
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);
*/

#include <museum/8.1.0/external/libcxx/__config>

#if !defined(_MUSEUM_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

#include <museum/8.1.0/bionic/libc/ctype.h>

#ifdef __cplusplus

#if defined(_MUSEUM_LIBCPP_MSVCRT)
// We support including .h headers inside 'extern "C"' contexts, so switch
// back to C++ linkage before including these C++ headers.
extern "C++" {
  #include <museum/8.1.0/external/libcxx/support/win32/support.h>
  #include <museum/8.1.0/external/libcxx/support/win32/locale_win32.h>
}
#endif // _MUSEUM_LIBCPP_MSVCRT

#undef isalnum
#undef isalpha
#undef isblank
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit
#undef tolower
#undef toupper

#endif

#endif  // _MUSEUM_LIBCPP_CTYPE_H

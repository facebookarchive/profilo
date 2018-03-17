// -*- C++ -*-
//===--------------------------- stddef.h ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#if defined(__need_ptrdiff_t) || defined(__need_size_t) || \
    defined(__need_wchar_t) || defined(__need_NULL) || defined(__need_wint_t)

#if !defined(_MUSEUM_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

#include_next <stddef.h>

#elif !defined(_MUSEUM_LIBCPP_STDDEF_H)
#define _MUSEUM_LIBCPP_STDDEF_H

/*
    stddef.h synopsis

Macros:

    offsetof(type,member-designator)
    NULL

Types:

    ptrdiff_t
    size_t
    max_align_t
    nullptr_t

*/

#include <museum/8.0.0/external/libcxx/__config>

#if !defined(_MUSEUM_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

#include_next <stddef.h>

#ifdef __cplusplus

extern "C++" {
#include <museum/8.0.0/external/libcxx/__nullptr>
using facebook::museum::MUSEUM_VERSION::std::nullptr_t;
}

// Re-use the compiler's <stddef.h> max_align_t where possible.
#if !defined(__CLANG_MAX_ALIGN_T_DEFINED) && !defined(_GCC_MAX_ALIGN_T) && \
    !defined(__DEFINED_max_align_t)
typedef long double max_align_t;
#endif

#endif

#endif  // _MUSEUM_LIBCPP_STDDEF_H

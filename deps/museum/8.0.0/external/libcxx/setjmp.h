// -*- C++ -*-
//===--------------------------- setjmp.h ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _MUSEUM_LIBCPP_SETJMP_H
#define _MUSEUM_LIBCPP_SETJMP_H

/*
    setjmp.h synopsis

Macros:

    setjmp

Types:

    jmp_buf

void longjmp(jmp_buf env, int val);

*/

#include <museum/8.0.0/external/libcxx/__config>

#if !defined(_MUSEUM_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

#include <museum/8.0.0/bionic/libc/setjmp.h>

#ifdef __cplusplus

#ifndef setjmp
#define setjmp(env) setjmp(env)
#endif

#endif // __cplusplus

#endif  // _MUSEUM_LIBCPP_SETJMP_H

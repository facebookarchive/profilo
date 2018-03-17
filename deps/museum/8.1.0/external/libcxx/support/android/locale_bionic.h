// -*- C++ -*-
//===------------------- support/android/locale_bionic.h ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _MUSEUM_LIBCPP_SUPPORT_ANDROID_LOCALE_BIONIC_H
#define _MUSEUM_LIBCPP_SUPPORT_ANDROID_LOCALE_BIONIC_H

#if defined(__BIONIC__)

#ifdef __cplusplus
extern "C" {
#endif

#include <museum/8.1.0/external/libcxx/stdlib.h>
#include <museum/8.1.0/bionic/libc/xlocale.h>

#ifdef __cplusplus
}
#endif

#include <museum/8.1.0/external/libcxx/support/xlocale/__posix_l_fallback.h>
#include <museum/8.1.0/external/libcxx/support/xlocale/__strtonum_fallback.h>

#endif // defined(__BIONIC__)
#endif // _MUSEUM_LIBCPP_SUPPORT_ANDROID_LOCALE_BIONIC_H

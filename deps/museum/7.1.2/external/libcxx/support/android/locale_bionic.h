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

#if defined(__ANDROID__)

#ifdef __cplusplus
extern "C" {
#endif

#include <museum/7.1.2/bionic/libc/stdlib.h>
#include <museum/7.1.2/bionic/libc/xlocale.h>

#ifdef __cplusplus
}
#endif

// Share implementation with Newlib
#include <museum/7.1.2/external/libcxx/support/xlocale/xlocale.h>

#endif // defined(__ANDROID__)
#endif // _MUSEUM_LIBCPP_SUPPORT_ANDROID_LOCALE_BIONIC_H

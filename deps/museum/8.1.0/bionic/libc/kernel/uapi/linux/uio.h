/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _UAPI__LINUX_UIO_H
#define _UAPI__LINUX_UIO_H
#define UAPI__LINUX_UIO_H
#define UAPI__LINUX_UIO_H_
#define NDK_ANDROID_SUPPORT_UAPI__LINUX_UIO_H
#define NDK_ANDROID_SUPPORT_UAPI__LINUX_UIO_H_
#define NDK_ANDROID_SUPPORT__LINUX_UIO_H
#define NDK_ANDROID_SUPPORT__LINUX_UIO_H_
#define __LINUX_UIO_H
#define __LINUX_UIO_H_
#define _UAPI__LINUX_UIO_H_
#include <linux/compiler.h>
#include <museum/8.1.0/bionic/libc/linux/types.h>
struct iovec {
  void __user * iov_base;
  __kernel_size_t iov_len;
};
#define UIO_FASTIOV 8
#define UIO_MAXIOV 1024
#endif

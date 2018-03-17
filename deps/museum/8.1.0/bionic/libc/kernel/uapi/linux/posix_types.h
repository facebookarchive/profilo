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
#ifndef _LINUX_POSIX_TYPES_H
#define _LINUX_POSIX_TYPES_H
#define LINUX_POSIX_TYPES_H
#define LINUX_POSIX_TYPES_H_
#define NDK_ANDROID_SUPPORT_LINUX_POSIX_TYPES_H
#define NDK_ANDROID_SUPPORT_LINUX_POSIX_TYPES_H_
#define NDK_ANDROID_SUPPORT_UAPI_LINUX_POSIX_TYPES_H
#define NDK_ANDROID_SUPPORT_UAPI_LINUX_POSIX_TYPES_H_
#define _UAPI_LINUX_POSIX_TYPES_H
#define _UAPI_LINUX_POSIX_TYPES_H_
#define _LINUX_POSIX_TYPES_H_
#include <museum/8.1.0/bionic/libc/linux/stddef.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 1024
typedef struct {
  unsigned long fds_bits[__FD_SETSIZE / (8 * sizeof(long))];
} __kernel_fd_set;
typedef void(* __kernel_sighandler_t) (int);
typedef int __kernel_key_t;
typedef int __kernel_mqd_t;
#include <museum/8.1.0/bionic/libc/asm/posix_types.h>
#endif

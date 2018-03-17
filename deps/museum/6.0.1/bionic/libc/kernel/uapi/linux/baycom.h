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
#ifndef _BAYCOM_H
#define _BAYCOM_H
#define BAYCOM_H
#define BAYCOM_H_
#define NDK_ANDROID_SUPPORT_BAYCOM_H
#define NDK_ANDROID_SUPPORT_BAYCOM_H_
#define NDK_ANDROID_SUPPORT_UAPI_BAYCOM_H
#define NDK_ANDROID_SUPPORT_UAPI_BAYCOM_H_
#define _UAPI_BAYCOM_H
#define _UAPI_BAYCOM_H_
#define _BAYCOM_H_
struct baycom_debug_data {
  unsigned long debug1;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned long debug2;
  long debug3;
};
struct baycom_ioctl {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int cmd;
  union {
    struct baycom_debug_data dbg;
  } data;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define BAYCOMCTL_GETDEBUG 0x92
#endif

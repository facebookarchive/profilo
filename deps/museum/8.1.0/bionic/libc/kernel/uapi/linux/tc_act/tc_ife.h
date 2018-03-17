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
#ifndef __UAPI_TC_IFE_H
#define __UAPI_TC_IFE_H
#define _UAPI_TC_IFE_H
#define _UAPI_TC_IFE_H_
#define _TC_IFE_H
#define _TC_IFE_H_
#define NDK_ANDROID_SUPPORT__UAPI_TC_IFE_H
#define NDK_ANDROID_SUPPORT__UAPI_TC_IFE_H_
#define NDK_ANDROID_SUPPORT_UAPI__UAPI_TC_IFE_H
#define NDK_ANDROID_SUPPORT_UAPI__UAPI_TC_IFE_H_
#define _UAPI__UAPI_TC_IFE_H
#define _UAPI__UAPI_TC_IFE_H_
#define __UAPI_TC_IFE_H_
#include <museum/8.1.0/bionic/libc/linux/types.h>
#include <museum/8.1.0/bionic/libc/linux/pkt_cls.h>
#define TCA_ACT_IFE 25
#define IFE_ENCODE 1
#define IFE_DECODE 0
struct tc_ife {
  tc_gen;
  __u16 flags;
};
enum {
  TCA_IFE_UNSPEC,
  TCA_IFE_PARMS,
  TCA_IFE_TM,
  TCA_IFE_DMAC,
  TCA_IFE_SMAC,
  TCA_IFE_TYPE,
  TCA_IFE_METALST,
  TCA_IFE_PAD,
  __TCA_IFE_MAX
};
#define TCA_IFE_MAX (__TCA_IFE_MAX - 1)
#define IFE_META_SKBMARK 1
#define IFE_META_HASHID 2
#define IFE_META_PRIO 3
#define IFE_META_QMAP 4
#define IFE_META_TCINDEX 5
#define __IFE_META_MAX 6
#define IFE_META_MAX (__IFE_META_MAX - 1)
#endif

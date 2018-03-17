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
#ifndef __LINUX_BRIDGE_EBT_PKTTYPE_H
#define __LINUX_BRIDGE_EBT_PKTTYPE_H
#define _LINUX_BRIDGE_EBT_PKTTYPE_H
#define _LINUX_BRIDGE_EBT_PKTTYPE_H_
#define _UAPI_LINUX_BRIDGE_EBT_PKTTYPE_H
#define _UAPI_LINUX_BRIDGE_EBT_PKTTYPE_H_
#define NDK_ANDROID_SUPPORT__LINUX_BRIDGE_EBT_PKTTYPE_H
#define NDK_ANDROID_SUPPORT__LINUX_BRIDGE_EBT_PKTTYPE_H_
#define NDK_ANDROID_SUPPORT_UAPI__LINUX_BRIDGE_EBT_PKTTYPE_H
#define NDK_ANDROID_SUPPORT_UAPI__LINUX_BRIDGE_EBT_PKTTYPE_H_
#define _UAPI__LINUX_BRIDGE_EBT_PKTTYPE_H
#define _UAPI__LINUX_BRIDGE_EBT_PKTTYPE_H_
#define __LINUX_BRIDGE_EBT_PKTTYPE_H_
#include <museum/8.1.0/bionic/libc/linux/types.h>
struct ebt_pkttype_info {
  __u8 pkt_type;
  __u8 invert;
};
#define EBT_PKTTYPE_MATCH "pkttype"
#endif

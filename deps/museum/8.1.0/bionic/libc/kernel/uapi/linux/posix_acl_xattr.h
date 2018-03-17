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
#ifndef __UAPI_POSIX_ACL_XATTR_H
#define __UAPI_POSIX_ACL_XATTR_H
#define _UAPI_POSIX_ACL_XATTR_H
#define _UAPI_POSIX_ACL_XATTR_H_
#define _POSIX_ACL_XATTR_H
#define _POSIX_ACL_XATTR_H_
#define NDK_ANDROID_SUPPORT__UAPI_POSIX_ACL_XATTR_H
#define NDK_ANDROID_SUPPORT__UAPI_POSIX_ACL_XATTR_H_
#define NDK_ANDROID_SUPPORT_UAPI__UAPI_POSIX_ACL_XATTR_H
#define NDK_ANDROID_SUPPORT_UAPI__UAPI_POSIX_ACL_XATTR_H_
#define _UAPI__UAPI_POSIX_ACL_XATTR_H
#define _UAPI__UAPI_POSIX_ACL_XATTR_H_
#define __UAPI_POSIX_ACL_XATTR_H_
#include <museum/8.1.0/bionic/libc/linux/types.h>
#define POSIX_ACL_XATTR_VERSION 0x0002
#define ACL_UNDEFINED_ID (- 1)
struct posix_acl_xattr_entry {
  __le16 e_tag;
  __le16 e_perm;
  __le32 e_id;
};
struct posix_acl_xattr_header {
  __le32 a_version;
};
#endif

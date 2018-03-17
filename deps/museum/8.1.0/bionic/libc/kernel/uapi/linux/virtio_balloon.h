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
#ifndef _LINUX_VIRTIO_BALLOON_H
#define _LINUX_VIRTIO_BALLOON_H
#define LINUX_VIRTIO_BALLOON_H
#define LINUX_VIRTIO_BALLOON_H_
#define NDK_ANDROID_SUPPORT_LINUX_VIRTIO_BALLOON_H
#define NDK_ANDROID_SUPPORT_LINUX_VIRTIO_BALLOON_H_
#define NDK_ANDROID_SUPPORT_UAPI_LINUX_VIRTIO_BALLOON_H
#define NDK_ANDROID_SUPPORT_UAPI_LINUX_VIRTIO_BALLOON_H_
#define _UAPI_LINUX_VIRTIO_BALLOON_H
#define _UAPI_LINUX_VIRTIO_BALLOON_H_
#define _LINUX_VIRTIO_BALLOON_H_
#include <museum/8.1.0/bionic/libc/linux/types.h>
#include <museum/8.1.0/bionic/libc/linux/virtio_types.h>
#include <museum/8.1.0/bionic/libc/linux/virtio_ids.h>
#include <museum/8.1.0/bionic/libc/linux/virtio_config.h>
#define VIRTIO_BALLOON_F_MUST_TELL_HOST 0
#define VIRTIO_BALLOON_F_STATS_VQ 1
#define VIRTIO_BALLOON_F_DEFLATE_ON_OOM 2
#define VIRTIO_BALLOON_PFN_SHIFT 12
struct virtio_balloon_config {
  __u32 num_pages;
  __u32 actual;
};
#define VIRTIO_BALLOON_S_SWAP_IN 0
#define VIRTIO_BALLOON_S_SWAP_OUT 1
#define VIRTIO_BALLOON_S_MAJFLT 2
#define VIRTIO_BALLOON_S_MINFLT 3
#define VIRTIO_BALLOON_S_MEMFREE 4
#define VIRTIO_BALLOON_S_MEMTOT 5
#define VIRTIO_BALLOON_S_AVAIL 6
#define VIRTIO_BALLOON_S_NR 7
struct virtio_balloon_stat {
  __virtio16 tag;
  __virtio64 val;
} __attribute__((packed));
#endif

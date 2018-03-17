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
#ifndef QXL_DRM_H
#define QXL_DRM_H
#include "drm.h"
#ifdef __cplusplus
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#define QXL_GEM_DOMAIN_CPU 0
#define QXL_GEM_DOMAIN_VRAM 1
#define QXL_GEM_DOMAIN_SURFACE 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_QXL_ALLOC 0x00
#define DRM_QXL_MAP 0x01
#define DRM_QXL_EXECBUFFER 0x02
#define DRM_QXL_UPDATE_AREA 0x03
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_QXL_GETPARAM 0x04
#define DRM_QXL_CLIENTCAP 0x05
#define DRM_QXL_ALLOC_SURF 0x06
struct drm_qxl_alloc {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 size;
  __u32 handle;
};
struct drm_qxl_map {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 offset;
  __u32 handle;
  __u32 pad;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define QXL_RELOC_TYPE_BO 1
#define QXL_RELOC_TYPE_SURF 2
struct drm_qxl_reloc {
  __u64 src_offset;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 dst_offset;
  __u32 src_handle;
  __u32 dst_handle;
  __u32 reloc_type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 pad;
};
struct drm_qxl_command {
  __u64 __user command;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 __user relocs;
  __u32 type;
  __u32 command_size;
  __u32 relocs_num;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 pad;
};
struct drm_qxl_execbuffer {
  __u32 flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 commands_num;
  __u64 __user commands;
};
struct drm_qxl_update_area {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 handle;
  __u32 top;
  __u32 left;
  __u32 bottom;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 right;
  __u32 pad;
};
#define QXL_PARAM_NUM_SURFACES 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define QXL_PARAM_MAX_RELOCS 2
struct drm_qxl_getparam {
  __u64 param;
  __u64 value;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_qxl_clientcap {
  __u32 index;
  __u32 pad;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_qxl_alloc_surf {
  __u32 format;
  __u32 width;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 height;
  __s32 stride;
  __u32 handle;
  __u32 pad;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define DRM_IOCTL_QXL_ALLOC DRM_IOWR(DRM_COMMAND_BASE + DRM_QXL_ALLOC, struct drm_qxl_alloc)
#define DRM_IOCTL_QXL_MAP DRM_IOWR(DRM_COMMAND_BASE + DRM_QXL_MAP, struct drm_qxl_map)
#define DRM_IOCTL_QXL_EXECBUFFER DRM_IOW(DRM_COMMAND_BASE + DRM_QXL_EXECBUFFER, struct drm_qxl_execbuffer)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IOCTL_QXL_UPDATE_AREA DRM_IOW(DRM_COMMAND_BASE + DRM_QXL_UPDATE_AREA, struct drm_qxl_update_area)
#define DRM_IOCTL_QXL_GETPARAM DRM_IOWR(DRM_COMMAND_BASE + DRM_QXL_GETPARAM, struct drm_qxl_getparam)
#define DRM_IOCTL_QXL_CLIENTCAP DRM_IOW(DRM_COMMAND_BASE + DRM_QXL_CLIENTCAP, struct drm_qxl_clientcap)
#define DRM_IOCTL_QXL_ALLOC_SURF DRM_IOWR(DRM_COMMAND_BASE + DRM_QXL_ALLOC_SURF, struct drm_qxl_alloc_surf)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifdef __cplusplus
#endif
#endif

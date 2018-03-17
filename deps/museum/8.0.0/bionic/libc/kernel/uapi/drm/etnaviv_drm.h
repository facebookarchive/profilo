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
#ifndef __ETNAVIV_DRM_H__
#define __ETNAVIV_DRM_H__
#include "drm.h"
#ifdef __cplusplus
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
struct drm_etnaviv_timespec {
  __s64 tv_sec;
  __s64 tv_nsec;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define ETNAVIV_PARAM_GPU_MODEL 0x01
#define ETNAVIV_PARAM_GPU_REVISION 0x02
#define ETNAVIV_PARAM_GPU_FEATURES_0 0x03
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNAVIV_PARAM_GPU_FEATURES_1 0x04
#define ETNAVIV_PARAM_GPU_FEATURES_2 0x05
#define ETNAVIV_PARAM_GPU_FEATURES_3 0x06
#define ETNAVIV_PARAM_GPU_FEATURES_4 0x07
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNAVIV_PARAM_GPU_FEATURES_5 0x08
#define ETNAVIV_PARAM_GPU_FEATURES_6 0x09
#define ETNAVIV_PARAM_GPU_STREAM_COUNT 0x10
#define ETNAVIV_PARAM_GPU_REGISTER_MAX 0x11
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNAVIV_PARAM_GPU_THREAD_COUNT 0x12
#define ETNAVIV_PARAM_GPU_VERTEX_CACHE_SIZE 0x13
#define ETNAVIV_PARAM_GPU_SHADER_CORE_COUNT 0x14
#define ETNAVIV_PARAM_GPU_PIXEL_PIPES 0x15
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNAVIV_PARAM_GPU_VERTEX_OUTPUT_BUFFER_SIZE 0x16
#define ETNAVIV_PARAM_GPU_BUFFER_SIZE 0x17
#define ETNAVIV_PARAM_GPU_INSTRUCTION_COUNT 0x18
#define ETNAVIV_PARAM_GPU_NUM_CONSTANTS 0x19
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNAVIV_PARAM_GPU_NUM_VARYINGS 0x1a
#define ETNA_MAX_PIPES 4
struct drm_etnaviv_param {
  __u32 pipe;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 param;
  __u64 value;
};
#define ETNA_BO_CACHE_MASK 0x000f0000
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNA_BO_CACHED 0x00010000
#define ETNA_BO_WC 0x00020000
#define ETNA_BO_UNCACHED 0x00040000
#define ETNA_BO_FORCE_MMU 0x00100000
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_etnaviv_gem_new {
  __u64 size;
  __u32 flags;
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_etnaviv_gem_info {
  __u32 handle;
  __u32 pad;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 offset;
};
#define ETNA_PREP_READ 0x01
#define ETNA_PREP_WRITE 0x02
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNA_PREP_NOSYNC 0x04
struct drm_etnaviv_gem_cpu_prep {
  __u32 handle;
  __u32 op;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_etnaviv_timespec timeout;
};
struct drm_etnaviv_gem_cpu_fini {
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 flags;
};
struct drm_etnaviv_gem_submit_reloc {
  __u32 submit_offset;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 reloc_idx;
  __u64 reloc_offset;
  __u32 flags;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNA_SUBMIT_BO_READ 0x0001
#define ETNA_SUBMIT_BO_WRITE 0x0002
struct drm_etnaviv_gem_submit_bo {
  __u32 flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 handle;
  __u64 presumed;
};
#define ETNA_PIPE_3D 0x00
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ETNA_PIPE_2D 0x01
#define ETNA_PIPE_VG 0x02
struct drm_etnaviv_gem_submit {
  __u32 fence;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 pipe;
  __u32 exec_state;
  __u32 nr_bos;
  __u32 nr_relocs;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 stream_size;
  __u64 bos;
  __u64 relocs;
  __u64 stream;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define ETNA_WAIT_NONBLOCK 0x01
struct drm_etnaviv_wait_fence {
  __u32 pipe;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 fence;
  __u32 flags;
  __u32 pad;
  struct drm_etnaviv_timespec timeout;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define ETNA_USERPTR_READ 0x01
#define ETNA_USERPTR_WRITE 0x02
struct drm_etnaviv_gem_userptr {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 user_ptr;
  __u64 user_size;
  __u32 flags;
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_etnaviv_gem_wait {
  __u32 pipe;
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 flags;
  __u32 pad;
  struct drm_etnaviv_timespec timeout;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_ETNAVIV_GET_PARAM 0x00
#define DRM_ETNAVIV_GEM_NEW 0x02
#define DRM_ETNAVIV_GEM_INFO 0x03
#define DRM_ETNAVIV_GEM_CPU_PREP 0x04
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_ETNAVIV_GEM_CPU_FINI 0x05
#define DRM_ETNAVIV_GEM_SUBMIT 0x06
#define DRM_ETNAVIV_WAIT_FENCE 0x07
#define DRM_ETNAVIV_GEM_USERPTR 0x08
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_ETNAVIV_GEM_WAIT 0x09
#define DRM_ETNAVIV_NUM_IOCTLS 0x0a
#define DRM_IOCTL_ETNAVIV_GET_PARAM DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, struct drm_etnaviv_param)
#define DRM_IOCTL_ETNAVIV_GEM_NEW DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_NEW, struct drm_etnaviv_gem_new)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IOCTL_ETNAVIV_GEM_INFO DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_INFO, struct drm_etnaviv_gem_info)
#define DRM_IOCTL_ETNAVIV_GEM_CPU_PREP DRM_IOW(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_CPU_PREP, struct drm_etnaviv_gem_cpu_prep)
#define DRM_IOCTL_ETNAVIV_GEM_CPU_FINI DRM_IOW(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_CPU_FINI, struct drm_etnaviv_gem_cpu_fini)
#define DRM_IOCTL_ETNAVIV_GEM_SUBMIT DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_SUBMIT, struct drm_etnaviv_gem_submit)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IOCTL_ETNAVIV_WAIT_FENCE DRM_IOW(DRM_COMMAND_BASE + DRM_ETNAVIV_WAIT_FENCE, struct drm_etnaviv_wait_fence)
#define DRM_IOCTL_ETNAVIV_GEM_USERPTR DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_USERPTR, struct drm_etnaviv_gem_userptr)
#define DRM_IOCTL_ETNAVIV_GEM_WAIT DRM_IOW(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_WAIT, struct drm_etnaviv_gem_wait)
#ifdef __cplusplus
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#endif

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
#ifndef __VMWGFX_DRM_H__
#define __VMWGFX_DRM_H__
#include "drm.h"
#ifdef __cplusplus
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#define DRM_VMW_MAX_SURFACE_FACES 6
#define DRM_VMW_MAX_MIP_LEVELS 24
#define DRM_VMW_GET_PARAM 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_ALLOC_DMABUF 1
#define DRM_VMW_UNREF_DMABUF 2
#define DRM_VMW_CURSOR_BYPASS 3
#define DRM_VMW_CONTROL_STREAM 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_CLAIM_STREAM 5
#define DRM_VMW_UNREF_STREAM 6
#define DRM_VMW_CREATE_CONTEXT 7
#define DRM_VMW_UNREF_CONTEXT 8
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_CREATE_SURFACE 9
#define DRM_VMW_UNREF_SURFACE 10
#define DRM_VMW_REF_SURFACE 11
#define DRM_VMW_EXECBUF 12
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_GET_3D_CAP 13
#define DRM_VMW_FENCE_WAIT 14
#define DRM_VMW_FENCE_SIGNALED 15
#define DRM_VMW_FENCE_UNREF 16
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_FENCE_EVENT 17
#define DRM_VMW_PRESENT 18
#define DRM_VMW_PRESENT_READBACK 19
#define DRM_VMW_UPDATE_LAYOUT 20
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_CREATE_SHADER 21
#define DRM_VMW_UNREF_SHADER 22
#define DRM_VMW_GB_SURFACE_CREATE 23
#define DRM_VMW_GB_SURFACE_REF 24
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_SYNCCPU 25
#define DRM_VMW_CREATE_EXTENDED_CONTEXT 26
#define DRM_VMW_PARAM_NUM_STREAMS 0
#define DRM_VMW_PARAM_NUM_FREE_STREAMS 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_PARAM_3D 2
#define DRM_VMW_PARAM_HW_CAPS 3
#define DRM_VMW_PARAM_FIFO_CAPS 4
#define DRM_VMW_PARAM_MAX_FB_SIZE 5
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_PARAM_FIFO_HW_VERSION 6
#define DRM_VMW_PARAM_MAX_SURF_MEMORY 7
#define DRM_VMW_PARAM_3D_CAPS_SIZE 8
#define DRM_VMW_PARAM_MAX_MOB_MEMORY 9
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_VMW_PARAM_MAX_MOB_SIZE 10
#define DRM_VMW_PARAM_SCREEN_TARGET 11
#define DRM_VMW_PARAM_DX 12
enum drm_vmw_handle_type {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DRM_VMW_HANDLE_LEGACY = 0,
  DRM_VMW_HANDLE_PRIME = 1
};
struct drm_vmw_getparam_arg {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 value;
  __u32 param;
  __u32 pad64;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_context_arg {
  __s32 cid;
  __u32 pad64;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_surface_create_req {
  __u32 flags;
  __u32 format;
  __u32 mip_levels[DRM_VMW_MAX_SURFACE_FACES];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 size_addr;
  __s32 shareable;
  __s32 scanout;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_surface_arg {
  __s32 sid;
  enum drm_vmw_handle_type handle_type;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_size {
  __u32 width;
  __u32 height;
  __u32 depth;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 pad64;
};
union drm_vmw_surface_create_arg {
  struct drm_vmw_surface_arg rep;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_vmw_surface_create_req req;
};
union drm_vmw_surface_reference_arg {
  struct drm_vmw_surface_create_req rep;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_vmw_surface_arg req;
};
#define DRM_VMW_EXECBUF_VERSION 2
struct drm_vmw_execbuf_arg {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 commands;
  __u32 command_size;
  __u32 throttle_us;
  __u64 fence_rep;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 version;
  __u32 flags;
  __u32 context_handle;
  __u32 pad64;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_vmw_fence_rep {
  __u32 handle;
  __u32 mask;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 seqno;
  __u32 passed_seqno;
  __u32 pad64;
  __s32 error;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_vmw_alloc_dmabuf_req {
  __u32 size;
  __u32 pad64;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_vmw_dmabuf_rep {
  __u64 map_handle;
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 cur_gmr_id;
  __u32 cur_gmr_offset;
  __u32 pad64;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
union drm_vmw_alloc_dmabuf_arg {
  struct drm_vmw_alloc_dmabuf_req req;
  struct drm_vmw_dmabuf_rep rep;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_unref_dmabuf_arg {
  __u32 handle;
  __u32 pad64;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_rect {
  __s32 x;
  __s32 y;
  __u32 w;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 h;
};
struct drm_vmw_control_stream_arg {
  __u32 stream_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 enabled;
  __u32 flags;
  __u32 color_key;
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 offset;
  __s32 format;
  __u32 size;
  __u32 width;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 height;
  __u32 pitch[3];
  __u32 pad64;
  struct drm_vmw_rect src;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_vmw_rect dst;
};
#define DRM_VMW_CURSOR_BYPASS_ALL (1 << 0)
#define DRM_VMW_CURSOR_BYPASS_FLAGS (1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_cursor_bypass_arg {
  __u32 flags;
  __u32 crtc_id;
  __s32 xpos;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s32 ypos;
  __s32 xhot;
  __s32 yhot;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_stream_arg {
  __u32 stream_id;
  __u32 pad64;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_get_3d_cap_arg {
  __u64 buffer;
  __u32 max_size;
  __u32 pad64;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define DRM_VMW_FENCE_FLAG_EXEC (1 << 0)
#define DRM_VMW_FENCE_FLAG_QUERY (1 << 1)
#define DRM_VMW_WAIT_OPTION_UNREF (1 << 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_fence_wait_arg {
  __u32 handle;
  __s32 cookie_valid;
  __u64 kernel_cookie;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 timeout_us;
  __s32 lazy;
  __s32 flags;
  __s32 wait_options;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s32 pad64;
};
struct drm_vmw_fence_signaled_arg {
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 flags;
  __s32 signaled;
  __u32 passed_seqno;
  __u32 signaled_flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 pad64;
};
struct drm_vmw_fence_arg {
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 pad64;
};
#define DRM_VMW_EVENT_FENCE_SIGNALED 0x80000000
struct drm_vmw_event_fence {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_event base;
  __u64 user_data;
  __u32 tv_sec;
  __u32 tv_usec;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define DRM_VMW_FE_FLAG_REQ_TIME (1 << 0)
struct drm_vmw_fence_event_arg {
  __u64 fence_rep;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 user_data;
  __u32 handle;
  __u32 flags;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_present_arg {
  __u32 fb_id;
  __u32 sid;
  __s32 dest_x;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s32 dest_y;
  __u64 clips_ptr;
  __u32 num_clips;
  __u32 pad64;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_vmw_present_readback_arg {
  __u32 fb_id;
  __u32 num_clips;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 clips_ptr;
  __u64 fence_rep;
};
struct drm_vmw_update_layout_arg {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 num_outputs;
  __u32 pad64;
  __u64 rects;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum drm_vmw_shader_type {
  drm_vmw_shader_type_vs = 0,
  drm_vmw_shader_type_ps,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_shader_create_arg {
  enum drm_vmw_shader_type shader_type;
  __u32 size;
  __u32 buffer_handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 shader_handle;
  __u64 offset;
};
struct drm_vmw_shader_arg {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 handle;
  __u32 pad64;
};
enum drm_vmw_surface_flags {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  drm_vmw_surface_flag_shareable = (1 << 0),
  drm_vmw_surface_flag_scanout = (1 << 1),
  drm_vmw_surface_flag_create_buffer = (1 << 2)
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_gb_surface_create_req {
  __u32 svga3d_flags;
  __u32 format;
  __u32 mip_levels;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  enum drm_vmw_surface_flags drm_surface_flags;
  __u32 multisample_count;
  __u32 autogen_filter;
  __u32 buffer_handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 array_size;
  struct drm_vmw_size base_size;
};
struct drm_vmw_gb_surface_create_rep {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 handle;
  __u32 backup_size;
  __u32 buffer_handle;
  __u32 buffer_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 buffer_map_handle;
};
union drm_vmw_gb_surface_create_arg {
  struct drm_vmw_gb_surface_create_rep rep;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_vmw_gb_surface_create_req req;
};
struct drm_vmw_gb_surface_ref_rep {
  struct drm_vmw_gb_surface_create_req creq;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_vmw_gb_surface_create_rep crep;
};
union drm_vmw_gb_surface_reference_arg {
  struct drm_vmw_gb_surface_ref_rep rep;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_vmw_surface_arg req;
};
enum drm_vmw_synccpu_flags {
  drm_vmw_synccpu_read = (1 << 0),
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  drm_vmw_synccpu_write = (1 << 1),
  drm_vmw_synccpu_dontblock = (1 << 2),
  drm_vmw_synccpu_allow_cs = (1 << 3)
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum drm_vmw_synccpu_op {
  drm_vmw_synccpu_grab,
  drm_vmw_synccpu_release
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_vmw_synccpu_arg {
  enum drm_vmw_synccpu_op op;
  enum drm_vmw_synccpu_flags flags;
  __u32 handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 pad64;
};
enum drm_vmw_extended_context {
  drm_vmw_context_legacy,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  drm_vmw_context_dx
};
union drm_vmw_extended_context_arg {
  enum drm_vmw_extended_context req;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_vmw_context_arg rep;
};
#ifdef __cplusplus
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

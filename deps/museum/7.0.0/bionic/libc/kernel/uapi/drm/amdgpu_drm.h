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
#ifndef __AMDGPU_DRM_H__
#define __AMDGPU_DRM_H__
#include "drm.h"
#define DRM_AMDGPU_GEM_CREATE 0x00
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_AMDGPU_GEM_MMAP 0x01
#define DRM_AMDGPU_CTX 0x02
#define DRM_AMDGPU_BO_LIST 0x03
#define DRM_AMDGPU_CS 0x04
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_AMDGPU_INFO 0x05
#define DRM_AMDGPU_GEM_METADATA 0x06
#define DRM_AMDGPU_GEM_WAIT_IDLE 0x07
#define DRM_AMDGPU_GEM_VA 0x08
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_AMDGPU_WAIT_CS 0x09
#define DRM_AMDGPU_GEM_OP 0x10
#define DRM_AMDGPU_GEM_USERPTR 0x11
#define DRM_IOCTL_AMDGPU_GEM_CREATE DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_GEM_CREATE, union drm_amdgpu_gem_create)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IOCTL_AMDGPU_GEM_MMAP DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_GEM_MMAP, union drm_amdgpu_gem_mmap)
#define DRM_IOCTL_AMDGPU_CTX DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_CTX, union drm_amdgpu_ctx)
#define DRM_IOCTL_AMDGPU_BO_LIST DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_BO_LIST, union drm_amdgpu_bo_list)
#define DRM_IOCTL_AMDGPU_CS DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_CS, union drm_amdgpu_cs)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IOCTL_AMDGPU_INFO DRM_IOW(DRM_COMMAND_BASE + DRM_AMDGPU_INFO, struct drm_amdgpu_info)
#define DRM_IOCTL_AMDGPU_GEM_METADATA DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_GEM_METADATA, struct drm_amdgpu_gem_metadata)
#define DRM_IOCTL_AMDGPU_GEM_WAIT_IDLE DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_GEM_WAIT_IDLE, union drm_amdgpu_gem_wait_idle)
#define DRM_IOCTL_AMDGPU_GEM_VA DRM_IOW(DRM_COMMAND_BASE + DRM_AMDGPU_GEM_VA, struct drm_amdgpu_gem_va)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IOCTL_AMDGPU_WAIT_CS DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_WAIT_CS, union drm_amdgpu_wait_cs)
#define DRM_IOCTL_AMDGPU_GEM_OP DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_GEM_OP, struct drm_amdgpu_gem_op)
#define DRM_IOCTL_AMDGPU_GEM_USERPTR DRM_IOWR(DRM_COMMAND_BASE + DRM_AMDGPU_GEM_USERPTR, struct drm_amdgpu_gem_userptr)
#define AMDGPU_GEM_DOMAIN_CPU 0x1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_GEM_DOMAIN_GTT 0x2
#define AMDGPU_GEM_DOMAIN_VRAM 0x4
#define AMDGPU_GEM_DOMAIN_GDS 0x8
#define AMDGPU_GEM_DOMAIN_GWS 0x10
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_GEM_DOMAIN_OA 0x20
#define AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED (1 << 0)
#define AMDGPU_GEM_CREATE_NO_CPU_ACCESS (1 << 1)
#define AMDGPU_GEM_CREATE_CPU_GTT_USWC (1 << 2)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_gem_create_in {
  uint64_t bo_size;
  uint64_t alignment;
  uint64_t domains;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint64_t domain_flags;
};
struct drm_amdgpu_gem_create_out {
  uint32_t handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t _pad;
};
union drm_amdgpu_gem_create {
  struct drm_amdgpu_gem_create_in in;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_amdgpu_gem_create_out out;
};
#define AMDGPU_BO_LIST_OP_CREATE 0
#define AMDGPU_BO_LIST_OP_DESTROY 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_BO_LIST_OP_UPDATE 2
struct drm_amdgpu_bo_list_in {
  uint32_t operation;
  uint32_t list_handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t bo_number;
  uint32_t bo_info_size;
  uint64_t bo_info_ptr;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_bo_list_entry {
  uint32_t bo_handle;
  uint32_t bo_priority;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_bo_list_out {
  uint32_t list_handle;
  uint32_t _pad;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
union drm_amdgpu_bo_list {
  struct drm_amdgpu_bo_list_in in;
  struct drm_amdgpu_bo_list_out out;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_CTX_OP_ALLOC_CTX 1
#define AMDGPU_CTX_OP_FREE_CTX 2
#define AMDGPU_CTX_OP_QUERY_STATE 3
#define AMDGPU_CTX_NO_RESET 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_CTX_GUILTY_RESET 1
#define AMDGPU_CTX_INNOCENT_RESET 2
#define AMDGPU_CTX_UNKNOWN_RESET 3
struct drm_amdgpu_ctx_in {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t op;
  uint32_t flags;
  uint32_t ctx_id;
  uint32_t _pad;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
union drm_amdgpu_ctx_out {
  struct {
    uint32_t ctx_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    uint32_t _pad;
  } alloc;
  struct {
    uint64_t flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    uint32_t hangs;
    uint32_t reset_status;
  } state;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
union drm_amdgpu_ctx {
  struct drm_amdgpu_ctx_in in;
  union drm_amdgpu_ctx_out out;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_GEM_USERPTR_READONLY (1 << 0)
#define AMDGPU_GEM_USERPTR_ANONONLY (1 << 1)
#define AMDGPU_GEM_USERPTR_VALIDATE (1 << 2)
#define AMDGPU_GEM_USERPTR_REGISTER (1 << 3)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_gem_userptr {
  uint64_t addr;
  uint64_t size;
  uint32_t flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t handle;
};
#define AMDGPU_TILING_ARRAY_MODE_SHIFT 0
#define AMDGPU_TILING_ARRAY_MODE_MASK 0xf
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_TILING_PIPE_CONFIG_SHIFT 4
#define AMDGPU_TILING_PIPE_CONFIG_MASK 0x1f
#define AMDGPU_TILING_TILE_SPLIT_SHIFT 9
#define AMDGPU_TILING_TILE_SPLIT_MASK 0x7
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_TILING_MICRO_TILE_MODE_SHIFT 12
#define AMDGPU_TILING_MICRO_TILE_MODE_MASK 0x7
#define AMDGPU_TILING_BANK_WIDTH_SHIFT 15
#define AMDGPU_TILING_BANK_WIDTH_MASK 0x3
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_TILING_BANK_HEIGHT_SHIFT 17
#define AMDGPU_TILING_BANK_HEIGHT_MASK 0x3
#define AMDGPU_TILING_MACRO_TILE_ASPECT_SHIFT 19
#define AMDGPU_TILING_MACRO_TILE_ASPECT_MASK 0x3
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_TILING_NUM_BANKS_SHIFT 21
#define AMDGPU_TILING_NUM_BANKS_MASK 0x3
#define AMDGPU_TILING_SET(field,value) (((value) & AMDGPU_TILING_ ##field ##_MASK) << AMDGPU_TILING_ ##field ##_SHIFT)
#define AMDGPU_TILING_GET(value,field) (((value) >> AMDGPU_TILING_ ##field ##_SHIFT) & AMDGPU_TILING_ ##field ##_MASK)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_GEM_METADATA_OP_SET_METADATA 1
#define AMDGPU_GEM_METADATA_OP_GET_METADATA 2
struct drm_amdgpu_gem_metadata {
  uint32_t handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t op;
  struct {
    uint64_t flags;
    uint64_t tiling_info;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    uint32_t data_size_bytes;
    uint32_t data[64];
  } data;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_gem_mmap_in {
  uint32_t handle;
  uint32_t _pad;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_gem_mmap_out {
  uint64_t addr_ptr;
};
union drm_amdgpu_gem_mmap {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_amdgpu_gem_mmap_in in;
  struct drm_amdgpu_gem_mmap_out out;
};
struct drm_amdgpu_gem_wait_idle_in {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t handle;
  uint32_t flags;
  uint64_t timeout;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_gem_wait_idle_out {
  uint32_t status;
  uint32_t domain;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
union drm_amdgpu_gem_wait_idle {
  struct drm_amdgpu_gem_wait_idle_in in;
  struct drm_amdgpu_gem_wait_idle_out out;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_wait_cs_in {
  uint64_t handle;
  uint64_t timeout;
  uint32_t ip_type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t ip_instance;
  uint32_t ring;
  uint32_t ctx_id;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_wait_cs_out {
  uint64_t status;
};
union drm_amdgpu_wait_cs {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_amdgpu_wait_cs_in in;
  struct drm_amdgpu_wait_cs_out out;
};
#define AMDGPU_GEM_OP_GET_GEM_CREATE_INFO 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_GEM_OP_SET_PLACEMENT 1
struct drm_amdgpu_gem_op {
  uint32_t handle;
  uint32_t op;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint64_t value;
};
#define AMDGPU_VA_OP_MAP 1
#define AMDGPU_VA_OP_UNMAP 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_VM_DELAY_UPDATE (1 << 0)
#define AMDGPU_VM_PAGE_READABLE (1 << 1)
#define AMDGPU_VM_PAGE_WRITEABLE (1 << 2)
#define AMDGPU_VM_PAGE_EXECUTABLE (1 << 3)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_gem_va {
  uint32_t handle;
  uint32_t _pad;
  uint32_t operation;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t flags;
  uint64_t va_address;
  uint64_t offset_in_bo;
  uint64_t map_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define AMDGPU_HW_IP_GFX 0
#define AMDGPU_HW_IP_COMPUTE 1
#define AMDGPU_HW_IP_DMA 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_HW_IP_UVD 3
#define AMDGPU_HW_IP_VCE 4
#define AMDGPU_HW_IP_NUM 5
#define AMDGPU_HW_IP_INSTANCE_MAX_COUNT 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_CHUNK_ID_IB 0x01
#define AMDGPU_CHUNK_ID_FENCE 0x02
#define AMDGPU_CHUNK_ID_DEPENDENCIES 0x03
struct drm_amdgpu_cs_chunk {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t chunk_id;
  uint32_t length_dw;
  uint64_t chunk_data;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_cs_in {
  uint32_t ctx_id;
  uint32_t bo_list_handle;
  uint32_t num_chunks;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t _pad;
  uint64_t chunks;
};
struct drm_amdgpu_cs_out {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint64_t handle;
};
union drm_amdgpu_cs {
  struct drm_amdgpu_cs_in in;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct drm_amdgpu_cs_out out;
};
#define AMDGPU_IB_FLAG_CE (1 << 0)
#define AMDGPU_IB_FLAG_PREAMBLE (1 << 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_cs_chunk_ib {
  uint32_t _pad;
  uint32_t flags;
  uint64_t va_start;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t ib_bytes;
  uint32_t ip_type;
  uint32_t ip_instance;
  uint32_t ring;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_amdgpu_cs_chunk_dep {
  uint32_t ip_type;
  uint32_t ip_instance;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t ring;
  uint32_t ctx_id;
  uint64_t handle;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_cs_chunk_fence {
  uint32_t handle;
  uint32_t offset;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_cs_chunk_data {
  union {
    struct drm_amdgpu_cs_chunk_ib ib_data;
    struct drm_amdgpu_cs_chunk_fence fence_data;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  };
};
#define AMDGPU_IDS_FLAGS_FUSION 0x1
#define AMDGPU_INFO_ACCEL_WORKING 0x00
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_INFO_CRTC_FROM_ID 0x01
#define AMDGPU_INFO_HW_IP_INFO 0x02
#define AMDGPU_INFO_HW_IP_COUNT 0x03
#define AMDGPU_INFO_TIMESTAMP 0x05
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_INFO_FW_VERSION 0x0e
#define AMDGPU_INFO_FW_VCE 0x1
#define AMDGPU_INFO_FW_UVD 0x2
#define AMDGPU_INFO_FW_GMC 0x03
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_INFO_FW_GFX_ME 0x04
#define AMDGPU_INFO_FW_GFX_PFP 0x05
#define AMDGPU_INFO_FW_GFX_CE 0x06
#define AMDGPU_INFO_FW_GFX_RLC 0x07
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_INFO_FW_GFX_MEC 0x08
#define AMDGPU_INFO_FW_SMC 0x0a
#define AMDGPU_INFO_FW_SDMA 0x0b
#define AMDGPU_INFO_NUM_BYTES_MOVED 0x0f
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_INFO_VRAM_USAGE 0x10
#define AMDGPU_INFO_GTT_USAGE 0x11
#define AMDGPU_INFO_GDS_CONFIG 0x13
#define AMDGPU_INFO_VRAM_GTT 0x14
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_INFO_READ_MMR_REG 0x15
#define AMDGPU_INFO_DEV_INFO 0x16
#define AMDGPU_INFO_VIS_VRAM_USAGE 0x17
#define AMDGPU_INFO_MMR_SE_INDEX_SHIFT 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_INFO_MMR_SE_INDEX_MASK 0xff
#define AMDGPU_INFO_MMR_SH_INDEX_SHIFT 8
#define AMDGPU_INFO_MMR_SH_INDEX_MASK 0xff
struct drm_amdgpu_info {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint64_t return_pointer;
  uint32_t return_size;
  uint32_t query;
  union {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    struct {
      uint32_t id;
      uint32_t _pad;
    } mode_crtc;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    struct {
      uint32_t type;
      uint32_t ip_instance;
    } query_hw_ip;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    struct {
      uint32_t dword_offset;
      uint32_t count;
      uint32_t instance;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      uint32_t flags;
    } read_mmr_reg;
    struct {
      uint32_t fw_type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      uint32_t ip_instance;
      uint32_t index;
      uint32_t _pad;
    } query_fw;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  };
};
struct drm_amdgpu_info_gds {
  uint32_t gds_gfx_partition_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t compute_partition_size;
  uint32_t gds_total_size;
  uint32_t gws_per_gfx_partition;
  uint32_t gws_per_compute_partition;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t oa_per_gfx_partition;
  uint32_t oa_per_compute_partition;
  uint32_t _pad;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct drm_amdgpu_info_vram_gtt {
  uint64_t vram_size;
  uint64_t vram_cpu_accessible_size;
  uint64_t gtt_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct drm_amdgpu_info_firmware {
  uint32_t ver;
  uint32_t feature;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define AMDGPU_VRAM_TYPE_UNKNOWN 0
#define AMDGPU_VRAM_TYPE_GDDR1 1
#define AMDGPU_VRAM_TYPE_DDR2 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_VRAM_TYPE_GDDR3 3
#define AMDGPU_VRAM_TYPE_GDDR4 4
#define AMDGPU_VRAM_TYPE_GDDR5 5
#define AMDGPU_VRAM_TYPE_HBM 6
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_VRAM_TYPE_DDR3 7
struct drm_amdgpu_info_device {
  uint32_t device_id;
  uint32_t chip_rev;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t external_rev;
  uint32_t pci_rev;
  uint32_t family;
  uint32_t num_shader_engines;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t num_shader_arrays_per_engine;
  uint32_t gpu_counter_freq;
  uint64_t max_engine_clock;
  uint64_t max_memory_clock;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t cu_active_number;
  uint32_t cu_ao_mask;
  uint32_t cu_bitmap[4][4];
  uint32_t enabled_rb_pipes_mask;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t num_rb_pipes;
  uint32_t num_hw_gfx_contexts;
  uint32_t _pad;
  uint64_t ids_flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint64_t virtual_address_offset;
  uint64_t virtual_address_max;
  uint32_t virtual_address_alignment;
  uint32_t pte_fragment_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t gart_page_size;
  uint32_t ce_ram_size;
  uint32_t vram_type;
  uint32_t vram_bit_width;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t vce_harvest_config;
};
struct drm_amdgpu_info_hw_ip {
  uint32_t hw_ip_version_major;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t hw_ip_version_minor;
  uint64_t capabilities_flags;
  uint32_t ib_start_alignment;
  uint32_t ib_size_alignment;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t available_rings;
  uint32_t _pad;
};
#define AMDGPU_FAMILY_UNKNOWN 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define AMDGPU_FAMILY_CI 120
#define AMDGPU_FAMILY_KV 125
#define AMDGPU_FAMILY_VI 130
#define AMDGPU_FAMILY_CZ 135
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

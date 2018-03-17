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
#ifndef _LINUX__HFI1_USER_H
#define _LINUX__HFI1_USER_H
#include <linux/types.h>
#define HFI1_USER_SWMAJOR 6
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_USER_SWMINOR 3
#define HFI1_SWMAJOR_SHIFT 16
#define HFI1_CAP_DMA_RTAIL (1UL << 0)
#define HFI1_CAP_SDMA (1UL << 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_CAP_SDMA_AHG (1UL << 2)
#define HFI1_CAP_EXTENDED_PSN (1UL << 3)
#define HFI1_CAP_HDRSUPP (1UL << 4)
#define HFI1_CAP_USE_SDMA_HEAD (1UL << 6)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_CAP_MULTI_PKT_EGR (1UL << 7)
#define HFI1_CAP_NODROP_RHQ_FULL (1UL << 8)
#define HFI1_CAP_NODROP_EGR_FULL (1UL << 9)
#define HFI1_CAP_TID_UNMAP (1UL << 10)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_CAP_PRINT_UNIMPL (1UL << 11)
#define HFI1_CAP_ALLOW_PERM_JKEY (1UL << 12)
#define HFI1_CAP_NO_INTEGRITY (1UL << 13)
#define HFI1_CAP_PKEY_CHECK (1UL << 14)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_CAP_STATIC_RATE_CTRL (1UL << 15)
#define HFI1_CAP_SDMA_HEAD_CHECK (1UL << 17)
#define HFI1_CAP_EARLY_CREDIT_RETURN (1UL << 18)
#define HFI1_RCVHDR_ENTSIZE_2 (1UL << 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_RCVHDR_ENTSIZE_16 (1UL << 1)
#define HFI1_RCVDHR_ENTSIZE_32 (1UL << 2)
#define HFI1_CMD_ASSIGN_CTXT 1
#define HFI1_CMD_CTXT_INFO 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_CMD_USER_INFO 3
#define HFI1_CMD_TID_UPDATE 4
#define HFI1_CMD_TID_FREE 5
#define HFI1_CMD_CREDIT_UPD 6
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_CMD_RECV_CTRL 8
#define HFI1_CMD_POLL_TYPE 9
#define HFI1_CMD_ACK_EVENT 10
#define HFI1_CMD_SET_PKEY 11
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_CMD_CTXT_RESET 12
#define HFI1_CMD_TID_INVAL_READ 13
#define HFI1_CMD_GET_VERS 14
#define IB_IOCTL_MAGIC 0x1b
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define __NUM(cmd) (HFI1_CMD_ ##cmd + 0xe0)
struct hfi1_cmd;
#define HFI1_IOCTL_ASSIGN_CTXT _IOWR(IB_IOCTL_MAGIC, __NUM(ASSIGN_CTXT), struct hfi1_user_info)
#define HFI1_IOCTL_CTXT_INFO _IOW(IB_IOCTL_MAGIC, __NUM(CTXT_INFO), struct hfi1_ctxt_info)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_IOCTL_USER_INFO _IOW(IB_IOCTL_MAGIC, __NUM(USER_INFO), struct hfi1_base_info)
#define HFI1_IOCTL_TID_UPDATE _IOWR(IB_IOCTL_MAGIC, __NUM(TID_UPDATE), struct hfi1_tid_info)
#define HFI1_IOCTL_TID_FREE _IOWR(IB_IOCTL_MAGIC, __NUM(TID_FREE), struct hfi1_tid_info)
#define HFI1_IOCTL_CREDIT_UPD _IO(IB_IOCTL_MAGIC, __NUM(CREDIT_UPD))
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_IOCTL_RECV_CTRL _IOW(IB_IOCTL_MAGIC, __NUM(RECV_CTRL), int)
#define HFI1_IOCTL_POLL_TYPE _IOW(IB_IOCTL_MAGIC, __NUM(POLL_TYPE), int)
#define HFI1_IOCTL_ACK_EVENT _IOW(IB_IOCTL_MAGIC, __NUM(ACK_EVENT), unsigned long)
#define HFI1_IOCTL_SET_PKEY _IOW(IB_IOCTL_MAGIC, __NUM(SET_PKEY), __u16)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_IOCTL_CTXT_RESET _IO(IB_IOCTL_MAGIC, __NUM(CTXT_RESET))
#define HFI1_IOCTL_TID_INVAL_READ _IOWR(IB_IOCTL_MAGIC, __NUM(TID_INVAL_READ), struct hfi1_tid_info)
#define HFI1_IOCTL_GET_VERS _IOR(IB_IOCTL_MAGIC, __NUM(GET_VERS), int)
#define _HFI1_EVENT_FROZEN_BIT 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define _HFI1_EVENT_LINKDOWN_BIT 1
#define _HFI1_EVENT_LID_CHANGE_BIT 2
#define _HFI1_EVENT_LMC_CHANGE_BIT 3
#define _HFI1_EVENT_SL2VL_CHANGE_BIT 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define _HFI1_EVENT_TID_MMU_NOTIFY_BIT 5
#define _HFI1_MAX_EVENT_BIT _HFI1_EVENT_TID_MMU_NOTIFY_BIT
#define HFI1_EVENT_FROZEN (1UL << _HFI1_EVENT_FROZEN_BIT)
#define HFI1_EVENT_LINKDOWN (1UL << _HFI1_EVENT_LINKDOWN_BIT)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_EVENT_LID_CHANGE (1UL << _HFI1_EVENT_LID_CHANGE_BIT)
#define HFI1_EVENT_LMC_CHANGE (1UL << _HFI1_EVENT_LMC_CHANGE_BIT)
#define HFI1_EVENT_SL2VL_CHANGE (1UL << _HFI1_EVENT_SL2VL_CHANGE_BIT)
#define HFI1_EVENT_TID_MMU_NOTIFY (1UL << _HFI1_EVENT_TID_MMU_NOTIFY_BIT)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_STATUS_INITTED 0x1
#define HFI1_STATUS_CHIP_PRESENT 0x20
#define HFI1_STATUS_IB_READY 0x40
#define HFI1_STATUS_IB_CONF 0x80
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_STATUS_HWERROR 0x200
#define HFI1_MAX_SHARED_CTXTS 8
#define HFI1_POLL_TYPE_ANYRCV 0x0
#define HFI1_POLL_TYPE_URGENT 0x1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct hfi1_user_info {
  __u32 userversion;
  __u32 pad;
  __u16 subctxt_cnt;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 subctxt_id;
  __u8 uuid[16];
};
struct hfi1_ctxt_info {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 runtime_flags;
  __u32 rcvegr_size;
  __u16 num_active;
  __u16 unit;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 ctxt;
  __u16 subctxt;
  __u16 rcvtids;
  __u16 credits;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 numa_node;
  __u16 rec_cpu;
  __u16 send_ctxt;
  __u16 egrtids;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 rcvhdrq_cnt;
  __u16 rcvhdrq_entsize;
  __u16 sdma_ring_size;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct hfi1_tid_info {
  __u64 vaddr;
  __u64 tidlist;
  __u32 tidcnt;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 length;
};
enum hfi1_sdma_comp_state {
  FREE = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  QUEUED,
  COMPLETE,
  ERROR
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct hfi1_sdma_comp_entry {
  __u32 status;
  __u32 errcode;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct hfi1_status {
  __u64 dev;
  __u64 port;
  char freezemsg[0];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct hfi1_base_info {
  __u32 hw_version;
  __u32 sw_version;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 jkey;
  __u16 padding1;
  __u32 bthqp;
  __u64 sc_credits_addr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 pio_bufbase_sop;
  __u64 pio_bufbase;
  __u64 rcvhdr_bufbase;
  __u64 rcvegr_bufbase;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 sdma_comp_bufbase;
  __u64 user_regbase;
  __u64 events_bufbase;
  __u64 status_bufbase;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 rcvhdrtail_base;
  __u64 subctxt_uregbase;
  __u64 subctxt_rcvegrbuf;
  __u64 subctxt_rcvhdrbuf;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum sdma_req_opcode {
  EXPECTED = 0,
  EAGER
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define HFI1_SDMA_REQ_VERSION_MASK 0xF
#define HFI1_SDMA_REQ_VERSION_SHIFT 0x0
#define HFI1_SDMA_REQ_OPCODE_MASK 0xF
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HFI1_SDMA_REQ_OPCODE_SHIFT 0x4
#define HFI1_SDMA_REQ_IOVCNT_MASK 0xFF
#define HFI1_SDMA_REQ_IOVCNT_SHIFT 0x8
struct sdma_req_info {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 ctrl;
  __u16 npkts;
  __u16 fragsize;
  __u16 comp_idx;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __packed;
struct hfi1_kdeth_header {
  __le32 ver_tid_offset;
  __le16 jkey;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le16 hcrc;
  __le32 swdata[7];
} __packed;
struct hfi1_pkt_header {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le16 pbc[4];
  __be16 lrh[4];
  __be32 bth[3];
  struct hfi1_kdeth_header kdeth;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __packed;
enum hfi1_ureg {
  ur_rcvhdrtail = 0,
  ur_rcvhdrhead = 1,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  ur_rcvegrindextail = 2,
  ur_rcvegrindexhead = 3,
  ur_rcvegroffsettail = 4,
  ur_maxreg,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  ur_rcvtidflowtable = 256
};
#endif

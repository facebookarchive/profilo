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
#ifndef _IPX_H_
#define _IPX_H_
#include <linux/libc-compat.h>
#include <linux/types.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <linux/sockios.h>
#include <linux/socket.h>
#define IPX_NODE_LEN 6
#define IPX_MTU 576
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#if __UAPI_DEF_SOCKADDR_IPX
struct sockaddr_ipx {
  __kernel_sa_family_t sipx_family;
  __be16 sipx_port;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __be32 sipx_network;
  unsigned char sipx_node[IPX_NODE_LEN];
  __u8 sipx_type;
  unsigned char sipx_zero;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#endif
#define sipx_special sipx_port
#define sipx_action sipx_zero
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPX_DLTITF 0
#define IPX_CRTITF 1
#if __UAPI_DEF_IPX_ROUTE_DEFINITION
struct ipx_route_definition {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __be32 ipx_network;
  __be32 ipx_router_network;
  unsigned char ipx_router_node[IPX_NODE_LEN];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#if __UAPI_DEF_IPX_INTERFACE_DEFINITION
struct ipx_interface_definition {
  __be32 ipx_network;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned char ipx_device[16];
  unsigned char ipx_dlink_type;
#define IPX_FRAME_NONE 0
#define IPX_FRAME_SNAP 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPX_FRAME_8022 2
#define IPX_FRAME_ETHERII 3
#define IPX_FRAME_8023 4
#define IPX_FRAME_TR_8022 5
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned char ipx_special;
#define IPX_SPECIAL_NONE 0
#define IPX_PRIMARY 1
#define IPX_INTERNAL 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned char ipx_node[IPX_NODE_LEN];
};
#endif
#if __UAPI_DEF_IPX_CONFIG_DATA
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct ipx_config_data {
  unsigned char ipxcfg_auto_select_primary;
  unsigned char ipxcfg_auto_create_interfaces;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#if __UAPI_DEF_IPX_ROUTE_DEF
struct ipx_route_def {
  __be32 ipx_network;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __be32 ipx_router_network;
#define IPX_ROUTE_NO_ROUTER 0
  unsigned char ipx_router_node[IPX_NODE_LEN];
  unsigned char ipx_device[16];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned short ipx_flags;
#define IPX_RT_SNAP 8
#define IPX_RT_8022 4
#define IPX_RT_BLUEBOOK 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPX_RT_ROUTED 1
};
#endif
#define SIOCAIPXITFCRT (SIOCPROTOPRIVATE)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIOCAIPXPRISLT (SIOCPROTOPRIVATE + 1)
#define SIOCIPXCFGDATA (SIOCPROTOPRIVATE + 2)
#define SIOCIPXNCPCONN (SIOCPROTOPRIVATE + 3)
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */

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
#ifndef _UAPI__SOUND_USB_STREAM_H
#define _UAPI__SOUND_USB_STREAM_H
#define USB_STREAM_INTERFACE_VERSION 2
#define SNDRV_USB_STREAM_IOCTL_SET_PARAMS _IOW('H', 0x90, struct usb_stream_config)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct usb_stream_packet {
  unsigned offset;
  unsigned length;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct usb_stream_config {
  unsigned version;
  unsigned sample_rate;
  unsigned period_frames;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned frame_size;
};
struct usb_stream {
  struct usb_stream_config cfg;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned read_size;
  unsigned write_size;
  int period_size;
  unsigned state;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int idle_insize;
  int idle_outsize;
  int sync_packet;
  unsigned insize_done;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned periods_done;
  unsigned periods_polled;
  struct usb_stream_packet outpacket[2];
  unsigned inpackets;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned inpacket_head;
  unsigned inpacket_split;
  unsigned inpacket_split_at;
  unsigned next_inpacket_split;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned next_inpacket_split_at;
  struct usb_stream_packet inpacket[0];
};
enum usb_stream_state {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  usb_stream_invalid,
  usb_stream_stopped,
  usb_stream_sync0,
  usb_stream_sync1,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  usb_stream_ready,
  usb_stream_running,
  usb_stream_xrun,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

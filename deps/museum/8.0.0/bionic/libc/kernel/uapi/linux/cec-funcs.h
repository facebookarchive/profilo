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
#ifndef _CEC_UAPI_FUNCS_H
#define _CEC_UAPI_FUNCS_H
#include <linux/cec.h>
struct cec_op_arib_data {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 transport_id;
  __u16 service_id;
  __u16 orig_network_id;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct cec_op_atsc_data {
  __u16 transport_id;
  __u16 program_number;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct cec_op_dvb_data {
  __u16 transport_id;
  __u16 service_id;
  __u16 orig_network_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct cec_op_channel_data {
  __u8 channel_number_fmt;
  __u16 major;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 minor;
};
struct cec_op_digital_service_id {
  __u8 service_id_method;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u8 dig_bcast_system;
  union {
    struct cec_op_arib_data arib;
    struct cec_op_atsc_data atsc;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    struct cec_op_dvb_data dvb;
    struct cec_op_channel_data channel;
  };
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct cec_op_record_src {
  __u8 type;
  union {
    struct cec_op_digital_service_id digital;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    struct {
      __u8 ana_bcast_type;
      __u16 ana_freq;
      __u8 bcast_system;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    } analog;
    struct {
      __u8 plug;
    } ext_plug;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    struct {
      __u16 phys_addr;
    } ext_phys_addr;
  };
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct cec_op_tuner_device_info {
  __u8 rec_flag;
  __u8 tuner_display_info;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u8 is_analog;
  union {
    struct cec_op_digital_service_id digital;
    struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      __u8 ana_bcast_type;
      __u16 ana_freq;
      __u8 bcast_system;
    } analog;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  };
};
struct cec_op_ui_command {
  __u8 ui_cmd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u8 has_opt_arg;
  union {
    struct cec_op_channel_data channel_identifier;
    __u8 ui_broadcast_type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    __u8 ui_sound_presentation_control;
    __u8 play_mode;
    __u8 ui_function_media;
    __u8 ui_function_select_av_input;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    __u8 ui_function_select_audio_input;
  };
};
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */

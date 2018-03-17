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
#ifndef _UAPI_INPUT_H
#define _UAPI_INPUT_H
#include <sys/time.h>
#include <sys/ioctl.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <sys/types.h>
#include <linux/types.h>
#include "input-event-codes.h"
struct input_event {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct timeval time;
  __u16 type;
  __u16 code;
  __s32 value;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define EV_VERSION 0x010001
struct input_id {
  __u16 bustype;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 vendor;
  __u16 product;
  __u16 version;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct input_absinfo {
  __s32 value;
  __s32 minimum;
  __s32 maximum;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s32 fuzz;
  __s32 flat;
  __s32 resolution;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct input_keymap_entry {
#define INPUT_KEYMAP_BY_INDEX (1 << 0)
  __u8 flags;
  __u8 len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 index;
  __u32 keycode;
  __u8 scancode[32];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct input_mask {
  __u32 type;
  __u32 codes_size;
  __u64 codes_ptr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define EVIOCGVERSION _IOR('E', 0x01, int)
#define EVIOCGID _IOR('E', 0x02, struct input_id)
#define EVIOCGREP _IOR('E', 0x03, unsigned int[2])
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EVIOCSREP _IOW('E', 0x03, unsigned int[2])
#define EVIOCGKEYCODE _IOR('E', 0x04, unsigned int[2])
#define EVIOCGKEYCODE_V2 _IOR('E', 0x04, struct input_keymap_entry)
#define EVIOCSKEYCODE _IOW('E', 0x04, unsigned int[2])
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EVIOCSKEYCODE_V2 _IOW('E', 0x04, struct input_keymap_entry)
#define EVIOCGNAME(len) _IOC(_IOC_READ, 'E', 0x06, len)
#define EVIOCGPHYS(len) _IOC(_IOC_READ, 'E', 0x07, len)
#define EVIOCGUNIQ(len) _IOC(_IOC_READ, 'E', 0x08, len)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EVIOCGPROP(len) _IOC(_IOC_READ, 'E', 0x09, len)
#define EVIOCGMTSLOTS(len) _IOC(_IOC_READ, 'E', 0x0a, len)
#define EVIOCGKEY(len) _IOC(_IOC_READ, 'E', 0x18, len)
#define EVIOCGLED(len) _IOC(_IOC_READ, 'E', 0x19, len)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EVIOCGSND(len) _IOC(_IOC_READ, 'E', 0x1a, len)
#define EVIOCGSW(len) _IOC(_IOC_READ, 'E', 0x1b, len)
#define EVIOCGBIT(ev,len) _IOC(_IOC_READ, 'E', 0x20 + (ev), len)
#define EVIOCGABS(abs) _IOR('E', 0x40 + (abs), struct input_absinfo)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EVIOCSABS(abs) _IOW('E', 0xc0 + (abs), struct input_absinfo)
#define EVIOCSFF _IOW('E', 0x80, struct ff_effect)
#define EVIOCRMFF _IOW('E', 0x81, int)
#define EVIOCGEFFECTS _IOR('E', 0x84, int)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EVIOCGRAB _IOW('E', 0x90, int)
#define EVIOCREVOKE _IOW('E', 0x91, int)
#define EVIOCGMASK _IOR('E', 0x92, struct input_mask)
#define EVIOCSMASK _IOW('E', 0x93, struct input_mask)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EVIOCSCLOCKID _IOW('E', 0xa0, int)
#define ID_BUS 0
#define ID_VENDOR 1
#define ID_PRODUCT 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ID_VERSION 3
#define BUS_PCI 0x01
#define BUS_ISAPNP 0x02
#define BUS_USB 0x03
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BUS_HIL 0x04
#define BUS_BLUETOOTH 0x05
#define BUS_VIRTUAL 0x06
#define BUS_ISA 0x10
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BUS_I8042 0x11
#define BUS_XTKBD 0x12
#define BUS_RS232 0x13
#define BUS_GAMEPORT 0x14
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BUS_PARPORT 0x15
#define BUS_AMIGA 0x16
#define BUS_ADB 0x17
#define BUS_I2C 0x18
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BUS_HOST 0x19
#define BUS_GSC 0x1A
#define BUS_ATARI 0x1B
#define BUS_SPI 0x1C
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BUS_RMI 0x1D
#define BUS_CEC 0x1E
#define BUS_INTEL_ISHTP 0x1F
#define MT_TOOL_FINGER 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MT_TOOL_PEN 1
#define MT_TOOL_PALM 2
#define MT_TOOL_MAX 2
#define FF_STATUS_STOPPED 0x00
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define FF_STATUS_PLAYING 0x01
#define FF_STATUS_MAX 0x01
struct ff_replay {
  __u16 length;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 delay;
};
struct ff_trigger {
  __u16 button;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 interval;
};
struct ff_envelope {
  __u16 attack_length;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 attack_level;
  __u16 fade_length;
  __u16 fade_level;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct ff_constant_effect {
  __s16 level;
  struct ff_envelope envelope;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct ff_ramp_effect {
  __s16 start_level;
  __s16 end_level;
  struct ff_envelope envelope;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct ff_condition_effect {
  __u16 right_saturation;
  __u16 left_saturation;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s16 right_coeff;
  __s16 left_coeff;
  __u16 deadband;
  __s16 center;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct ff_periodic_effect {
  __u16 waveform;
  __u16 period;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s16 magnitude;
  __s16 offset;
  __u16 phase;
  struct ff_envelope envelope;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 custom_len;
  __s16 __user * custom_data;
};
struct ff_rumble_effect {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 strong_magnitude;
  __u16 weak_magnitude;
};
struct ff_effect {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 type;
  __s16 id;
  __u16 direction;
  struct ff_trigger trigger;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct ff_replay replay;
  union {
    struct ff_constant_effect constant;
    struct ff_ramp_effect ramp;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    struct ff_periodic_effect periodic;
    struct ff_condition_effect condition[2];
    struct ff_rumble_effect rumble;
  } u;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define FF_RUMBLE 0x50
#define FF_PERIODIC 0x51
#define FF_CONSTANT 0x52
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define FF_SPRING 0x53
#define FF_FRICTION 0x54
#define FF_DAMPER 0x55
#define FF_INERTIA 0x56
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define FF_RAMP 0x57
#define FF_EFFECT_MIN FF_RUMBLE
#define FF_EFFECT_MAX FF_RAMP
#define FF_SQUARE 0x58
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define FF_TRIANGLE 0x59
#define FF_SINE 0x5a
#define FF_SAW_UP 0x5b
#define FF_SAW_DOWN 0x5c
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define FF_CUSTOM 0x5d
#define FF_WAVEFORM_MIN FF_SQUARE
#define FF_WAVEFORM_MAX FF_CUSTOM
#define FF_GAIN 0x60
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define FF_AUTOCENTER 0x61
#define FF_MAX_EFFECTS FF_GAIN
#define FF_MAX 0x7f
#define FF_CNT (FF_MAX + 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

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
#ifndef _UAPI_DVBVIDEO_H_
#define _UAPI_DVBVIDEO_H_
#include <linux/types.h>
#include <time.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef enum {
  VIDEO_FORMAT_4_3,
  VIDEO_FORMAT_16_9,
  VIDEO_FORMAT_221_1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} video_format_t;
typedef enum {
  VIDEO_SYSTEM_PAL,
  VIDEO_SYSTEM_NTSC,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  VIDEO_SYSTEM_PALN,
  VIDEO_SYSTEM_PALNc,
  VIDEO_SYSTEM_PALM,
  VIDEO_SYSTEM_NTSC60,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  VIDEO_SYSTEM_PAL60,
  VIDEO_SYSTEM_PALM60
} video_system_t;
typedef enum {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  VIDEO_PAN_SCAN,
  VIDEO_LETTER_BOX,
  VIDEO_CENTER_CUT_OUT
} video_displayformat_t;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef struct {
  int w;
  int h;
  video_format_t aspect_ratio;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} video_size_t;
typedef enum {
  VIDEO_SOURCE_DEMUX,
  VIDEO_SOURCE_MEMORY
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} video_stream_source_t;
typedef enum {
  VIDEO_STOPPED,
  VIDEO_PLAYING,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  VIDEO_FREEZED
} video_play_state_t;
#define VIDEO_CMD_PLAY (0)
#define VIDEO_CMD_STOP (1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_CMD_FREEZE (2)
#define VIDEO_CMD_CONTINUE (3)
#define VIDEO_CMD_FREEZE_TO_BLACK (1 << 0)
#define VIDEO_CMD_STOP_TO_BLACK (1 << 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_CMD_STOP_IMMEDIATELY (1 << 1)
#define VIDEO_PLAY_FMT_NONE (0)
#define VIDEO_PLAY_FMT_GOP (1)
struct video_command {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 cmd;
  __u32 flags;
  union {
    struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      __u64 pts;
    } stop;
    struct {
      __s32 speed;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      __u32 format;
    } play;
    struct {
      __u32 data[16];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    } raw;
  };
};
#define VIDEO_VSYNC_FIELD_UNKNOWN (0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_VSYNC_FIELD_ODD (1)
#define VIDEO_VSYNC_FIELD_EVEN (2)
#define VIDEO_VSYNC_FIELD_PROGRESSIVE (3)
struct video_event {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s32 type;
#define VIDEO_EVENT_SIZE_CHANGED 1
#define VIDEO_EVENT_FRAME_RATE_CHANGED 2
#define VIDEO_EVENT_DECODER_STOPPED 3
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_EVENT_VSYNC 4
  __kernel_time_t timestamp;
  union {
    video_size_t size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    unsigned int frame_rate;
    unsigned char vsync_field;
  } u;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct video_status {
  int video_blank;
  video_play_state_t play_state;
  video_stream_source_t stream_source;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  video_format_t video_format;
  video_displayformat_t display_format;
};
struct video_still_picture {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char __user * iFrame;
  __s32 size;
};
typedef struct video_highlight {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int active;
  __u8 contrast1;
  __u8 contrast2;
  __u8 color1;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u8 color2;
  __u32 ypos;
  __u32 xpos;
} video_highlight_t;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef struct video_spu {
  int active;
  int stream_id;
} video_spu_t;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef struct video_spu_palette {
  int length;
  __u8 __user * palette;
} video_spu_palette_t;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef struct video_navi_pack {
  int length;
  __u8 data[1024];
} video_navi_pack_t;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef __u16 video_attributes_t;
#define VIDEO_CAP_MPEG1 1
#define VIDEO_CAP_MPEG2 2
#define VIDEO_CAP_SYS 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_CAP_PROG 8
#define VIDEO_CAP_SPU 16
#define VIDEO_CAP_NAVI 32
#define VIDEO_CAP_CSS 64
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_STOP _IO('o', 21)
#define VIDEO_PLAY _IO('o', 22)
#define VIDEO_FREEZE _IO('o', 23)
#define VIDEO_CONTINUE _IO('o', 24)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_SELECT_SOURCE _IO('o', 25)
#define VIDEO_SET_BLANK _IO('o', 26)
#define VIDEO_GET_STATUS _IOR('o', 27, struct video_status)
#define VIDEO_GET_EVENT _IOR('o', 28, struct video_event)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_SET_DISPLAY_FORMAT _IO('o', 29)
#define VIDEO_STILLPICTURE _IOW('o', 30, struct video_still_picture)
#define VIDEO_FAST_FORWARD _IO('o', 31)
#define VIDEO_SLOWMOTION _IO('o', 32)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_GET_CAPABILITIES _IOR('o', 33, unsigned int)
#define VIDEO_CLEAR_BUFFER _IO('o', 34)
#define VIDEO_SET_ID _IO('o', 35)
#define VIDEO_SET_STREAMTYPE _IO('o', 36)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_SET_FORMAT _IO('o', 37)
#define VIDEO_SET_SYSTEM _IO('o', 38)
#define VIDEO_SET_HIGHLIGHT _IOW('o', 39, video_highlight_t)
#define VIDEO_SET_SPU _IOW('o', 50, video_spu_t)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_SET_SPU_PALETTE _IOW('o', 51, video_spu_palette_t)
#define VIDEO_GET_NAVI _IOR('o', 52, video_navi_pack_t)
#define VIDEO_SET_ATTRIBUTES _IO('o', 53)
#define VIDEO_GET_SIZE _IOR('o', 55, video_size_t)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_GET_FRAME_RATE _IOR('o', 56, unsigned int)
#define VIDEO_GET_PTS _IOR('o', 57, __u64)
#define VIDEO_GET_FRAME_COUNT _IOR('o', 58, __u64)
#define VIDEO_COMMAND _IOWR('o', 59, struct video_command)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDEO_TRY_COMMAND _IOWR('o', 60, struct video_command)
#endif

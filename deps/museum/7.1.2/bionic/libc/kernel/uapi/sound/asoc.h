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
#ifndef __LINUX_UAPI_SND_ASOC_H
#define __LINUX_UAPI_SND_ASOC_H
#include <linux/types.h>
#include <sound/asound.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#error This API is an early revision and not enabled in the current
#error kernel release , it will be enabled in a future kernel version
#error with incompatible changes to what is here .
#define SND_SOC_TPLG_MAX_CHAN 8
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_MAX_FORMATS 16
#define SND_SOC_TPLG_STREAM_CONFIG_MAX 8
#define SND_SOC_TPLG_CTL_VOLSW 1
#define SND_SOC_TPLG_CTL_VOLSW_SX 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_CTL_VOLSW_XR_SX 3
#define SND_SOC_TPLG_CTL_ENUM 4
#define SND_SOC_TPLG_CTL_BYTES 5
#define SND_SOC_TPLG_CTL_ENUM_VALUE 6
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_CTL_RANGE 7
#define SND_SOC_TPLG_CTL_STROBE 8
#define SND_SOC_TPLG_DAPM_CTL_VOLSW 64
#define SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE 65
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT 66
#define SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE 67
#define SND_SOC_TPLG_DAPM_CTL_PIN 68
#define SND_SOC_TPLG_DAPM_INPUT 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_DAPM_OUTPUT 1
#define SND_SOC_TPLG_DAPM_MUX 2
#define SND_SOC_TPLG_DAPM_MIXER 3
#define SND_SOC_TPLG_DAPM_PGA 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_DAPM_OUT_DRV 5
#define SND_SOC_TPLG_DAPM_ADC 6
#define SND_SOC_TPLG_DAPM_DAC 7
#define SND_SOC_TPLG_DAPM_SWITCH 8
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_DAPM_PRE 9
#define SND_SOC_TPLG_DAPM_POST 10
#define SND_SOC_TPLG_DAPM_AIF_IN 11
#define SND_SOC_TPLG_DAPM_AIF_OUT 12
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_DAPM_DAI_IN 13
#define SND_SOC_TPLG_DAPM_DAI_OUT 14
#define SND_SOC_TPLG_DAPM_DAI_LINK 15
#define SND_SOC_TPLG_DAPM_LAST SND_SOC_TPLG_DAPM_DAI_LINK
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_MAGIC 0x41536F43
#define SND_SOC_TPLG_NUM_TEXTS 16
#define SND_SOC_TPLG_ABI_VERSION 0x4
#define SND_SOC_TPLG_TLV_SIZE 32
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_TYPE_MIXER 1
#define SND_SOC_TPLG_TYPE_BYTES 2
#define SND_SOC_TPLG_TYPE_ENUM 3
#define SND_SOC_TPLG_TYPE_DAPM_GRAPH 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_TYPE_DAPM_WIDGET 5
#define SND_SOC_TPLG_TYPE_DAI_LINK 6
#define SND_SOC_TPLG_TYPE_PCM 7
#define SND_SOC_TPLG_TYPE_MANIFEST 8
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_TYPE_CODEC_LINK 9
#define SND_SOC_TPLG_TYPE_BACKEND_LINK 10
#define SND_SOC_TPLG_TYPE_PDATA 11
#define SND_SOC_TPLG_TYPE_MAX SND_SOC_TPLG_TYPE_PDATA
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_TYPE_VENDOR_FW 1000
#define SND_SOC_TPLG_TYPE_VENDOR_CONFIG 1001
#define SND_SOC_TPLG_TYPE_VENDOR_COEFF 1002
#define SND_SOC_TPLG_TYPEVENDOR_CODEC 1003
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SND_SOC_TPLG_STREAM_PLAYBACK 0
#define SND_SOC_TPLG_STREAM_CAPTURE 1
struct snd_soc_tplg_hdr {
  __le32 magic;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 abi;
  __le32 version;
  __le32 type;
  __le32 size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 vendor_type;
  __le32 payload_size;
  __le32 index;
  __le32 count;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct snd_soc_tplg_private {
  __le32 size;
  char data[0];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct snd_soc_tplg_tlv_dbscale {
  __le32 min;
  __le32 step;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 mute;
} __attribute__((packed));
struct snd_soc_tplg_ctl_tlv {
  __le32 size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 type;
  union {
    __le32 data[SND_SOC_TPLG_TLV_SIZE];
    struct snd_soc_tplg_tlv_dbscale scale;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  };
} __attribute__((packed));
struct snd_soc_tplg_channel {
  __le32 size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 reg;
  __le32 shift;
  __le32 id;
} __attribute__((packed));
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct snd_soc_tplg_io_ops {
  __le32 get;
  __le32 put;
  __le32 info;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct snd_soc_tplg_ctl_hdr {
  __le32 size;
  __le32 type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
  __le32 access;
  struct snd_soc_tplg_io_ops ops;
  struct snd_soc_tplg_ctl_tlv tlv;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct snd_soc_tplg_stream_caps {
  __le32 size;
  char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le64 formats;
  __le32 rates;
  __le32 rate_min;
  __le32 rate_max;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 channels_min;
  __le32 channels_max;
  __le32 periods_min;
  __le32 periods_max;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 period_size_min;
  __le32 period_size_max;
  __le32 buffer_size_min;
  __le32 buffer_size_max;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct snd_soc_tplg_stream {
  __le32 size;
  char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le64 format;
  __le32 rate;
  __le32 period_bytes;
  __le32 buffer_bytes;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 channels;
} __attribute__((packed));
struct snd_soc_tplg_manifest {
  __le32 size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 control_elems;
  __le32 widget_elems;
  __le32 graph_elems;
  __le32 dai_elems;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 dai_link_elems;
  struct snd_soc_tplg_private priv;
} __attribute__((packed));
struct snd_soc_tplg_mixer_control {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct snd_soc_tplg_ctl_hdr hdr;
  __le32 size;
  __le32 min;
  __le32 max;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 platform_max;
  __le32 invert;
  __le32 num_channels;
  struct snd_soc_tplg_channel channel[SND_SOC_TPLG_MAX_CHAN];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct snd_soc_tplg_private priv;
} __attribute__((packed));
struct snd_soc_tplg_enum_control {
  struct snd_soc_tplg_ctl_hdr hdr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 size;
  __le32 num_channels;
  struct snd_soc_tplg_channel channel[SND_SOC_TPLG_MAX_CHAN];
  __le32 items;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 mask;
  __le32 count;
  char texts[SND_SOC_TPLG_NUM_TEXTS][SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
  __le32 values[SND_SOC_TPLG_NUM_TEXTS * SNDRV_CTL_ELEM_ID_NAME_MAXLEN / 4];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct snd_soc_tplg_private priv;
} __attribute__((packed));
struct snd_soc_tplg_bytes_control {
  struct snd_soc_tplg_ctl_hdr hdr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 size;
  __le32 max;
  __le32 mask;
  __le32 base;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 num_regs;
  struct snd_soc_tplg_io_ops ext_ops;
  struct snd_soc_tplg_private priv;
} __attribute__((packed));
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct snd_soc_tplg_dapm_graph_elem {
  char sink[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
  char control[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
  char source[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct snd_soc_tplg_dapm_widget {
  __le32 size;
  __le32 id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
  char sname[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
  __le32 reg;
  __le32 shift;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 mask;
  __le32 subseq;
  __le32 invert;
  __le32 ignore_suspend;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le16 event_flags;
  __le16 event_type;
  __le32 num_kcontrols;
  struct snd_soc_tplg_private priv;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct snd_soc_tplg_pcm {
  __le32 size;
  char pcm_name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char dai_name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
  __le32 pcm_id;
  __le32 dai_id;
  __le32 playback;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 capture;
  __le32 compress;
  struct snd_soc_tplg_stream stream[SND_SOC_TPLG_STREAM_CONFIG_MAX];
  __le32 num_streams;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct snd_soc_tplg_stream_caps caps[2];
} __attribute__((packed));
struct snd_soc_tplg_link_config {
  __le32 size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __le32 id;
  struct snd_soc_tplg_stream stream[SND_SOC_TPLG_STREAM_CONFIG_MAX];
  __le32 num_streams;
} __attribute__((packed));
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

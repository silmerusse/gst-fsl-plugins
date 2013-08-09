/*
* Copyright (c) 2011-2012, Freescale Semiconductor, Inc. All rights reserved.
*
*/

/*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


/*
* Module Name:    vpuenc.h
*
* Description:    Head file of VPU-based video encoder gstreamer plugin
*
* Portability:    This code is written for Linux OS and Gstreamer
*/

/*
* Changelog: 
*
*/

#ifndef __VPUENC_H__
#define __VPUENC_H__

#include <gst/gst.h>
#include <gst/tag/tag.h>

#include "mfw_gst_ts.h"
#include "gstbufmeta.h"

#include "mfw_gst_utils.h"

#include "vpu_wrapper.h"




#define GST_TYPE_VPUENC \
  (gst_vpuenc_get_type ())
#define GST_VPUENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VPUENC, GstVpuEnc))
#define GST_VPUENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VPUENC, GstVpuEncClass))
#define GST_IS_VPUENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VPUENC))
#define GST_IS_VPUENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VPUENC))

#define GST_VPUENC_CAST(obj) ((GstVpuEnc *)(obj))

typedef struct _VpuEncMem VpuEncMem;

typedef void (*VpuEncMemFreeFunc) (VpuEncMem *);


struct _VpuEncMem
{
  void *handle;
  void *parent;
  void *paddr;
  void *vaddr;
  gint size;
  VpuEncMemFreeFunc freefunc;
  struct _VpuEncMem *next;
};

typedef struct
{
  VpuMemInfo meminfo;
  VpuEncOpenParamSimp openparam;
  VpuEncInitInfo initinfo;
  VpuEncHandle handle;
  VpuEncEncParam params;
} VpuEncContext;
typedef struct _VpuEncFrame
{
  GstBuffer *gstbuf;
  VpuFrameBuffer *display_handle;
  gint id;
  guint32 key;
  gint age;
} VpuEncFrame;
typedef struct
{
  gint seqheader_method;
  gint timestamp_method;
  gint64 bitrate;
  gint gopsize;
  gint quant;
  gint framerate_nu;
  gint framerate_de;
  gboolean force_framerate;
  gint codec;
} VpuEncOption;


typedef struct
{
  gint width;
  gint height;
  gint crop_left;
  gint crop_right;
  gint crop_top;
  gint crop_bottom;
  gint frame_size;
  gint pad_frame_size;
  gint frame_extra_size;
  gint ysize;
  gint uvsize;
  gint fmt;
  gint height_ratio;
  gint width_ratio;
  gint buffer_align;
  gboolean tiled;
} VpuInputSpec;


typedef struct
{
  guint64 in_cnt;
  guint64 out_cnt;
  guint64 show_cnt;
} VpuEncStat;


typedef struct _GstVpuEnc GstVpuEnc;

typedef struct _GstVpuEncClass GstVpuEncClass;

typedef struct {
  gint frame_size;
  VpuEncMem * mems;
} VpuEncFrameMemPool;

struct _GstVpuEnc
{
  GstElement element;
  GstPad *sinkpad;
  GstPad *srcpad;
  gint std;
  gboolean framed;
  VpuEncContext context;

  VpuEncMem *mems;
  VpuEncFrameMemPool framemem_pool;

  gint frame_num;

  VpuEncOption options;
  VpuInputSpec ispec;

  void *tsm;
  TSMGR_MODE tsm_mode;
  gboolean new_segment;

  gint64 segment_start;
  GstClockTime frame_interval;
  gint mosaic_cnt;
  GMutex *lock;
  GMutex * framemem_pool_lock;
  VpuEncStat vpu_stat;
  gboolean init;
  gboolean downstream_caps_set;

  gboolean force_copy;
  VpuEncMem * obuf;
  guint64 frame_cnt;

  GstBuffer *codec_data;
};


struct _GstVpuEncClass
{
  GstElementClass parent_class;
};

#endif

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
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. All rights reserved. 
 *
 */




/*
 * Module Name:    vpudec.h
 *
 * Description:    Head file of VPU-based videcoder decoder gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __VPUDEC_H__
#define __VPUDEC_H__

#include <gst/gst.h>
#include <gst/tag/tag.h>

#include "mfw_gst_ts.h"
#include "gstbufmeta.h"


#include "mfw_gst_utils.h"

#include "vpu_wrapper.h"


#define GST_TYPE_VPUDEC \
  (gst_vpudec_get_type())
#define GST_VPUDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VPUDEC,GstVpuDec))
#define GST_VPUDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VPUDEC,GstVpuDecClass))
#define GST_IS_VPUDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VPUDEC))
#define GST_IS_VPUDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VPUDEC))

#define GST_VPUDEC_CAST(obj) ((GstVpuDec *)(obj))

#define PLANE_MAX 4

typedef struct _VpuDecMem VpuDecMem;

typedef void (*VpuDecMemFreeFunc) (VpuDecMem *);


struct _VpuDecMem
{
  void *handle;
  void *parent;
  VpuDecMemFreeFunc freefunc;
  struct _VpuDecMem *next;
};

typedef struct
{
  VpuMemInfo meminfo;
  VpuDecOpenParam openparam;
  VpuDecInitInfo initinfo;
  VpuDecHandle handle;
} VpuDecContext;

typedef struct _VpuDecFrame
{
  GstBuffer *gstbuf;
  VpuFrameBuffer *display_handle;
  gint id;
  void *key;
  gint age;
} VpuDecFrame;


typedef struct
{
  gint delay_cnt;
  gint delay_threshold;
  gint guard_ms;
  gint l1_ms;
  gint l2_ms;
  gint l3_ms;
  gint l4_ms;
  guint cur_drop_level;
} VpuDecQosCtl;

typedef struct
{
  gint bufferplus;
  gint ofmt;
  gint allocframetimeout;
  gint mosaic_threshold;
  gint decode_retry_cnt;
  gboolean adaptive_drop;
  gboolean low_latency;

  guint drop_level_mask;

  gint framerate_n;
  gint framerate_d;

  gboolean experimental_tsm;
  gboolean profiling;
} VpuDecOption;

typedef struct
{
  gint width;
  gint height;
  gint crop_left;
  gint crop_right;
  gint crop_top;
  gint crop_bottom;
  gint width_align;
  gint height_align;
  gint buffer_align;
  gint frame_size;
  gint frame_extra_size;
  gint plane_size[PLANE_MAX];
  gint planes;
  gint height_ratio;
  gint width_ratio;
  GstStructure *ostructure;
  guint32 fourcc;
} VpuOutPutSpec;


typedef struct
{
  guint64 in_cnt;
  guint64 out_cnt;
  guint64 show_cnt;
} VpuDecStat;

typedef struct
{
  GstClockTime decode_time;
} VpuDecProfileCount;

typedef struct _GstVpuDec GstVpuDec;
typedef struct _GstVpuDecClass GstVpuDecClass;

struct _GstVpuDec
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  gint std;
  GstBuffer *codec_data;

  gboolean framed;


  VpuDecContext context;
  VpuDecMem *mems;

  gint frame_size;
  gint frame_num;


  VpuDecFrame *frames;

  VpuDecOption options;
  VpuOutPutSpec ospec;

  void *tsm;
  TSMGR_MODE tsm_mode;


  gboolean new_segment;
  gint64 segment_start;

  gint mosaic_cnt;

  GMutex *lock;
  VpuDecConfig drop_policy;
  gint drop_level;

  VpuDecStat vpu_stat;
  gint age;

  gint output_size;

  gboolean prerolling;

  VpuFieldType field_info;

  gboolean use_new_tsm;

  gint64 predict_ts;

  VpuDecProfileCount profile_count;

  VpuDecQosCtl qosctl;
};

struct _GstVpuDecClass
{
  GstElementClass parent_class;
};


#endif /* __VPUDEC_H__ */

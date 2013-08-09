/*
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc. 
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Module Name:    mfw_gst_v4lsink.h
 *
 * Description:    Header file for V4L Sink Plug-in for GStreamer.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */

/*=============================================================================
                                INCLUDE FILES
=============================================================================*/

#ifndef _MFW_GST_V4LSINK_H_
#define _MFW_GST_V4LSINK_H_

/*FIXME: there is no uint32_t type definition in mxc_v4l2.h */
typedef unsigned int uint32_t;

#include <gst/gst.h>
#include <gst/video/video.h>
#include <linux/types.h>
#include <gst/video/gstvideosink.h>
#include <linux/videodev2.h>
#if(defined(_MX51) || defined(_MX6))
#include <linux/mxc_v4l2.h>
//#include <linux/ipu.h>
#else
struct v4l2_mxc_offset
{
  uint32_t u_offset;
  uint32_t v_offset;
};
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

#include "mfw_gst_utils.h"

#ifdef USE_X11
#include "mfw_gst_xlib.h"
#include "mfw_gst_fb.h"

#undef LOC_ALPHA_SUPPORT
#endif

//#define LOC_ALPHA_SUPPORT
//#define VL4_STREAM_CALLBACK

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
#include <linux/mxcfb.h>
#endif

#include "mfw_gst_utils.h"

#define fourcc(a, b, c, d)\
   (((guint32)(a)<<0)|((guint32)(b)<<8)|((guint32)(c)<<16)|((guint32)(d)<<24))
#define IPU_PIX_FMT_TILED_NV12   fourcc ('T','N','V','P')
#define IPU_PIX_FMT_TILED_NV12F  fourcc ('T','N','V','F')
#define IPU_PIX_FMT_YUV444P      fourcc ('4','4','4','P')


/*=============================================================================
                                FUNCTIONS
=============================================================================*/


/*=============================================================================
                                CONSTANTS
=============================================================================*/
/* None. */

/*=============================================================================
                                ENUMS
=============================================================================*/

/* None. */

/*=============================================================================
                                MACROS
=============================================================================*/


#if defined(_MX6)
#define MIN_BUFFER_NUM              2   /* minimal 2 is default for compability of non-updated codec like wmv */
#define MAX_BUFFER_NUM              10  /* this number is related to driver */
#define BUFFER_RESERVED_NUM         0   /* 0 addtional buffer need reserved for v4l queue in vpu based decoder */
#define MAX_V4L_ALLOW_SIZE_IN_MB    48  /* 48MB limitation */
#define RESERVEDHWBUFFER_DEPTH      0   /* must less than MIN_BUFFER_NUM */
#elif defined(_MX51)
#define MIN_BUFFER_NUM              2   /* minimal 2 is default for compability of non-updated codec like wmv */
#define MAX_BUFFER_NUM              10  /* this number is related to driver */
#define BUFFER_RESERVED_NUM         0   /* 0 addtional buffer need reserved for v4l queue in vpu based decoder */
#define MAX_V4L_ALLOW_SIZE_IN_MB    48  /* 48MB limitation */
#define RESERVEDHWBUFFER_DEPTH      0   /* must less than MIN_BUFFER_NUM */
#elif defined (_MX37)
#define MIN_BUFFER_NUM              2   /* minimal 2 is default for compability of non-updated codec like wmv */
#define MAX_BUFFER_NUM              10  /* this number is related to driver */
#define BUFFER_RESERVED_NUM         0   /* 0 addtional buffer need reserved for v4l queue in vpu based decoder */
#define MAX_V4L_ALLOW_SIZE_IN_MB    15  /* 15MB limitation */
#define RESERVEDHWBUFFER_DEPTH      0   /* must less than MIN_BUFFER_NUM */
#elif defined(_MX27)
#define MIN_BUFFER_NUM              2   /* minimal 2, 5 is default for compability of non-updated codec like wmv */
#define MAX_BUFFER_NUM              10  /* this number is related to driver */
#define BUFFER_RESERVED_NUM         0   /* 0 addtional buffer need reserved for v4l queue in vpu based decoder */
#define MAX_V4L_ALLOW_SIZE_IN_MB    10  /* 10MB limitation */
#define RESERVEDHWBUFFER_DEPTH      0   /* must less than MIN_BUFFER_NUM */
#else
#define MIN_BUFFER_NUM              5   /* minimal 2, 5 is default for compability of non-updated codec like wmv */
#define MAX_BUFFER_NUM              10  /* this number is related to driver */
#define BUFFER_RESERVED_NUM         0   /*2*/   /* 2 addtional buffer need reserved for v4l queue */
#define MAX_V4L_ALLOW_SIZE_IN_MB    7   /* 7MB limitation */
#define RESERVEDHWBUFFER_DEPTH      0   /*2*/   /* must less than MIN_BUFFER_NUM */
#endif

#define MAX_V4L_ALLOW_SIZE_IN_BYTE  (MAX_V4L_ALLOW_SIZE_IN_MB*1024*1024)
#define MIN_QUEUE_NUM             1 

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_V4LSINK (mfw_gst_v4lsink_get_type())
#define MFW_GST_V4LSINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_V4LSINK,MFW_GST_V4LSINK_INFO_T))
#define MFW_GST_V4LSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_V4LSINK,MFW_GST_V4LSINK_INFO_CLASS_T))
#define MFW_GST_IS_V4LSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_V4LSINK))
#define MFW_GST_IS_V4LSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_V4LSINK))
/*=============================================================================
                  STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
    typedef enum
{
  ALPHA_GLOBAL = 0x1,
  ALPHA_LOCAL = 0x2,
} ALPHA_ENABLE_MASK;


#endif
enum
{
  SIGNAL_V4L_STREAM_CALLBACK,
  SIGNAL_LOCALPHA_BUFFER_READY,
  SIGNAL_LAST,
};


enum {
  FMT_FLAG_GRAY8 = 0x1
};

#if defined (VL4_STREAM_CALLBACK)
typedef enum
{
  VS_EVENT_BEFORE_STREAMON,
  VS_EVENT_AFTER_STREAMON,
  VS_EVENT_BEFORE_STREAMOFF,
  VS_EVENT_AFTER_STREAMOFF
} V4L_STREAM_EVENT;
#endif


typedef enum
{
  PARAM_NULL = 0x0,
  PARAM_SET_V4L = 0x1,
  PARAM_SET_COLOR_KEY = 0x2,
} PARAM_SET;



typedef struct MFW_GST_V4LSINK_INFO_S
{

  GstVideoSink videosink;
  GMutex *lock;                 //lock for v4ldevice operation
  gint framerate_n;
  gint framerate_d;
  gboolean full_screen;
  gboolean init;
  guint fourcc;                 /* our fourcc from the caps            */
#ifdef ENABLE_TVOUT
  /*For TV-Out & change para on-the-fly */
  gboolean tv_out;
  gint tv_mode;
  gint fd_tvout;
#endif
  PARAM_SET setpara;
  gint width;
  gint height;                  /* the size of the incoming YUV stream */
  gint in_width;
  gint in_height;
  gint disp_height;             /* resize display height */
  gint disp_width;              /* resize display width */
  gint axis_top;                /* diplay top co-ordinate */
  gint axis_left;               /* diplay left co-ordinate */
  gint rotate;                  /* display rotate angle */
  gint prevRotate;              /* Previous display rotate angle */
  gchar v4l_dev_name[64];       /*  strings of device name */
  gint v4l_id;                  /* device ID */
  gint cr_left_bypixel;         /* crop left offset set by decoder in caps */
  gint cr_right_bypixel;        /* crop right offset set by decoder in caps */
  gint cr_top_bypixel;          /* crop top offset set by decoder in caps */
  gint cr_bottom_bypixel;       /* crop bottom offset set by decoder in caps */
  gint crop_left;               /* crop left offset set through propery */
  gint crop_right;              /* crop right offset set through propery */
  gint crop_top;                /* crop top offset set through propery */
  gint crop_bottom;             /* crop bottom offset set through propery */
  gint fullscreen_width;
  gint fullscreen_height;
  gint base_offset;
  gboolean buffer_alloc_called;
  GstCaps *store_caps;

  gint cr_left_bypixel_orig;    /* original crop left offset set by decoder in caps */
  gint cr_right_bypixel_orig;   /* original crop right offset set by decoder in caps */
  gint cr_top_bypixel_orig;     /* original crop top offset set by decoder in caps */
  gint cr_bottom_bypixel_orig;  /* original crop bottom offset set by decoder in caps */
  gboolean enable_dump;
  gchar *dump_location;
  FILE *dumpfile;
  guint64 dump_length;


  gint qbuff_count;             /* buffer counter, increase when frame queued to v4l device */

  guint buffers_required;       /* hwbuffer limitation */
  gint swbuffer_max;            /* swbuffer limitation */

  gint querybuf_index;          /* pre-allocated hw/sw buffer counter */
  gint swbuffer_count;          /* pre-allocated sw buffer counter */

  GMutex *pool_lock;            /* lock for buffer pool operation */
  GSList *free_pool;            /* pool for free v4l buffer */

  void *reservedhwbuffer_list;  /* list to a hw v4l buffer reserved for render a swbuffer
                                 */
  gint v4lqueued;               /* counter for queued v4l buffer in device queue */
  GList *v4llist;
  void **all_buffer_pool;       /* malloced array to store all hw/sw buffers */
  int additional_buffer_depth;
  int frame_dropped;
  guint outformat;
  guint32 outformat_flags;

#ifdef USE_X11

  GstXInfo *gstXInfo;
#endif
  gint fd_fb;
  gulong colorSrc;              /* The color for FB0 */

  GMutex *flow_lock;

  gboolean stream_on;
  struct v4l2_crop crop;
  struct v4l2_crop prevCrop;    /* The previous crop information */

  gint field;
  gboolean is_paused;
  gboolean suspend;

  gboolean stretch;

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
  ALPHA_ENABLE_MASK alpha_enable;
  gint alpha;

  gint fd_lalpfb;
  struct mxcfb_loc_alpha lalpha;
  void *lalp_buf_vaddr[2];
  gint lalp_buf_cidx;
#endif
  gboolean x11enabled;          /* Enable or Disable the X11 feature */

  gboolean force_aspect_ratio;
  gboolean setXid;

  guint64 rendered;
  CHIP_CODE chipcode;
  GstClockTime running_time;
  gint motion;

  gboolean enable_deinterlace;
} MFW_GST_V4LSINK_INFO_T;




typedef struct MFW_GST_V4LSINK_INFO_CLASS_S
{
  GstVideoSinkClass parent_class;
#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))

  void (*lalp_buf_ready_notify) (gpointer buf0);
#endif

#if defined (VL4_STREAM_CALLBACK)
  void (*v4lstream_callback) (gint eventtype);
#endif
} MFW_GST_V4LSINK_INFO_CLASS_T;




/*=============================================================================
                  GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                  FUNCTION PROTOTYPES
=============================================================================*/

extern GType mfw_gst_v4lsink_get_type (void);




G_END_DECLS
/*===========================================================================*/
#endif /* _MFW_GST_V4LSINK_H_ */

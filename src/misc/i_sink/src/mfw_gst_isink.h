/*
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_isink.h
 *
 * Description:    Header file for Isink Plug-in for GStreamer.
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

#ifndef _MFW_GST_ISINK_H_
#define _MFW_GST_ISINK_H_
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>

#include "mfw_gst_video_surface.h"


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
G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_ISINK (mfw_gst_isink_get_type())
#define MFW_GST_ISINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_ISINK,MfwGstISink))
#define MFW_GST_ISINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_ISINK,MfwGstISinkClass))
#define MFW_GST_IS_ISINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_ISINK))
#define MFW_GST_IS_ISINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_ISINK))
#define MFW_GST_TYPE_ISINK_BUFFER (mfw_gst_isink_buffer_get_type())
#define MFW_GST_IS_ISINK_BUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MFW_GST_TYPE_ISINK_BUFFER))
#define MFW_GST_ISINK_BUFFER(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_ISINK,MfwGstISink))
    typedef enum
{
  PARAM_NULL = 0x0,
  PARAM_SET_V4L = 0x1,
  PARAM_SET_COLOR_KEY = 0x2,
} PARAM_SET;


/*=============================================================================
                  STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

#define ISINK_CONFIG_MAX 4

enum
{
  ISINK_DISP_LCD = 1,
  ISINK_DISP_TV = 2,
};

enum
{
  ISINK_TV_SYS_NTSC = 0,
  ISINK_TV_SYS_PAL = 1,
  ISINK_TV_SYS_720P = 2,
};

typedef enum
{
  IBUF_STATE_ILLEGAL,
  IBUF_STATE_ALLOCATED,         /* buffer occured by codec or pp */
  IBUF_STATE_SHOWED,            /* buffer is successlly showed */
  IBUF_STATE_SHOWING,           /* buffer is showing(in sl queue) */
  IBUF_STATE_IDLE,              /* buffer is idle, can be allocated to codec or pp */
  IBUF_STATE_FREE,              /* buffer need to be freed, the acctually free precedure will happen when future unref */
} IBufferState;

typedef enum
{
  IBUF_TYPE_ILLEGAL,
  IBUF_TYPE_NATIVE,
  IBUF_TYPE_VIRTUAL,
} IBufferType;

typedef struct
{
  SourceFmt srcfmt;
  SourceFmt origsrcfmt;
  gint aspectRatioN;
  gint aspectRatioD;
  gint framerateN;
  gint framerateD;
  gint requestBufferNum;
  gint framebufsize;

  gboolean clearbuf;
  gint clearcolor;
} InCfg;



typedef struct
{

  DestinationFmt desfmt;
  Rect origrect;
  gint devid;
  gint mode;
  gint deinterlace_mode;
  void *vshandle;
  gboolean enabled;
} OutCfg;



typedef struct
{
  GstVideoSinkClass parent_class;
  VideoDeviceDesc vd_desc[VD_MAX + 1];

  void (*isink_render_callback) (gpointer buf);
  void (*isink_allocbuffers_callback) (gpointer buf);
} MfwGstISinkClass;

typedef struct _MfwGstISink MfwGstISink;

struct _MfwGstISink
{
  GstVideoSink videosink;
  gboolean init;
  InCfg icfg;
  OutCfg ocfg[ISINK_CONFIG_MAX];
  MfwGstISinkClass *class;
  gboolean buffer_alloc_called;
  gboolean closed;
  GstBuffer *curbuf;
  gint cleft;
  gint cright;
  gint ctop;
  gint cbottom;
  void *vshandle;
  gint subid;
  gboolean subset;

#ifdef USE_X11
  GstXInfo *gstXInfo;
  gboolean x11enabled;          /* Enable or Disable the X11 feature */
  gboolean setXid;
#endif
  PARAM_SET setpara;
  gulong colorkey;
  gint colorkey_red;
  gint colorkey_green;
  gint colorkey_blue;

  guint reconfig;

  GSList *free_pool;
  gboolean need_render_notify;
  gboolean need_use_ex_buffer;
  gboolean need_force_copy;

};

/*=============================================================================
                  GLOBAL VARIABLE DECLARATIONS
=============================================================================*/
/* None. */

/*=============================================================================
                  FUNCTION PROTOTYPES
=============================================================================*/
extern GType mfw_gst_isink_get_type (void);

G_END_DECLS
/*===========================================================================*/
#endif /* _MFW_GST_ISINK_H_ */

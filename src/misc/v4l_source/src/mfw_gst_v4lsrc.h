/*
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_v4lsrc.h
 *
 * Description:    Header file of V4L Source (Capture) Plug-in for 
 *                 GStreamer.
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

#ifndef _MFW_GST_V4LSRC_H_
#define _MFW_GST_V4LSRC_H_

/*=============================================================================
                                CONSTANTS
=============================================================================*/

/*=============================================================================
                                ENUMS
=============================================================================*/

/* None. */

/*=============================================================================
                                MACROS
=============================================================================*/
G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_V4LSRC (mfw_gst_v4lsrc_get_type())
#define MFW_GST_V4LSRC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_V4LSRC,MFWGstV4LSrc))
#define MFW_GST_V4LSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_V4LSRC,MFWGstV4LSrcClass))
#define MFW_GST_IS_V4LSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_V4LSRC))
#define MFW_GST_IS_V4LSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_V4LSRC))
#define MFW_GST_TYPE_V4LSRC_BUFFER (mfw_gst_v4lsrc_buffer_get_type())
#define MFW_GST_IS_V4LSRC_BUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MFW_GST_TYPE_V4LSRC_BUFFER))
#define MFW_GST_V4LSRC_BUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MFW_GST_TYPE_V4LSRC_BUFFER, MFWGstV4LSrcBuffer))
#define MFW_GST_V4LSRC_BUFFER_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MFW_GST_TYPE_V4LSRC_BUFFER, MFWGstV4LSrcBufferClass))
/*=============================================================================
                  STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef struct MFW_GST_V4LSRC_INFO_S
{

  GstPushSrc element;
  gint capture_width;           /* width to be captured */
  gint capture_height;          /* height to be captured */
  gint rotate;
  gint crop_pixel;
  gint fps_n;
  gint fps_d;
  GstBuffer **buffers;
  void **buf_pools;
  GstElementClass *parent_class;
  guint32 offset;
  guint32 buffer_size;
  guint32 count;
  gboolean preview;
  gint preview_width;
  gint preview_height;
  gint preview_top;
  gint preview_left;
  gint fd_v4l;
  gint sensor_width;
  gint sensor_height;
  GstClockTime time_per_frame;
  GstClockTime last_ts;
  gint capture_mode;
  gint input;
  gboolean bg;
  char *devicename;
  int g_display_lcd;
  int queue_size;               /* v4l request buffer number */
  GList *free_pool;             /* pool for v4l buffers. */
  gboolean start;
} MFWGstV4LSrc;

typedef struct MFW_GST_V4LSRC_INFO_CLASS_S
{
  GstPushSrcClass parent_class;
} MFWGstV4LSrcClass;

struct v4l2_mxc_offset
{
  uint32_t u_offset;
  uint32_t v_offset;
};


typedef struct _MFWGstV4LSrcBuffer MFWGstV4LSrcBuffer;
typedef struct _MFWGstV4LSrcBufferClass MFWGstV4LSrcBufferClass;

struct _MFWGstV4LSrcBuffer
{
  GstBuffer buffer;
  struct v4l2_buffer v4l2_buf;
  MFWGstV4LSrc *v4lsrccontext;
  gint num;
};









/*=============================================================================
                  GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                  FUNCTION PROTOTYPES
=============================================================================*/

extern GType mfw_gst_v4lsrc_get_type (void);

G_END_DECLS
/*===========================================================================*/
#endif /* _MFW_GST_V4LSRC_H_ */

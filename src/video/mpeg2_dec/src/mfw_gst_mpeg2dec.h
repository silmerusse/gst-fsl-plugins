/*
 * Copyright (c) 2009-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_mpeg2dec.h
 *
 * Description:    Header file for MPEG2-Decode Plug-in for GStreamer. 
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

#ifndef __MFW_GST_MPEG2DEC_H__
#define __MFW_GST_MPEG2DEC_H__

#include "mfw_gst_ts.h"
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
#define MFW_GST_TYPE_MPEG2DEC (mfw_gst_mpeg2dec_get_type())
#define MFW_GST_MPEG2DEC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_MPEG2DEC,MFW_GST_MPEG2DEC_INFO_T))
#define MFW_GST_MPEG2DEC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_MPEG2DEC,MFW_GST_MPEG2DEC_INFO_CLASS_T))
#define MFW_GST_IS_MPEG2DEC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_MPEG2DEC))
#define MFW_GST_IS_MPEG2DEC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_MPEG2DEC))
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
#define MAX_TIMESTAMP_SHIFT 6
#define MAX_TIMESTAMP (1<<MAX_TIMESTAMP_SHIFT)
#define TIMESTAMP_INDEX_MASK (MAX_TIMESTAMP-1)
#if 0
    typedef struct
{
  GstClockTime timestamp[MAX_TIMESTAMP];
  GstClockTime last_send;
  GstClockTime half_interval;
  gint no_timestamp_count;
  gint timestamp_tx;
  gint timestamp_rx;
} sMpeg2TimestampObject;
#endif
typedef struct MFW_GST_MPEG2DEC_INFO_S
{
  GstElement element;
  gboolean is_init_done;        // Variable to check whether
  // initialization is done.
  GstPad *srcpad;               // Pointer to the srcpad.
  GstPad *sinkpad;              // Pointer to the sinkpad.
  sMpeg2DecObject *dec_object;  // Poinetr to decoder configuration
  //   structure
  sMpeg2DecMemAllocInfo *memalloc_info; // Pointer to decoder memory allocation
  // information                
  GstBuffer *input_buffer;      // Pointer to input buffer
  gboolean caps_set;            // flag for capability set
  guint64 bit_rate;             // bit rate      
  GstBuffer *outbuffer[3];      // poinnter to output buffer
  gfloat frame_rate;            // video frame rate  
  gint32 decoded_frames;        // number of decoded frames 
  gint32 padded_width;
  gint32 padded_height;
  gint64 start_time;
  /* Variable to set buffer manager working mode */
  gint bmmode;
#if 0
  sMpeg2TimestampObject timestamp_object;
#endif
  void *pTS_Mgr;                // Time Stamp manager
  gint demo_mode;               /* 0: Normal mode, 1: Demo mode 2: Demo ending */
  GstBuffer *codec_data;        // codec data, from demux;
  guint codec_data_len;
} MFW_GST_MPEG2DEC_INFO_T;


typedef struct MFW_GST_MPEG2DEC_INFO_CLASS_S
{
  GstElementClass parent_class;

} MFW_GST_MPEG2DEC_INFO_CLASS_T;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

extern GType mfw_gst_mpeg2dec_get_type (void);

G_END_DECLS
/*===========================================================================*/
#endif /* _MFW_GST_MPEG2DEC_H_ */

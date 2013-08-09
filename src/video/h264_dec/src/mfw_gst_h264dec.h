/*
 * Copyright (c) 2009-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_h264dec.h
 *
 * Description:
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 */



#ifndef _MFW_GST_H264DEC_H_
#define _MFW_GST_H264DEC_H_
/*=============================================================================

     Header Name:   mfw_gst_h264dec.h

     General Description:   This Header file contains the declaration required
			    for implementation of H264  decode plugin .

 Portability:	This code is written for Linux OS and Gstreamer 10
===============================================================================

===============================================================================
                                         INCLUDE FILES
=============================================================================*/
/* None */
#include "mfw_gst_ts.h"

/*=============================================================================
                                           CONSTANTS
=============================================================================*/
#define MAX_NAL 1000
//#define MAX_STREAM_BUF  2048
/*=============================================================================
                                            MACROS
=============================================================================*/
G_BEGIN_DECLS
#define MFW_GST_TYPE_H264DEC  (mfw_gst_h264dec_get_type())
#define MFW_GST_H264DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_H264DEC,MFW_GST_H264DEC_INFO_T))
#define MFW_GST_H264DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_H264DEC,MFW_GST_H264DEC_INFO_T))
#define MFW_GST_IS_H264DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_H264DEC))
#define MFW_GST_IS_H264DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_H264DEC))
/*=============================================================================
                                             ENUMS
=============================================================================*/
/* None */
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef struct MFW_GST_H264DEC_INFO_S {

    /* plugin related variables */
    GstElement element;		/* instance of base class */
    GstPad *sinkpad;		/* sink pad for element */
    GstPad *srcpad;		/* source pad of element */
    gboolean caps_set;		/* cabability setting */
    GstBuffer *input_buffer;	/* pointer to input buffer */
    gboolean is_decode_init;	/* flag for decoder initialization */
    gint32 ff_flag;		/* flag for fast forward */
    gint32 number_of_bytes;	/* bytes in one NAL unit */
    gint32 number_of_nal_units;	/* total number of NAL units in a chunk */
    gint32 nal_size[MAX_NAL];	/* number of bytes in a NAL unit */
    gfloat frame_rate;		/* frame rate */
    gint framerate_n;
    gint framerate_d;

    gint32 frame_width;		/* frame width */
    gint32 frame_height;	/* frame height */
    gint32 frame_width_padded;  /* padded frame width */
    gint32 frame_height_padded; /* padded frame height */
    // Time Stamp manager
    void * pTS_Mgr;

    /* h264 decoder related variables */
    sAVCDecoderConfig dec_config;	/* decoder configuration */
    eAVCDRetType status;	/* return value of decode library */

    /* pofiling information variables  */
    glong Time;			/* time taken by the decode library */
    glong chain_Time;		/* time taken by chain function */
    glong no_of_frames;		/* Total number of frame */
    glong no_of_frames_dropped;	/* numbers of frames dropped */
    gboolean profile;		/* flag for enabling profiling info */
    gboolean display_baddata;   /* property to display bad data or not */
    /* Variable to set buffer manager working mode */
    gint bmmode;

    GstBuffer*      codec_data;        // Header data needed for VC-1 and some codecs
    guint           codec_data_len;    // Header Extension obtained through caps negotiation


    gboolean is_sfd;
    struct sfd_frames_info sfd_info;
    gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */
} MFW_GST_H264DEC_INFO_T;


typedef struct MFW_GST_H264DEC_INFO_CLASS_S {
    GstElementClass parent_class;
} MFW_GST_H264DEC_INFO_CLASS_T;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/
/* None */

/*===========================================================================*/


G_END_DECLS
#endif				/* _MFW_GST_H264DEC_H_ */


/*
 * Copyright (C) 2005-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_wma10dec.h
 *
 * Description:    This Header file contains the declaration required
 *                 for implementation of WMA-10 decode plugin .
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */



#ifndef _MFW_GST_WMA10DEC_H_
#define _MFW_GST_WMA10DEC_H_

/*=============================================================================
                                         INCLUDE FILES
=============================================================================*/
/* None */


/*=============================================================================
                                           CONSTANTS
=============================================================================*/

/* None. */

/*=============================================================================
                                            MACROS
=============================================================================*/
G_BEGIN_DECLS

#define MFW_GST_TYPE_WMA10DEC  (mfw_gst_wma10dec_get_type())

#define MFW_GST_WMA10DEC(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_WMA10DEC,MFW_GST_WMA10DEC_INFO_T))

#define MFW_GST_WMA10DEC_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_WMA10DEC,MFW_GST_WMA10DEC_INFO_T))

#define MFW_GST_IS_WMA10DEC(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_WMA10DEC))

#define MFW_GST_IS_WMA10DEC_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_WMA10DEC))
/*=============================================================================
                                             ENUMS
=============================================================================*/
/* None */
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
typedef struct MFW_GST_WMA10DEC_INFO_S {

    /* Plugin specific information. */

    GstElement element;		        /* instance of base class */
    GstPad *sinkpad;		        /* sink pad for element */
    GstPad *srcpad;		            /* source pad of element */
    gboolean caps_set;		        /* cabability setting */
    gboolean new_seg_flag;	        /* flag for sending new segment */
    guint is_decode_init;	        /* flag for decoder initialization */
    guint end_of_stream;	        /* flag for EOS */
    gint64 time_offset;		        /* time stamp offset */

    /* codec specific information. */

    gint32 version;		            /* version of wma decoder */
    gint32 format_tag;		        /* format tag */
    gint32 channels;		        /* number of channels */
    gint32 samples_per_second;	    /* samples per second */
    gint32 avg_bytes_per_second;	/* avg bytes per sec */
    gint32 block_align;		        /* block alignment */
    gint32 bits_per_sample;	        /* number of bits per sample */
    gint32 channel_mask;            /* channel mask */
    gint32 encode_opt;	        	/* encode option */
    gint32 advanced_encode_opt;     /* advanced encode option*/
    gint32 advanced_encode_opt2;    /* advanced encode option 2*/
    WMADDecoderConfig *dec_config;	/* wma decoder configuration */
    WMADDecoderParams *dec_param;	/* wma decoder parameters */
    WMADMemAllocInfoSub *mem;	    /* wma decoder memory allocation */
    WMAD_INT16 *output_buff;	    /* output buffer pointer */

    /* Misc Information. */
    gboolean seek_flag;		        /* flag for seek event */
    gint64 time_added;		        /* time by which seek happened */
    gboolean profile;		        /* enabling time profiling of decoder and plugin */
    glong Time;                     /* for codec profiling. */
    glong chain_Time;               /* for plugin profiling. */
    glong no_of_frames_dropped;     /* Number of error frames. */
    guint total_frames;	            /* Total Number of frames */
    gboolean flush_complete;        /* flag */
    gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */
    GstAdapter * pAdapter;
    gint32 inbuffsize;				/* adapter available size */
    gint32 inbuffsize_used;         /* this size should be an integral number of blockAlign */
    GstBuffer * fillbuf;
} MFW_GST_WMA10DEC_INFO_T;


typedef struct MFW_GST_WMA10DEC_INFO_CLASS_S {
    GstElementClass parent_class;
} MFW_GST_WMA10DEC_INFO_CLASS_T;

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
#endif				/* _MFW_GST_WMA10DEC_H_ */

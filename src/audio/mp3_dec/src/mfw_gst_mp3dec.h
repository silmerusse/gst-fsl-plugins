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
 * Module Name:    mfw_gst_mp3dec.h
 *
 * Description:
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
#ifndef __MFW_GST_MP3DEC_H__
#define __MFW_GST_MP3DEC_H__
#include "mp3_parser/mp3_parse.h"
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
#define MFW_GST_TYPE_MP3DEC \
  (mfw_gst_type_mp3dec_get_type())
#define MFW_GST_MP3DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_MP3DEC,MfwGstMp3DecInfo))
#define MFW_GST_MP3DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_MP3DEC,MfwGstMp3DecInfoClass))
#define MFW_GST_IS_MP3DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_MP3DEC))
#define MFW_GST_IS_MP3DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_MP3DEC))
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef struct _MfwGstMp3DecInfo {
    GstElement element;		/* instance of base class */
    GstPad *sinkpad, *srcpad;	/* source and sink pad of element */
    MP3D_Decode_Config *dec_config;	/* mp3 decoder configuration */
    MP3D_Decode_Params *dec_param;	/* mp3 decoder parameters */
    gboolean eos_event;		/* flag for end of event */
    gboolean init_flag;		/* flag for decoder initialization */
    gboolean caps_set;		/* capablity setting */
    guint frame_no;		/* frame number count */
    gint id3v2_size;		/* ID3 data size */
    gint64 time_offset;        /* time stamp offset */
    guint64 offset;
    gboolean new_seg_flag;	/* new segment recieved flag */
    GstAdapter *adapter;	/* adapter to queue in the buffers */
    guint8 *send_buff;		/* buffer used to send data during call
				   back */
    guint64 total_bitrate;	/* total bitrate of all the frames */
    guint64 total_frames;	/* total number of frames */
    gfloat avg_bitrate;		/* average bitrate */
    gboolean full_parse;	/* enable full parse in the preroll state
				   to calculate avg bitrate */

    GstIndex *index;		/* index of the plugin- element */
    gint index_id;		/* index id of the plugin - element */
    guint64 total_samples;	/* total samples decoded */
    gint64 seeked_time;		/* seeked time */
    gboolean id3tag;		/* to check if ID3 tag is parsed */
    gboolean profile;		/* enabling time profiling of decoder and plugin */
    glong Time;
    glong chain_Time;
    glong no_of_frames_dropped;
    gfloat avg_fps_decoding;
    gboolean stopped;
    gboolean layerchk;
    gint num;
    guint mpeg_layer;     /* layer I, layer II,  layer III */
    guint mpeg_version;   /* mpeg1, mpeg2, mpeg2.5*/
    gboolean strm_info_get;
    mp3_fr_info strm_info;
    gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */
    gint new_segment; 

    /* buffer list for calculating timestamp */
    GList *buf_list;
    gint flushed_bytes;

    /* for fade in after error frame */
    gint fadein_factor;

} MfwGstMp3DecInfo;

typedef struct _MfwGstMp3DecInfoClass {
    GstElementClass parent_class;
} MfwGstMp3DecInfoClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_mp3dec_get_type(void);

G_END_DECLS
#endif				/* __MFW_GST_MP3DEC_H__ */

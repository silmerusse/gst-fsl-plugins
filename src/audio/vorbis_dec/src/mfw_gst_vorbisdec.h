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
 * Module Name:    mfw_gst_vorbisdec.c
 *
 * Description:    Gstreamer plugin for Ogg Vorbis decoder
                   capable of decoding Vorbis raw data.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 *
 * Mar. 9th 2010 Lyon Wang
 * - Inital version.
 *
 */

/*============================================================================
                            INCLUDE FILES
=============================================================================*/

#ifndef __MFW_GST_VORBISDEC_H__
#define __MFW_GST_VORBISDEC_H__

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
#define MFW_GST_TYPE_VORBISDEC \
  (mfw_gst_vorbisdec_get_type())
#define MFW_GST_VORBISDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_VORBISDEC,\
   MFW_GST_VORBISDEC_INFO_T))
#define MFW_GST_VORBISDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_VORBISDEC,MFW_GST_VORBISDEC_CLASS_T))
#define MFW_GST_IS_VORBISDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_VORBISDEC))
#define MFW_GST_IS_VORBISDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_VORBISDEC))
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
 typedef struct _blocktimestamp{
     struct _blocktimestamp * next;
     guint buflen;
     GstClockTime timestamp;
 }Block_Timestamp;

 typedef struct _timestampmanager{
     Block_Timestamp * allocatedbuffer;
     Block_Timestamp * freelist;
     Block_Timestamp * head;
     Block_Timestamp * tail;
     guint  allocatednum;
 }Timestamp_Manager;


typedef struct MFW_GST_VORBISDEC_INFO_S {
        GstElement element;
        GstPad *sinkpad;
        GstPad *srcpad;
        gboolean init_done;     /* flag to check whether the initialisation is done or not */
        gboolean flow_error;    /* flag to indicate a fatal flow error */
        GstBuffer *inbuffer1;   /* input buffer */
        GstBuffer *inbuffer2;   /* addition input buffer used for the call back */
        gboolean caps_set;      /* flag to check whether the source pad capabilities  are set or not */
        sOggVorbisDecObj *psOVDecObj; /* decoder config structure */
        gboolean eos;           /* flag to update end of stream staus */
        guint64  time_offset;   /* time increment of the output  */
        guint64 sampling_freq;
        gint bit_rate;
        guint32 number_of_channels;
        guint64 total_frames;
        gint64 total_time;
        guint64 total_sample;   /* total sample each channel */
        guint64 buffer_time;
        gboolean corrupt_bs;
        GstAdapter * pAdapter;
        Timestamp_Manager tsMgr;
        gint error_cnt;
        gint packetised;
        GstBuffer *extra_codec_data;
        GstBuffer *codec_data;
} MFW_GST_VORBISDEC_INFO_T;

typedef struct MFW_GST_VORBISDEC_CLASS_S {
    GstElementClass parent_class;
} MFW_GST_VORBISDEC_CLASS_T;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_vorbisdec_get_type(void);

G_END_DECLS
/*===========================================================================*/
#endif				/* __MFW_GST_VORBISDEC_H__ */

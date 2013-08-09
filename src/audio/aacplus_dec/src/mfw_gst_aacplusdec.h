/*
 * Copyright (C) 2009-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_aacplusdec.h
 *
 * Description:    Gstreamer plugin for AAC Plus decoder
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 *
 *
 * May, 6 2009 Dexter JI<b01140@freescale.com>
 * - Add extra ADTS header for compatible with qtdemux.
 *
 */

/*============================================================================
                            INCLUDE FILES
=============================================================================*/

#ifndef __MFW_GST_AACDECPLUS_H__
#define __MFW_GST_AACDECPLUS_H__

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
#define MFW_GST_TYPE_AACPLUSDEC \
  (mfw_gst_aacplusdec_get_type())
#define MFW_GST_AACPLUSDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_AACPLUSDEC,\
   MFW_GST_AACPLUSDEC_INFO_T))
#define MFW_GST_AACPLUSDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_AACPLUSDEC,MFW_GST_AACPLUSDEC_CLASS_T))
#define MFW_GST_IS_AACPLUSDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_AACPLUSDEC))
#define MFW_GST_IS_AACPLUSDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_AACPLUSDEC))

/*=============================================================================
                                LOCAL MACROS
=============================================================================*/
#define LONG_BOUNDARY 4
#define	GST_CAT_DEFAULT    mfw_gst_aacplusdec_debug

#define BS_BUF_SIZE AACD_INPUT_BUFFER_SIZE
#define MAX_ENC_BUF_SIZE 400*BS_BUF_SIZE
#define LONG_BOUNDARY 4
#define FRAMESIZE 1024

int sampling_frequency[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0, 0};
enum { BITSPERFRAME_SCALE = 4 } ;
unsigned int lastGoodBufferFullness,lastGoodBufferFullness,firstBufferFullness;

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

/* Bitstream parameters structure */
#if 0
typedef struct{  
    long bit_counter;  
    unsigned long bit_register;  
    unsigned char *bs_curr;  
    unsigned char *bs_end;  
    unsigned char *bs_curr_ext;  
    unsigned char *bs_end_ext;  
    unsigned int bs_eof;  
    unsigned int bs_seek_needed;
}
BitstreamParam;
#endif

typedef struct AACD_App_params_S {
    BitstreamParam bs_param;	/* bitstream parameters */
    AACD_Block_Params BlockParams;	/* frame parameters */
    gint BitsInHeader;		/* number of bits used to decode the 
				   header */
    gboolean App_adif_header_present;
    /* flag to check if the header present 
       is ADIF */
    gboolean App_adts_header_present;
    /* flag to check if the header present 
       is ADTS */

    AACD_Decoder_Config *dec_config;	/* decoder context */
} AACD_App_params;

#ifdef PUSH_MODE
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
#endif

typedef struct MFW_GST_AACPLUSDEC_INFO_S {
    GstElement element;
    GstPad *sinkpad;
    GstPad *srcpad;
    gboolean init_done;		/* flag to check whether the initialisation 
				   is done or not */
    gboolean flow_error;        /* flag to indicate a fatal flow error */
    GstBuffer *inbuffer1;	/* input buffer */
    GstBuffer *inbuffer2;	/* addition input buffer used for the  
				   call back */
    gboolean caps_set;		/* flag to check whether the source pad 
				   capabilities  are set or not */
    AACD_App_params app_params;	/* parameters of the decoder used by 
					   the plugin */
    gboolean eos;		/* flag to update end of stream staus */
    gboolean seek_flag;		/* flag to check whether seek is in demuxer or in decoder  */
    guint64  time_offset;	/* time increment of the output  */
    guint64 sampling_freq;
    gint bit_rate;
    guint32 number_of_channels;
    guint64 total_frames;
    gint64 total_time;
    guint bitsPerFrame;
    guint SampFreqIdx;
    gint nBitsReceive;
    gint nFramesReceived;
    gint bitstream_count;
    gint bitstream_buf_index;
    gint in_buf_done;
gint nBitsReceived;
    guint64 buffer_time;
    gboolean corrupt_bs;
#ifdef PUSH_MODE    
    GstAdapter * pAdapter;
    Timestamp_Manager tsMgr;
#endif    
    gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */
    gint error_cnt;
    gint packetised;
    GstBuffer *extra_codec_data;
    GstBuffer *codec_data;
    guint32 bword, bbit;
    SBR_FRAME_TYPE sbr_frame_type;
    gint32 num_channels_pre;
    gint32 samplerate_pre;

    gint sample_rate_in_file;   /* sample rate given by container. if no sample rate in container, it should be 0 */
    gboolean profile_not_support; /* if the aac profile is not LC, set this to TRUE*/

     BitstreamParam App_bs_param;
    char bitstream_buf[MAX_ENC_BUF_SIZE];
    unsigned char App_ibs_buf[INTERNAL_BS_BUFSIZE];

} MFW_GST_AACPLUSDEC_INFO_T;


typedef struct MFW_GST_AACPLUSDEC_CLASS_S {
    GstElementClass parent_class;
} MFW_GST_AACPLUSDEC_CLASS_T;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_aacplusdec_get_type(void);

G_END_DECLS
/*===========================================================================*/
#endif				/* __MFW_GST_AACPLUSDEC_H__ */

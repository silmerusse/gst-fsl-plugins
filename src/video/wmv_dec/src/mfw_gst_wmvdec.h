/*
 *  Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
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
 * Module Name:    mfw_gst_wmvdecode.h
 *
 * Description:    Header file for WMV-Decode Plug-in for GStreamer.  
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

#ifndef _MFW_GST_WMVDEC_H_
#define _MFW_GST_WMVDEC_H_

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
#define MFW_GST_TYPE_WMVDEC (mfw_gst_wmvdec_get_type())
#define MFW_GST_WMVDEC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_WMVDEC,MFW_GST_WMVDEC_INFO_T))
#define MFW_GST_WMVDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_WMVDEC,MFW_GST_WMVDEC_INFO_CLASS_T))
#define MFW_GST_IS_WMVDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_WMVDEC))
#define MFW_GST_IS_WMVDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_WMVDEC))

#define DEFAULT_FRAME_RATE_NUMERATOR 30

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef struct {
    /* rcv header information, passed from the demuxer using Gst-Sructure */
    WMV9D_S32 s32Width;
    WMV9D_S32 s32Height;
    WMV9D_S32 s32BitRate;
    WMV9D_S32 s32FrameRate;
    WMV9D_U32 u32FrameRateDe;        // frame rate denominator   
    WMV9D_U32 u32FrameRateNu;        // frame rate nominator
    WMV9D_U8 *pu8CompFmtString;
    WMV9D_S32 s32SeqDataLen;
    guint8 *SeqData;
    sWmv9DecObjectType *pWmv9Object;	/* WMV9 decoder object pointer */
    GstBuffer *gst_input_buffer;	/* Pointer to the incoming Buffer. */
    sWmv9DecObjectType sDecObj;	/* Object type for decoder. */
    GstClockTime timestamp;
    GstClockTime u32TimePerFrame;

} sInputHandlerType;

typedef struct MFW_GST_WMVDEC_INFO_S {

    GstElement element;
    GstPad *sinkpad;
    GstPad *srcpad;
    gboolean is_init_done;	/* Variable to check whether decoder
				   initialization is done or not. */

    sInputHandlerType wmvdec_handle_info;	/* Header information for decoder. */
    gboolean caps_set;		/* Variable to find whether capabilities
				   is set for the decoder or not. */
    GstClockTime last_ts;
    guint8 state;
    GstStateChange transistion;
    gboolean seek_flag;
    GstClockTime seeked_time;
    GstAdapter *adapter;
    GstCaps *savecaps;
    gboolean flush;
    gint error_count;

    gint32 padded_width;
    gint32 padded_height;
    gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */
    gboolean fatal_err_found;

} MFW_GST_WMVDEC_INFO_T;

typedef struct MFW_GST_WMVDEC_INFO_CLASS_S {

    GstElementClass parent_class;

} MFW_GST_WMVDEC_INFO_CLASS_T;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

extern GType mfw_gst_wmvdec_get_type(void);

G_END_DECLS
/*===========================================================================*/
#endif				/* _MFW_GST_WMVDEC_H_ */

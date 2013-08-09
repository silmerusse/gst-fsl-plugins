/*
 * Copyright (c) 2008-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_ac3dec.h
 *
 * Description:    Head files for  ac3 plugin for Gstreamer using push
 *                 based method.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#ifndef __MFW_GST_AC3DEC_H__
#define __MFW_GST_AC3DEC_H__
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
#define MFW_GST_TYPE_AC3DEC \
  (mfw_gst_type_ac3dec_get_type())
#define MFW_GST_AC3DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_AC3DEC,MfwGstAc3DecInfo))
#define MFW_GST_AC3DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_AC3DEC,MfwGstAc3DecInfoClass))
#define MFW_GST_IS_AC3DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_AC3DEC))
#define MFW_GST_IS_AC3DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_AC3DEC))
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef struct _MfwGstAc3DecInfo {
    GstElement element;		/* instance of base class */
    GstPad *sinkpad, *srcpad;	/* source and sink pad of element */
    AC3DDecoderConfig *dec_config;	/* ac3 decoder configuration */
    AC3D_PARAM *dec_param;	/* ac3 decoder parameters */
    gboolean eos_event;		/* flag for end of event */
    gboolean init_flag;		/* flag for decoder initialization */
    gboolean tags_set;		/* tags setting */
    guint frame_no;		/* frame number count */
    gfloat time_offset;		/* time stamp offset */
    GstAdapter *adapter;	/* adapter to queue in the buffers */
    guint8 *send_buff;		/* buffer used to send data during call 
				   back */

    guint64 total_samples;	/* total samples decoded */
    gint64 seeked_time;		/* seeked time */
    gboolean seek_flag;		/* flag to check whether seek is in demuxer or in decoder  */
    glong Time;
    glong chain_Time;
    glong no_of_frames_dropped;
    gboolean stopped;

    guint64 totalBytes;
    guint64 duration;
    guint64 **seek_index[10];

    gint32 sampling_freq_pre;
    gint32 num_channels_pre;
    gint16 outputmask_pre;

    /* codec property */
    gint    outputmode;         /* output channel layout */
    gint    wordsize;           /* pcm depth */
    gint    outlfeon;           /* output subwoofer present flag */
    gint    stereomode ;        /* stereo downmix mode */
    gint    dualmonomode ;      /* dual mono reproduction mode */
#ifdef KCAPABLE
    gint    kcapablemode ;	/* karaoke capable mode */
#endif
    gfloat  pcmscalefac;        /* PCM scale factor */
    gint    compmode ;		/* compression mode */
    gfloat  dynrngscalelow ;	/* dynamic range scale factor (low) */
    gfloat  dynrngscalehi ;	/* dynamic range scale factor (high) */
    gint    debug_arg ;		/* debug argument */

    gboolean profile;
    gint    demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */

} MfwGstAc3DecInfo;

typedef struct _MfwGstAc3DecInfoClass {
    GstElementClass parent_class;
} MfwGstAc3DecInfoClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_ac3dec_get_type(void);

G_END_DECLS
#endif				/* __MFW_GST_AC3DEC_H__ */

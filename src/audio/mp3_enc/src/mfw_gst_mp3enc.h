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
 * Module Name:    mfw_gst_mp3enc.h
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

#ifndef __MFW_GST_MP3ENC_H__
#define __MFW_GST_MP3ENC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include "mp3_enc_interface.h"
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
#define MFW_GST_TYPE_MP3ENC \
  (mfw_gst_mp3enc_get_type())
#define MFW_GST_MP3ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_MP3ENC,MfwGstMp3EncInfo))
#define MFW_GST_MP3ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_MP3ENC,MfwGstMp3EncInfoClass))
#define MFW_GST_IS_MP3ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_MP3ENC))
#define MFW_GST_IS_MP3ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_MP3ENC))

#define MAXIMUM_INSTANCES 	1          /* maximum number of instances supported*/

typedef struct _MfwGstMp3EncInfo     MfwGstMp3EncInfo;
typedef struct _MfwGstMp3EncInfoClass MfwGstMp3EncInfoClass;

struct _MfwGstMp3EncInfo
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  MP3E_Encoder_Config  enc_config;

  gint32	channels;
  gint32	inputfmt;
  gint32	optmod;
  /*ENGR39447:set caps when data come but not by property*/
  gboolean	encinit;
  
  MP3E_Encoder_Parameter params; 

    guint8* W1[MAXIMUM_INSTANCES]; 
    guint8* W2[MAXIMUM_INSTANCES];
    guint8* W3[MAXIMUM_INSTANCES];
    guint8* W4[MAXIMUM_INSTANCES];
    guint8* W5[MAXIMUM_INSTANCES];
    guint8* W6[MAXIMUM_INSTANCES];

  GstAdapter * adapter;
  guint64 sample_duration;
  gint sample_size;

  gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */
};

struct _MfwGstMp3EncInfoClass 
{
  GstElementClass parent_class;
};

GType mfw_gst_mp3enc_get_type (void);

G_END_DECLS

#endif				/* __MFW_GST_MP3ENC_H__ */

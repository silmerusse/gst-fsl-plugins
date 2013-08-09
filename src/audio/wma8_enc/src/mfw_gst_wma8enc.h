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
 * Module Name:    mfw_gst_wma8enc.h
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Apr, 1 2008 Li Jian<b06256@freescale.com>
 * - Initial version
 *
 */

 


/*===============================================================================
                            INCLUDE FILES
=============================================================================*/

#ifndef __MFW_GST_WMA8ENC_H__
#define __MFW_GST_WMA8ENC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include "wma8_enc_interface.h"
#include "asf.h"
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
#define MFW_GST_TYPE_WMA8ENC \
  (mfw_gst_wma8enc_get_type())
#define MFW_GST_WMA8ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_WMA8ENC,MfwGstWma8EncInfo))
#define MFW_GST_WMA8ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_WMA8ENC,MfwGstWma8EncInfoClass))
#define MFW_GST_IS_WMA8ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_WMA8ENC))
#define MFW_GST_IS_WMA8ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_WMA8ENC))


typedef struct _MfwGstWma8EncInfo     MfwGstWma8EncInfo;
typedef struct _MfwGstWma8EncInfoClass MfwGstWma8EncInfoClass;

struct _MfwGstWma8EncInfo
{
  GstElement element;
  GstPad *sinkpad, *srcpad;
  GstAdapter *adapter;

  WMAEEncoderConfig *psEncConfig;
  ASFParams *psASFParams;

  gint32    sampling_rate;
  gint32    bit_rate;
  gint32	channels;
  gboolean	init;
  gint32    frameSize;
  guint8    *inbuf;
  guint8    *outbuf;
  guint64   total_input;
  gint32    asf_header_size;
  gchar    *title, *author, *desc, *cr, *rating;

  gint byte_rate;
  gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */
};

struct _MfwGstWma8EncInfoClass 
{
  GstElementClass parent_class;
};

GType mfw_gst_wma8enc_get_type (void);

G_END_DECLS

#endif				/* __MFW_GST_WMA8ENC_H__ */

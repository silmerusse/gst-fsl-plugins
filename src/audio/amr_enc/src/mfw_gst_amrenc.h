/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/* GStreamer Adaptive Multi-Rate Narrow-Band (AMR-NB) plugin
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
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

#ifndef __GST_AMRNBENC_H__
#define __GST_AMRNBENC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "nb_amr_enc_api.h"

G_BEGIN_DECLS

#define GST_TYPE_AMRNBENC \
  (gst_amrnbenc_get_type())
#define GST_AMRNBENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMRNBENC, MfwGstAmrnbEnc))
#define GST_AMRNBENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMRNBENC, MfwGstAmrnbEncClass))
#define GST_IS_AMRNBENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AMRNBENC))
#define GST_IS_AMRNBENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AMRNBENC))

typedef struct _MfwGstAmrnbEnc MfwGstAmrnbEnc;
typedef struct _MfwGstAmrnbEncClass MfwGstAmrnbEncClass;

struct _MfwGstAmrnbEnc {
  GstElement element;

  /* pads */
  GstPad *sinkpad, *srcpad;
  GstClockTime ts;
  gboolean discont;

  GstAdapter *adapter;

  /* input settings */
  sAMREEncoderConfigType   *psEncConfig; /* Pointer to encoder config structure */
  gint                  bandmode;
  gchar                 *mode_str;
  NBAMR_S16             dtx_flag;
  NBAMR_U8              bitstream_format;
  NBAMR_U8              number_frame;    /* number of speech frame to be encoded 
                                          * in single call of EncodeFrame */
                                          
  gint channels, rate;
  gint duration;
};

struct _MfwGstAmrnbEncClass {
  GstElementClass parent_class;
};

GType gst_amrnbenc_get_type (void);

G_END_DECLS

#endif /* __GST_AMRNBENC_H__ */

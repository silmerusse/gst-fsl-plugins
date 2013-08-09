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
 * Module Name:    mfw_gst_fb.h
 *
 * Description:    Header file of Frame buffer related function for GStreamer.
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

#ifndef _MFW_GST_FB_H_
#define _MFW_GST_FB_H_

#include <gst/gst.h>

#ifdef USE_X11
#include "mfw_gst_xlib.h"
#undef LOC_ALPHA_SUPPORT
#endif
#include "mfw_gst_v4lsink.h"

gboolean mfw_gst_set_gbl_alpha(gint fb, gint alphaVal);
gboolean mfw_gst_fb0_set_colorkey(gint fb,gulong *colorSrc);
gboolean mfw_gst_fb0_open(gint *fb);
gboolean mfw_gst_fb0_close(gint *fb);

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
gboolean mfw_gst_v4l2_localpha_open(MFW_GST_V4LSINK_INFO_T *v4l);
void mfw_gst_v4l2_localpha_close(MFW_GST_V4LSINK_INFO_T *v4l);
void mfw_gst_v4l2_disable_local_alpha(MFW_GST_V4LSINK_INFO_T *v4l);
void mfw_gst_v4l2_disable_global_alpha(MFW_GST_V4LSINK_INFO_T *v4l);
void mfw_gst_v4l2_enable_local_alpha(MFW_GST_V4LSINK_INFO_T *v4l);
void mfw_gst_v4l2_set_local_alpha(MFW_GST_V4LSINK_INFO_T *v4l, gint alpha);
void mfw_gst_v4l2_set_global_alpha(MFW_GST_V4LSINK_INFO_T *v4l, gint alpha);
void mfw_gst_v4l2_set_alpha(MFW_GST_V4LSINK_INFO_T *v4l);
void mfw_gst_v4l2_set_alpha_enable(MFW_GST_V4LSINK_INFO_T *v4l, gint newmask);


#endif



#endif				/* _MFW_GST_FB_H_ */

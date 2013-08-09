/*
 * Copyright (c) 2012, Freescale Semiconductor, Inc. All rights reserved. 
 * Based on gststructure.h by
 * Copyright (C) 2003 David A. Schleef <ds@schleef.org>
*/

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Module Name:   gstnext.h 
 *
 * Description:   gstreamer core help functions from later gstreamer
 *
 * Portability:   This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#ifndef __GSTNEXT_H__
#define __GSTNEXT_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#if (!GST_CHECK_VERSION(0, 10, 36))
gboolean
gst_structure_is_subset (const GstStructure * subset,
    const GstStructure * superset);
#endif

#if (!GST_CHECK_VERSION(0, 10, 30))
typedef enum
{
  GST_VIDEO_FORMAT_RGB16 = 30,
} GstVideoFormatNext;
#define gst_video_format_parse_caps gst_video_format_parse_caps_next
#define gst_video_format_get_size gst_video_format_get_size_next

gboolean gst_video_format_parse_caps_next (const GstCaps * caps,
    GstVideoFormat * format, int *width, int *height);
int gst_video_format_get_size_next (GstVideoFormat format, int width,
    int height);
#endif


#endif /* #ifndef __GSTNEXT_H__ */

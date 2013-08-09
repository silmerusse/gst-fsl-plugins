/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    gstbufmeta.c
 *
 * Description:    Implementation of Multimedia Buffer management for Gstreamer
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 *   Initialization version 
 *
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#include "gstbufmeta.h"

GType
gst_buffer_meta_get_type (void)
{
  static GType _gst_buffer_meta_type = 0;

  if (G_UNLIKELY (_gst_buffer_meta_type == 0))
  {
    _gst_buffer_meta_type =
        g_boxed_type_register_static ("GstBufMeta",
                                      (GBoxedCopyFunc) gst_buffer_meta_copy,
                                      (GBoxedFreeFunc) gst_buffer_meta_free);
  }
  
  return _gst_buffer_meta_type;
}

GstBufferMeta *
gst_buffer_meta_new  ()
{
  GstBufferMeta *meta = g_slice_new0(GstBufferMeta);
  meta->type = GST_TYPE_BUFFER_META;
  return meta;
}

GstBufferMeta *
gst_buffer_meta_copy (const GstBufferMeta *meta)
{
  if (G_LIKELY (meta != NULL))
    return g_slice_dup (GstBufferMeta, meta);
  return NULL;
}

void
gst_buffer_meta_free (GstBufferMeta       *meta)
{
  if (G_LIKELY (meta != NULL))
    g_slice_free (GstBufferMeta, meta);
}


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
 * Module Name:    gstbufmeta.h
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

#ifndef _GST_BUFFER_META_H_
#define _GST_BUFFER_META_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define GST_TYPE_BUFFER_META (gst_buffer_meta_get_type())
#define GST_BUFFER_META_CAST(obj)                    ((GstBufferMeta *)(obj))
#define GST_IS_BUFFER_META(obj) ((obj) != NULL ? *((GType*)(obj)) == GST_TYPE_BUFFER_META : FALSE)
#define GST_BUFFER_META(obj) (GST_IS_BUFFER_META(obj) ? (GstBufferMeta*)(obj) : NULL)
#define GST_BUFFER_META_PRIVOBJ(buf)			(GST_BUFFER_META_CAST(buf)->priv)
typedef struct _GstBufferMeta GstBufferMeta;

struct _GstBufferMeta {
  GType type;
  gpointer physical_data;
  void * priv; /* caller defined priv */
};

GType gst_buffer_meta_get_type (void);

GstBufferMeta *gst_buffer_meta_new  ();
GstBufferMeta *gst_buffer_meta_copy (const GstBufferMeta *meta);
void              gst_buffer_meta_free (GstBufferMeta    *meta);

G_END_DECLS

#endif /* _GST_META_BUFFER_H_ */


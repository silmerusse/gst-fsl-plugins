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
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. All rights reserved. 
 *
 */


/*
 * Module Name:    gstsutils.h
 *
 * Description:    simple utils head file for gst plugins
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */

#include <gst/gst.h>

#ifndef __GST_SUTILS__
#define __GST_SUTILS__

#define G_MININT_STR "0x80000000"
#define G_MAXINT_STR "0x7fffffff"

#define G_MINUINT_STR "0"
#define G_MAXUINT_STR "0xffffffff"

#define G_MININT64_STR "0x8000000000000000"
#define G_MAXINT64_STR "0x7fffffffffffffff"

#define G_MINUINT64_STR "0"
#define G_MAXUINT64_STR "0xffffffffffffffffU"

typedef GType (*gtype_func) (void);

typedef struct
{
  gint id;
  gchar *name;
  gchar *nickname;
  gchar *desc;
  GType gtype;
  int offset;
  char *def;
  char *min;
  char *max;
  gtype_func typefunc;
} GstsutilsOptionEntry;

void
gstsutils_options_install_properties_by_options (GstsutilsOptionEntry * table,
    GObjectClass * oclass);
gboolean
gstsutils_options_get_option (GstsutilsOptionEntry * table, gchar * option,
    guint id, GValue * value);
gboolean gstsutils_options_set_option (GstsutilsOptionEntry * table,
    gchar * option, guint id, const GValue * value);
void gstsutils_options_load_default (GstsutilsOptionEntry * table,
    gchar * option);
gboolean gstsutils_options_load_from_keyfile (GstsutilsOptionEntry * table,
    gchar * option, gchar * filename, gchar * group);

gboolean
gstsutils_elementutil_get_int (gchar * filename, gchar * group, gchar * field,
    gint * value);


#endif

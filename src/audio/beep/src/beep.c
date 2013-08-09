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
 * Module Name:    beep.c
 *
 * Description:    Registration of universal audio decoder gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */
/*
 * Changelog:
 *
 */
#include "beepdec.h"
#include "mfw_gst_utils.h"
#include "gstsutils/gstsutils.h"

#define ELEMENT_MODULE_NAME0 "beepdec"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gint rank;
  BeepCoreDlEntry *entries, *entry;

  beep_register_external_typefinders (plugin);

  if (FALSE == gstsutils_elementutil_get_int (FSL_GST_CONF_DEFAULT_FILENAME,
          ELEMENT_MODULE_NAME0, "rank", &rank)) {
    rank = (FSL_GST_DEFAULT_DECODER_RANK - 1);
  }
  if (!gst_element_register
      (plugin, ELEMENT_MODULE_NAME0, rank, GST_TYPE_BEEPDEC))
    return FALSE;

  entry = entries = beep_get_core_entry ();

  while (entry) {
    gchar *modulename =
        g_strdup_printf (ELEMENT_MODULE_NAME0 ".%s", entry->name);
    gboolean ret = gst_element_register (plugin, modulename, entry->rank,
        gst_beepdec_subelement_get_type (entry));

    g_free (modulename);

    entry = entry->next;
  }

  return TRUE;
}

FSL_GST_PLUGIN_DEFINE ("beep", "universal audio codec", plugin_init);

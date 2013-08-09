/*
 * Copyright (c) 2012, Freescale Semiconductor, Inc. All rights reserved. 
 * Based on gststructure.c by
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
 * Module Name:   gstnext.c 
 *
 * Description:   gstreamer core help functions from later gstreamer
 *
 * Portability:   This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#include <gst/gst.h>
#include "gstnext.h"

#if (!GST_CHECK_VERSION(0, 10, 36))
static gboolean
gst_caps_structure_is_subset_field (GQuark field_id, const GValue * value,
    gpointer user_data)
{
  GstStructure *superset = user_data;
  const GValue *other;
  int comparison;

  if (!(other = gst_structure_id_get_value (superset, field_id)))
    /* field is missing in the superset => is subset */
    return TRUE;

  comparison = gst_value_compare (other, value);

  /* equal values are subset */
  if (comparison == GST_VALUE_EQUAL)
    return TRUE;

  /* ordered, but unequal, values are not */
  if (comparison != GST_VALUE_UNORDERED)
    return FALSE;

  /*
   * 1 - [1,2] = empty
   * -> !subset
   *
   * [1,2] - 1 = 2
   *  -> 1 - [1,2] = empty
   *  -> subset
   *
   * [1,3] - [1,2] = 3
   * -> [1,2] - [1,3] = empty
   * -> subset
   *
   * {1,2} - {1,3} = 2
   * -> {1,3} - {1,2} = 3
   * -> !subset
   *
   *  First caps subtraction needs to return a non-empty set, second
   *  subtractions needs to give en empty set.
   *  Both substractions are switched below, as it's faster that way.
   */
  if (!gst_value_can_subtract (value, other)) {
    if (gst_value_can_subtract (other, value)) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * gst_structure_is_subset:
 * @subset: a #GstStructure
 * @superset: a potentially greater #GstStructure
 *
 * Checks if @subset is a subset of @superset, i.e. has the same
 * structure name and for all fields that are existing in @superset,
 * @subset has a value that is a subset of the value in @superset.
 *
 * Returns: %TRUE if @subset is a subset of @superset
 *
 * Since: 0.10.36
 */
gboolean
gst_structure_is_subset (const GstStructure * subset,
    const GstStructure * superset)
{
  if ((superset->name != subset->name) ||
      (gst_structure_n_fields (superset) > gst_structure_n_fields (subset)))
    return FALSE;

  return gst_structure_foreach ((GstStructure *) subset,
      gst_caps_structure_is_subset_field, (gpointer) superset);
}

#endif

#if (!GST_CHECK_VERSION(0, 10, 30))
#undef gst_video_format_parse_caps

gboolean
gst_video_format_parse_caps_next (const GstCaps * caps, GstVideoFormat * format,
    int *width, int *height)
{
  gboolean ret;
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  if (gst_structure_has_name (structure, "video/x-raw-rgb")) {
    gint depth, bpp, red_mask, green_mask, blue_mask;
    if ((gst_structure_get_int (structure, "depth", &depth))
        && (gst_structure_get_int (structure, "bpp", &bpp))
        && (gst_structure_get_int (structure, "red_mask", &red_mask))
        && (gst_structure_get_int (structure, "green_mask", &green_mask))
        && (gst_structure_get_int (structure, "blue_mask", &blue_mask))
        && (gst_structure_get_int (structure, "width", width))
        && (gst_structure_get_int (structure, "height", height))) {
      if ((depth == 16) && (bpp == 16) && (red_mask == 0xf800)
          && (green_mask == 0x07e0) && (blue_mask == 0x001f)) {
        *format = GST_VIDEO_FORMAT_RGB16;
        return TRUE;
      }
    }
  }
  return gst_video_format_parse_caps (caps, format, width, height);
}

#undef gst_video_format_get_size
int
gst_video_format_get_size_next (GstVideoFormat format, int width, int height)
{
  int ret = 0;
  switch (format) {
    case (GST_VIDEO_FORMAT_RGB16):
      ret = 2 * width * height;
      break;
    default:
      ret = gst_video_format_get_size (format, width, height);
      break;
  };
  return ret;
}

#endif

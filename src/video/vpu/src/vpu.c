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
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
 * Module Name:    vpu.c
 *
 * Description:    Registration of universal video decoder gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */
/*
 * Changelog: 
 *
 */
#include "vpudec.h"
#include "vpuenc.h"


static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register
      (plugin, "vpudec", FSL_GST_RANK_HIGH, GST_TYPE_VPUDEC))
    return FALSE;
  if (!gst_element_register
      (plugin, "vpuenc", FSL_GST_RANK_HIGH, GST_TYPE_VPUENC))
    return FALSE;
  return TRUE;
}

FSL_GST_PLUGIN_DEFINE ("vpu", "VPU-based video codec", plugin_init);

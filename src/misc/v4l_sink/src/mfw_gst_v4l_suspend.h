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
 * Module Name:    mfw_gst_supsend.h
 *
 * Description:    Header file of V4L suspend/resume function for GStreamer.
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

#ifndef _MFW_GST_SUSPEND_H_
#define _MFW_GST_SUSPEND_H_

#include "mfw_gst_v4lsink.h"

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_get_runinfo

DESCRIPTION:        This function check the suspend/resume flag to decide stop
                    the video and clear the v4l framebuffer or not.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4lsink_get_runinfo(MFW_GST_V4LSINK_INFO_T * v4l_info);

#endif				/* _MFW_GST_SUSPEND_H_ */

/*
 *  Copyright (c) 2009-2012, Freescale Semiconductor, Inc.
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
 * Module Name:    mfw_gst_v4l_xlib.h
 *
 * Description:    Header file of v4l xlib related funciton for GStreamer.
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

#ifndef _MFW_GST_V4L_XLIB_H_
#define _MFW_GST_V4L_XLIB_H_

#include <gst/gst.h>
#include "mfw_gst_xlib.h"
#include "mfw_gst_v4lsink.h"

/*=============================================================================
FUNCTION:           mfw_gst_xv4l2_get_geometry

DESCRIPTION:        This function get the geometry of Xwindow and set the 
                     return the parameter flag.

ARGUMENTS PASSED:
        v4l_info  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       PARAM_SET(SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
PARAM_SET mfw_gst_xv4l2_get_geometry(MFW_GST_V4LSINK_INFO_T * v4l_info);


/*=============================================================================
FUNCTION:           mfw_gst_xv4l2_refresh_geometry

DESCRIPTION:        This function save the latest geometry of Xwindow.

ARGUMENTS PASSED:
        v4l_info  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_xv4l2_refresh_geometry(MFW_GST_V4LSINK_INFO_T * v4l_info);

/*=============================================================================
FUNCTION:           mfw_gst_xv4l2_set_color

DESCRIPTION:        This function set the window color and draw the borders.

ARGUMENTS PASSED:
        v4l_info  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_xv4l2_set_color (MFW_GST_V4LSINK_INFO_T *v4l_info);

/*=============================================================================
FUNCTION:           mfw_gst_xv4l2_clear_color

DESCRIPTION:        This function clear the window color.

ARGUMENTS PASSED:
        v4l_info  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_xv4l2_clear_color (MFW_GST_V4LSINK_INFO_T *v4l_info);

/*=============================================================================
FUNCTION:           mfw_gst_xv4l2_event_thread

DESCRIPTION:        This function handle the X11 events.

ARGUMENTS PASSED:
        v4l_info  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gpointer mfw_gst_xv4l2_event_thread (MFW_GST_V4LSINK_INFO_T *v4l_info);


#endif				/* _MFW_GST_V4L_XLIB_H_ */

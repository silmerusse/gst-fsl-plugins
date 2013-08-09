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
 * Description:    Header file of X11 related funciton for GStreamer.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */

#ifndef _MFW_GST_V4L_H_
#define _MFW_GST_V4L_H_


/*=============================================================================
                                INCLUDE FILES
=============================================================================*/

#include <gst/gst.h>
#include "mfw_gst_v4lsink.h"

/*=============================================================================
                                MACROS
=============================================================================*/
#define NTSC    0
#define PAL     1
#define DISP_720P    2
#define NV_MODE 3


#ifndef IPU_ROTATE_NONE
#define IPU_ROTATE_NONE     0
#define IPU_ROTATE_180      3
#define IPU_ROTATE_90_LEFT  7
#define IPU_ROTATE_90_RIGHT 4
#endif

#if 0
#if (defined (_MX233) || defined (_MX28)  || (defined (_MX50)))
#define V4L_DEVICE "/dev/video0"
#else
#define V4L_DEVICE "/dev/video16";
#endif
#endif




#define MAX_LATENESS_TIME 1000000000     /* 1 s */


/*=============================================================================
FUNCTION:           mfw_gst_v4l2_input_init

DESCRIPTION:        This function initialise the display device with
                    the specified parameters.

ARGUMENTS PASSED:
        v4l_info  -   pointer to MFW_GST_V4LSINK_INFO_T
        inp_format     -   the display foramt

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4l2_input_init (MFW_GST_V4LSINK_INFO_T * v4l_info,
                                   guint inp_format);

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_display_init

DESCRIPTION:        This function initialise the display device with
                    the crop parameters.

ARGUMENTS PASSED:
        v4l_info  -   pointer to MFW_GST_V4LSINK_INFO_T
        disp_width     -   width to be displayed
        disp_height    -   height to be displayed

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean mfw_gst_v4l2_display_init (MFW_GST_V4LSINK_INFO_T * v4l_info,
                           guint disp_width, guint disp_height);

#if defined(ENABLE_TVOUT) && (defined (_MX31) || defined (_MX35))
/*=============================================================================
FUNCTION:           mfw_gst_v4l2_mx31_mx35_set_lcdmode

DESCRIPTION:        This function set the lcd mode.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4l2_mx31_mx35_set_lcdmode (MFW_GST_V4LSINK_INFO_T *
                                             v4l_info);
#endif

#if defined(ENABLE_TVOUT) && (defined (_MX37) || defined (_MX51))
/*=============================================================================
FUNCTION:           mfw_gst_v4l2_mx37_mx51_tv_setblank

DESCRIPTION:        This function set the TV-out to blank.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4l2_mx37_mx51_tv_setblank (MFW_GST_V4LSINK_INFO_T *
                                             v4l_info);
#endif

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_tv_init

DESCRIPTION:        This function initialise the TV out.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4l2_tv_init (MFW_GST_V4LSINK_INFO_T * v4l_info);


/*=============================================================================
FUNCTION:           mfw_gst_v4l2_set_filed

DESCRIPTION:        This function set field parameters of v4l

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4l2_set_field (MFW_GST_V4LSINK_INFO_T * v4l_info,
                                 GstCaps * caps);

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_streamon

DESCRIPTION:        This function set the v4l2 to streamon

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean mfw_gst_v4l2_streamon (MFW_GST_V4LSINK_INFO_T * v4l_info);

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_streamoff

DESCRIPTION:        This function set the v4l2 to streamoff

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean mfw_gst_v4l2_streamoff (MFW_GST_V4LSINK_INFO_T * v4l_info);

#endif /* _MFW_GST_V4L_H_ */

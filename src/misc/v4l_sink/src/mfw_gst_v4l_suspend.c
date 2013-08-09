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
 * Module Name:    mfw_gst_v4l_suspend.c
 *
 * Description:    Implementation of V4L Sink Plugin for Gstreamer
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

#include <stdio.h>
#include <stdlib.h>

#include <gst/gst.h>
#include "mfw_gst_utils.h"
#include "mfw_gst_v4l_buffer.h"
#include "mfw_gst_v4l_suspend.h"

typedef struct v4l_run_info {
    guint8 v4l_id;  /* v4l handle id */
    guint8 pm_mode; /* 0:Normal, 1:suspend, 2: resume */
}V4L_RUN_INFO;

/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/

/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/
/* None */


/*=============================================================================
                            GLOBAL FUNCTIONS
=============================================================================*/

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_suspend

DESCRIPTION:        This function Clear the V4L framebuffer and 
                      set the streamon flag to FALSE.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

void mfw_gst_v4l2_suspend(MFW_GST_V4LSINK_INFO_T *v4l_info)
{
    gint type;
    int i;
    /* swicth off the video stream */
    v4l_info->suspend = TRUE;
    g_print("SUSPEND\n");
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (v4l_info->stream_on){
        v4l_info->stream_on = FALSE;
        mfw_gst_v4l2_clear_showingbuf(v4l_info);
        v4l_info->qbuff_count = 0;
    }
    return;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_resume

DESCRIPTION:        This function Clear the V4L framebuffer and 
                      set the streamon flag to FALSE.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

void mfw_gst_v4l2_resume(MFW_GST_V4LSINK_INFO_T *v4l_info)
{
    int i;
    g_print("RESUME\n");
    if (v4l_info->stream_on){
 
        v4l_info->stream_on = FALSE;
		
        mfw_gst_v4l2_clear_showingbuf(v4l_info);

        v4l_info->qbuff_count = 0;
    }
    v4l_info->suspend = FALSE;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_clear_runinfo

DESCRIPTION:        This function reset the suspend/resume flag.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static gboolean mfw_gst_v4lsink_clear_runinfo(MFW_GST_V4LSINK_INFO_T * v4l_sink_info)
{
    FILE *fp;
    char command[32]; /* v4l_id:powerstate */
    strcpy(command, "echo \"00\" > /tmp/v4l.pid");
    system(command);
    return TRUE;
}

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

gboolean mfw_gst_v4lsink_get_runinfo(MFW_GST_V4LSINK_INFO_T * v4l_info)
{
    FILE *fp;
    V4L_RUN_INFO run_info; 
    if (v4l_info->is_paused == TRUE)
        return FALSE;
    /* FIXME: Not support for multi-instance */
    memset(&run_info,0, sizeof(V4L_RUN_INFO));
    fp = fopen("/tmp/v4l.pid","r");
    if (fp == NULL)
        return FALSE;

    fread((char *)&run_info,sizeof(V4L_RUN_INFO),1,fp);
    GST_DEBUG("%c,%c",run_info.v4l_id,run_info.pm_mode);
    fclose(fp);
    if (run_info.pm_mode == '1') { // suspend
       mfw_gst_v4l2_suspend(v4l_info);
	   v4l_info->suspend = TRUE;
    }
    else if (run_info.pm_mode == '2') { // resume
       
       mfw_gst_v4l2_resume(v4l_info);
       mfw_gst_v4lsink_clear_runinfo(v4l_info);

    }
    return TRUE;
}


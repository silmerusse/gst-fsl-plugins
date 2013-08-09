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
 * Module Name:    mfw_gst_v4l_xlib.c
 *
 * Description:    Implementation of V4L related xlib functions for Gstreamer
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

#include <gst/gst.h>
#include "mfw_gst_utils.h"
#include "mfw_gst_v4l_xlib.h"






/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/

/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/
GST_DEBUG_CATEGORY_EXTERN (mfw_gst_v4lsink_debug);
#define GST_CAT_DEFAULT mfw_gst_v4lsink_debug


/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/


/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

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


PARAM_SET
mfw_gst_xv4l2_get_geometry(MFW_GST_V4LSINK_INFO_T * v4l_info)
{
    PARAM_SET ret = PARAM_NULL;
    gint x=0, y=0;
    gint width = 0,height = 0;
    GstXInfo *gstXInfo = v4l_info->gstXInfo;

    if ((!gstXInfo) || (!gstXInfo->xwindow))
        return FALSE;

    if (!mfw_gst_x11_get_geometry(gstXInfo))
       return ret;
    
    width = gstXInfo->xwindow->width;
    height = gstXInfo->xwindow->height;    

    if ( (width<16) || (height<16)) {
        GST_WARNING("Display window is :%d,%d,ignore it.",width, height);
        return ret;
    }

    x = gstXInfo->xwindow->x;
    y = gstXInfo->xwindow->y;

    GST_INFO("%s:called(x,y,width,height): (%d,%d,%d,%d)",__FUNCTION__,x,y,width,height);

    if(v4l_info->axis_left != x
        || v4l_info->axis_top != y) {

        GST_INFO("set v4l param.");
        ret |= PARAM_SET_V4L;

    }

    if ( v4l_info->disp_width != width
    || v4l_info->disp_height != height) {
        GST_INFO("set v4l param and color key.\n");
        ret |=  PARAM_SET_COLOR_KEY | PARAM_SET_V4L;
    }

    GST_INFO("%s:return %x.",__FUNCTION__,ret);
    return ret;
}




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

gboolean
mfw_gst_xv4l2_refresh_geometry(MFW_GST_V4LSINK_INFO_T * v4l_info)
{
    GstXInfo *gstXInfo = v4l_info->gstXInfo;

    if ((!gstXInfo) || (!gstXInfo->xwindow))
        return FALSE;


    while (1) {
        mfw_gst_x11_get_geometry(gstXInfo);
        
        if ((v4l_info->axis_left == gstXInfo->xwindow->x) 
            &&(v4l_info->axis_top == gstXInfo->xwindow->y )
            && (v4l_info->disp_width == gstXInfo->xwindow->width) 
            && (v4l_info->disp_height == gstXInfo->xwindow->height))
            break;
        v4l_info->axis_left = gstXInfo->xwindow->x;
        v4l_info->axis_top = gstXInfo->xwindow->y;
        v4l_info->disp_width = gstXInfo->xwindow->width;
        v4l_info->disp_height = gstXInfo->xwindow->height;
        /* 
         * FixME:The video window need some tiem to be stale in this case,
         * Wait for 10 milliseconds can make it work 
         * Still need a way to confirm the window is complete 
         */
        usleep(10000);
    }
    GST_INFO("%s:called(x,y,width,height): (%d,%d,%d,%d)",__FUNCTION__, v4l_info->axis_left
        ,v4l_info->axis_top ,v4l_info->disp_width,v4l_info->disp_height);
    
    return TRUE;
}

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

gboolean mfw_gst_xv4l2_set_color (MFW_GST_V4LSINK_INFO_T *v4l_info)
{
    GstXInfo *gstXInfo = v4l_info->gstXInfo;
    struct v4l2_crop * crop = &v4l_info->crop;

    gint width,height;
    gint lw, lh, rw, rh;

    if ((!gstXInfo) || (!gstXInfo->xwindow))
        return FALSE;

    GST_DEBUG("crop->c.left:%d,v4l_info->axis_left:%d,v4l_info->disp_width:%d,"
        " crop->c.width:%d, crop->c.top:%d,v4l_info->axis_top:%d, "
        " v4l_info->disp_height:%d, crop->c.height:%d",
        crop->c.left, v4l_info->axis_left,
        v4l_info->disp_width, crop->c.width, crop->c.top, v4l_info->axis_top, 
        v4l_info->disp_height, crop->c.height);
    
    lw = crop->c.left - v4l_info->axis_left;
    rw = v4l_info->disp_width-lw-crop->c.width;

    lw = (lw < 0) ? 0: lw;
    rw = (rw < 0) ? 0: rw;

    lh = crop->c.top - v4l_info->axis_top;
    rh = v4l_info->disp_height-lh - crop->c.height;

    lh = (lh < 0) ? 0: lh;
    rh = (rh < 0) ? 0: rh;



    width = crop->c.width;
    height = crop->c.height;

    GST_INFO("Set color and borders: (lw,lh,rw,rh,width,height,disp_width,"
        "disp_height) :(%d,%d,%d,%d,%d,%d,%d,%d\n",lw, lh, rw, rh,
        width, height, v4l_info->disp_width, v4l_info->disp_height);
    
    mfw_gst_x11_set_color_borders(gstXInfo, v4l_info->colorSrc,lw, lh, rw, rh, 
        width, height, v4l_info->disp_width, v4l_info->disp_height);


}

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
gboolean mfw_gst_xv4l2_clear_color (MFW_GST_V4LSINK_INFO_T *v4l_info)
{
    GstXInfo *gstXInfo = v4l_info->gstXInfo;
    struct v4l2_crop * crop = &v4l_info->crop;

    PARAM_SET param = PARAM_NULL;
    gint width,height;
    gint lw, lh, rw, rh;
    gulong color;

    
    if (v4l_info->stream_on == TRUE)
        return FALSE;

    if ((!gstXInfo) || (!gstXInfo->xwindow))
        return FALSE;


    GST_DEBUG("crop->c.left:%d,v4l_info->axis_left:%d,v4l_info->disp_width:%d,"
        " crop->c.width:%d, crop->c.top:%d,v4l_info->axis_top:%d, "
        " v4l_info->disp_height:%d, crop->c.height:%d",
        crop->c.left, v4l_info->axis_left,
        v4l_info->disp_width, crop->c.width, crop->c.top, v4l_info->axis_top, 
        v4l_info->disp_height, crop->c.height);

    lw = crop->c.left - v4l_info->axis_left;
    rw = v4l_info->disp_width-lw-crop->c.width;

    lw = (lw < 0) ? 0: lw;
    rw = (rw < 0) ? 0: rw;

    lh = crop->c.top - v4l_info->axis_top;
    rh = v4l_info->disp_height-lh - crop->c.height;

    lh = (lh < 0) ? 0: lh;
    rh = (rh < 0) ? 0: rh;


    width = crop->c.width;
    height = crop->c.height;
    
    mfw_gst_x11_clear_color(gstXInfo, lw, lh, width, height);

}

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
gpointer
mfw_gst_xv4l2_event_thread (MFW_GST_V4LSINK_INFO_T *v4l_info)
{
    while (v4l_info->gstXInfo->running) {
        if (!v4l_info->flow_lock)
            return NULL;
        g_mutex_lock(v4l_info->flow_lock);
       
        if ( (v4l_info->gstXInfo) && (v4l_info->gstXInfo->xwindow) &&
            (v4l_info->gstXInfo->xwindow->win != 0))  {

            g_mutex_unlock(v4l_info->flow_lock);
            /* Release the mutex which could be used in navigation event */
            if ( (v4l_info->setXid) || (mfw_gst_x11_handle_xevents (v4l_info->gstXInfo))) {

                PARAM_SET param = PARAM_NULL;
                g_mutex_lock(v4l_info->flow_lock);
                param = mfw_gst_xv4l2_get_geometry(v4l_info);
#ifdef _MX6
                if ((param & PARAM_SET_V4L) == PARAM_SET_V4L)
                {
                    mfw_gst_xv4l2_refresh_geometry (v4l_info);
                    mfw_gst_v4l2_set_crop(v4l_info, v4l_info->disp_width, v4l_info->disp_height);
                }
#endif
                v4l_info->setpara |= param;
                v4l_info->setXid = FALSE;
                g_mutex_unlock(v4l_info->flow_lock);
                GST_DEBUG("%s:set param to %x.",__FUNCTION__, v4l_info->setpara);
                g_mutex_lock(v4l_info->flow_lock);

                /* 
                 * Without set the window color, if the display area
                 * colorkey has been changed by others, the display will
                 * be wrong.
                 *
                 */
                if (v4l_info->init)
                    mfw_gst_xv4l2_set_color(v4l_info);    

            }
            else {
                g_mutex_lock(v4l_info->flow_lock);

            }
        }
        
        g_mutex_unlock(v4l_info->flow_lock);
        
        g_usleep (10000);
    }

    GST_INFO("event thread exit.\n");
    
    return NULL;
}




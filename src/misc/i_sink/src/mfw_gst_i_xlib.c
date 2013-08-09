/*
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. 
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
#include "mfw_gst_i_xlib.h"






/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/

/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/
/* None */

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/


/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
FUNCTION:           mfw_gst_xisink_get_geometry

DESCRIPTION:        This function get the geometry of Xwindow and set the 
                     return the parameter flag.

ARGUMENTS PASSED:
        isink  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       PARAM_SET(SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/


PARAM_SET
mfw_gst_xisink_get_geometry (MfwGstISink * isink)
{
  PARAM_SET ret = PARAM_NULL;
  gint x = 0, y = 0;
  gint width = 0, height = 0;
  GstXInfo *gstXInfo = isink->gstXInfo;

  if ((!gstXInfo) || (!gstXInfo->xwindow))
    return FALSE;

  if (!mfw_gst_x11_get_geometry (gstXInfo))
    return ret;

  width = gstXInfo->xwindow->width;
  height = gstXInfo->xwindow->height;

  if ((width < 16) || (height < 16)) {
    GST_WARNING ("Display window is :%d,%d,ignore it.\n", width, height);
    return ret;
  }

  x = gstXInfo->xwindow->x;
  y = gstXInfo->xwindow->y;

  if (isink->ocfg[0].desfmt.rect.left != x
      || isink->ocfg[0].desfmt.rect.top != y) {

    GST_INFO ("set v4l param.\n");
    ret |= PARAM_SET_V4L;

  }

  if ((isink->ocfg[0].desfmt.rect.right - isink->ocfg[0].desfmt.rect.left) !=
      width
      || (isink->ocfg[0].desfmt.rect.bottom - isink->ocfg[0].desfmt.rect.top) !=
      height) {
    GST_INFO ("set v4l param and color key.\n");
    ret |= PARAM_SET_COLOR_KEY | PARAM_SET_V4L;
  }

  GST_INFO ("%s:return %x.\n", __FUNCTION__, ret);
  return ret;
}




/*=============================================================================
FUNCTION:           mfw_gst_xisink_refresh_geometry

DESCRIPTION:        This function save the latest geometry of Xwindow.

ARGUMENTS PASSED:


RETURN VALUE:       

PRE-CONDITIONS:     None
isink  -  Pointer to MFW_GST_V4LSINK_INFO_T
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_xisink_refresh_geometry (MfwGstISink * isink, Rect * rect)
{
  GstXInfo *gstXInfo = isink->gstXInfo;

  if ((!gstXInfo) || (!gstXInfo->xwindow))
    return FALSE;

  while (1) {
    mfw_gst_x11_get_geometry (gstXInfo);

    if ((rect->left == gstXInfo->xwindow->x)
        && (rect->top == gstXInfo->xwindow->y)
        && (rect->right - rect->left == gstXInfo->xwindow->width)
        && (rect->bottom - rect->top == gstXInfo->xwindow->height))
      break;
    rect->left = gstXInfo->xwindow->x;
    rect->top = gstXInfo->xwindow->y;
    rect->right = gstXInfo->xwindow->width + gstXInfo->xwindow->x;
    rect->bottom = gstXInfo->xwindow->height + gstXInfo->xwindow->y;
    /* 
     * FixME:The video window need some tiem to be stale in this case,
     * Wait for 10 milliseconds can make it work 
     * Still need a way to confirm the window is complete 
     */
    usleep (10000);
  }
  return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_xisink_set_color

DESCRIPTION:        This function set the window color and draw the borders.

ARGUMENTS PASSED:
        isink  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_xisink_set_color (MfwGstISink * isink, Rect * setrect)
{
  GstXInfo *gstXInfo = isink->gstXInfo;
#if 1
  Rect *rect = &isink->ocfg[0].desfmt.rect;

  gint width, height;
  gint lw, lh, rw, rh;

  if ((!gstXInfo) || (!gstXInfo->xwindow))
    return FALSE;


  lw = rect->left - setrect->left;
  rw = (setrect->right - setrect->left) - (rect->right - rect->left);

  lw = (lw < 0) ? 0 : lw;
  rw = (rw < 0) ? 0 : rw;

  lh = rect->top - setrect->top;
  rh = (setrect->bottom - setrect->top) - (rect->bottom - rect->top);

  lh = (lh < 0) ? 0 : lh;
  rh = (rh < 0) ? 0 : rh;



  width = (rect->right - rect->left);
  height = (rect->bottom - rect->top);



  mfw_gst_x11_set_color_borders (gstXInfo, isink->colorkey, lw, lh, rw, rh,
      width, height, (setrect->right - setrect->left),
      (setrect->bottom - setrect->top));
#endif

}

/*=============================================================================
FUNCTION:           mfw_gst_xisink_clear_color

DESCRIPTION:        This function clear the window color.

ARGUMENTS PASSED:
        isink  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
mfw_gst_xisink_clear_color (MfwGstISink * isink, Rect * newrect)
{
  GstXInfo *gstXInfo = isink->gstXInfo;
#if 1

  Rect *oldrect = &isink->ocfg[0].desfmt.rect;
  PARAM_SET param = PARAM_NULL;
  gint width, height;
  gint lw, lh, rw, rh;
  gulong color;



  if ((!gstXInfo) || (!gstXInfo->xwindow))
    return FALSE;


  lw = oldrect->left - newrect->left;
  rw = (newrect->right - newrect->left) - (oldrect->right - oldrect->left);

  lw = (lw < 0) ? 0 : lw;
  rw = (rw < 0) ? 0 : rw;

  lh = oldrect->top - newrect->top;
  rh = (newrect->bottom - newrect->top) - (oldrect->bottom - oldrect->top);


  lh = (lh < 0) ? 0 : lh;
  rh = (rh < 0) ? 0 : rh;


  width = (oldrect->right - oldrect->left);
  height = (oldrect->bottom - oldrect->top);

  mfw_gst_x11_clear_color (gstXInfo, lw, lh, width, height);
#endif
}

/*=============================================================================
FUNCTION:           mfw_gst_xisink_event_thread

DESCRIPTION:        This function handle the X11 events.

ARGUMENTS PASSED:
        isink  -  Pointer to MFW_GST_V4LSINK_INFO_T


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gpointer
mfw_gst_xisink_event_thread (MfwGstISink * isink)
{
  while (isink->gstXInfo->running) {

    if ((isink->gstXInfo) && (isink->gstXInfo->xwindow) &&
        (isink->gstXInfo->xwindow->win != 0)) {

      /* Release the mutex which could be used in navigation event */
      if ((mfw_gst_x11_handle_xevents (isink->gstXInfo)) || (isink->setXid)) {

        PARAM_SET param = PARAM_NULL;
        param = mfw_gst_xisink_get_geometry (isink);


        if (param) {
          Rect *newrect = &isink->ocfg[0].origrect;
          mfw_gst_xisink_refresh_geometry (isink, newrect);
          mfw_gst_xisink_clear_color (isink, newrect);

          isink->ocfg[0].desfmt.rect = *newrect;

          if (isink->ocfg[0].vshandle) {
            VSConfig config;
            config.length = sizeof (DestinationFmt);
            config.data = &isink->ocfg[0].desfmt;
            configVideoSurface (isink->ocfg[0].vshandle,
                CONFIG_MASTER_PARAMETER, &config);
            mfw_gst_xisink_set_color (isink, &newrect);

          }
          mfw_gst_xisink_set_color (isink, &newrect);
        }
        isink->setpara |= param;
        isink->setXid = FALSE;

        /* 
         * Without set the window color, if the display area
         * colorkey has been changed by others, the display will
         * be wrong.
         *
         */
        if (isink->init)
          mfw_gst_xisink_set_color (isink, &isink->ocfg[0].origrect);

      } else {

      }
    }


    g_usleep (10000);
  }

  g_print ("event thread exit.\n");

  return NULL;
}

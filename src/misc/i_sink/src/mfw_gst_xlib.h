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
 * Module Name:    mfw_gst_Xlib.h
 *
 * Description:    Header file of X11 related funciton for GStreamer.
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

#ifndef _MFW_GST_XLIB_H_
#define _MFW_GST_XLIB_H_

#include <gst/gst.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/interfaces/navigation.h>

#include <linux/mxcfb.h>


#define FB_DEIVCE "/dev/fb0"



typedef struct _GstXWindow
{
  Window win;                   /* X window id */
  GC gc;                        /* Graphic configuration */
  gint width, height;           /* X window width and height */
  gboolean internal;            /* Created by internal or not */
  gint x, y;                    /* X window axis x and y */
} GstXWindow;

typedef struct _GstXContext
{
  Display *disp;

  Screen *screen;
  gint screen_num;

  Visual *visual;

  Window root;

  gulong white, black;

  gint depth;
  gint bpp;
  gint endianness;

  gint width, height;
  gint widthmm, heightmm;
  GValue *par;                  /* calculated pixel aspect ratio */

  gboolean use_xshm;

  GstCaps *caps;
} GstXContext;


typedef struct _GstXInfo
{

  void *parent;
  gboolean handle_events;       /* Handle the X events or not */
  gboolean running;
  /* Xwindow parameters */
  GstXContext *xcontext;
  GstXWindow *xwindow;

  GThread *event_thread;

  GMutex *x_lock;
} GstXInfo;

typedef struct
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
} MotifWmHints, MwmHints;

#define MWM_HINTS_DECORATIONS   (1L << 1)

/*=============================================================================
FUNCTION:           mfw_gst_x11_xcontext_get

DESCRIPTION:        This function get display context.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GstXContext *mfw_gst_x11_xcontext_get ();

/*=============================================================================
FUNCTION:           mfw_gst_x11_get_geometry

DESCRIPTION:        This function get the geometry information.

ARGUMENTS PASSED:
        gstXInfo  -  Pointer to GstXInfo, the geometry information
                        will be stored in GstXWindow structure.

RETURN VALUE:       TRUE/FALSE (TRUE:Geometry has been changed)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_x11_get_geometry (GstXInfo * gstXInfo);


/*=============================================================================
FUNCTION:           mfw_gst_xinfo_new

DESCRIPTION:        This function create a X info structure.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GstXInfo *mfw_gst_xinfo_new ();

/*=============================================================================
FUNCTION:           mfw_gst_xinfo_free

DESCRIPTION:        This function create a X info structure.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void mfw_gst_xinfo_free (GstXInfo * gstXInfo);

/*=============================================================================
FUNCTION:           mfw_gst_x11_xwindow_new

DESCRIPTION:        This function create a new X window.

ARGUMENTS PASSED:
        gstXInfo  -  Pointer to GstXInfo
        width   -  Width to be displayed
        height  -  Height to be displayed

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GstXWindow *mfw_gst_x11_xwindow_new (GstXInfo * gstXInfo,
    gint width, gint height);

/*=============================================================================
FUNCTION:           mfw_gst_xwindow_destroy

DESCRIPTION:        This function destroy a GstXWindow.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void mfw_gst_x11_xwindow_destroy (GstXInfo * gstXInfo, GstXWindow * xwindow);

/*=============================================================================
FUNCTION:           mfw_gst_x11_clear_color

DESCRIPTION:        This function clear the color for the display screen of Video

ARGUMENTS PASSED:
        gstXInfo  -  Pointer to GstXInfo
        lw           -  The Left border width
        lh           -  The Left border height        
        width        -  The cropped video width        
        height       -  The cropped video height        

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_x11_clear_color (GstXInfo * gstXInfo, gint lw, gint lh,
    gint width, gint height);


/*=============================================================================
FUNCTION:           mfw_gst_x11_set_color_borders

DESCRIPTION:        This function set the color for the display screen of
                    Video and draw the borders with black.
ARGUMENTS PASSED:
        gstXInfo  -  Pointer to GstXInfo
        color        -  The colorkey for video display.
        lw           -  The Left border width
        lh           -  The Left border height        
        rw           -  The Right border width        
        rh           -  The Right border height        
        width        -  The cropped video width        
        height       -  The cropped video height        
        disp_width   -  Width to be displayed
        disp_height  -  Height to be displayed

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_x11_set_color_borders (GstXInfo * gstXInfo, gulong color,
    gint lw, gint lh, gint rw, gint rh,
    gint width, gint height, gint disp_width, gint disp_height);

#endif /* _MFW_GST_XLIB_H_ */

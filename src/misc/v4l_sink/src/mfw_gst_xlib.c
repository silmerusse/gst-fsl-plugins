/*
 * Copyright (C) 2009-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_x1ib.c
 *
 * Description:    Implementation of XLib functions for Gstreamer
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

#include <fcntl.h>

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <linux/videodev2.h>

#ifdef USE_X11
#include <linux/mxcfb.h>
#undef LOC_ALPHA_SUPPORT
//#include "mxcfb.h"

#endif

#include "mfw_gst_utils.h"
#include "mfw_gst_xlib.h"





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
FUNCTION:           mfw_gst_x11_handle_xevents

DESCRIPTION:        This function handles XEvents that might be in the queue. 
                    It generates GstEvent that will be sent upstream in the 
                    pipeline to handle interactivity and navigation. 
                    It will also listen for configure events on the window to 
                    trigger caps renegotiation so on the fly software scaling
                    can work.

ARGUMENTS PASSED:
        gstXInfo  -  Pointer to GstXInfo


RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_x11_handle_xevents (void *context)
{
    XEvent e;
    guint pointer_x = 0, pointer_y = 0;
    gboolean pointer_moved = FALSE;
    GstXInfo *gstXInfo = (GstXInfo *) context;

    /* Handle Expose */
    gboolean exposed = FALSE, configured = FALSE;

    if (!gstXInfo->x_lock)
        return FALSE;
    g_mutex_lock (gstXInfo->x_lock);
    while (XCheckWindowEvent (gstXInfo->xcontext->disp,
                              gstXInfo->xwindow->win,
                              ExposureMask | StructureNotifyMask |
                              KeyPressMask | KeyReleaseMask | ButtonPressMask
                              | ButtonReleaseMask, &e)) {
        KeySym keysym;

        g_mutex_unlock (gstXInfo->x_lock);

        switch (e.type) {
        case Expose:
            exposed = TRUE;
            break;
        case ConfigureNotify:
            configured = TRUE;
        case ButtonPress:

            /* Mouse button pressed over our window. We send upstream
               events for interactivity/navigation */
            GST_DEBUG ("v4lsink button %d pressed over window at %d,%d",
                       e.xbutton.button, e.xbutton.x, e.xbutton.y);
            gst_navigation_send_mouse_event (GST_NAVIGATION
                                             (gstXInfo->parent),
                                             "mouse-button-press",
                                             e.xbutton.button, e.xbutton.x,
                                             e.xbutton.y);
            break;
        case ButtonRelease:
            /* Mouse button released over our window. We send upstream
               events for interactivity/navigation */
            GST_DEBUG ("v4lsink button %d released over window at %d,%d",
                       e.xbutton.button, e.xbutton.x, e.xbutton.y);
            gst_navigation_send_mouse_event (GST_NAVIGATION
                                             (gstXInfo->parent),
                                             "mouse-button-release",
                                             e.xbutton.button, e.xbutton.x,
                                             e.xbutton.y);
            break;
        case KeyPress:
        case KeyRelease:
            /* Key pressed/released over our window. We send upstream
               events for interactivity/navigation */
            GST_DEBUG ("v4lsink key %d pressed over window at %d,%d",
                       e.xkey.keycode, e.xkey.x, e.xkey.y);
            g_mutex_lock (gstXInfo->x_lock);
            keysym = XKeycodeToKeysym (gstXInfo->xcontext->disp,
                                       e.xkey.keycode, 0);
            g_mutex_unlock (gstXInfo->x_lock);
            if (keysym != NoSymbol) {
                char *key_str = NULL;

                g_mutex_lock (gstXInfo->x_lock);
                key_str = XKeysymToString (keysym);
                g_mutex_unlock (gstXInfo->x_lock);
                gst_navigation_send_key_event (GST_NAVIGATION
                                               (gstXInfo->parent),
                                               e.type ==
                                               KeyPress ? "key-press" :
                                               "key-release", key_str);
            }
            else {
                gst_navigation_send_key_event (GST_NAVIGATION
                                               (gstXInfo->parent),
                                               e.type ==
                                               KeyPress ? "key-press" :
                                               "key-release", "unknown");
            }
            break;
        default:
            GST_WARNING ("v4lsink unhandled X event (%d)", e.type);
            break;
        }
        g_mutex_lock (gstXInfo->x_lock);
    }
    g_mutex_unlock (gstXInfo->x_lock);

    if (configured) {
        GST_INFO ("Get configure events(%dx%d).", e.xconfigure.width,
                  e.xconfigure.height);
        return TRUE;
    }
    if (exposed) {
        GST_INFO ("Get the expose event.");
        return TRUE;
    }
    return FALSE;
}



/*=============================================================================
FUNCTION:           mfw_gst_x11_set_black

DESCRIPTION:        This function draw the black borders of Video.

ARGUMENTS PASSED:
        gstXInfo  -  Pointer to GstXInfo
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

static gboolean
mfw_gst_x11_set_black_borders (GstXInfo * gstXInfo,
                               gint lw, gint lh, gint rw, gint rh,
                               gint width, gint height,
                               gint disp_width, gint disp_height)
{

    /*
     * FixME: Do we still need this walkaround. 
     * Have removed the Full-screen walkaround for IPU, 
     * if there is anything wrong, please refer the previous code.
     *
     */
    if ((lh == 1) && (rh == 0)) {
        rh = 7;
    }

    if ((lw == 1) && (rw == 0)) {
        rw = 7;
    }


    GST_INFO (BLUE_STR
              ("Set Black Borders (lw,lh,rw,rh) = (%d, %d, %d, %d).\n", lw,
               lh, rw, rh));

    g_mutex_lock (gstXInfo->x_lock);


    XSetForeground (gstXInfo->xcontext->disp, gstXInfo->xwindow->gc,
                    gstXInfo->xcontext->black);


    /* Left border */
    if (lw > 0) {
        XFillRectangle (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                        gstXInfo->xwindow->gc, 0, 0, lw, disp_height);

    }
    /* Right border */
    if (rw > 0) {
        XFillRectangle (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                        gstXInfo->xwindow->gc, lw + width, 0,
                        rw, disp_height);
    }

    /* Top border */
    if (lh > 0) {
        XFillRectangle (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                        gstXInfo->xwindow->gc, 0, 0, disp_width, lh);
    }
    /* Bottom border */
    if (rh > 0) {
        XFillRectangle (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                        gstXInfo->xwindow->gc, 0, lh + height,
                        disp_width, rh);
    }

    XSync (gstXInfo->xcontext->disp, FALSE);

    g_mutex_unlock (gstXInfo->x_lock);

    return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_x11_set_color

DESCRIPTION:        This function set the color for the display screen of Video.

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


static gboolean
mfw_gst_x11_set_color (GstXInfo * gstXInfo, gulong color,
                       gint lw, gint lh, gint width, gint height)
{


    GST_INFO ("set xwindow color :0x%08x in (%dx%d)", color, width, height);

    GST_INFO ("Borders (lw,lh,width, height) = (%d, %d, %d, %d).", lw, lh,
              width, height);

    g_mutex_lock (gstXInfo->x_lock);

    XSetForeground (gstXInfo->xcontext->disp, gstXInfo->xwindow->gc, color);


    XSetBackground (gstXInfo->xcontext->disp, gstXInfo->xwindow->gc,
                    gstXInfo->xcontext->black);

    XClearWindow (gstXInfo->xcontext->disp, gstXInfo->xwindow->win);

    XFillRectangle (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                    gstXInfo->xwindow->gc, lw, lh, width, height);

    XSync (gstXInfo->xcontext->disp, FALSE);

    g_mutex_unlock (gstXInfo->x_lock);


    return TRUE;
}

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


gboolean
mfw_gst_x11_clear_color (GstXInfo * gstXInfo,
                         gint lw, gint lh, gint width, gint height)
{
    GST_INFO (RED_STR
              ("Clear xwindow by set black from (%d,%d) in screen (%d x %d)",
               lw, lh, width, height));

    g_return_if_fail (gstXInfo->xcontext != NULL);
    g_return_if_fail (gstXInfo->xwindow->win != NULL);
    g_return_if_fail (gstXInfo->xwindow->gc != NULL);


    g_mutex_lock (gstXInfo->x_lock);


    XSetForeground (gstXInfo->xcontext->disp, gstXInfo->xwindow->gc,
                    gstXInfo->xcontext->black);


    XFillRectangle (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                    gstXInfo->xwindow->gc, lw, lh, width, height);

    XSync (gstXInfo->xcontext->disp, FALSE);


    g_mutex_unlock (gstXInfo->x_lock);


    return TRUE;
}


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


gboolean
mfw_gst_x11_set_color_borders (GstXInfo * gstXInfo, gulong color,
                               gint lw, gint lh, gint rw, gint rh,
                               gint width, gint height,
                               gint disp_width, gint disp_height)
{



    if ((width == 0) || (height == 0))
        return FALSE;

    GST_INFO (RED_STR
              ("[%s]:set xwindow color :0x%08x in (%dx%d)", __FUNCTION__,
               color, width, height));

    GST_DEBUG ("Borders (lw,lh,rw,rh) = (%d, %d, %d, %d).", lw, lh, rw, rh);



    mfw_gst_x11_set_black_borders (gstXInfo,
                                   lw, lh, rw, rh, width, height, disp_width,
                                   disp_height);


    mfw_gst_x11_set_color (gstXInfo, color, lw, lh, width, height);



    return TRUE;
}


/*=============================================================================
FUNCTION:           mfw_gst_x11_xwindow_decorate

DESCRIPTION:        This function decorate the window.

ARGUMENTS PASSED:
        gstXInfo  -  Pointer to GstXInfo
        window       -  Pointer to GstXWindow        

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_x11_xwindow_decorate (GstXInfo * gstXInfo, GstXWindow * window)
{
    Atom hints_atom = None;
    MotifWmHints *hints;

    g_return_val_if_fail (window != NULL, FALSE);

    g_mutex_lock (gstXInfo->x_lock);

    hints_atom = XInternAtom (gstXInfo->xcontext->disp, "_MOTIF_WM_HINTS",
                              True);
    if (hints_atom == None) {
        g_mutex_unlock (gstXInfo->x_lock);
        return FALSE;
    }

    hints = g_malloc0 (sizeof (MotifWmHints));

    hints->flags |= MWM_HINTS_DECORATIONS;
    hints->decorations = 1 << 0;

    XChangeProperty (gstXInfo->xcontext->disp, window->win,
                     hints_atom, hints_atom, 32, PropModeReplace,
                     (guchar *) hints, sizeof (MotifWmHints) / sizeof (long));

    XSync (gstXInfo->xcontext->disp, FALSE);

    g_mutex_unlock (gstXInfo->x_lock);

    g_free (hints);

    return TRUE;
}


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

gboolean
mfw_gst_x11_get_geometry (GstXInfo * gstXInfo)
{
    gboolean ret = 0;
    XWindowAttributes attr, root_attr;
    gint x = 0, y = 0;
    Window w;


    if (gstXInfo->xcontext == NULL
        || gstXInfo->xwindow->win == 0 || gstXInfo->xwindow->gc == 0)
        return FALSE;


    g_mutex_lock (gstXInfo->x_lock);

    XGetWindowAttributes (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                          &attr);
    XGetWindowAttributes (gstXInfo->xcontext->disp, attr.root, &root_attr);
    XTranslateCoordinates (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
                           attr.root, 0, 0, &x, &y, &w);

    g_mutex_unlock (gstXInfo->x_lock);

    if ((x != gstXInfo->xwindow->x) || (y != gstXInfo->xwindow->y)
        || (attr.width != gstXInfo->xwindow->width)
        || (attr.height != gstXInfo->xwindow->height)) {
        gstXInfo->xwindow->x = x;
        gstXInfo->xwindow->y = y;
        gstXInfo->xwindow->width = attr.width;
        gstXInfo->xwindow->height = attr.height;
        GST_INFO ("%s:Got windows info: x,y,width,height:%d,%d,%d,%d",
                  __FUNCTION__, x, y, attr.width, attr.height);
        return TRUE;

    }
    return FALSE;
}

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

GstXWindow *
mfw_gst_x11_xwindow_new (GstXInfo * gstXInfo, gint width, gint height)
{
    XGCValues values;
    GstXWindow *xwindow = gstXInfo->xwindow;

    GST_DEBUG ("%s: create sink own window.", __FUNCTION__);

    xwindow->width = width;
    xwindow->height = height;
    xwindow->internal = TRUE;

    g_mutex_lock (gstXInfo->x_lock);

    xwindow->win = XCreateSimpleWindow (gstXInfo->xcontext->disp,
                                        gstXInfo->xcontext->root,
                                        0, 0, xwindow->width, xwindow->height,
                                        0, 0, gstXInfo->xcontext->black);

    /* We have to do that to prevent X from redrawing the background on
     * ConfigureNotify. This takes away flickering of video when resizing. */
    XSetWindowBackgroundPixmap (gstXInfo->xcontext->disp, xwindow->win, None);

    if (gstXInfo->handle_events) {
        Atom wm_delete;

        XSelectInput (gstXInfo->xcontext->disp, xwindow->win, ExposureMask |
                      StructureNotifyMask | PointerMotionMask | KeyPressMask |
                      KeyReleaseMask | ButtonPressMask | ButtonReleaseMask);

        /* Tell the window manager we'd like delete client messages instead of
         * being killed */
        wm_delete = XInternAtom (gstXInfo->xcontext->disp,
                                 "WM_DELETE_WINDOW", True);
        if (wm_delete != None) {
            (void) XSetWMProtocols (gstXInfo->xcontext->disp, xwindow->win,
                                    &wm_delete, 1);
        }

    }

    xwindow->gc = XCreateGC (gstXInfo->xcontext->disp,
                             xwindow->win, 0, &values);

    XMapRaised (gstXInfo->xcontext->disp, xwindow->win);

    XSync (gstXInfo->xcontext->disp, FALSE);

    g_mutex_unlock (gstXInfo->x_lock);

    mfw_gst_x11_xwindow_decorate (gstXInfo, xwindow);

    gst_x_overlay_got_xwindow_id (GST_X_OVERLAY (gstXInfo->parent),
                                  xwindow->win);

    return xwindow;
}


/*=============================================================================
FUNCTION:           mfw_gst_x11_xcontext_get

DESCRIPTION:        This function get display context.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

GstXContext *
mfw_gst_x11_xcontext_get ()
{
    GstXContext *xcontext = NULL;
    XPixmapFormatValues *px_formats = NULL;
    gint nb_formats = 0, i;

    xcontext = g_new0 (GstXContext, 1);

    xcontext->disp = XOpenDisplay (NULL);
    if (!xcontext->disp) {
        g_free (xcontext);
        return NULL;
    }

    xcontext->screen = DefaultScreenOfDisplay (xcontext->disp);
    xcontext->screen_num = DefaultScreen (xcontext->disp);
    xcontext->visual = DefaultVisual (xcontext->disp, xcontext->screen_num);
    xcontext->root = DefaultRootWindow (xcontext->disp);
    xcontext->white = XWhitePixel (xcontext->disp, xcontext->screen_num);
    xcontext->black = XBlackPixel (xcontext->disp, xcontext->screen_num);
    xcontext->depth = DefaultDepthOfScreen (xcontext->screen);

    xcontext->width = DisplayWidth (xcontext->disp, xcontext->screen_num);
    xcontext->height = DisplayHeight (xcontext->disp, xcontext->screen_num);
    xcontext->widthmm = DisplayWidthMM (xcontext->disp, xcontext->screen_num);
    xcontext->heightmm =
        DisplayHeightMM (xcontext->disp, xcontext->screen_num);

    GST_DEBUG ("xcontext: width=%d, height=%d", xcontext->width,
               xcontext->height);
    GST_DEBUG ("black: %llx, white: %llx", xcontext->black,
               xcontext->white);

    /* We get supported pixmap formats at supported depth */
    px_formats = XListPixmapFormats (xcontext->disp, &nb_formats);

    if (!px_formats) {
        XCloseDisplay (xcontext->disp);
        g_free (xcontext);
        return NULL;
    }

    /* We get bpp value corresponding to our running depth */
    for (i = 0; i < nb_formats; i++) {
        if (px_formats[i].depth == xcontext->depth)
            xcontext->bpp = px_formats[i].bits_per_pixel;
    }
    XFree (px_formats);

    xcontext->endianness =
        (ImageByteOrder (xcontext->disp) ==
         LSBFirst) ? G_LITTLE_ENDIAN : G_BIG_ENDIAN;

    GST_DEBUG ("depth: %d, bpp: %d, endianess: %d",
               xcontext->depth, xcontext->bpp, xcontext->endianness);

    return xcontext;
}

/*=============================================================================
FUNCTION:           mfw_gst_xcontext_free

DESCRIPTION:        This function free display context structure.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

void
mfw_gst_xcontext_free (GstXInfo * gstXInfo)
{
    GstXContext *xcontext = gstXInfo->xcontext;

    if (xcontext) {
        if (xcontext->disp)
            XCloseDisplay (xcontext->disp);
        g_free (xcontext);
        gstXInfo->xcontext = NULL;
    }

}



/*=============================================================================
FUNCTION:           mfw_gst_xinfo_new

DESCRIPTION:        This function create a X info structure.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

GstXInfo *
mfw_gst_xinfo_new ()
{
    GstXInfo *gstXInfo = NULL;

    gstXInfo = g_new0 (GstXInfo, 1);
    gstXInfo->xwindow = NULL;

    gstXInfo->running = FALSE;
    gstXInfo->xcontext = NULL;
    gstXInfo->x_lock = g_mutex_new ();

    return gstXInfo;

}


/*=============================================================================
FUNCTION:           mfw_gst_xinfo_free

DESCRIPTION:        This function create a X info structure.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

void
mfw_gst_xinfo_free (GstXInfo * gstXInfo)
{
    /* should we release the Xwindow and XContext? */
    if (gstXInfo->x_lock) {
        g_mutex_lock (gstXInfo->x_lock);
        g_mutex_unlock (gstXInfo->x_lock);
        g_mutex_free (gstXInfo->x_lock);
        gstXInfo->x_lock = NULL;
    }

    g_free (gstXInfo);

}

/*=============================================================================
FUNCTION:           mfw_gst_xwindow_create

DESCRIPTION:        This function create a X window .

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

void
mfw_gst_xwindow_create (GstXInfo * gstXInfo, XID xwindow_id)
{
    GstXWindow *xwindow = NULL;
    XWindowAttributes attr;

    xwindow = g_new0 (GstXWindow, 1);

    xwindow->win = xwindow_id;

    /* We get window geometry, set the event we want to receive,
       and create a GC */
    g_mutex_lock (gstXInfo->x_lock);
    XGetWindowAttributes (gstXInfo->xcontext->disp, xwindow->win, &attr);
    xwindow->width = attr.width;
    xwindow->height = attr.height;
    xwindow->internal = FALSE;


    XSelectInput (gstXInfo->xcontext->disp, xwindow->win, ExposureMask |
                  StructureNotifyMask | KeyPressMask | KeyReleaseMask);

    xwindow->gc = XCreateGC (gstXInfo->xcontext->disp, xwindow->win, 0, NULL);


    XMapRaised (gstXInfo->xcontext->disp, xwindow->win);

    XSync (gstXInfo->xcontext->disp, FALSE);


    gstXInfo->xwindow = xwindow;

    g_mutex_unlock (gstXInfo->x_lock);

    return;
}

/*=============================================================================
FUNCTION:           mfw_gst_xwindow_destroy

DESCRIPTION:        This function destroy a GstXWindow.

ARGUMENTS PASSED:

RETURN VALUE:       GstXContext

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
mfw_gst_xwindow_destroy (GstXInfo * gstXInfo, GstXWindow * xwindow)
{
    g_return_if_fail (xwindow != NULL);

    g_mutex_lock (gstXInfo->x_lock);

    /* If we did not create that window we just free the GC and let it live */
    if (xwindow->internal)
        XDestroyWindow (gstXInfo->xcontext->disp, xwindow->win);
    else
        XSelectInput (gstXInfo->xcontext->disp, xwindow->win, 0);

    XFreeGC (gstXInfo->xcontext->disp, xwindow->gc);

    XSync (gstXInfo->xcontext->disp, FALSE);

    g_mutex_unlock (gstXInfo->x_lock);

    g_free (xwindow);

}

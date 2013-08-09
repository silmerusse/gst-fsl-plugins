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
 * Module Name:    mfw_gst_isink.c
 *
 * Description:    Implementation of Video Sink Plugin based on FSL IPU LIB API
 *                 for Gstreamer
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
#include <gst/video/gstvideosink.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include "mfw_gst_utils.h"
#include <stdio.h>
#include <unistd.h>
#include "linux/mxcfb.h"
#include "gstbufmeta.h"
#include "mfw_gst_video_surface.h"
#include <sys/time.h>

#if USE_X11
#include "mfw_gst_i_xlib.h"
#endif

#include "mfw_isink_frame.h"
#include "mfw_gst_isink.h"

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None */

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/


typedef enum
{
  ISINK_PROP_0,

  ISINK_PROP_USE_EXTERNAL_BUFFER,
  ISINK_PROP_RENDER_NOTIFY,

  ISINK_PROP_INPUT_CROP_LEFT,
  ISINK_PROP_INPUT_CROP_RIGHT,
  ISINK_PROP_INPUT_CROP_TOP,
  ISINK_PROP_INPUT_CROP_BOTTOM,


  ISINK_PROP_COLORKEY_RED,
  ISINK_PROP_COLORKEY_GREEN,
  ISINK_PROP_COLORKEY_BLUE,

  /* display0 = LCD */
  ISINK_PROP_DISP_NAME_0,
  ISINK_PROP_DISP_MODE_0,
  ISINK_PROP_ROTATION_0,
  ISINK_PROP_DISPWIN_LEFT_0,
  ISINK_PROP_DISPWIN_TOP_0,
  ISINK_PROP_DISPWIN_WIDTH_0,
  ISINK_PROP_DISPWIN_HEIGHT_0,
  ISINK_PROP_DISP_CONFIG,
  ISINK_PROP_DISP_DEINTERLACE_MODE0,

  ISINK_PROP_DISP_MAX_0,


} ISINK_PROPERTY_T;

#define ISINK_PROP_DISP_LENGTH (ISINK_PROP_DISP_MAX_0-ISINK_PROP_DISP_NAME_0)


enum
{
  SIGNAL_ISINK_RENDER,
  SIGNAL_ISINK_ALLOC_BUFFERS,
  SIGNAL_LAST,
};

typedef enum
{
  RGB_SPACE,
  YUV_SPACE
} ColorSpace;

typedef struct
{
  guint32 fourcc;
  guint32 ifmt;
  gint bpp;
} ISinkFmt;



/*=============================================================================
                              LOCAL MACROS
=============================================================================*/
/* used for debugging */
#define ISINK_ERROR(format,...)  g_print(RED_STR(format, ##__VA_ARGS__))

#define ISINK_MEMCPY memcpy

#define COLORKEY_RED        1
#define COLORKEY_GREEN      2
#define COLORKEY_BLUE       3

#define RGB888(r,g,b)\
        ((((guint32)(r))<<16)|(((guint32)(g))<<8)|(((guint32)(b))))
#define RGB888TORGB565(rgb)\
        ((((rgb)<<8)>>27<<11)|(((rgb)<<18)>>26<<5)|(((rgb)<<27)>>27))

#define RGB565TOCOLORKEY(rgb)                              \
          ( ((rgb & 0xf800)<<8)  |  ((rgb & 0xe000)<<3)  |     \
            ((rgb & 0x07e0)<<5)  |  ((rgb & 0x0600)>>1)  |     \
            ((rgb & 0x001f)<<3)  |  ((rgb & 0x001c)>>2)  )

#define FOURCC_NONE 0x0
#define GST_CAT_DEFAULT mfw_gst_isink_debug


/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/


typedef struct
{
  char *mime;
  guint32 ifmt;
  gint bpp;
  /* ugly workaround since not support y800 */
  gboolean clearbuf;
  gint clearcolor;
} ISinkFmtMapper;

static GstElementDetails mfw_gst_isink_details =
GST_ELEMENT_DETAILS ("Freescale: i_sink",
    "Sink/Video",
    "Video rendering plugin support DR",
    FSL_GST_MM_PLUGIN_AUTHOR);

static ISinkFmtMapper g_isink_fmt_mappers[] = {
  /* yuv */
  {"video/x-raw-yuv, format=(fourcc)NV12", GST_MAKE_FOURCC ('N', 'V', '1', '2'),
      12},
  {"video/x-raw-yuv, format=(fourcc)I420", GST_MAKE_FOURCC ('I', '4', '2', '0'),
      12},
  {"video/x-raw-yuv, format=(fourcc)YUY2", GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V'),
      16},
  {"video/x-raw-yuv, format=(fourcc)Y444", GST_MAKE_FOURCC ('Y', '4', '4', '4'),
      24},
  {"video/x-raw-yuv, format=(fourcc)Y42B", GST_MAKE_FOURCC ('4', '2', '2', 'P'),
      16},
  /* as old vpu will use 422p as gstreamer format */
  {"video/x-raw-yuv, format=(fourcc)422P", GST_MAKE_FOURCC ('4', '2', '2', 'P'),
      16},
  {"video/x-raw-yuv, format=(fourcc)UYVY", GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'),
      16},
  {"video/x-raw-yuv, format=(fourcc)YUYV", GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V'),
      16},
  {"video/x-raw-yuv, format=(fourcc)YUY2", GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V'),
      16},
  /* gray */
  /* workaround, ipu does not support y800 resize */
  {"video/x-raw-yuv, format=(fourcc)Y800", GST_MAKE_FOURCC ('N', 'V', '1', '2'),
      12, TRUE, 128},
  {"video/x-raw-gray, bpp=8", GST_MAKE_FOURCC ('N', 'V', '1', '2'), 12, TRUE,
      128},

  /* rgb */
  {"video/x-raw-rgb, bpp=16", GST_MAKE_FOURCC ('R', 'G', 'B', 'P'), 16},
  {"video/x-raw-rgb, bpp=24", GST_MAKE_FOURCC ('R', 'G', 'B', '3'), 24},
  {"video/x-raw-rgb, bpp=32", GST_MAKE_FOURCC ('R', 'G', 'B', '4'), 32},
  /* terminator */
  {NULL, 0, 0}
};

static guint mfw_gst_isink_signals[SIGNAL_LAST] = { 0 };

/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/
static GstElementClass *parent_class = NULL;

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/

GST_DEBUG_CATEGORY_STATIC (mfw_gst_isink_debug);
static void mfw_gst_isink_base_init (gpointer);
static void mfw_gst_isink_class_init (MfwGstISinkClass *);
static void mfw_gst_isink_init (MfwGstISink *, MfwGstISinkClass *);

static void mfw_gst_isink_get_property (GObject *,
    guint, GValue *, GParamSpec *);
static void mfw_gst_isink_set_property (GObject *,
    guint, const GValue *, GParamSpec *);

static GstStateChangeReturn mfw_gst_isink_change_state
    (GstElement *, GstStateChange);

static gboolean mfw_gst_isink_setcaps (GstBaseSink *, GstCaps *);

static GstFlowReturn mfw_gst_isink_show_frame (GstBaseSink *, GstBuffer *);

static GstFlowReturn mfw_gst_isink_buffer_alloc (GstBaseSink * bsink,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

static ISinkFmtMapper *
gst_mfw_isink_find_format_mapper_from_caps (GstCaps * caps)
{

  ISinkFmtMapper *mapper = g_isink_fmt_mappers;
  while (mapper->mime) {
    GstCaps *scaps = gst_caps_from_string (mapper->mime);
    if (scaps) {
      if (gst_caps_is_subset (caps, scaps)) {
        gst_caps_unref (scaps);
        return mapper;
      }
    }
    gst_caps_unref (scaps);
    mapper++;
  }
  return NULL;
}



static int
fmt2bpp (ISinkFmt * fmttable, guint afmt)
{
  ISinkFmt *fmt = fmttable;
  while (fmt->fourcc != FOURCC_NONE) {
    if (fmt->fourcc == afmt) {
      return fmt->bpp;
    }
    fmt++;
  };
  return 0;
}

static guint
bpp2fmt (ISinkFmt * fmttable, gint abpp)
{
  ISinkFmt *fmt = fmttable;
  while (fmt->fourcc != FOURCC_NONE) {
    if (fmt->bpp == abpp) {
      return fmt->fourcc;
    }
    fmt++;
  };
  return FOURCC_NONE;
}

static guint
fmt2fmt (ISinkFmt * fmttable, gint afmt)
{
  ISinkFmt *fmt = fmttable;
  while (fmt->fourcc != FOURCC_NONE) {
    if (fmt->fourcc == afmt) {
      return fmt->ifmt;
    }
    fmt++;
  };
  return 0;
}




#ifdef USE_X11
/*=============================================================================
FUNCTION:           mfw_gst_isink_set_xwindow_id

DESCRIPTION:        This function handle the set_xwindow_id event.

ARGUMENTS PASSED:
        overlay     -  Pointer to GstXOverlay
        xwindow_id  - Pointer to XID


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_set_xwindow_id (GstXOverlay * overlay, XID xwindow_id)
{
  MfwGstISink *isink = MFW_GST_ISINK (overlay);
  GstXInfo *gstXInfo;
  /* If we already use that window return */
  if (xwindow_id == 0) {
    ISINK_ERROR ("invalid window id.\n");
    return;
  }

  if (isink->gstXInfo == NULL) {
    isink->gstXInfo = mfw_gst_xinfo_new ();
    isink->gstXInfo->parent = (void *) isink;
  }

  if (isink->gstXInfo->xcontext == NULL) {
    isink->gstXInfo->xcontext = mfw_gst_x11_xcontext_get ();
    if (isink->gstXInfo->xcontext == NULL) {
      GST_ERROR ("could not open display");
      mfw_gst_xinfo_free (isink->gstXInfo);
      return;
    }
    mfw_gst_xwindow_create (isink->gstXInfo, xwindow_id);
  }

  /* Enable the x11 capabilities */
  isink->x11enabled = TRUE;

  gstXInfo = isink->gstXInfo;
  isink->setXid = FALSE;
  /* If we already use that window return */
  if (gstXInfo->xwindow) {
    if (gstXInfo->xwindow->win == xwindow_id) {
      /* Handle all the events in the threads */
      isink->setXid = TRUE;
      return;
    }
  }

  return;
}


/*=============================================================================
FUNCTION:           mfw_gst_isink_expose

DESCRIPTION:        This function handle the expose event.

ARGUMENTS PASSED:
        overlay  -  Pointer to GstXOverlay


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_expose (GstXOverlay * overlay)
{

  MfwGstISink *isink = MFW_GST_ISINK (overlay);
  VSConfig config;
  config.length = 0;
  config.data = NULL;
  configVideoSurface (isink->ocfg[0].vshandle, CONFIG_LAYER, &config);

#if 0
  PARAM_SET param = PARAM_NULL;

  if (!v4l_info->flow_lock)
    return;
  g_mutex_lock (v4l_info->flow_lock);
  param = mfw_gst_xv4l2_get_geometry (v4l_info);
  v4l_info->setpara |= param;   // PARAM_SET_V4L | PARAM_SET_COLOR_KEY;
  g_mutex_unlock (v4l_info->flow_lock);
#endif
  GST_DEBUG ("%s invoked", __FUNCTION__);
}

/*=============================================================================
FUNCTION:           mfw_gst_isink_set_event_handling

DESCRIPTION:        This function set the X window events.

ARGUMENTS PASSED:
        overlay  -  Pointer to GstXOverlay
        handle_events - TRUE/FALSE


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static void
mfw_gst_isink_set_event_handling (GstXOverlay * overlay, gboolean handle_events)
{
  MfwGstISink *isink = MFW_GST_ISINK (overlay);
  GstXInfo *gstXInfo = isink->gstXInfo;


  GST_WARNING ("%s: handle events:%d.", __FUNCTION__, handle_events);
  gstXInfo->handle_events = handle_events;


  if (G_UNLIKELY (!gstXInfo->xwindow->win)) {
    return;
  }

  g_mutex_lock (gstXInfo->x_lock);

  if (handle_events) {
    XSelectInput (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
        ExposureMask | StructureNotifyMask | PointerMotionMask |
        KeyPressMask | KeyReleaseMask);
  } else {
    XSelectInput (gstXInfo->xcontext->disp, gstXInfo->xwindow->win, 0);
  }

  g_mutex_unlock (gstXInfo->x_lock);

  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_isink_xoverlay_init

DESCRIPTION:        This function set the X window events.

ARGUMENTS PASSED:
        iface  -  Pointer to GstXOverlayClass


RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_xoverlay_init (GstXOverlayClass * iface)
{
  iface->set_xwindow_id = mfw_gst_isink_set_xwindow_id;
  iface->expose = mfw_gst_isink_expose;
  iface->handle_events = mfw_gst_isink_set_event_handling;

}

/*=============================================================================
FUNCTION:           mfw_gst_isink_got_xwindow_id

DESCRIPTION:        This function decorate the window.

ARGUMENTS PASSED:
        v4l_info  -  Pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_isink_got_xwindow_id (MfwGstISink * isink)
{
  gst_x_overlay_prepare_xwindow_id (GST_X_OVERLAY (isink));
}

/*=============================================================================
FUNCTION:           mfw_gst_isink_interface_supported

DESCRIPTION:        This function decorate the window.

ARGUMENTS PASSED:
        iface  -  Pointer to GstImplementsInterface
        type   -  Pointer to GType

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static gboolean
mfw_gst_isink_interface_supported (GstImplementsInterface * iface, GType type)
{
  g_assert ((type == GST_TYPE_X_OVERLAY) || (type == GST_TYPE_NAVIGATION));
  return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_isink_interface_init

DESCRIPTION:        This function decorate the window.

ARGUMENTS PASSED:
        klass  -  Pointer to GstImplementsInterfaceClass

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_interface_init (GstImplementsInterfaceClass * klass)
{
  klass->supported = mfw_gst_isink_interface_supported;
}


static void
mfw_gst_isink_navigation_send_event (GstNavigation * navigation,
    GstStructure * structure)
{
  MfwGstISink *isink = MFW_GST_ISINK (navigation);
  GstPad *peer;

  GST_INFO ("send the navigation event.");
  if ((peer = gst_pad_get_peer (GST_VIDEO_SINK_PAD (isink)))) {
    GstEvent *event;
    GstVideoRectangle src, dst, result;
    gdouble x, y, xscale = 1.0, yscale = 1.0;

    event = gst_event_new_navigation (structure);


    /* We take the flow_lock while we look at the window */



    if ((!isink->gstXInfo) || (!isink->gstXInfo->xwindow->win)) {
      return;
    }

    /* We get the frame position using the calculated geometry from _setcaps
       that respect pixel aspect ratios */
    src.w = isink->icfg.srcfmt.croprect.width;
    src.h = isink->icfg.srcfmt.croprect.height;
    dst.w = isink->ocfg[0].desfmt.rect.right - isink->ocfg[0].desfmt.rect.left;
    dst.h = isink->ocfg[0].desfmt.rect.bottom - isink->ocfg[0].desfmt.rect.top;


    if (0) {                    //!isink->stretch) {
      gst_video_sink_center_rect (src, dst, &result, TRUE);
    } else {
      result.x = result.y = 0;
      result.w = dst.w;
      result.h = dst.h;
    }

    /* We calculate scaling using the original video frames geometry to include
       pixel aspect ratio scaling. */
    xscale = (gdouble) isink->icfg.srcfmt.croprect.width / result.w;
    yscale = (gdouble) isink->icfg.srcfmt.croprect.height / result.h;

    /* Converting pointer coordinates to the non scaled geometry */
    if (gst_structure_get_double (structure, "pointer_x", &x)) {
      x = MIN (x, result.x + result.w);
      x = MAX (x - result.x, 0);
      gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE,
          (gdouble) x * xscale, NULL);
    }
    if (gst_structure_get_double (structure, "pointer_y", &y)) {
      y = MIN (y, result.y + result.h);
      y = MAX (y - result.y, 0);
      gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE,
          (gdouble) y * yscale, NULL);
    }

    gst_pad_send_event (peer, event);
    gst_object_unref (peer);
  }
}

static void
mfw_gst_isink_navigation_init (GstNavigationInterface * iface)
{
  iface->send_event = mfw_gst_isink_navigation_send_event;
}

#endif



void
mfw_gst_isink_close (MfwGstISink * isink)
{
  int i;

  for (i = 0; i < ISINK_CONFIG_MAX; i++) {
    if (isink->ocfg[i].vshandle) {
      destroyVideoSurface (isink->ocfg[i].vshandle);
      isink->ocfg[i].vshandle = NULL;
    }
  }


  if (isink->curbuf) {
    gst_buffer_unref (isink->curbuf);
    isink->curbuf = NULL;
  }

  isink->closed = TRUE;
}

#if 0
void
isink_free_external_frame (GstMfwBuffer * buf)
{

  ISinkFrame *iframe;
  g_print ("isink free external frame\n");

  if ((buf) && (iframe = GST_MFWBUFFER_PRIVOBJ (buf))) {
    MfwGstISink *isink = iframe->context1;
    (GST_MFWBUFFER_PRIVOBJ (buf)) = NULL;

    isink->free_pool = g_list_append (isink->free_pool, iframe);

    gst_object_unref (GST_OBJECT (isink));
  }
}
#else
static void
isink_free_external_frame (gpointer data)
{

  GstBufferMeta *meta;
  if (meta = (GstBufferMeta *) data) {
    ISinkFrame *iframe = (ISinkFrame *) meta->priv;
    MfwGstISink *isink = iframe->context1;
    isink->free_pool = g_list_append (isink->free_pool, iframe);
    gst_object_unref (GST_OBJECT (isink));
    gst_buffer_meta_free (meta);
  }

}

static void
isink_free_hwbuffer (gpointer data)
{
  GstBufferMeta *meta;
  if (meta = (GstBufferMeta *) data) {
    mfw_free_hw_buffer (meta->priv);
    gst_buffer_meta_free (meta);
  }

}
#endif


static void
mfw_gst_isink_init_setting (MfwGstISink * isink, GstCaps * caps)
{
  gint right = 0, bottom = 0;
  ISinkFmtMapper *fmt_mapper = NULL;
  guint fmt;
  gint bpp;
  int i;

  ISinkFrame *iframe;

  GstStructure *s = NULL;

  s = gst_caps_get_structure (caps, 0);

  isink->need_force_copy = FALSE;

  InCfg *icfg = &isink->icfg;

  memset (icfg, 0, sizeof (InCfg));

  gst_structure_get_int (s, "width", &icfg->srcfmt.croprect.width);
  gst_structure_get_int (s, "height", &icfg->srcfmt.croprect.height);
  gst_structure_get_fraction (s, "pixel-aspect-ratio", &icfg->aspectRatioN,
      &icfg->aspectRatioD);

  gst_structure_get_int (s, CAPS_FIELD_CROP_LEFT,
      &icfg->srcfmt.croprect.win.left);
  gst_structure_get_int (s, CAPS_FIELD_CROP_TOP,
      &icfg->srcfmt.croprect.win.top);
  gst_structure_get_int (s, CAPS_FIELD_CROP_RIGHT, &right);
  gst_structure_get_int (s, CAPS_FIELD_CROP_BOTTOM, &bottom);

  icfg->srcfmt.croprect.win.left += isink->cleft;
  icfg->srcfmt.croprect.win.top += isink->ctop;
  right += isink->cright;
  bottom += isink->cbottom;

  icfg->srcfmt.croprect.win.right = icfg->srcfmt.croprect.width - right;
  icfg->srcfmt.croprect.win.bottom = icfg->srcfmt.croprect.height - bottom;

  gst_structure_get_int (s, CAPS_FIELD_REQUIRED_BUFFER_NUMBER,
      &icfg->requestBufferNum);

  fmt_mapper = gst_mfw_isink_find_format_mapper_from_caps (caps);

  if (fmt_mapper) {
    fmt = icfg->srcfmt.fmt = fmt_mapper->ifmt;
    bpp = fmt_mapper->bpp;
    icfg->clearbuf = fmt_mapper->clearbuf;
    icfg->clearcolor = fmt_mapper->clearcolor;
  }

  icfg->framebufsize =
      icfg->srcfmt.croprect.width * icfg->srcfmt.croprect.height * bpp / 8;

  if (isink->need_use_ex_buffer) {

    if (icfg->requestBufferNum == 0) {
      isink->need_force_copy = TRUE;
      icfg->requestBufferNum = 6;
      icfg->origsrcfmt = icfg->srcfmt;
      icfg->srcfmt.croprect.width =
          ((icfg->origsrcfmt.croprect.win.right -
              icfg->origsrcfmt.croprect.win.left) + 63) / 64 * 64;
      icfg->srcfmt.croprect.win.left = 0;
      icfg->srcfmt.croprect.win.right =
          (icfg->origsrcfmt.croprect.win.right -
          icfg->origsrcfmt.croprect.win.left);

      icfg->srcfmt.croprect.height =
          ((icfg->origsrcfmt.croprect.win.bottom -
              icfg->origsrcfmt.croprect.win.top) + 15) / 16 * 16;
      icfg->srcfmt.croprect.win.top = 0;
      icfg->srcfmt.croprect.win.bottom =
          (icfg->origsrcfmt.croprect.win.bottom -
          icfg->origsrcfmt.croprect.win.top);
    }
    int i;
    ISinkCallBack icallback;
    ISinkFrameAllocInfo *bufinfo =
        g_malloc (sizeof (ISinkFrameAllocInfo) +
        sizeof (ISinkFrame *) * icfg->requestBufferNum);
    bufinfo->frame_num = icfg->requestBufferNum;
    bufinfo->fmt = fmt;

    for (i = 0; i < icfg->requestBufferNum; i++) {
      iframe = g_new0 (ISinkFrame, 1);
      iframe->width = icfg->srcfmt.croprect.width;
      iframe->height = icfg->srcfmt.croprect.height;
      iframe->left = icfg->srcfmt.croprect.win.left;
      iframe->right = iframe->width - icfg->srcfmt.croprect.win.right;
      iframe->top = icfg->srcfmt.croprect.win.top;
      iframe->bottom = iframe->height - icfg->srcfmt.croprect.win.bottom;


      bufinfo->frames[i] = iframe;
    }

    icallback.result = ICB_RESULT_INIT;
    icallback.version = ICB_VERSION;
    icallback.data = (void *) bufinfo;


    g_signal_emit (G_OBJECT (isink),
        mfw_gst_isink_signals[SIGNAL_ISINK_ALLOC_BUFFERS], 0, &icallback);

    if (icallback.result == ICB_RESULT_SUCCESSFUL) {
      for (i = 0; i < bufinfo->frame_num; i++) {


        isink->free_pool = g_list_append (isink->free_pool, bufinfo->frames[i]);
      }
    }
    g_free (bufinfo);
  }



  isink->init = TRUE;
}


/*=============================================================================
FUNCTION:           mfw_gst_isink_buffer_alloc   
        
DESCRIPTION:        This function initailise the sl driver
                    and gets the new buffer for display             

ARGUMENTS PASSED:  
          bsink :   pointer to GstBaseSink
		  buf   :   pointer to new GstBuffer
		  size  :   size of the new buffer
          offset:   buffer offset
		  caps  :   pad capability
        
RETURN VALUE:       GST_FLOW_OK/GST_FLOW_ERROR
      
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstFlowReturn
mfw_gst_isink_buffer_alloc (GstBaseSink * bsink, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buf)
{
  GstBuffer *newbuf = NULL;
  MfwGstISink *isink = MFW_GST_ISINK (bsink);
  GstBuffer *ibuf = NULL;
  ISinkFrame *iframe;


  if (G_UNLIKELY (isink->init == FALSE)) {
    mfw_gst_isink_init_setting (isink, caps);
  }


  if (isink->need_use_ex_buffer) {

    if (isink->need_force_copy) {

      newbuf = gst_buffer_new_and_alloc (size);
      if (newbuf) {
        gst_buffer_set_caps (newbuf, caps);
        *buf = newbuf;
        return GST_FLOW_OK;
      } else {
        GST_ERROR (">>I_SINK: Could not allocate buffer");
        *buf = NULL;
        return GST_FLOW_ERROR;

      }
    } else if (isink->free_pool) {

      iframe = isink->free_pool->data;
      isink->free_pool = g_list_remove (isink->free_pool, iframe);
#if 0
      ibuf = gst_mfw_buffer_new ();
      GST_MFWBUFFER_SET_DMABLE (ibuf);
      GST_MFWBUFFER_PRIVOBJ (ibuf) = iframe;
      iframe->context1 = gst_object_ref (GST_OBJECT (isink));
      GST_MFWBUFFER_DEF_FINALIZE (ibuf) = isink_free_external_frame;
      GST_BUFFER_SIZE (ibuf) = size;
      GST_BUFFER_DATA (ibuf) = iframe->vaddr[0];
      newbuf = GST_BUFFER_CAST (ibuf);
      gst_buffer_set_caps (newbuf, caps);
#else
      {
        gint index;
        GstBufferMeta *bufmeta;
        ibuf = gst_buffer_new ();

        GST_BUFFER_SIZE (ibuf) = size;
        GST_BUFFER_DATA (ibuf) = iframe->vaddr[0];
        iframe->context1 = gst_object_ref (GST_OBJECT (isink));
        bufmeta = gst_buffer_meta_new ();

        index = G_N_ELEMENTS (ibuf->_gst_reserved) - 1;
        bufmeta->physical_data = (gpointer) iframe->paddr[0];
        bufmeta->priv = iframe;
        ibuf->_gst_reserved[index] = bufmeta;

        GST_BUFFER_MALLOCDATA (ibuf) = bufmeta;
        GST_BUFFER_FREE_FUNC (ibuf) = isink_free_external_frame;
      }

#endif
      *buf = ibuf;
      return GST_FLOW_OK;
    } else {

      GST_ERROR (">>I_SINK: Could not allocate buffer");
      *buf = NULL;
      return GST_FLOW_ERROR;
    }
  } else {
#if 0
    ibuf =
        gst_mfw_buffer_new_and_alloc (isink->icfg.framebufsize,
        GST_MFW_BUFFER_FLAG_DMABLE);
#else
    gint index;
    GstBufferMeta *bufmeta;
    unsigned int *vaddr, *paddr;
    void *handle;
    if (!(handle =
            mfw_new_hw_buffer (isink->icfg.framebufsize, &paddr, &vaddr, 0))) {
      GST_ERROR (">>I_SINK: Could not allocate hardware buffer");
      *buf = NULL;
      return GST_FLOW_ERROR;
    }
    ibuf = gst_buffer_new ();
    GST_BUFFER_SIZE (ibuf) = size;
    GST_BUFFER_DATA (ibuf) = vaddr;

    if (isink->icfg.clearbuf) {
      memset (vaddr, isink->icfg.clearcolor, isink->icfg.framebufsize);
    }

    bufmeta = gst_buffer_meta_new ();
    index = G_N_ELEMENTS (ibuf->_gst_reserved) - 1;
    bufmeta->physical_data = (gpointer) paddr;
    ibuf->_gst_reserved[index] = bufmeta;
    bufmeta->priv = handle;
    GST_BUFFER_MALLOCDATA (ibuf) = bufmeta;
    GST_BUFFER_FREE_FUNC (ibuf) = isink_free_hwbuffer;
#endif
  }
  if (ibuf == NULL) {
    GST_ERROR (">>I_SINK: Could not allocate buffer");
    *buf = NULL;

    return GST_FLOW_ERROR;
  } else {
    GST_BUFFER_SIZE (ibuf) = size;
    newbuf = GST_BUFFER_CAST (ibuf);
    gst_buffer_set_caps (newbuf, caps);
    *buf = newbuf;
    return GST_FLOW_OK;
  }
}

gint
mfw_isink_find_devid_by_name (MfwGstISink * isink, const gchar * name)
{
  gint devid = -1;
  MfwGstISinkClass *class = isink->class;
  VideoDeviceDesc *vd_desc = class->vd_desc;

  if (name == NULL)
    return -1;

  while (vd_desc->devid >= 0) {
    if (strcmp (vd_desc->name, name) == 0) {
      devid = vd_desc->devid;
      break;
    }
    vd_desc++;
  }
  return devid;

}


/*=============================================================================
FUNCTION:           mfw_gst_isink_set_property   
        
DESCRIPTION:        This function is notified if application changes the 
                    values of a property.            

ARGUMENTS PASSED:
        object  -   pointer to GObject   
        prop_id -   id of element
        value   -   pointer to Gvalue
        pspec   -   pointer to GParamSpec
        
RETURN VALUE:       GST_FLOW_OK/GST_FLOW_ERROR
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  MfwGstISink *isink = MFW_GST_ISINK (object);
  gint intvalue;
  gchar *strvalue;

  switch (prop_id) {
    case ISINK_PROP_INPUT_CROP_LEFT:
      isink->cleft = g_value_get_int (value);
      break;
    case ISINK_PROP_USE_EXTERNAL_BUFFER:
      isink->need_use_ex_buffer = g_value_get_boolean (value);
      break;
    case ISINK_PROP_RENDER_NOTIFY:
      isink->need_render_notify = g_value_get_boolean (value);
      break;
    case ISINK_PROP_INPUT_CROP_RIGHT:
      isink->cright = g_value_get_int (value);
      break;
    case ISINK_PROP_INPUT_CROP_TOP:
      isink->ctop = g_value_get_int (value);
      break;
    case ISINK_PROP_INPUT_CROP_BOTTOM:
      isink->cbottom = g_value_get_int (value);
      break;

    default:
    {

      OutCfg *ocfg;
      gint idx;
      gint prop;

      idx = (prop_id - ISINK_PROP_DISP_NAME_0) / ISINK_PROP_DISP_LENGTH;
      ocfg = &isink->ocfg[idx];
      prop = prop_id - idx * ISINK_PROP_DISP_LENGTH;

      if (prop != ISINK_PROP_DISP_NAME_0) {

        intvalue = g_value_get_int (value);
      } else {
        strvalue = g_value_get_string (value);
      }

      switch (prop) {
        case ISINK_PROP_DISP_NAME_0:
          ocfg->devid = mfw_isink_find_devid_by_name (isink, strvalue);
          break;
        case ISINK_PROP_DISP_MODE_0:
          ocfg->mode = intvalue;
          break;
        case ISINK_PROP_ROTATION_0:
          ocfg->desfmt.rot = intvalue;
          break;
        case ISINK_PROP_DISPWIN_LEFT_0:
          ocfg->desfmt.rect.right += (intvalue - ocfg->desfmt.rect.left);
          ocfg->desfmt.rect.left = intvalue;
          break;
        case ISINK_PROP_DISPWIN_TOP_0:
          ocfg->desfmt.rect.bottom += (intvalue - ocfg->desfmt.rect.top);
          ocfg->desfmt.rect.top = intvalue;
          break;
        case ISINK_PROP_DISPWIN_WIDTH_0:
          ocfg->desfmt.rect.right = ocfg->desfmt.rect.left + intvalue;
          break;
        case ISINK_PROP_DISPWIN_HEIGHT_0:
          ocfg->desfmt.rect.bottom = ocfg->desfmt.rect.top + intvalue;
          break;
        case ISINK_PROP_DISP_CONFIG:
          isink->reconfig |= (1 << idx);
          break;
        case ISINK_PROP_DISP_DEINTERLACE_MODE0:
          if (intvalue)
            ocfg->deinterlace_mode = 2; /* only support high motion */
          else
            ocfg->deinterlace_mode = 0;
          break;
      }
    }
      break;
  }
  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_isink_get_property    
        
DESCRIPTION:        This function is notified if application requests the 
                    values of a property.                  

ARGUMENTS PASSED:
        object  -   pointer to GObject   
        prop_id -   id of element
        value   -   pointer to Gvalue
        pspec   -   pointer to GParamSpec
  
RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{

  return;
}

int
mfw_gst_isink_get_default_width (MfwGstISink * isink, int devid, int mode)
{
  int value = 0;
  MfwGstISinkClass *class = isink->class;
  VideoDeviceDesc *vd_desc = class->vd_desc;

  while (vd_desc->devid >= 0) {
    if (vd_desc->devid == devid) {
      if (vd_desc->custom_mode_num) {
        if ((mode >= 0) && (mode < vd_desc->custom_mode_num)) {
          value = vd_desc->modes[mode].resx;
        }
      } else {
        value = vd_desc->resx;
      }
      break;
    }
    vd_desc++;
  };
  return value;
}

int
mfw_gst_isink_get_default_height (MfwGstISink * isink, int devid, int mode)
{
  int value = 0;
  MfwGstISinkClass *class = isink->class;
  VideoDeviceDesc *vd_desc = class->vd_desc;

  while (vd_desc->devid >= 0) {
    if (vd_desc->devid == devid) {
      if (vd_desc->custom_mode_num) {
        if ((mode >= 0) && (mode < vd_desc->custom_mode_num)) {
          value = vd_desc->modes[mode].resy;
        }
      } else {
        value = vd_desc->resy;
      }
      break;
    }
    vd_desc++;
  };
  return value;
}


static void
mfw_gst_create_display (MfwGstISink * isink, int idx)
{
  OutCfg *ocfg = &isink->ocfg[idx];

  if (RECT_WIDTH (&ocfg->desfmt.rect) == 0)
    ocfg->desfmt.rect.right +=
        mfw_gst_isink_get_default_width (isink, ocfg->devid, ocfg->mode);
  if (RECT_HEIGHT (&ocfg->desfmt.rect) == 0)
    ocfg->desfmt.rect.bottom +=
        mfw_gst_isink_get_default_height (isink, ocfg->devid, ocfg->mode);

  ocfg->vshandle =
      createVideoSurface (ocfg->devid, ocfg->mode, &isink->icfg.srcfmt,
      &ocfg->desfmt);
  if (ocfg->vshandle) {
    VSConfig config;
    config.length = sizeof (int);
    config.data = &ocfg->deinterlace_mode;
    configVideoSurface (ocfg->vshandle, CONFIG_DEINTERLACE_MODE, &config);
  }
}

typedef struct
{
  int width;
  int height;
  int posx;
  int posy;
  unsigned long count;
  char image[0];
} SubDesc;


/*=============================================================================
FUNCTION:           mfw_gst_isink_show_frame   
        
DESCRIPTION:        Process data to display      

ARGUMENTS PASSED:
        pad -   pointer to GstPad;
        buf -   pointer to GstBuffer
        
RETURN VALUE:       GST_FLOW_OK/GST_FLOW_ERROR
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gint
diff_time (struct timeval *past, struct timeval *now)
{
  return (gint) (((gint64) (now->tv_sec) -
          (gint64) (past->tv_sec)) * (gint64) (1000000) +
      ((gint64) (now->tv_usec) - (gint64) (past->tv_usec)));
}


#define SETSUB(subframe) do{\
    needsetsub = TRUE;\
    sub = (subframe);\
}while(0)



static void
mfw_gst_isink_copy_frame_i420_y (char *dest, SourceFmt * destfmt, char *src,
    SourceFmt * srcfmt)
{
  int i, width, height;
  dest +=
      (destfmt->croprect.win.left +
      destfmt->croprect.win.top * destfmt->croprect.width);
  src +=
      (srcfmt->croprect.win.left +
      srcfmt->croprect.win.top * srcfmt->croprect.width);

  height = destfmt->croprect.win.bottom - destfmt->croprect.win.top;
  width = destfmt->croprect.win.right - destfmt->croprect.win.left;

  for (i = 0; i < height; i++) {
    memcpy (dest, src, width);
    src += srcfmt->croprect.width;
    dest += destfmt->croprect.width;
  }
}

static void
mfw_gst_isink_copy_frame_i420_uv (char *dest, SourceFmt * destfmt, char *src,
    SourceFmt * srcfmt)
{
  int i, width, height;
  dest +=
      (destfmt->croprect.win.left / 2 +
      destfmt->croprect.win.top * destfmt->croprect.width / 4);
  src +=
      (srcfmt->croprect.win.left / 2 +
      srcfmt->croprect.win.top * srcfmt->croprect.width / 4);

  height = destfmt->croprect.win.bottom - destfmt->croprect.win.top;
  width = destfmt->croprect.win.right - destfmt->croprect.win.left;

  height /= 2;
  width /= 2;

  for (i = 0; i < height; i++) {
    memcpy (dest, src, width);
    src += srcfmt->croprect.width / 2;
    dest += destfmt->croprect.width / 2;
  }
}

static GstFlowReturn
mfw_gst_isink_show_frame (GstBaseSink * basesink, GstBuffer * gstbuf)
{
  MfwGstISink *isink = MFW_GST_ISINK (basesink);
  OutCfg *ocfg;
  SourceFrame frame;
  SubFrame *sub, subframe;
  gboolean needsetsub = FALSE;
  GstFlowReturn ret = GST_FLOW_OK;
  int i;
  GstBuffer *curbuf = NULL;
  int needref = 1;
  gint index = G_N_ELEMENTS (gstbuf->_gst_reserved) - 1;
  GstBufferMeta *bufmeta = NULL;


  if (isink->need_render_notify) {
    ISinkFrame *iframe = NULL;
    ISinkCallBack icallback;

    icallback.result = ICB_RESULT_INIT;
    icallback.version = ICB_VERSION;

    if ((isink->need_use_ex_buffer) && (isink->need_force_copy)) {
      char *src = GST_BUFFER_DATA (gstbuf);

      iframe = isink->free_pool->data;
      isink->free_pool = g_list_remove (isink->free_pool, iframe);
#if 0
      curbuf = gst_mfw_buffer_new ();
      GST_MFWBUFFER_SET_DMABLE (curbuf);
      GST_MFWBUFFER_PRIVOBJ (curbuf) = iframe;
      iframe->context1 = gst_object_ref (GST_OBJECT (isink));
      GST_MFWBUFFER_DEF_FINALIZE (curbuf) = isink_free_external_frame;
#else
      curbuf = gst_buffer_new ();

      iframe->context1 = gst_object_ref (GST_OBJECT (isink));
      bufmeta = gst_buffer_meta_new ();
      bufmeta->physical_data = (gpointer) iframe->paddr[0];
      bufmeta->priv = iframe;
      curbuf->_gst_reserved[index] = bufmeta;

      GST_BUFFER_MALLOCDATA (curbuf) = bufmeta;
      GST_BUFFER_FREE_FUNC (curbuf) = isink_free_external_frame;
#endif
      mfw_gst_isink_copy_frame_i420_y (iframe->vaddr[0], &isink->icfg.srcfmt,
          src, &isink->icfg.origsrcfmt);
      src +=
          isink->icfg.origsrcfmt.croprect.width *
          isink->icfg.origsrcfmt.croprect.height;
      mfw_gst_isink_copy_frame_i420_uv (iframe->vaddr[1], &isink->icfg.srcfmt,
          src, &isink->icfg.origsrcfmt);
      src +=
          isink->icfg.origsrcfmt.croprect.width *
          isink->icfg.origsrcfmt.croprect.height / 4;
      mfw_gst_isink_copy_frame_i420_uv (iframe->vaddr[2], &isink->icfg.srcfmt,
          src, &isink->icfg.origsrcfmt);


      gstbuf = curbuf;
      needref = 0;
    }
    /* Get the priv from gstbufmeta */
    if (gstbuf->_gst_reserved[index]) {
      bufmeta = gstbuf->_gst_reserved[index];
      iframe = bufmeta->priv;
    }
    icallback.data = (void *) iframe;


    g_signal_emit (G_OBJECT (isink),
        mfw_gst_isink_signals[SIGNAL_ISINK_RENDER], 0, &icallback);


  }

  bufmeta = NULL;

  if (gstbuf->_gst_reserved[index]) {
    curbuf = gstbuf;
    bufmeta = curbuf->_gst_reserved[index];
    frame.paddr = bufmeta->physical_data;
#if 0
    if (GST_MFWBUFFER_SUBBUF (gstbuf)) {
      SubDesc *subdesc =
          (SubDesc *) GST_BUFFER_DATA (GST_MFWBUFFER_SUBBUF (gstbuf));

      if ((!(isink->subset)) || (isink->subid != subdesc->count)) {

        subframe.width = subdesc->width;
        subframe.height = subdesc->height;
        subframe.posx = subdesc->posx;
        subframe.posy = subdesc->posy;
        subframe.image = subdesc->image;
        isink->subid = subdesc->count;
        SETSUB (&subframe);
      }
      isink->subset = TRUE;
    } else {
      if (isink->subset) {
        SETSUB (NULL);
        isink->subset = FALSE;
      }
    }
#endif
  } else {

    GstBuffer *ibuf;
    if ((ret =
            mfw_gst_isink_buffer_alloc (isink, 0, GST_BUFFER_SIZE (gstbuf),
                GST_BUFFER_CAPS (gstbuf), &ibuf)) != GST_FLOW_OK) {
      GST_ERROR ("Can not allocate ibuf for non-dma buffer, ret=%d", ret);
      goto done;
    }
    if (ibuf->_gst_reserved[index]) {
      bufmeta = ibuf->_gst_reserved[index];

      ISINK_MEMCPY (GST_BUFFER_DATA (ibuf), GST_BUFFER_DATA (gstbuf),
          GST_BUFFER_SIZE (gstbuf));
      frame.paddr = bufmeta->physical_data;
      curbuf = ibuf;
      needref = 0;
    } else {                    /* nothing to render */
      gst_buffer_unref (ibuf);
      goto done;
    }

  }

  for (i = 0; i < ISINK_CONFIG_MAX; i++) {
    ocfg = &isink->ocfg[i];
    if (ocfg->devid >= 0) {
      if (ocfg->vshandle == NULL) {
        mfw_gst_create_display (isink, i);
        isink->reconfig &= (~(1 << i));
      }
      if (ocfg->vshandle) {
        if (isink->reconfig & (1 << i)) {
          VSConfig config;
          config.length = sizeof (DestinationFmt);
          config.data = &isink->ocfg[i].desfmt;
          configVideoSurface (isink->ocfg[i].vshandle, CONFIG_MASTER_PARAMETER,
              &config);
          isink->reconfig &= (~(1 << i));
        }
        if (needsetsub) {
          updateSubFrame2VideoSurface (ocfg->vshandle, sub, 0);
        }
        render2VideoSurface (ocfg->vshandle, &frame, NULL);
      }
    } else if (ocfg->vshandle) {
      destroyVideoSurface (ocfg->vshandle);
      ocfg->vshandle = NULL;
    }
  }

done:

  if (curbuf) {
    if (needref)
      gst_buffer_ref (curbuf);
    if (isink->curbuf) {
      gst_buffer_unref (isink->curbuf);
    }
    isink->curbuf = curbuf;
  }
  return GST_FLOW_OK;
}

/*=============================================================================
FUNCTION:           mfw_gst_isink_setcaps
         
DESCRIPTION:        This function does the capability negotiation between adjacent pad  

ARGUMENTS PASSED:    
        basesink    -   pointer to isink
        vscapslist  -   pointer to GstCaps
          
RETURN VALUE:       TRUE or FALSE depending on capability is negotiated or not.
        
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
   	    
=============================================================================*/

static gboolean
mfw_gst_isink_setcaps (GstBaseSink * basesink, GstCaps * vscapslist)
{
  MfwGstISink *isink = MFW_GST_ISINK (basesink);
  guint32 format = 0;
  GstStructure *structure = NULL;

  if (G_UNLIKELY (isink->init == FALSE)) {
    mfw_gst_isink_init_setting (isink, vscapslist);
  }

  structure = gst_caps_get_structure (vscapslist, 0);
  {
    gint sfd_val = 0;
    gboolean ret;

    ret = gst_structure_get_int (structure, "sfd", &sfd_val);
    if (ret == TRUE) {
      GST_DEBUG ("sfd = %d.", sfd_val);
      if (sfd_val == 1)
        basesink->abidata.ABI.max_lateness = -1;
    } else {
      GST_DEBUG (">>I_SINK: no sfd field found in caps.");
    }
  }

#ifdef USE_X11
  /* Send the got-xwindow-id message to request 
   * the set-xwindow-id callback.
   */
  mfw_gst_isink_got_xwindow_id (isink);
  if (isink->need_render_notify == FALSE) {
    gint timeout = 20;          //timeout 2s
    while ((isink->gstXInfo == NULL) && (timeout-- > 0)) {      //((isink->disp_height<16) && (timeout-->0)) {
      usleep (200000);

      //mfw_gst_xisink_refresh_geometry(isink);

    }
    if ((isink->gstXInfo) && (isink->gstXInfo->running == FALSE)) {
      isink->gstXInfo->running = TRUE;
      isink->gstXInfo->event_thread = g_thread_create (
          (GThreadFunc) mfw_gst_xisink_event_thread, isink, TRUE, NULL);
    } else {
      ISINK_ERROR ("can not create thread%s:%d\n", __FUNCTION__, __LINE__);
    }

  } else {
  }

#endif

  return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_isink_change_state   
        
DESCRIPTION:        This function keeps track of different states of pipeline.        

ARGUMENTS PASSED:
        element     -   pointer to element 
        transition  -   state of the pipeline       
  
RETURN VALUE:
        GST_STATE_CHANGE_FAILURE    - the state change failed  
        GST_STATE_CHANGE_SUCCESS    - the state change succeeded  
        GST_STATE_CHANGE_ASYNC      - the state change will happen asynchronously  
        GST_STATE_CHANGE_NO_PREROLL - the state change cannot be prerolled  
      
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstStateChangeReturn
mfw_gst_isink_change_state (GstElement * element, GstStateChange transition)
{
  MfwGstISink *isink = MFW_GST_ISINK (element);

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  guint8 index;
  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      isink->init = FALSE;

      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
#ifdef USE_X11

      // mfw_gst_xv4l2_clear_color(v4l_info);

      /* Set the running flag to false and wait for the thread exit */
      if (isink->gstXInfo) {
        isink->gstXInfo->running = FALSE;
        if (isink->x11enabled) {
          g_thread_join (isink->gstXInfo->event_thread);
        }
        if (isink->gstXInfo->xwindow) {
          mfw_gst_xwindow_destroy (isink->gstXInfo, isink->gstXInfo->xwindow);
          isink->gstXInfo->xwindow = NULL;
        }
        mfw_gst_xcontext_free (isink->gstXInfo);

        mfw_gst_xinfo_free (isink->gstXInfo);
        isink->gstXInfo = NULL;
      }
      isink->x11enabled = FALSE;


#endif
      mfw_gst_isink_close (isink);


      isink->init = FALSE;



      break;
    }
    default:                   /* do nothing */
      break;
  }
  return ret;
}

void
mfw_gst_fb0_set_colorkey (MfwGstISink * isink)
{
  gboolean ret = TRUE;
#ifndef _MX233
  struct mxcfb_color_key colorKey;
  struct fb_var_screeninfo fbVar;

  int fb = open ("/dev/fb0", O_RDWR, 0);
  if (fb <= 0)
    return;

  if (ioctl (fb, FBIOGET_VSCREENINFO, &fbVar) < 0) {
    g_print ("get vscreen info failed.\n");
    ret = FALSE;
  }

  if (fbVar.bits_per_pixel == 16) {
    isink->colorkey =
        RGB888TORGB565 (RGB888 (isink->colorkey_red, isink->colorkey_green, isink->colorkey_blue));
    GST_DEBUG ("%08X:%08X:%8X", RGB888 (isink->colorkey_red, isink->colorkey_green,
            isink->colorkey_blue), RGB888TORGB565 (RGB888 (isink->colorkey_red,
                isink->colorkey_green, isink->colorkey_blue)),
        RGB565TOCOLORKEY (RGB888TORGB565 (RGB888 (isink->colorkey_red, isink->colorkey_green,
                    isink->colorkey_blue))));
    colorKey.color_key = RGB565TOCOLORKEY (isink->colorkey);
  } else if ((fbVar.bits_per_pixel == 32) || (fbVar.bits_per_pixel == 24)) {
    isink->colorkey = RGB888 (isink->colorkey_red, isink->colorkey_green, isink->colorkey_blue);
    colorKey.color_key = isink->colorkey;

  }

  colorKey.enable = 1;
  if (ioctl (fb, MXCFB_SET_CLR_KEY, &colorKey) < 0) {
    g_print ("set color key failed.\n");
    ret = FALSE;
  }
#if 1
  struct mxcfb_gbl_alpha g_alpha;
  g_alpha.alpha = 255;
  g_alpha.enable = 1;

  if (ioctl (fb, MXCFB_SET_GBL_ALPHA, &g_alpha) < 0) {
    g_print ("set color key failed.\n");
    ret = FALSE;
  }
#endif
  g_print (RED_STR ("set color key\n"));
  close (fb);
#endif
}


/*=============================================================================
FUNCTION:           mfw_gst_isink_init   
        
DESCRIPTION:        Create the pad template that has been registered with the 
                    element class in the _base_init and do library table 
                    initialization      

ARGUMENTS PASSED:
        isink  -    pointer to isink element structure      
  
RETURN VALUE:       NONE
PRE-CONDITIONS:     _base_init and _class_init are called 
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_init (MfwGstISink * isink, MfwGstISinkClass * klass)
{
  OutCfg *ocfg;
  int i;

  isink->class = klass;

  memset (&isink->icfg, 0, sizeof (InCfg));
  memset (isink->ocfg, 0, sizeof (OutCfg) * ISINK_CONFIG_MAX);

  for (i = 0; i < ISINK_CONFIG_MAX; i++) {
    isink->ocfg[i].devid = -1;
    isink->ocfg[i].deinterlace_mode = 0;
  }

  ocfg = &isink->ocfg[0];
  ocfg->devid = klass->vd_desc[0].devid;

  isink->subid = -1;

  isink->curbuf = NULL;

  isink->free_pool = NULL;
  isink->reconfig = 0;
  isink->need_render_notify = FALSE;
  isink->need_use_ex_buffer = FALSE;

  isink->colorkey_red = COLORKEY_RED;
  isink->colorkey_green = COLORKEY_GREEN;
  isink->colorkey_blue = COLORKEY_BLUE;

#ifdef USE_X11
  mfw_gst_fb0_set_colorkey (isink);
#endif
#define MFW_GST_ISINK_PLUGIN VERSION
  PRINT_PLUGIN_VERSION (MFW_GST_ISINK_PLUGIN);
  return;
}

static void
mfw_gst_isink_finalize (GObject * object)
{
  MfwGstISink *isink = MFW_GST_ISINK (object);
  void *data;

  while (isink->free_pool) {
    data = isink->free_pool->data;
    isink->free_pool = g_list_remove (isink->free_pool, data);
    g_free (data);
  }
  PRINT_FINALIZE ("isink");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/*=============================================================================
FUNCTION:           mfw_gst_isink_class_init    
        
DESCRIPTION:        Initialise the class only once (specifying what signals,
                    arguments and virtual functions the class has and 
                    setting up global state)    

ARGUMENTS PASSED:
            klass   -   pointer to mp3decoder element class
  
RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_isink_class_init (MfwGstISinkClass * klass)
{

  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstvs_class;
  int i;
  int prop;
  gchar *prop_name;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstvs_class = (GstBaseSinkClass *) klass;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (mfw_gst_isink_change_state);

  gobject_class->set_property = mfw_gst_isink_set_property;

  gobject_class->get_property = mfw_gst_isink_get_property;
  gobject_class->finalize = mfw_gst_isink_finalize;

  gstvs_class->set_caps = GST_DEBUG_FUNCPTR (mfw_gst_isink_setcaps);
  gstvs_class->render = GST_DEBUG_FUNCPTR (mfw_gst_isink_show_frame);
  gstvs_class->buffer_alloc = GST_DEBUG_FUNCPTR (mfw_gst_isink_buffer_alloc);

  parent_class = g_type_class_peek_parent (klass);

  g_object_class_install_property (gobject_class,
      ISINK_PROP_USE_EXTERNAL_BUFFER,
      g_param_spec_boolean ("use-external-buffer", "use-external-buffer",
          "set to make isink use external buffer", FALSE, G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, ISINK_PROP_RENDER_NOTIFY,
      g_param_spec_boolean ("render-notify", "render-notify",
          "set to make isink callback when render", FALSE, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ISINK_PROP_INPUT_CROP_LEFT,
      g_param_spec_int ("crop-left",
          "crop-left",
          "get/set the height of the image to be displayed",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ISINK_PROP_INPUT_CROP_RIGHT,
      g_param_spec_int ("crop-right",
          "crop-right",
          "get/set the height of the image to be displayed",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ISINK_PROP_INPUT_CROP_TOP,
      g_param_spec_int ("crop-top",
          "crop-top",
          "get/set the height of the image to be displayed",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ISINK_PROP_INPUT_CROP_BOTTOM,
      g_param_spec_int ("crop-bottom",
          "crop-bottom",
          "get/set the height of the image to be displayed",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ISINK_PROP_COLORKEY_RED,
      g_param_spec_int ("colorkey-red",
          "colorkey red",
          "set red for colorkey",
          0, 255, COLORKEY_RED, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ISINK_PROP_COLORKEY_GREEN,
      g_param_spec_int ("colorkey-green",
          "colorkey green",
          "set green for colorkey",
          0, 255, COLORKEY_GREEN, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ISINK_PROP_COLORKEY_BLUE,
      g_param_spec_int ("colorkey-blue",
          "colorkey blue",
          "set blue for colorkey",
          0, 255, COLORKEY_BLUE, G_PARAM_READWRITE));

#if 1
  for (i = 0; i < VD_MAX + 1; i++) {
    if (queryVideoDevice (i, &klass->vd_desc[i]) != 0) {
      klass->vd_desc[i].devid = -1;
      break;
    }
  }
#endif

  prop = ISINK_PROP_DISP_NAME_0;

  prop_name = g_strdup_printf ("display", i);
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_string (prop_name,
          prop_name,
          "get/set the device of the output",
          klass->vd_desc[0].name, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;


  prop_name = g_strdup_printf ("mode");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name,
          "get/set the device mode of the output",
          0, VM_MAX, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;
  prop_name = g_strdup_printf ("rotation");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name,
          "get/set the rotation of the output", 0, 7, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;

  prop_name = g_strdup_printf ("axis-left");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name,
          "get/set the left of the output",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;

  prop_name = g_strdup_printf ("axis-top");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name,
          "get/set the right of the output",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;

  prop_name = g_strdup_printf ("disp-width");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name,
          "get/set the width of the device",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;

  prop_name = g_strdup_printf ("disp-height");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name,
          "get/set the height of the device",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;


  prop_name = g_strdup_printf ("disp-config");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name,
          "trigger reconfig for display0", 0, 1, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;

  prop_name = g_strdup_printf ("disp-deinterlace");
  g_object_class_install_property (gobject_class, prop,
      g_param_spec_int (prop_name,
          prop_name, "deinterlace mode", 0, 1, 0, G_PARAM_READWRITE));
  g_free (prop_name);
  prop++;

  for (i = 1; i < ISINK_CONFIG_MAX; i++) {
    prop = i * ISINK_PROP_DISP_LENGTH + ISINK_PROP_DISP_NAME_0;

    prop_name = g_strdup_printf ("display-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_string (prop_name,
            prop_name,
            "get/set the device of the output", "NULL", G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("mode-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name,
            "get/set the device mode of the output",
            0, VM_MAX, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("rotation-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name,
            "get/set the rotation of the output", 0, 7, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("axis-left-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name,
            "get/set the left of the output",
            0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("axis-top-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name,
            "get/set the right of the output",
            0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("disp-width-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name,
            "get/set the width of the device",
            0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("disp-height-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name,
            "get/set the height of the device",
            0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("disp-config-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name,
            "trigger reconfig for display", 0, 1, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    prop_name = g_strdup_printf ("disp-deinterlace-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
            prop_name, "deinterlace mode", 0, 1, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;
  }

  mfw_gst_isink_signals[SIGNAL_ISINK_RENDER] =
      g_signal_new ("show-video-frame", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (MfwGstISinkClass,
          isink_render_callback), NULL, NULL,
      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  mfw_gst_isink_signals[SIGNAL_ISINK_ALLOC_BUFFERS] =
      g_signal_new ("allocate-video-frames", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (MfwGstISinkClass,
          isink_allocbuffers_callback), NULL, NULL,
      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  return;
}


static GstCaps *
gst_isink_get_sink_caps ()
{
  GstCaps *caps = NULL;
  ISinkFmtMapper *map = g_isink_fmt_mappers;
  while ((map) && (map->mime)) {
    if (caps) {
      GstCaps *newcaps = gst_caps_from_string (map->mime);
      if (newcaps) {
        if (!gst_caps_is_subset (newcaps, caps)) {
          gst_caps_append (caps, newcaps);
        } else {
          gst_caps_unref (newcaps);
        }
      }
    } else {
      caps = gst_caps_from_string (map->mime);
    }
    map++;
  }
  return caps;
}

static GstPadTemplate *
gst_isink_sink_pad_template (void)
{
  static GstPadTemplate *templ = NULL;

  if (!templ) {
    GstCaps *caps = gst_isink_get_sink_caps ();

    if (caps) {
      templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    }
  }
  return templ;
}


/*=============================================================================
FUNCTION:           mfw_gst_isink_base_init   
        
DESCRIPTION:       sl Sink element details are registered with the plugin during
                   _base_init ,This function will initialise the class and child 
                    class properties during each new child class creation       

ARGUMENTS PASSED:
            Klass   -   void pointer
  
RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/


static void
mfw_gst_isink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstPadTemplate *sink_template = NULL;

  /* make a list of all available caps */
  gst_element_class_add_pad_template (element_class,
      gst_isink_sink_pad_template ());


  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE (element_class, "IPU-based video sink",
      "Sink/Video", "Display video on different displays by using IPU");

  return;

}

/*=============================================================================
FUNCTION:           mfw_gst_isink_get_type    
        
DESCRIPTION:        Interfaces are initiated in this function.you can register one 
                    or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:   None
  
RETURN VALUE:       A numerical value ,which represents the unique identifier 
                    of this element(isink)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GType
mfw_gst_isink_get_type (void)
{
  static GType mfwIsink_type = 0;

  if (!mfwIsink_type) {
    static const GTypeInfo mfwIsink_info = {
      sizeof (MfwGstISinkClass),
      mfw_gst_isink_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_isink_class_init,
      NULL,
      NULL,
      sizeof (MfwGstISink),
      0,
      (GInstanceInitFunc) mfw_gst_isink_init,
    };

    mfwIsink_type = g_type_register_static (GST_TYPE_VIDEO_SINK,
        "MfwGstISink", &mfwIsink_info, 0);

#ifdef USE_X11
    {
      static const GInterfaceInfo iface_info = {
        (GInterfaceInitFunc) mfw_gst_isink_interface_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo overlay_info = {
        (GInterfaceInitFunc) mfw_gst_isink_xoverlay_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo navigation_info = {
        (GInterfaceInitFunc) mfw_gst_isink_navigation_init,
        NULL,
        NULL,
      };

      g_type_add_interface_static (mfwIsink_type, GST_TYPE_IMPLEMENTS_INTERFACE,
          &iface_info);
      g_type_add_interface_static (mfwIsink_type, GST_TYPE_X_OVERLAY,
          &overlay_info);
      g_type_add_interface_static (mfwIsink_type, GST_TYPE_NAVIGATION,
          &navigation_info);
    }
#endif


  }

  GST_DEBUG_CATEGORY_INIT (mfw_gst_isink_debug, "mfw_isink", 0, "Isink");
  return mfwIsink_type;
}



/*=============================================================================
FUNCTION:           plugin_init

DESCRIPTION:        Special function , which is called as soon as the plugin or 
                    element is loaded and information returned by this function 
                    will be cached in central registry

ARGUMENTS PASSED:
        plugin     -    pointer to container that contains features loaded 
                        from shared object module

RETURN VALUE:
        return TRUE or FALSE depending on whether it loaded initialized any 
        dependency correctly

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "mfw_isink", FSL_GST_RANK_HIGH,
          MFW_GST_TYPE_ISINK))
    return FALSE;

  return TRUE;
}

FSL_GST_PLUGIN_DEFINE ("isink", "IPU-based video sink", plugin_init);

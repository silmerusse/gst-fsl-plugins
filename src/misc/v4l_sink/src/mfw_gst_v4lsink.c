/*
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_v4lsink.c
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
#include "mfw_gst_v4l_buffer.h"
#include "mfw_gst_v4l.h"
#include "mfw_gst_fb.h"

#ifdef USE_X11
#include "mfw_gst_v4l_xlib.h"
#endif

#include "mfw_gst_v4lsink.h"



#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
#include <linux/mxcfb.h>
#endif


#if defined(ENABLE_TVOUT) && defined (_MX27)
/*For TV-Out & change para on-the-fly*/
#include <errno.h>
#include <sys/time.h>
struct v4l2_output_dev
{
  __u32 disp_num;               /* output device index, for TV is 2, for LCD is 3 */
  __u32 id_len;                 /* string id length */
  __u8 id[16];                  /* string id of deivce, e.g. TV "DISP3 TV" */
};
#define VIDIOC_PUT_OUTPUT       _IOW  ('V', 90, struct v4l2_output_dev)
#define VIDIOC_GET_OUTPUT       _IOW  ('V', 91, struct v4l2_output_dev)
#endif

#if 0
void
dprintf (FILE * fp, const char *file, size_t line, int enable,
    const char *fmt, ...)
{
  va_list ap;
  if (enable) {
    fprintf (fp, "%s (%d): ", file, line);
    va_start (ap, fmt);
    vfprintf (fp, fmt, ap);
    va_end (ap);
    fflush (fp);
  }
}
#endif

/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/

/*=============================================================================
                             SUSPEND SUPPORT
=============================================================================*/



#if defined(ENABLE_TVOUT) && defined (_MX27)
/*For TV-Out & change para on-the-fly*/
#include <errno.h>
#include <sys/time.h>
struct v4l2_output_dev
{
  __u32 disp_num;               /* output device index, for TV is 2, for LCD is 3 */
  __u32 id_len;                 /* string id length */
  __u8 id[16];                  /* string id of deivce, e.g. TV "DISP3 TV" */
};
#define VIDIOC_PUT_OUTPUT       _IOW  ('V', 90, struct v4l2_output_dev)
#define VIDIOC_GET_OUTPUT       _IOW  ('V', 91, struct v4l2_output_dev)
#endif



//#define GST_DEBUG g_print
/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None */

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
typedef struct {
  gchar * mime;
  guint32 v4lfmt;
  guint32 flags;
  gboolean enable;
}MfwV4lFmtMap;

enum
{
  PROP_0,
  //PROP_FULLSCREEN,      /* enable full screen image display */
  DISP_WIDTH,                   /* display image width */
  DISP_HEIGHT,                  /* display image height */
  AXIS_TOP,                     /* image top axis offset */
  AXIS_LEFT,                    /* image left axis offset */
  ROTATE,                       /* image rotation value (0 - 7) */
  CROP_LEFT,                    /* input image cropping in the left */
  CROP_RIGHT,                   /* input image cropping in the right */
  CROP_TOP,                     /* input image cropping in the top */
  CROP_BOTTOM,                  /* input image cropping in the top */
  //BASE_OFFSET,
#ifdef ENABLE_TVOUT
  TV_OUT,
  TV_MODE,
#endif
  DUMP_LOCATION,
  ADDITIONAL_BUFFER_DEPTH,
  SETPARA,
  PROP_STRETCH,
  PROP_FORCE_ASPECT_RATIO,
  PROP_PIXEL_ASPECT_RATIO,

#ifdef USE_X11
  PROP_X11ENABLED,
#endif

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
  PROP_ALPHA_ENABLE,
  PROP_ALPHA_VALUE,
#endif
  PROP_RENDERED_FRAMES,
  PROP_DEVICE_NAME,
  PROP_DEINTERLACE_MOTION,
  PROP_DEINTERLACE_ENABLE,
};


static MfwV4lFmtMap g_v4lfmt_maps[] = {
  {GST_VIDEO_CAPS_YUV("NV12"), V4L2_PIX_FMT_NV12, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("I420"), V4L2_PIX_FMT_YUV420, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("TNVP"), IPU_PIX_FMT_TILED_NV12, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("TNVF"), IPU_PIX_FMT_TILED_NV12F, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("YV12"), V4L2_PIX_FMT_YVU420, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("Y42B"), V4L2_PIX_FMT_YUV422P, 0, FALSE},
//{GST_VIDEO_CAPS_YUV("422P"), V4L2_PIX_FMT_YUV422P, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("Y444"), IPU_PIX_FMT_YUV444P, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("Y800"), V4L2_PIX_FMT_YUV420, FMT_FLAG_GRAY8, FALSE},
  {GST_VIDEO_CAPS_YUV("YV12"), V4L2_PIX_FMT_YVU420, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("UYVY"), V4L2_PIX_FMT_UYVY, 0, FALSE},
//{GST_VIDEO_CAPS_YUV("YUYV"), V4L2_PIX_FMT_YUYV, 0, FALSE},
  {GST_VIDEO_CAPS_YUV("YUY2"), V4L2_PIX_FMT_YUYV, 0, FALSE},
  {GST_VIDEO_CAPS_RGBx, V4L2_PIX_FMT_RGB32, 0, FALSE},
  {GST_VIDEO_CAPS_RGB, V4L2_PIX_FMT_RGB24, 0, FALSE},
  {GST_VIDEO_CAPS_RGB_16, V4L2_PIX_FMT_RGB565, 0, FALSE},
  {GST_VIDEO_CAPS_RGB_15, V4L2_PIX_FMT_RGB555, 0, FALSE},
  {GST_VIDEO_CAPS_BGRx, V4L2_PIX_FMT_BGR32, 0, FALSE},
  {GST_VIDEO_CAPS_BGR, V4L2_PIX_FMT_BGR24, 0, FALSE},
  {NULL},
};

static guint mfw_gst_v4lsink_signals[SIGNAL_LAST] = { 0 };


#define HW_DEINTERLACE
#define QUEUE_SIZE_HIGH 5
#define DEQUEUE_TIMES_IN_SHOW 10000

/*=============================================================================
                              LOCAL MACROS
=============================================================================*/

#define GST_CAT_DEFAULT mfw_gst_v4lsink_debug


/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/


/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/
/* None */
GST_DEBUG_CATEGORY (mfw_gst_v4lsink_debug);

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/


static void mfw_gst_v4lsink_base_init (gpointer);
static void mfw_gst_v4lsink_class_init (MFW_GST_V4LSINK_INFO_CLASS_T *);
static void mfw_gst_v4lsink_init (MFW_GST_V4LSINK_INFO_T *,
    MFW_GST_V4LSINK_INFO_CLASS_T *);

static void mfw_gst_v4lsink_get_property (GObject *,
    guint, GValue *, GParamSpec *);
static void mfw_gst_v4lsink_set_property (GObject *,
    guint, const GValue *, GParamSpec *);

static GstStateChangeReturn mfw_gst_v4lsink_change_state
    (GstElement *, GstStateChange);

static gboolean mfw_gst_v4lsink_setcaps (GstBaseSink *, GstCaps *);

static GstFlowReturn mfw_gst_v4lsink_show_frame (GstBaseSink *, GstBuffer *);


static GstFlowReturn mfw_gst_v4lsink_buffer_alloc (GstBaseSink * bsink,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);



static GstElementClass *parent_class = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

int
mfw_gst_get_first_odev ()
{
#define V4L2_DEVICE_MAX 32
  int i;
  int fd = -1;
  char devname[20];
  for (i=0;i<V4L2_DEVICE_MAX;i++){
     sprintf(devname, "/dev/video%d", i);
     if ((fd=open (devname, O_RDWR | O_NONBLOCK, 0))>=0){
        struct v4l2_capability cap;
        if (!ioctl(fd, VIDIOC_QUERYCAP, &cap)){
          if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
             GST_INFO("Get first device:%s, capabilities 0x%x", devname, cap.capabilities);
             break;
          }
        }
        close(fd);
     }
     fd = -1;
  }
  return fd;
}

static gboolean
mfw_gst_v4l2sink_query_support_formats ()
{
  gboolean ret = FALSE;
  struct v4l2_fmtdesc fmtdesc;
  char * devname;
  int fd;
  if ((fd=mfw_gst_get_first_odev())<0){
    goto fail;
  }


  fmtdesc.index = 0;
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  while (!(ioctl (fd, VIDIOC_ENUM_FMT, &fmtdesc))) {
    gboolean found = FALSE;
    MfwV4lFmtMap * map = g_v4lfmt_maps;
    while(map->mime!=NULL){
      if (fmtdesc.pixelformat==map->v4lfmt){
         map->enable = TRUE;
         found = TRUE;
      }
      map++;
    }
    if (found){
      GST_INFO ("supported format:[%c%c%c%c]%s", (fmtdesc.pixelformat & 0xff),
          (fmtdesc.pixelformat & 0xff00)>>8, (fmtdesc.pixelformat & 0xff0000)>>16, (fmtdesc.pixelformat & 0xff000000) >>24, fmtdesc.description);
    }else{
      GST_WARNING ("unrecognized format:[%c%c%c%c]%s", (fmtdesc.pixelformat & 0xff),
          (fmtdesc.pixelformat & 0xff00)>>8, (fmtdesc.pixelformat & 0xff0000)>>16, (fmtdesc.pixelformat & 0xff000000) >>24, fmtdesc.description);
    }
    fmtdesc.index++;

  }

  close (fd);

  ret = TRUE;

fail:
  return ret;

}

#ifdef USE_X11

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_create_event_thread

DESCRIPTION:        This function create event thread.

ARGUMENTS PASSED:   v4l_info  - V4lsink plug-in context

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_v4lsink_create_event_thread (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  if (v4l_info->x11enabled) {
    gint timeout = 20;          //timeout 2s
    while ((v4l_info->disp_height < 16) && (timeout-- > 0)) {
      mfw_gst_xv4l2_refresh_geometry (v4l_info);
      usleep (100000);

    }

    if ((v4l_info->gstXInfo)
        && (v4l_info->gstXInfo->running == FALSE)) {
      v4l_info->gstXInfo->running = TRUE;
      v4l_info->gstXInfo->event_thread =
          g_thread_create ((GThreadFunc) mfw_gst_xv4l2_event_thread, v4l_info,
          TRUE, NULL);
    }

    if (!(IS_PXP (v4l_info->chipcode)))
      mfw_gst_set_gbl_alpha (v4l_info->fd_fb, 255);

  } else {
    if (!(IS_PXP (v4l_info->chipcode)))
      mfw_gst_set_gbl_alpha (v4l_info->fd_fb, 128);
  }
}



/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_set_xwindow_id

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
mfw_gst_v4lsink_set_xwindow_id (GstXOverlay * overlay, XID xwindow_id)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (overlay);
  GstXInfo *gstXInfo;



  /* If we already use that window return */
  if (xwindow_id == 0) {
    g_print ("invalid window id.\n");
    return;
  }

  if (v4l_info->gstXInfo == NULL) {
    g_print ("create a xinfo\n");
    v4l_info->gstXInfo = mfw_gst_xinfo_new ();
    v4l_info->gstXInfo->parent = (void *) v4l_info;
  }

  if (v4l_info->gstXInfo->xcontext == NULL) {
    v4l_info->gstXInfo->xcontext = mfw_gst_x11_xcontext_get ();
    if (v4l_info->gstXInfo->xcontext == NULL) {
      g_print ("could not open display\n");
      mfw_gst_xinfo_free (v4l_info->gstXInfo);
      return;
    }
    mfw_gst_xwindow_create (v4l_info->gstXInfo, xwindow_id);
  }

  /* Enable the x11 capabilities */
  v4l_info->x11enabled = TRUE;

  gstXInfo = v4l_info->gstXInfo;
  v4l_info->setXid = FALSE;

  /* If we already use that window return */
  if (gstXInfo->xwindow) {
    if (gstXInfo->xwindow->win == xwindow_id) {
      /* Handle all the events in the threads */
      v4l_info->setXid = TRUE;
      GST_DEBUG ("[%s] xwindow_id: Param %d", __FUNCTION__, v4l_info->setpara);
      return;
    }
  }

  return;

}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_expose

DESCRIPTION:        This function handle the expose event.

ARGUMENTS PASSED:
        overlay  -  Pointer to GstXOverlay


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_v4lsink_expose (GstXOverlay * overlay)
{

  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (overlay);
  /* FixME: There is no image can be exposed currently */

  GST_DEBUG ("%s invoked", __FUNCTION__);
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_set_event_handling

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
mfw_gst_v4lsink_set_event_handling (GstXOverlay * overlay,
    gboolean handle_events)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (overlay);
  GstXInfo *gstXInfo = v4l_info->gstXInfo;

  if (!v4l_info->flow_lock)
    return;

  g_print ("%s: handle events:%d.\n", __FUNCTION__, handle_events);
  gstXInfo->handle_events = handle_events;

  g_mutex_lock (v4l_info->flow_lock);

  if (G_UNLIKELY (!gstXInfo->xwindow->win)) {
    g_mutex_unlock (v4l_info->flow_lock);
    return;
  }

  g_mutex_lock (gstXInfo->x_lock);

  if (handle_events) {
    XSelectInput (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
        ExposureMask | StructureNotifyMask | PointerMotionMask |
        KeyPressMask | KeyReleaseMask);
  } else {
    /* Enable the X Events anyway */
    XSelectInput (gstXInfo->xcontext->disp, gstXInfo->xwindow->win,
        ExposureMask | StructureNotifyMask | PointerMotionMask |
        KeyPressMask | KeyReleaseMask);
  }

  g_mutex_unlock (gstXInfo->x_lock);

  g_mutex_unlock (v4l_info->flow_lock);
  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_xoverlay_init

DESCRIPTION:        This function set the X window events.

ARGUMENTS PASSED:
        iface  -  Pointer to GstXOverlayClass


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_v4lsink_xoverlay_init (GstXOverlayClass * iface)
{
  iface->set_xwindow_id = mfw_gst_v4lsink_set_xwindow_id;
  iface->expose = mfw_gst_v4lsink_expose;
  iface->handle_events = mfw_gst_v4lsink_set_event_handling;

}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_got_xwindow_id

DESCRIPTION:        This function decorate the window.

ARGUMENTS PASSED:
        v4l_info  -  Pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4lsink_got_xwindow_id (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  gst_x_overlay_prepare_xwindow_id (GST_X_OVERLAY (v4l_info));
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_interface_supported

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
mfw_gst_v4lsink_interface_supported (GstImplementsInterface * iface, GType type)
{
  g_assert ((type == GST_TYPE_X_OVERLAY) || (type == GST_TYPE_NAVIGATION));
  return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_interface_init

DESCRIPTION:        This function decorate the window.

ARGUMENTS PASSED:
        klass  -  Pointer to GstImplementsInterfaceClass

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_v4lsink_interface_init (GstImplementsInterfaceClass * klass)
{
  klass->supported = mfw_gst_v4lsink_interface_supported;
}


static void
mfw_gst_v4lsink_navigation_send_event (GstNavigation * navigation,
    GstStructure * structure)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (navigation);
  GstPad *peer;

  GST_LOG ("send the navigation event.");
  if ((peer = gst_pad_get_peer (GST_VIDEO_SINK_PAD (v4l_info)))) {
    GstEvent *event;
    GstVideoRectangle src, dst, result;
    gdouble x, y, xscale = 1.0, yscale = 1.0;

    event = gst_event_new_navigation (structure);

    if (!v4l_info->flow_lock)
      return;

    /* We take the flow_lock while we look at the window */
    g_mutex_lock (v4l_info->flow_lock);
    if (!v4l_info->gstXInfo) {
      g_mutex_unlock (v4l_info->flow_lock);
      return;
    }
    if (!v4l_info->gstXInfo->xwindow) {
      g_mutex_unlock (v4l_info->flow_lock);
      return;
    }

    if (!v4l_info->gstXInfo->xwindow->win) {
      g_mutex_unlock (v4l_info->flow_lock);
      return;
    }

    /* We get the frame position using the calculated geometry from _setcaps
       that respect pixel aspect ratios */
    src.w = v4l_info->width;
    src.h = v4l_info->height;
    dst.w = v4l_info->disp_width;
    dst.h = v4l_info->disp_height;

    g_mutex_unlock (v4l_info->flow_lock);

    if (!v4l_info->stretch) {
      gst_video_sink_center_rect (src, dst, &result, TRUE);
    } else {
      result.x = result.y = 0;
      result.w = dst.w;
      result.h = dst.h;
    }

    /* We calculate scaling using the original video frames geometry to include
       pixel aspect ratio scaling. */
    xscale = (gdouble) v4l_info->width / result.w;
    yscale = (gdouble) v4l_info->height / result.h;

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
mfw_gst_v4lsink_navigation_init (GstNavigationInterface * iface)
{
  iface->send_event = mfw_gst_v4lsink_navigation_send_event;
}

#endif

/*=============================================================================
FUNCTION:          mfw_gst_v4lsink_close

DESCRIPTION:       This funtion clears the list of all the buffers maintained
                   in the buffer pool. swirches of the video stream and closes
                   the V4L device driver.

ARGUMENTS PASSED:   v4l_info  - V4lsink plug-in context

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_v4lsink_close (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  // Exit if we have already closed before to avoid hangs
  if (v4l_info->pool_lock == NULL) {
    return;
  }

  g_mutex_lock (v4l_info->pool_lock);

  if (v4l_info->init) {
#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
    mfw_gst_v4l2_disable_local_alpha (v4l_info);
#endif
    GST_INFO ("Stream off.");
    mfw_gst_v4l2_streamoff (v4l_info);
  }

  mfw_gst_v4l2_free_buffers (v4l_info);
  g_mutex_unlock (v4l_info->pool_lock);

  if (v4l_info->enable_dump)
    dumpfile_close (v4l_info);

#ifdef ENABLE_TVOUT
  if (v4l_info->tv_out == TRUE) {

#if defined(ENABLE_TVOUT) && (defined(_MX31) || defined(_MX35))
    mfw_gst_v4l2_mx31_mx35_set_lcdmode (v4l_info);
#endif

#if defined(ENABLE_TVOUT) && ( defined(_MX37) || defined(_MX51))
    mfw_gst_v4l2_mx37_mx51_tv_setblank (v4l_info);
#endif

#if defined(ENABLE_TVOUT) && defined (_MX27)
    mfw_gst_v4l2_mx27_tv_close (v4l_info);
#endif
  }
#endif

  GST_DEBUG ("close the fb0 device.");

  GST_DEBUG (">>V4L SINK: Close the v4l device.");

  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_set_property

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
mfw_gst_v4lsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (object);
  switch (prop_id) {

    case DISP_WIDTH:
      v4l_info->disp_width = g_value_get_int (value);
      GST_DEBUG ("width = %d", v4l_info->disp_width);
      break;
    case DISP_HEIGHT:
      v4l_info->disp_height = g_value_get_int (value);
      GST_DEBUG ("height = %d", v4l_info->disp_height);
      break;
    case AXIS_TOP:
      v4l_info->axis_top = g_value_get_int (value);
      GST_DEBUG ("axis_top = %d", v4l_info->axis_top);
      break;
    case AXIS_LEFT:
      v4l_info->axis_left = g_value_get_int (value);
      GST_DEBUG ("axis_left = %d", v4l_info->axis_left);
      break;
    case ROTATE:
      v4l_info->rotate = g_value_get_int (value);
      g_print ("rotate = %d", v4l_info->rotate);

      break;
    case CROP_LEFT:
      v4l_info->crop_left = g_value_get_int (value);
      GST_DEBUG ("crop_left = %d", v4l_info->crop_left);
      break;

    case CROP_RIGHT:
      v4l_info->crop_right = g_value_get_int (value);
      GST_DEBUG ("crop_right = %d", v4l_info->crop_right);
      break;

    case CROP_TOP:
      v4l_info->crop_top = g_value_get_int (value);
      GST_DEBUG ("crop_top = %d", v4l_info->crop_top);
      break;

    case CROP_BOTTOM:
      v4l_info->crop_bottom = g_value_get_int (value);
      GST_DEBUG ("crop_bottom = %d", v4l_info->crop_bottom);
      break;

#ifdef ENABLE_TVOUT
    case TV_OUT:
      v4l_info->tv_out = g_value_get_boolean (value);
      break;
    case TV_MODE:
      v4l_info->tv_mode = g_value_get_int (value);
      break;
      /*It's an ugly code,consider how to realize it by event */
#endif
    case DUMP_LOCATION:
      dumpfile_set_location (v4l_info, g_value_get_string (value));
      break;
    case SETPARA:
      v4l_info->setpara |= g_value_get_int (value);
      break;

    case PROP_FORCE_ASPECT_RATIO:
      v4l_info->stretch = !g_value_get_boolean (value);

      g_print ("set stretch:%d.\n", v4l_info->stretch);
      break;


#ifdef USE_X11
    case PROP_X11ENABLED:
      v4l_info->x11enabled = g_value_get_boolean (value);
      break;
#endif

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
    case PROP_ALPHA_VALUE:
      g_mutex_lock (v4l_info->flow_lock);
      if (g_value_get_int (value) >= 0) {
        v4l_info->alpha = g_value_get_int (value);
        mfw_gst_v4l2_set_alpha (v4l_info);
      } else if (v4l_info->alpha_enable & ALPHA_LOCAL) {
        mfw_gst_v4l2_set_local_alpha (v4l_info, -1);
      }
      g_mutex_unlock (v4l_info->flow_lock);
      break;

    case PROP_ALPHA_ENABLE:
      mfw_gst_v4lsink_set_alpha_enable (v4l_info, g_value_get_int (value));
      break;
#endif
    case PROP_DEVICE_NAME:
      strcpy (v4l_info->v4l_dev_name, g_value_get_string (value));
      break;

    case PROP_DEINTERLACE_MOTION:
      v4l_info->motion = g_value_get_int (value);
      break;

    case PROP_DEINTERLACE_ENABLE:
      v4l_info->enable_deinterlace = g_value_get_boolean(value);
      break;

    default:
      GST_DEBUG ("unkwown id:%d", prop_id);
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_get_property

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
mfw_gst_v4lsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (object);
  switch (prop_id) {
    case DISP_WIDTH:
      g_value_set_int (value, v4l_info->crop.c.width);
      break;
    case DISP_HEIGHT:
      g_value_set_int (value, v4l_info->crop.c.height);
      break;
    case AXIS_TOP:
      g_value_set_int (value, v4l_info->crop.c.top);
      break;
    case AXIS_LEFT:
      g_value_set_int (value, v4l_info->crop.c.left);
      break;
    case ROTATE:
      g_value_set_int (value, v4l_info->rotate);
      break;
    case CROP_LEFT:
      g_value_set_int (value, v4l_info->crop_left);
      break;
    case CROP_TOP:
      g_value_set_int (value, v4l_info->crop_top);
      break;
    case CROP_RIGHT:
      g_value_set_int (value, v4l_info->crop_right);
      break;
    case CROP_BOTTOM:
      g_value_set_int (value, v4l_info->crop_bottom);
      break;

#ifdef ENABLE_TVOUT
    case TV_OUT:
      g_value_set_boolean (value, v4l_info->tv_out);
      break;
    case TV_MODE:
      g_value_set_int (value, v4l_info->tv_mode);
      break;
#endif
    case DUMP_LOCATION:
      g_value_set_string (value, v4l_info->dump_location);
      break;
    case SETPARA:
      g_value_set_int (value, v4l_info->setpara);
      break;

    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, !(v4l_info->stretch));

      break;

#ifdef USE_X11
    case PROP_X11ENABLED:
      g_value_set_boolean (value, v4l_info->x11enabled);
      break;
#endif


#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
    case PROP_ALPHA_ENABLE:
      g_value_set_int (value, v4l_info->alpha_enable);
      break;

    case PROP_ALPHA_VALUE:
      g_value_set_int (value, v4l_info->alpha);
      break;

#endif
    case PROP_RENDERED_FRAMES:
      g_value_set_int (value, v4l_info->rendered);
      break;

    case PROP_DEVICE_NAME:
      g_value_set_string (value, v4l_info->v4l_dev_name);
      break;

    case PROP_DEINTERLACE_MOTION:
      g_value_set_int (value, v4l_info->motion);
      break;

    case PROP_DEINTERLACE_ENABLE:
      g_value_set_boolean(value, v4l_info->enable_deinterlace);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  return;
}

static gboolean
mfw_gst_v4l2_try_dq_buffer(MFW_GST_V4LSINK_INFO_T *v4l_info, int cntout)
{
  gint cnt;
  for (cnt = 0; cnt < cntout; cnt++) {
    if (v4l_info->v4lqueued <= MIN_QUEUE_NUM) {
      return TRUE;
    }
    if (mfw_gst_v4l2_dq_buffer (v4l_info)){
      return TRUE;
    }
    usleep (WAIT_ON_DQUEUE_FAIL_IN_MS);
  }
  GST_WARNING("Dqueue failed, %d buffers in v4l2 queue", v4l_info->v4lqueued);
  return FALSE;
}
/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_show_frame

DESCRIPTION:        Process data to display

ARGUMENTS PASSED:
        pad -   pointer to GstPad;
        buf -   pointer to GstBuffer

RETURN VALUE:       GST_FLOW_OK/GST_FLOW_ERROR
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstFlowReturn
mfw_gst_v4lsink_show_frame (GstBaseSink * basesink, GstBuffer * buf)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (basesink);
  struct v4l2_buffer *v4l_buf = NULL;
  GstBuffer *outbuffer = NULL;
  GSList *searchlist;

  guint8 i = 0;
  MFWGstV4LSinkBuffer *v4lsink_buffer = NULL;
  /* This is to enable the integration of the peer elements which do not
     call the gst_pad_alloc_buffer() to allocate their output buffers */

#ifdef SUSPEND_SUPPORT
  mfw_gst_v4lsink_get_runinfo (v4l_info);
  if (v4l_info->suspend) {
    return GST_FLOW_OK;
  }
#endif

  if (G_UNLIKELY (v4l_info->buffer_alloc_called == FALSE)) {
    mfw_gst_v4lsink_buffer_alloc (basesink, 0, GST_BUFFER_SIZE (buf),
        v4l_info->store_caps, &outbuffer);
    memcpy (GST_BUFFER_DATA (outbuffer), GST_BUFFER_DATA (buf),
        GST_BUFFER_SIZE (buf));
    v4l_info->buffer_alloc_called = FALSE;
  } else {
    outbuffer = buf;
  }


  /* if the input buffer stride is not multiple of 8, drop it since IPU does
   * not support such buffer */

  if ((v4l_info->width & 0x7) != 0) {
    GST_ERROR ("buffer stride is (%d)not multiple of 8, drop it",
        v4l_info->width);
    return GST_FLOW_OK;
  }


  if (v4l_info->stream_on == TRUE) {
    g_mutex_lock (v4l_info->flow_lock);

    if (G_UNLIKELY ((v4l_info->setpara & PARAM_SET_V4L) == PARAM_SET_V4L)) {
      gint type;
      gboolean result = FALSE;
      /* Fresh the latest geometry value */
#ifdef USE_X11
      if (v4l_info->x11enabled) {
        mfw_gst_xv4l2_refresh_geometry (v4l_info);
        mfw_gst_xv4l2_clear_color (v4l_info);
      }
#endif

      v4l_info->setpara &= (~PARAM_SET_V4L);



#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
      //mfw_gst_v4lsink_disable_global_alpha(v4l_info);
      mfw_gst_v4l2_disable_local_alpha (v4l_info);
#endif
      /* Clear the buffer in queue */
#if 0
      {
        gint i;
        i = g_list_length (v4l_info->v4llist);
        while (i > 1) {
          mfw_gst_v4l2_dq_buffer (v4l_info);
          i = g_list_length (v4l_info->v4llist);

        }
        mfw_gst_v4l2_streamoff (v4l_info);
        mfw_gst_v4l2_clear_showingbuf (v4l_info);
      }
#endif
      result = mfw_gst_v4l2_input_init (v4l_info, v4l_info->outformat);
      if (result != TRUE) {
        GST_ERROR ("Failed to initalize the v4l driver");
        g_mutex_unlock (v4l_info->flow_lock);
        return GST_FLOW_ERROR;
      }
      result = mfw_gst_v4l2_display_init (v4l_info, v4l_info->disp_width,
          v4l_info->disp_height);

      if (result != TRUE) {
        g_print ("\nFailed to initalize the display\n");
        g_mutex_unlock (v4l_info->flow_lock);
        return GST_FLOW_OK;
      }
#ifdef USE_X11
      if (v4l_info->x11enabled) {
        mfw_gst_xv4l2_set_color (v4l_info);
      }
#endif

      g_mutex_unlock (v4l_info->flow_lock);
      GST_DEBUG ("setpara end");

    } else {
      g_mutex_unlock (v4l_info->flow_lock);
    }

    g_mutex_lock (v4l_info->flow_lock);

    if (G_UNLIKELY ((v4l_info->setpara & PARAM_SET_COLOR_KEY)
            == PARAM_SET_COLOR_KEY)) {
      gint type;
      gboolean result = FALSE;
      v4l_info->setpara &= (~PARAM_SET_COLOR_KEY);
      g_mutex_unlock (v4l_info->flow_lock);

#ifdef USE_X11
      if (v4l_info->x11enabled) {
        mfw_gst_xv4l2_set_color (v4l_info);
      }
#endif

    } else {
      g_mutex_unlock (v4l_info->flow_lock);

    }
  }

  /* Clear the frame buffer */

  if (G_UNLIKELY (!GST_BUFFER_FLAG_IS_SET (outbuffer, GST_BUFFER_FLAG_LAST))) { // SW buffer

    POP_RESERVED_HWBUFFER (v4l_info, v4lsink_buffer);

    if (v4lsink_buffer) {
      memcpy (GST_BUFFER_DATA (v4lsink_buffer),
          GST_BUFFER_DATA (outbuffer), GST_BUFFER_SIZE (outbuffer));
      GST_DEBUG ("Framebuffer outside the v4l allocator");
      ((MFWGstV4LSinkBuffer *) outbuffer)->bufstate = BUF_STATE_SHOWED;
    } else {
      //try to dq once only
      if (v4l_info->v4lqueued > MIN_QUEUE_NUM) {
        mfw_gst_v4l2_dq_buffer (v4l_info);
      }

      POP_RESERVED_HWBUFFER (v4l_info, v4lsink_buffer);

      if (v4lsink_buffer) {

        memcpy (GST_BUFFER_DATA (v4lsink_buffer),
            GST_BUFFER_DATA (outbuffer), GST_BUFFER_SIZE (outbuffer));
        ((MFWGstV4LSinkBuffer *) outbuffer)->bufstate = BUF_STATE_SHOWED;
      } else {
        GST_WARNING
            ("Drop because no reserved hwbuffer%d", v4l_info->v4lqueued);
        return GST_FLOW_OK;
      }
    }
  } else {                      // HW buffer
    v4lsink_buffer = (MFWGstV4LSinkBuffer *) outbuffer;
    if (v4l_info->buffer_alloc_called == TRUE) {
      gst_buffer_ref (GST_BUFFER_CAST (v4lsink_buffer));
    }
  }

  v4l_buf = &v4lsink_buffer->v4l_buf;

  if (v4l_info->enable_dump) {
    dumpfile_write (v4l_info, v4lsink_buffer);
    v4lsink_buffer->bufstate = BUF_STATE_SHOWED;
    g_mutex_unlock (v4l_info->pool_lock);
    gst_buffer_unref (GST_BUFFER_CAST (v4lsink_buffer));
    return GST_FLOW_OK;

  }

  {
    /*display immediately */
    struct timeval queuetime;
    gettimeofday (&queuetime, NULL);
    v4l_buf->timestamp = queuetime;
  }

  /* queue the buffer to be displayed into the V4L queue */

#if (defined(HW_DEINTERLACE) && (defined (_MX51) || (defined(_MX6))))
  /* Set the field information to v4l buffer */
  GstCaps *caps;
  caps = GST_BUFFER_CAPS (buf);
  mfw_gst_v4l2_set_field (v4l_info, caps);
  v4l_buf->field = v4l_info->field;
  GST_LOG ("set field:%d", v4l_buf->field);
#endif

  v4lsink_buffer->showcnt++;
  v4l_info->rendered++;
  if (g_list_find (v4l_info->v4llist, (gpointer) v4l_buf->index)) {
    GST_WARNING ("Try to display frame %s(state %d) which already in displaying queue!",
            v4l_buf->index, v4lsink_buffer->bufstate );

    gst_buffer_unref(v4lsink_buffer);
    goto trydq;

  } else {
    v4l_info->v4llist =
        g_list_append (v4l_info->v4llist, (gpointer) v4l_buf->index);
    gint i;
    GST_LOG ("total queued:%d", g_list_length (v4l_info->v4llist));
    for (i = 0; i < g_list_length (v4l_info->v4llist); i++) {
      GST_LOG ("\t:%d", (gint) (g_list_nth_data (v4l_info->v4llist, i)));
    }
  }

  int err_num = ioctl (v4l_info->v4l_id, VIDIOC_QBUF, v4l_buf);
  if (G_UNLIKELY (err_num < 0)) {
    gint i;
    gpointer data;
    GST_ERROR ("VIDIOC_QBUF:%d failed, error:%d, queued:%d\n", v4l_buf->index,
        err_num, v4l_info->v4lqueued);
    GST_ERROR ("total queued:%d", g_list_length (v4l_info->v4llist));
    for (i = 0; i < g_list_length (v4l_info->v4llist); i++) {
      data = g_list_nth_data (v4l_info->v4llist, i);
      GST_ERROR ("\t:%d", (gint) data);
    }
    g_mutex_unlock (v4l_info->pool_lock);
    return GST_FLOW_ERROR;
  } else
    GST_LOG ("queued: %d", v4l_buf->index);

  v4lsink_buffer->bufstate = BUF_STATE_SHOWING;
  v4l_info->v4lqueued++;

  /* Switch on the stream display as soon as there are more than 1 buffer
     in the V4L queue */

  if ((v4l_info->chipcode == CC_MX6Q) && (v4l_info->motion == 2) && (v4l_info->enable_deinterlace)
    && (v4l_info->outformat != IPU_PIX_FMT_TILED_NV12F) ) {
    if (G_UNLIKELY (v4l_info->qbuff_count == 0)) {

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))

      if (v4l_info->alpha_enable & ALPHA_LOCAL)
        mfw_gst_v4l2_enable_local_alpha (v4l_info);
#endif

#ifdef USE_X11
      mfw_gst_xv4l2_set_color (v4l_info);
#endif

      mfw_gst_v4l2_streamon (v4l_info);


#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
      mfw_gst_v4l2_set_alpha (v4l_info);
#endif

    }
  } else if (G_UNLIKELY (v4l_info->qbuff_count == 1)) {

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
    if (v4l_info->alpha_enable & ALPHA_LOCAL)
      mfw_gst_v4l2_enable_local_alpha (v4l_info);
#endif

#ifdef USE_X11
    mfw_gst_xv4l2_set_color (v4l_info);
#endif

    mfw_gst_v4l2_streamon (v4l_info);


#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
    mfw_gst_v4l2_set_alpha (v4l_info);
#endif



  }

  v4l_info->qbuff_count++;

trydq:

  if (G_LIKELY (GST_BUFFER_FLAG_IS_SET (outbuffer, GST_BUFFER_FLAG_LAST))) {
    gboolean dq_ret;
    dq_ret = mfw_gst_v4l2_try_dq_buffer(v4l_info, DEQUEUE_TIMES_IN_SHOW);

    if ((dq_ret==TRUE) && (v4l_info->v4lqueued>QUEUE_SIZE_HIGH)){
    /* try dqueue once more for recovery from dqueue failed due to timeout */
      mfw_gst_v4l2_try_dq_buffer(v4l_info, DEQUEUE_TIMES_IN_SHOW);
    }
  }
#if 0
  {
    gint i;
    GST_LOG ("total queued:%d", g_list_length (v4l_info->v4llist));
    for (i = 0; i < g_list_length (v4l_info->v4llist); i++) {
      GST_LOG ("\t:%d", (gint) (g_list_nth_data (v4l_info->v4llist, i)));
    }
  }
#endif
  return GST_FLOW_OK;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_set_format

DESCRIPTION:        This function set the format value

ARGUMENTS PASSED:
        basesink    -   pointer to v4lsink
        vscapslist  -   pointer to GstCaps

RETURN VALUE:       TRUE or FALSE depending on capability is negotiated or not.

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None

=============================================================================*/

static gboolean
mfw_gst_v4lsink_set_format (MFW_GST_V4LSINK_INFO_T * v4l_info, GstCaps * caps)
{
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  MfwV4lFmtMap * map;

  gst_structure_get_int (structure, "width", &v4l_info->width);
  gst_structure_get_int (structure, "height", &v4l_info->height);

  gst_structure_get_int (structure, CAPS_FIELD_CROP_LEFT,
      &v4l_info->cr_left_bypixel);
  gst_structure_get_int (structure, CAPS_FIELD_CROP_TOP,
      &v4l_info->cr_top_bypixel);
  gst_structure_get_int (structure, CAPS_FIELD_CROP_RIGHT,
      &v4l_info->cr_right_bypixel);
  gst_structure_get_int (structure, CAPS_FIELD_CROP_BOTTOM,
      &v4l_info->cr_bottom_bypixel);

  v4l_info->cr_left_bypixel = ROUNDUP8 (v4l_info->cr_left_bypixel);
  v4l_info->cr_right_bypixel = ROUNDUP8 (v4l_info->cr_right_bypixel);
  v4l_info->cr_top_bypixel = ROUNDUP8 (v4l_info->cr_top_bypixel);
  v4l_info->cr_bottom_bypixel = ROUNDUP8 (v4l_info->cr_bottom_bypixel);

  v4l_info->outformat_flags = 0;

  map = g_v4lfmt_maps;
  while(map->mime){
    GstCaps * mcaps = gst_caps_from_string (map->mime);
    if (gst_caps_is_subset(caps, mcaps)){
      v4l_info->outformat = map->v4lfmt;
      v4l_info->outformat_flags = map->flags;
      gst_caps_unref(mcaps);
      break;
    }
    gst_caps_unref(mcaps);
    map++;
  };

  return TRUE;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_setcaps

DESCRIPTION:        This function does the capability negotiation between adjacent pad

ARGUMENTS PASSED:
        basesink    -   pointer to v4lsink
        vscapslist  -   pointer to GstCaps

RETURN VALUE:       TRUE or FALSE depending on capability is negotiated or not.

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None

=============================================================================*/

static gboolean
mfw_gst_v4lsink_setcaps (GstBaseSink * basesink, GstCaps * vscapslist)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (basesink);
  guint32 format = 0;
  GstStructure *structure = NULL;
  v4l_info->store_caps = vscapslist;
  gint new_width, new_height;

  gint bpp, depth;
  structure = gst_caps_get_structure (vscapslist, 0);

  gst_structure_get_fraction (structure, "framerate",
      &v4l_info->framerate_n, &v4l_info->framerate_d);

#if 0
  {
    gint sfd_val = 0;
    gboolean ret;

    ret = gst_structure_get_int (structure, "sfd", &sfd_val);
    if (ret == TRUE) {
      GST_DEBUG ("sfd = %d.", sfd_val);
      if (sfd_val == 1)
        basesink->abidata.ABI.max_lateness = -1;
      else
        basesink->abidata.ABI.max_lateness = MAX_LATENESS_TIME;


    } else {
      basesink->abidata.ABI.max_lateness = MAX_LATENESS_TIME;
      GST_DEBUG ("No sfd field found in caps.");
    }

  }
  GST_INFO ("Set max lateness = %lld.", basesink->abidata.ABI.max_lateness);
#endif
  mfw_gst_v4lsink_set_format (v4l_info, vscapslist);
#ifdef USE_X11
  mfw_gst_v4lsink_got_xwindow_id (v4l_info);
#endif
  return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_V4Lsink_change_state

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
mfw_gst_v4lsink_change_state (GstElement * element, GstStateChange transition)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (element);

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  guint8 index;
  GST_DEBUG ("\n>>V4LSINK: State: %d", transition);

  switch (transition) {

    case GST_STATE_CHANGE_NULL_TO_READY:
      v4l_info->width = -1;
      v4l_info->height = -1;
      v4l_info->in_width = -1;
      v4l_info->in_height = -1;
      v4l_info->framerate_n = 0;
      v4l_info->framerate_d = 1;

      v4l_info->init = FALSE;
      v4l_info->buffer_alloc_called = FALSE;
      v4l_info->free_pool = NULL;
      v4l_info->reservedhwbuffer_list = NULL;
      v4l_info->v4lqueued = 0;
      // v4l_info->v4llist = g_list_alloc();

      v4l_info->swbuffer_count = 0;
      v4l_info->frame_dropped = 0;
      v4l_info->swbuffer_max = 0;
      v4l_info->rendered = 0;

      v4l_info->cr_left_bypixel = 0;
      v4l_info->cr_right_bypixel = 0;
      v4l_info->cr_top_bypixel = 0;
      v4l_info->cr_bottom_bypixel = 0;


      memset (&v4l_info->crop, 0, sizeof (struct v4l2_crop));
      memset (&v4l_info->prevCrop, 0, sizeof (struct v4l2_crop));

#ifdef USE_X11
      if (v4l_info->gstXInfo == NULL) {
        GST_INFO ("create a xinfo");
        v4l_info->gstXInfo = mfw_gst_xinfo_new ();
        v4l_info->gstXInfo->parent = (void *) v4l_info;
      }
#endif
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:

      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:



#ifdef USE_X11
      if (v4l_info->x11enabled) {
        if (v4l_info->width != -1)
          mfw_gst_v4lsink_create_event_thread (v4l_info);

        mfw_gst_xv4l2_refresh_geometry (v4l_info);
      }
#else
      mfw_gst_set_gbl_alpha (v4l_info->fd_fb, 0);
#endif

      mfw_gst_v4l2_display_init (v4l_info, v4l_info->disp_width,
          v4l_info->disp_height);

      break;

    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      v4l_info->running_time =
          gst_element_get_start_time (GST_ELEMENT (v4l_info));
      if (v4l_info->running_time){
        g_print("Running time %"GST_TIME_FORMAT" render fps %.3f\n", GST_TIME_ARGS(v4l_info->running_time), (gfloat)GST_SECOND*v4l_info->rendered/v4l_info->running_time);
      }
#if 0
      if (GST_TIME_AS_MSECONDS (v4l_info->running_time) > 0)
        g_print ("total time:%" GST_TIME_FORMAT " ,Render fps:%.3f\n",
            GST_TIME_ARGS (v4l_info->running_time),
            (gfloat) (v4l_info->rendered * 1000 /
                GST_TIME_AS_MSECONDS (v4l_info->running_time)));
#endif
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
    {

      g_print ("Total rendered:%lld\n", v4l_info->rendered);
#ifdef USE_X11

      // mfw_gst_xv4l2_clear_color(v4l_info);

      /* Set the running flag to false and wait for the thread exit */
      g_mutex_lock (v4l_info->flow_lock);
      v4l_info->gstXInfo->running = FALSE;
      g_mutex_unlock (v4l_info->flow_lock);
      if ((v4l_info->x11enabled)&&(v4l_info->gstXInfo) &&(v4l_info->gstXInfo->event_thread)) {
        g_thread_join (v4l_info->gstXInfo->event_thread);
      }
      if (v4l_info->gstXInfo->xwindow) {
        mfw_gst_xwindow_destroy (v4l_info->gstXInfo,
            v4l_info->gstXInfo->xwindow);
        v4l_info->gstXInfo->xwindow = NULL;
      }
      mfw_gst_xcontext_free (v4l_info->gstXInfo);

      mfw_gst_xinfo_free (v4l_info->gstXInfo);
      v4l_info->gstXInfo = NULL;
      v4l_info->x11enabled = FALSE;


#endif
      v4l_info->setpara = PARAM_NULL;
      mfw_gst_v4lsink_close (v4l_info);
      v4l_info->init = FALSE;
      break;
    }
    default:                   /* do nothing */
      break;
  }
  return ret;
}

/*======================================================================================
FUNCTION:           mfw_gst_v4lsink_finalize

DESCRIPTION:        Class finalized

ARGUMENTS PASSED:   object     - pointer to the elements object

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static void
mfw_gst_v4lsink_finalize (GObject * object)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (object);

  g_mutex_free (v4l_info->pool_lock);
  v4l_info->pool_lock = NULL;

  g_mutex_free (v4l_info->flow_lock);
  v4l_info->flow_lock = NULL;

  GST_DEBUG ("close the fb0 device.");
  mfw_gst_fb0_close (&v4l_info->fd_fb);

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
  mfw_gst_v4l2_localpha_close (v4l_info);
#endif

  mfw_gst_v4l2_close (v4l_info);

  MFW_WEAK_ASSERT (v4l_info->free_pool == NULL);

  PRINT_FINALIZE ("v4l_sink");
  G_OBJECT_CLASS (parent_class)->finalize (object);

}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_init

DESCRIPTION:        Create the pad template that has been registered with the
                    element class in the _base_init and do library table
                    initialization

ARGUMENTS PASSED:
        v4l_info  -    pointer to v4lsink element structure

RETURN VALUE:       NONE
PRE-CONDITIONS:     _base_init and _class_init are called
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static void
mfw_gst_v4lsink_init (MFW_GST_V4LSINK_INFO_T * v4l_info,
    MFW_GST_V4LSINK_INFO_CLASS_T * klass)
{
  v4l_info->all_buffer_pool = NULL;
  v4l_info->disp_height = 0;
  v4l_info->disp_width = 0;
  v4l_info->axis_top = 0;
  v4l_info->axis_left = 0;
  v4l_info->rotate = 0;
  v4l_info->prevRotate = 0;
  v4l_info->crop_left = 0;
  v4l_info->crop_top = 0;

  v4l_info->enable_deinterlace = TRUE;

  v4l_info->full_screen = FALSE;
  v4l_info->base_offset = 0;

#ifdef ENABLE_TVOUT
  /*For TV-Out & para change on-the-fly */
  v4l_info->tv_out = FALSE;
  v4l_info->tv_mode = NV_MODE;
#endif

  /* Initialization for the dump image to local file */
  v4l_info->enable_dump = FALSE;
  v4l_info->dump_location = NULL;
  v4l_info->dumpfile = NULL;
  v4l_info->dump_length = 0;
  v4l_info->cr_left_bypixel_orig = 0;
  v4l_info->cr_right_bypixel_orig = 0;
  v4l_info->cr_top_bypixel_orig = 0;
  v4l_info->cr_bottom_bypixel_orig = 0;

  v4l_info->pool_lock = g_mutex_new ();
  v4l_info->flow_lock = g_mutex_new ();

  v4l_info->setpara = PARAM_NULL;
  v4l_info->outformat = V4L2_PIX_FMT_YUV420;

  v4l_info->fd_fb = 0;


  v4l_info->stretch = TRUE;
  v4l_info->field = V4L2_FIELD_ANY;

  v4l_info->colorSrc = 0;

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
  v4l_info->fd_lalpfb = 0;
  v4l_info->alpha_enable = ALPHA_GLOBAL;
  memset (&v4l_info->lalpha, 0, sizeof (struct mxcfb_loc_alpha));
  v4l_info->lalp_buf_vaddr[0] = v4l_info->lalp_buf_vaddr[1] = 0;
  v4l_info->alpha = 255;
#endif

  v4l_info->setXid = FALSE;

  memset (&v4l_info->crop, 0, sizeof (struct v4l2_crop));
  memset (&v4l_info->prevCrop, 0, sizeof (struct v4l2_crop));

  mfw_gst_fb0_open (&v4l_info->fd_fb);
  v4l_info->chipcode = getChipCode ();

  v4l_info->v4l_id = -1;

  if (IS_PXP (v4l_info->chipcode)) {
  } else {
    if (v4l_info->chipcode == CC_MX6Q)
      strcpy (v4l_info->v4l_dev_name, "/dev/video17");
    else
      strcpy (v4l_info->v4l_dev_name, "/dev/video16");
  }

  v4l_info->motion = 2;         /* high motion */

#ifdef USE_X11
  if (!(IS_PXP (v4l_info->chipcode)))
    mfw_gst_fb0_set_colorkey (v4l_info->fd_fb, &v4l_info->colorSrc);
#endif

#define MFW_GST_V4LSINK_PLUGIN VERSION
  PRINT_PLUGIN_VERSION (MFW_GST_V4LSINK_PLUGIN);

  return;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_class_init

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
mfw_gst_v4lsink_class_init (MFW_GST_V4LSINK_INFO_CLASS_T * klass)
{

  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstvs_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstvs_class = (GstBaseSinkClass *) klass;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (mfw_gst_v4lsink_change_state);


  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = mfw_gst_v4lsink_set_property;
  gobject_class->get_property = mfw_gst_v4lsink_get_property;
  gobject_class->finalize = mfw_gst_v4lsink_finalize;

  gstvs_class->set_caps = GST_DEBUG_FUNCPTR (mfw_gst_v4lsink_setcaps);
  gstvs_class->render = GST_DEBUG_FUNCPTR (mfw_gst_v4lsink_show_frame);
  gstvs_class->buffer_alloc = GST_DEBUG_FUNCPTR (mfw_gst_v4lsink_buffer_alloc);


  g_object_class_install_property (gobject_class, DISP_WIDTH,
      g_param_spec_int ("disp-width",
          "Disp_Width",
          "gets the width of the image to be displayed",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, DISP_HEIGHT,
      g_param_spec_int ("disp-height",
          "Disp_Height",
          "gets the height of the image to be displayed",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, AXIS_TOP,
      g_param_spec_int ("axis-top",
          "axis-top",
          "gets the top co-ordinate of the origin of display",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, AXIS_LEFT,
      g_param_spec_int ("axis-left",
          "axis-left",
          "gets the left co-ordinate of the origin of display",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  /* FixME: The i.MX233 does not support rotate */
#if ((!defined (_MX233)) && (!defined (_MX28)))
  g_object_class_install_property (gobject_class, ROTATE,
      g_param_spec_int ("rotate", "Rotate",
          "gets the angle at which display is to be rotated",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
#endif

  g_object_class_install_property (gobject_class, CROP_LEFT,
      g_param_spec_int ("crop_left_by_pixel",
          "crop-left-by-pixel",
          "set the input image cropping in the left (width)",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, CROP_RIGHT,
      g_param_spec_int ("crop_right_by_pixel",
          "crop-right-by-pixel",
          "set the input image cropping in the right (width)",
          0, G_MAXINT, 0, G_PARAM_READWRITE));


  g_object_class_install_property (gobject_class, CROP_TOP,
      g_param_spec_int ("crop_top_by_pixel",
          "crop-top-by-pixel",
          "set the input image cropping in the top (height)",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, CROP_BOTTOM,
      g_param_spec_int ("crop_bottom_by_pixel",
          "crop-bottom-by-pixel",
          "set the input image cropping in the bottom (height)",
          0, G_MAXINT, 0, G_PARAM_READWRITE));


  g_object_class_install_property (gobject_class, SETPARA,
      g_param_spec_int ("setpara", "Setpara",
          "set parameter of V4L2, 1: Set V4L 2: Set Color",
          0, 3, 0, G_PARAM_READWRITE));

  /*For TV-Out & para change on-the-fly */
#ifdef ENABLE_TVOUT
  g_object_class_install_property (gobject_class, TV_OUT,
      g_param_spec_boolean ("tv-out", "TV-OUT",
          "set output to TV-OUT", FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, TV_MODE,
      g_param_spec_int ("tv-mode", "TV-MODE",
          "set mode to TV-OUT, 0: NTSC, 1: PAL, 2: 720p ",
          0, 2, 0, G_PARAM_READWRITE));
#endif

  g_object_class_install_property (gobject_class, DUMP_LOCATION,
      g_param_spec_string ("dump_location",
          "Dump File Location",
          "Location of the file to write cropped video YUV stream."
          "Enable it will output image to file instead of V4L device",
          NULL, G_PARAM_READWRITE));


  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean
      ("force-aspect-ratio",
          "force-aspect-ratio",
          "Force Aspect Ratio", FALSE, G_PARAM_READWRITE));

#ifdef USE_X11
  g_object_class_install_property (gobject_class, PROP_X11ENABLED,
      g_param_spec_boolean ("x11enable",
          "X11Enable", "Enabled x11 event handle", TRUE, G_PARAM_READWRITE));
#endif


#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
  g_object_class_install_property (gobject_class, PROP_ALPHA_ENABLE,
      g_param_spec_int ("alpha-enable",
          "alpha enable",
          "set/get alpha enable mask, 0:disable, 1:local, 2:global",
          1, 2, 1, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ALPHA_VALUE,
      g_param_spec_int ("alpha",
          "alpha value",
          "set/get alpha value, -1:set alpha by local buffer, 0-255: set all same alpha value",
          -1, 255, 255, G_PARAM_READWRITE));

  mfw_gst_v4lsink_signals[SIGNAL_LOCALPHA_BUFFER_READY] =
      g_signal_new ("loc-buf-ready", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (MFW_GST_V4LSINK_INFO_CLASS_T,
          lalp_buf_ready_notify), NULL, NULL,
      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

#endif

  g_object_class_install_property (gobject_class, PROP_RENDERED_FRAMES,
      g_param_spec_int ("rendered",
          "rendered",
          "Get the total rendered frames", 0, G_MAXINT, 0, G_PARAM_READWRITE));

#if defined (VL4_STREAM_CALLBACK)
  mfw_gst_v4lsink_signals[SIGNAL_V4L_STREAM_CALLBACK] =
      g_signal_new ("v4lstreamevent", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (MFW_GST_V4LSINK_INFO_CLASS_T,
          v4lstream_callback), NULL, NULL,
      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
#endif
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device",
          "V4L device name", "V4L device name", NULL, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DEINTERLACE_MOTION,
      g_param_spec_int ("motion",
          "motion",
          "The interlace motion setting: 0 - low motion, 1 - medium motion, 2 - high motion.",
          0, 2, 2, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,PROP_DEINTERLACE_ENABLE,
      g_param_spec_boolean ("deinterlace", "deinterlace",
          "set deinterlace enabled", FALSE, G_PARAM_READWRITE));


  return;

}





/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_buffer_alloc

DESCRIPTION:        This function initailise the v4l driver
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
mfw_gst_v4lsink_buffer_alloc (GstBaseSink * bsink, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buf)
{
  GstBuffer *newbuf = NULL;
  MFW_GST_V4LSINK_INFO_T *v4l_info = MFW_GST_V4LSINK (bsink);
  MFWGstV4LSinkBuffer *v4lsink_buffer = NULL;
  GstStructure *s = NULL;
  GstElement *element = GST_ELEMENT (bsink);
  gint frame_buffer_size;
  gint max_frames;
  gint hwbuffernumforcodec;

  gboolean result = FALSE;

  gint aspectratio_n = 0, aspectratio_d = 0;
  gint bpp, depth;


  v4l_info->buffer_alloc_called = TRUE;

  if (G_UNLIKELY (v4l_info->init == FALSE)) {

    // comment out because using videotestsrc as input it causes gst_caps asserts
    // not needed it seems
    //caps = gst_caps_make_writable(caps);
    s = gst_caps_get_structure (caps, 0);

    v4l_info->buffers_required = 0;

    gst_structure_get_int (s, "width", &v4l_info->width);
    gst_structure_get_int (s, "height", &v4l_info->height);
    gst_structure_get_fraction (s, "pixel-aspect-ratio", &aspectratio_n,
        &aspectratio_d);
    gst_structure_get_int (s, CAPS_FIELD_CROP_LEFT, &v4l_info->cr_left_bypixel);
    gst_structure_get_int (s, CAPS_FIELD_CROP_TOP, &v4l_info->cr_top_bypixel);
    gst_structure_get_int (s, CAPS_FIELD_CROP_RIGHT,
        &v4l_info->cr_right_bypixel);
    gst_structure_get_int (s, CAPS_FIELD_CROP_BOTTOM,
        &v4l_info->cr_bottom_bypixel);
    gst_structure_get_int (s, CAPS_FIELD_REQUIRED_BUFFER_NUMBER,
        &v4l_info->buffers_required);

    v4l_info->in_width = v4l_info->width;
    v4l_info->in_height = v4l_info->height;

    v4l_info->cr_left_bypixel = ROUNDUP8 (v4l_info->cr_left_bypixel);
    v4l_info->cr_right_bypixel = ROUNDUP8 (v4l_info->cr_right_bypixel);
    v4l_info->cr_top_bypixel = ROUNDUP8 (v4l_info->cr_top_bypixel);
    v4l_info->cr_bottom_bypixel = ROUNDUP8 (v4l_info->cr_bottom_bypixel);


    GST_DEBUG ("crop_left_bypixel=%d", v4l_info->cr_left_bypixel);
    GST_DEBUG ("crop_top_by_pixel=%d", v4l_info->cr_top_bypixel);
    GST_DEBUG ("crop_right_bypixel=%d", v4l_info->cr_right_bypixel);
    GST_DEBUG ("crop_bottom_by_pixel=%d", v4l_info->cr_bottom_bypixel);

    if (v4l_info->enable_dump) {
      v4l_info->cr_left_bypixel_orig = v4l_info->cr_left_bypixel;
      v4l_info->cr_right_bypixel_orig = v4l_info->cr_right_bypixel;
      v4l_info->cr_top_bypixel_orig = v4l_info->cr_top_bypixel;
      v4l_info->cr_bottom_bypixel_orig = v4l_info->cr_bottom_bypixel;
    }


    GST_DEBUG ("aspectratio_n=%d", aspectratio_n);
    GST_DEBUG ("aspectratio_d=%d", aspectratio_d);
    GST_DEBUG ("Decoded Width = %d, Decoded Height = %d",
        v4l_info->width, v4l_info->height);

    /*
     *   Set the default buffer_required number when
     * the linked pad is not FSL private elements
     */
    if (v4l_info->buffers_required == 0)
      v4l_info->buffers_required = 12;

    mfw_gst_v4lsink_set_format (v4l_info, caps);

    GST_DEBUG ("Decoder maximal reserved %d buffers.",
        v4l_info->buffers_required);

    v4l_info->buffers_required += RESERVEDHWBUFFER_DEPTH;

    if (v4l_info->buffers_required < MIN_BUFFER_NUM) {
      v4l_info->buffers_required = MIN_BUFFER_NUM;
    }


    switch (v4l_info->outformat) {
      case V4L2_PIX_FMT_RGB32:
        frame_buffer_size = (v4l_info->width * v4l_info->height) * 4;
        break;
      case V4L2_PIX_FMT_RGB24:
        frame_buffer_size = (v4l_info->width * v4l_info->height) * 3;
        break;
      case V4L2_PIX_FMT_RGB565:
        frame_buffer_size = (v4l_info->width * v4l_info->height) * 2;
        break;
      case V4L2_PIX_FMT_UYVY:
      case V4L2_PIX_FMT_YUYV:
      case V4L2_PIX_FMT_YUV422P:
        frame_buffer_size = (v4l_info->width * v4l_info->height) * 2;
        break;
      case IPU_PIX_FMT_YUV444P:
        frame_buffer_size = (v4l_info->width * v4l_info->height) * 3;
        break;
      default:
        frame_buffer_size = (v4l_info->width * v4l_info->height) * 3 / 2;
        break;
    }

    max_frames = MAX_V4L_ALLOW_SIZE_IN_BYTE / frame_buffer_size;

    GST_DEBUG ("Decoder maximal support %d buffers.", max_frames);

    if (v4l_info->buffers_required > max_frames) {
      v4l_info->buffers_required = max_frames;
    }
#if !defined(_Mx27)
    if ((v4l_info->cr_left_bypixel == 0) && (v4l_info->crop_left != 0))
#endif
    {
      v4l_info->cr_left_bypixel = v4l_info->crop_left;
      v4l_info->cr_top_bypixel = v4l_info->crop_top;
      v4l_info->cr_right_bypixel = v4l_info->crop_right;
      v4l_info->cr_bottom_bypixel = v4l_info->crop_bottom;
    }

    v4l_info->width = v4l_info->width -
        v4l_info->cr_left_bypixel - v4l_info->cr_right_bypixel;

    v4l_info->height = v4l_info->height -
        v4l_info->cr_top_bypixel - v4l_info->cr_bottom_bypixel;

    result = mfw_gst_v4l2_input_init (v4l_info, v4l_info->outformat);

    if (result != TRUE) {
      g_print ("\n>>V4L_SINK: Failed to initalize the v4l driver\n");
      mfw_gst_v4lsink_close (v4l_info);
      v4l_info->init = FALSE;
      return GST_FLOW_ERROR;
    }
#ifdef USE_X11
    /* Two cases:
     * 1. VPU will request buffer first, when pipeline enter running state,
     *    every parameter is ready.
     * 2. Visualization: The pipeline will enter running state first then buffer_alloc
     *  will be invoked.
     */

    if (element->current_state == GST_STATE_PLAYING) {
      GST_INFO ("element state already switch to PLAYING, create event thread");
      mfw_gst_v4lsink_create_event_thread (v4l_info);

      mfw_gst_v4l2_display_init (v4l_info, v4l_info->disp_width,
          v4l_info->disp_height);

    }
#endif

    /*
     *   The software H264 decoder need check the "num-buffers-required"
     * to decide how to apply the deblock function.
     */
    hwbuffernumforcodec = v4l_info->buffers_required;
    hwbuffernumforcodec -= (BUFFER_RESERVED_NUM + RESERVEDHWBUFFER_DEPTH);

    if (v4l_info->buffers_required != hwbuffernumforcodec) {
      GValue value = { G_TYPE_INT, hwbuffernumforcodec };
      gst_structure_set_value (s, "num-buffers-required", &value);
    }

    g_print
        (">>V4L_SINK: Actually buffer status:\n\thardware buffer : %d\n\tsoftware buffer : %d\n",
        v4l_info->buffers_required, v4l_info->swbuffer_max);

    mfw_gst_v4l2_buffer_init (v4l_info);

    // v4l_info->setpara = PARAM_SET_V4L;

    v4l_info->init = TRUE;

  }

  /* get the V4L hardware buffer */
  v4lsink_buffer = mfw_gst_v4l2_new_buffer (v4l_info);
  if (v4lsink_buffer == NULL) {
    GST_ERROR ("Could not allocate buffer from V4L Driver");
    *buf = NULL;
    return GST_FLOW_ERROR;
  } else {
    GST_BUFFER_SIZE (v4lsink_buffer) = size;
    newbuf = GST_BUFFER_CAST (v4lsink_buffer);
    gst_buffer_set_caps (newbuf, caps);
    *buf = newbuf;
    return GST_FLOW_OK;
  }
}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_base_init

DESCRIPTION:       v4l Sink element details are registered with the plugin during
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
mfw_gst_v4lsink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstCaps *capslist;
  GstPadTemplate *sink_template = NULL;

  mfw_gst_v4l2sink_query_support_formats ();
  /* make a list of all available caps */
  capslist = gst_caps_new_empty ();

  MfwV4lFmtMap * map = g_v4lfmt_maps;

  while (map->mime){
    if (map->enable){
      GstStructure * structure = gst_structure_from_string(map->mime, NULL);
      gst_caps_append_structure (capslist, structure);
    }
    map++;
  };

  sink_template = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, capslist);

  gst_element_class_add_pad_template (element_class, sink_template);

  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "v4l2 video sink",
          "Sink/Video", "Display video by using v4l2 interface");

  return;

}



/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_get_type

DESCRIPTION:        Interfaces are initiated in this function.you can register one
                    or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:   None

RETURN VALUE:       A numerical value ,which represents the unique identifier
                    of this element(v4lsink)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GType
mfw_gst_v4lsink_get_type (void)
{
  static GType mfwV4Lsink_type = 0;

  if (!mfwV4Lsink_type) {
    static const GTypeInfo mfwV4Lsink_info = {
      sizeof (MFW_GST_V4LSINK_INFO_CLASS_T),
      mfw_gst_v4lsink_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_v4lsink_class_init,
      NULL,
      NULL,
      sizeof (MFW_GST_V4LSINK_INFO_T),
      0,
      (GInstanceInitFunc) mfw_gst_v4lsink_init,
    };


    mfwV4Lsink_type = g_type_register_static (GST_TYPE_VIDEO_SINK,
        "MFW_GST_V4LSINK_INFO_T", &mfwV4Lsink_info, 0);
#ifdef USE_X11
    {
      static const GInterfaceInfo iface_info = {
        (GInterfaceInitFunc) mfw_gst_v4lsink_interface_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo overlay_info = {
        (GInterfaceInitFunc) mfw_gst_v4lsink_xoverlay_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo navigation_info = {
        (GInterfaceInitFunc) mfw_gst_v4lsink_navigation_init,
        NULL,
        NULL,
      };

      g_type_add_interface_static (mfwV4Lsink_type,
          GST_TYPE_IMPLEMENTS_INTERFACE, &iface_info);
      g_type_add_interface_static (mfwV4Lsink_type, GST_TYPE_X_OVERLAY,
          &overlay_info);
      g_type_add_interface_static (mfwV4Lsink_type, GST_TYPE_NAVIGATION,
          &navigation_info);
    }
#endif

  }

  GST_DEBUG_CATEGORY_INIT (mfw_gst_v4lsink_debug, "mfw_v4lsink",
      0, "FSL V4L Sink");

  /* Register the v4l own buffer management */
  mfw_gst_v4lsink_buffer_get_type ();

  return mfwV4Lsink_type;
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
  if (!gst_element_register (plugin, "mfw_v4lsink", (FSL_GST_RANK_HIGH + 1),
          MFW_GST_TYPE_V4LSINK))
    return FALSE;

  return TRUE;
}

FSL_GST_PLUGIN_DEFINE("v4lsink", "v4l2-based video sink", plugin_init);


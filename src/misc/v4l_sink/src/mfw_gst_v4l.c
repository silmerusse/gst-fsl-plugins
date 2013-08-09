/*
 * Copyright (c) 2009-2012, Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_v4l.c
 *
 * Description:    Implementation of V4L CORE functions for Gstreamer
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
#include <errno.h>
#include <gst/gst.h>

#include "mfw_gst_v4l.h"
#include "mfw_gst_v4l_buffer.h"
#include "mfw_gst_v4lsink.h"


/*=============================================================================
                             MACROS
=============================================================================*/


//#if (defined (_MX233) || defined (_MX28) || (defined (_MX50)))
#define V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY  8
#define V4L2_FBUF_FLAG_GLOBAL_ALPHA 0x0010
#define V4L2_CAP_VIDEO_OUTPUT_OVERLAY   0x00000200      /* Can do video output overlay */
//#endif


#define DE_INTERLACE_MAX_WIDTH 720



/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/
GST_DEBUG_CATEGORY_EXTERN (mfw_gst_v4lsink_debug);
#define GST_CAT_DEFAULT mfw_gst_v4lsink_debug


/*=============================================================================
                             GLOBAL FUNCTIONS
=============================================================================*/

extern guint mfw_gst_v4lsink_signals[SIGNAL_LAST];

/*=============================================================================
FUNCTION:           dumpfile_open

DESCRIPTION:        This function will open the location file.

ARGUMENTS PASSED:

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
dumpfile_open (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  /* open the file */
  if (v4l_info->dump_location == NULL || v4l_info->dump_location[0] == '\0')
    goto no_dumpfilename;

  v4l_info->dumpfile = fopen (v4l_info->dump_location, "wb");
  if (v4l_info->dumpfile == NULL)
    goto open_failed;

  v4l_info->dump_length = 0;

  GST_DEBUG_OBJECT (v4l_info, "opened file %s", v4l_info->dump_location);

  return TRUE;

  /* ERRORS */
no_dumpfilename:
  {
    GST_ERROR ("No file name specified for dumping.");
    return FALSE;
  }
open_failed:
  {
    GST_ERROR ("Could not open file \"%s\" for writing.",
        v4l_info->dump_location);
    return FALSE;
  }
}

/*=============================================================================
FUNCTION:           dumpfile_close

DESCRIPTION:        This function will close the location file.

ARGUMENTS PASSED:

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
dumpfile_close (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  if (v4l_info->dumpfile) {
    if (fclose (v4l_info->dumpfile) != 0)
      goto close_failed;

    GST_DEBUG_OBJECT (v4l_info, "closed file");
    v4l_info->dumpfile = NULL;
  }
  return;

  /* ERRORS */
close_failed:
  {
    GST_ERROR ("Error closing file:%s", v4l_info->dump_location);
    return;
  }
}

/*=============================================================================
FUNCTION:           dumpfile_set_location

DESCRIPTION:        This function initialize the dump file environment.

ARGUMENTS PASSED:

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
dumpfile_set_location (MFW_GST_V4LSINK_INFO_T * v4l_info,
    const gchar * location)
{
  if (v4l_info->dumpfile)
    goto was_open;

  g_free (v4l_info->dump_location);
  if (location != NULL) {
    v4l_info->enable_dump = TRUE;
    v4l_info->dump_location = g_strdup (location);
  } else {
    v4l_info->enable_dump = FALSE;
    v4l_info->dump_location = NULL;
  }

  return TRUE;

  /* ERRORS */
was_open:
  {
    g_warning ("Changing the `dump_location' property on v4lsink when "
        "a file is open not supported.");
    return FALSE;
  }
}

/*=============================================================================
FUNCTION:           dumpfile_write

DESCRIPTION:        This function write the image to file.

ARGUMENTS PASSED:

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
dumpfile_write (MFW_GST_V4LSINK_INFO_T * v4l_info, GstBuffer * buffer)
{
  guint64 cur_pos;
  guint size;

  size = GST_BUFFER_SIZE (buffer);

  cur_pos = v4l_info->dump_length;

  GST_DEBUG_OBJECT (v4l_info, "writing %u bytes at %" G_GUINT64_FORMAT,
      size, cur_pos);

  if (size > 0 && GST_BUFFER_DATA (buffer) != NULL) {
    if (v4l_info->cr_left_bypixel != 0 || v4l_info->cr_right_bypixel != 0
        || v4l_info->cr_top_bypixel != 0 || v4l_info->cr_bottom_bypixel != 0) {
      /* remove black edge */
      gint y;
      char *p;
      gint cr_left = v4l_info->cr_left_bypixel_orig;
      gint cr_right = v4l_info->cr_right_bypixel_orig;
      gint cr_top = v4l_info->cr_top_bypixel_orig;
      gint cr_bottom = v4l_info->cr_bottom_bypixel_orig;
      gint stride = v4l_info->width + cr_left + cr_right;

      /* Y */
      for (y = cr_top; y < v4l_info->height + cr_top; y++) {
        p = (char *) (GST_BUFFER_DATA (buffer)) + y * stride + cr_left;
        fwrite (p, 1, v4l_info->width, v4l_info->dumpfile);
        v4l_info->dump_length += v4l_info->width;
      }

      /* U */
      for (y = cr_top / 2; y < (v4l_info->height + cr_top) / 2; y++) {
        p = (char *) (GST_BUFFER_DATA (buffer)) +
            stride * (v4l_info->height + cr_top + cr_bottom) +
            (y * stride + cr_left) / 2;
        fwrite (p, 1, v4l_info->width / 2, v4l_info->dumpfile);
        v4l_info->dump_length += (v4l_info->width / 2);
      }

      /* V */
      for (y = cr_top / 2; y < (v4l_info->height + cr_top) / 2; y++) {
        p = (char *) (GST_BUFFER_DATA (buffer)) +
            stride * (v4l_info->height + cr_top + cr_bottom) * 5 / 4 +
            (y * stride + cr_left) / 2;
        fwrite (p, 1, v4l_info->width / 2, v4l_info->dumpfile);
        v4l_info->dump_length += (v4l_info->width / 2);
      }
    } else {
      if (fwrite (GST_BUFFER_DATA (buffer), size, 1, v4l_info->dumpfile)
          != 1)
        goto handle_error;

      v4l_info->dump_length += size;
    }
  }

  return TRUE;

handle_error:
  {
    switch (errno) {
      case ENOSPC:{
        GST_ELEMENT_ERROR (v4l_info, RESOURCE, NO_SPACE_LEFT, (NULL), (NULL));
        break;
      }
      default:{
        GST_ELEMENT_ERROR (v4l_info, RESOURCE, WRITE,
            (("Error while writing to file \"%s\"."),
                v4l_info->dump_location), ("%s", g_strerror (errno)));
      }
    }
    return FALSE;
  }
}

#if defined(ENABLE_TVOUT) && (defined (_MX31) || defined (_MX35))

#define MX31_MX35_PAL_MODE      "U:640x480p-50\n"
#define MX31_MX35_NTSC_MODE     "U:640x480p-60\n"
#define MX31_MX35_LCD_MODE      "U:480x640p-67\n"

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_mx31_mx35_tv_open

DESCRIPTION:        This function open the TV out related device.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_mx31_mx35_tv_open (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  gint out;
  gchar *mode;
  FILE *pfb0_mode;

  pfb0_mode = fopen ("/sys/class/graphics/fb0/mode", "w");
  if (pfb0_mode < 0) {
    g_print ("No /sys/class/graphics/fb0/mode device to open\n");
    return FALSE;
  }

  GST_DEBUG ("pfb0_mode : %x", pfb0_mode);
  if (v4l_info->tv_mode == PAL) {
    mode = MX31_MX35_PAL_MODE;
    fwrite (mode, 1, strlen (mode), pfb0_mode);
  } else if (v4l_info->tv_mode == NTSC) {
    mode = MX31_MX35_NTSC_MODE;
    fwrite (mode, 1, strlen (mode), pfb0_mode);
  } else {
    GST_ERROR ("Wrong TV mode.");
    fclose (pfb0_mode);
    return FALSE;
  }
  fflush (pfb0_mode);
  fclose (pfb0_mode);

  out = 3;
  ioctl (v4l_info->v4l_id, VIDIOC_S_OUTPUT, &out);

  return TRUE;

}

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

gboolean
mfw_gst_v4l2_mx31_mx35_set_lcdmode (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  gchar *mode;
  FILE *pfb0_mode;

  pfb0_mode = fopen ("/sys/class/graphics/fb0/mode", "w");
  if (pfb0_mode < 0) {
    g_print ("No /sys/class/graphics/fb0/mode device to open\n");
    return FALSE;
  }
  mode = MX31_MX35_LCD_MODE;
  fwrite (mode, 1, strlen (mode), pfb0_mode);
  fflush (pfb0_mode);
  fclose (pfb0_mode);

  return TRUE;

}

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_mx31_mx35_tv_close

DESCRIPTION:        This function close the TV related device.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_mx31_mx35_tv_close (MFW_GST_V4LSINK_INFO_T * v4l_info)
{


  gint out = 3;
  gchar *mode;
  FILE *pfb0_mode;

  if ((v4l_info->tv_mode == PAL) || (v4l_info->tv_mode == NTSC)) {
    v4l_info->tv_mode = NV_MODE;
    mfw_gst_v4l2_mx31_mx35_set_lcdmode (v4l_info);
  }

  ioctl (v4l_info->v4l_id, VIDIOC_S_OUTPUT, &out);
  return TRUE;

}

#endif

#if defined(ENABLE_TVOUT) && (defined (_MX37) || defined (_MX51))

#define MX37_MX51_PAL_MODE      "U:720x576i-50\n"
#define MX37_MX51_NTSC_MODE     "U:720x480i-60\n"
#define MX37_MX51_PAL_MODE      "U:720x576i-50\n"
#define MX37_MX51_720P_MODE      "U:1280x720p-60\n"

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_mx37_mx51_tv_open

DESCRIPTION:        This function open the TV out related device.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_mx37_mx51_tv_open (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  gint out;
  gchar *mode;
  FILE *pfb1_mode;

  pfb1_mode = fopen ("/sys/class/graphics/fb1/mode", "w");
  if (pfb1_mode < 0) {
    g_print ("No /sys/class/graphics/fb1/mode device to open\n");
    return FALSE;
  }

  if (v4l_info->tv_mode == PAL) {
    mode = MX37_MX51_PAL_MODE;
    fwrite (mode, 1, strlen (mode), pfb1_mode);
  } else if (v4l_info->tv_mode == NTSC) {
    mode = MX37_MX51_NTSC_MODE;
    fwrite (mode, 1, strlen (mode), pfb1_mode);
  } else if (v4l_info->tv_mode == DISP_720P) {
    mode = MX37_MX51_720P_MODE;
    fwrite (mode, 1, strlen (mode), pfb1_mode);
  } else {
    GST_ERROR ("Wrong TV mode.");
    fclose (pfb1_mode);
    return FALSE;
  }
  fflush (pfb1_mode);
  fclose (pfb1_mode);

  out = 5;
  ioctl (v4l_info->v4l_id, VIDIOC_S_OUTPUT, &out);

  return TRUE;

}

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

gboolean
mfw_gst_v4l2_mx37_mx51_tv_setblank (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  FILE *pfb1_blank;
  gchar *blank = "4\n";

  pfb1_blank = fopen ("/sys/class/graphics/fb1/blank", "w");
  if (pfb1_blank == NULL) {
    GST_DEBUG ("No /sys/class/graphics/fb1/blank device to open\n");
    return FALSE;
  }

  fwrite (blank, 1, strlen (blank), pfb1_blank);
  fflush (pfb1_blank);
  fclose (pfb1_blank);

  return TRUE;

}

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_mx37_mx51_tv_close

DESCRIPTION:        This function close the TV related device.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_mx37_mx51_tv_close (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  gint out = 3;

  // mfw_gst_v4l2_mx37_mx51_tv_setblank (v4l_info);

  v4l_info->tv_mode = NV_MODE;
  ioctl (v4l_info->v4l_id, VIDIOC_S_OUTPUT, &out);

  return TRUE;

}

#endif

#if defined(ENABLE_TVOUT) && defined (_MX27)
gboolean
mfw_gst_v4l2_mx27_tv_open (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  struct v4l2_output_dev odev = {
    .disp_num = 2,
    .id_len = 11,
    .id = "DISP3 TVOUT"
  };

  v4l_info->fd_tvout = open ("/dev/fb/1", O_RDWR);
  if (v4l_info->fd_tvout < 0) {
    GST_ERROR ("Unable to open /dev/fb/1");
    return FALSE;
  }

  if (ioctl (v4l_info->v4l_id, VIDIOC_PUT_OUTPUT, &odev) < 0)
    GST_ERROR ("TV-OUT ioctl VIDIOC_PUT_OUTPUT failed!");

  return TRUE;

}

gboolean
mfw_gst_v4l2_mx27_tv_close (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  if (v4l_info->fd_tvout > 0) {
    struct v4l2_output_dev odev = {
      .disp_num = 2,
      .id_len = 11,
      .id = "DISP3 TVOUT"
    };

    if (ioctl (v4l_info->v4l_id, VIDIOC_GET_OUTPUT, &odev) < 0)
      GST_ERROR ("TV-OUT ioctl VIDIOC_GET_OUTPUT failed!");

    close (v4l_info->fd_tvout);
    v4l_info->fd_tvout = 0;
  }
  return TRUE;
}


#endif


/*=============================================================================
FUNCTION:           mfw_gst_v4l2_open

DESCRIPTION:        This function open the default v4l device.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_open (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  gboolean retval = TRUE;

  /*No need to open v4l device when it has opened--change para on-the-fly */
    if (v4l_info->v4l_id<0){

      if (v4l_info->v4l_dev_name[0]!='\0'){
        v4l_info->v4l_id =
                  open (v4l_info->v4l_dev_name, O_RDWR | O_NONBLOCK, 0);
      }else{
        v4l_info->v4l_id = mfw_gst_get_first_odev();
      }
    }
    /* open the V4l device */
    if ((v4l_info->v4l_id) < 0) {
      GST_ERROR ("Unable to open %s", v4l_info->v4l_dev_name);
      retval = FALSE;
    }
#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
    mfw_gst_v4l2_localpha_open (v4l_info);
#endif


  return retval;

}



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

gboolean
mfw_gst_v4l2_tv_init (MFW_GST_V4LSINK_INFO_T * v4l_info)
{


#if defined (ENABLE_TVOUT) && (defined(_MX31) || defined(_MX35))
  if (TRUE == v4l_info->tv_out) {

    if (!mfw_gst_v4l2_mx31_mx35_tv_open (v4l_info))
      return FALSE;
  } else
    mfw_gst_v4l2_mx31_mx35_tv_close (v4l_info);
#elif defined (ENABLE_TVOUT) &&  (defined(_MX37) || defined(_MX51))
  if (TRUE == v4l_info->tv_out) {
    if (!mfw_gst_v4l2_mx37_mx51_tv_open (v4l_info))
      return FALSE;
  } else
    mfw_gst_v4l2_mx37_mx51_tv_close (v4l_info);
#elif defined (ENABLE_TVOUT) && defined(_MX27)
  if (TRUE == v4l_info->tv_out) {
    if (!mfw_gst_v4l2_mx27_tv_open (v4l_info))
      return FALSE;
  } else
    mfw_gst_v4l2_mx27_tv_close (v4l_info);
  return TRUE;

#endif

}


/*=============================================================================
FUNCTION:           mfw_gst_v4l2_pxp_crop_init

DESCRIPTION:        This function initialise the display device with the specified parameters.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_pxp_crop_init (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  struct v4l2_framebuffer fb;
  struct v4l2_capability cap;
  struct v4l2_output output;

  gboolean retval = TRUE;
  gint out_idx;


  if (ioctl (v4l_info->v4l_id, VIDIOC_QUERYCAP, &cap) < 0) {
    GST_ERROR ("query cap failed");
    retval = FALSE;
    goto err0;;
  }

  if (!(cap.capabilities &
          (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_OVERLAY))) {
    GST_ERROR ("video output overlay not detected");
    retval = FALSE;
    goto err0;

  }


  out_idx = 1;

  if (ioctl (v4l_info->v4l_id, VIDIOC_S_OUTPUT, &out_idx) < 0) {
    GST_ERROR ("failed to set output");
    retval = FALSE;
    goto err0;

  }

  output.index = out_idx;

  if (ioctl (v4l_info->v4l_id, VIDIOC_ENUMOUTPUT, &output) < 0) {
    GST_ERROR ("failed to VIDIOC_ENUMOUTPUT");
    retval = FALSE;
    goto err0;
  }


  fb.flags = V4L2_FBUF_FLAG_OVERLAY;
  fb.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
  if (ioctl (v4l_info->v4l_id, VIDIOC_S_FBUF, &fb) < 0) {
    GST_ERROR ("set fbuf failed");
    retval = FALSE;
    goto err0;
  }

err0:
  return retval;

}


/*=============================================================================
FUNCTION:           mfw_gst_v4l2_get_crop_cap

DESCRIPTION:        This function get the crop capability value.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_get_crop_cap (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  struct v4l2_cropcap cropcap;
  gboolean retval = TRUE;

  /*
   * FixME: The MX233 PXP driver does not support this setting.
   * The define will be removed after BSP Fix
   */
//#if ((!defined (_MX233)) && (!defined (_MX28)) && (!defined (_MX50)))
  if (!(IS_PXP (v4l_info->chipcode))) {
    memset (&cropcap, 0, sizeof (cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl (v4l_info->v4l_id, VIDIOC_CROPCAP, &cropcap) < 0) {
      GST_ERROR ("get crop capability failed");
      retval = FALSE;
    }

    GST_DEBUG ("Capability:screen size:%dx%d.", v4l_info->fullscreen_width,
        v4l_info->fullscreen_height);
  }
//#endif

  return retval;

}




/*=============================================================================
FUNCTION:           mfw_gst_v4l2_set_rotation

DESCRIPTION:        This function set rotation parameters.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_set_rotation (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  struct v4l2_control ctrl;
  gboolean retval;

  /* Set the rotation */
  ctrl.id = V4L2_CID_ROTATE;
  ctrl.value = v4l_info->rotate;
  v4l_info->prevRotate = v4l_info->rotate;
  if (ioctl (v4l_info->v4l_id, VIDIOC_S_CTRL, &ctrl) < 0) {
    GST_ERROR ("set ctrl failed");
    retval = FALSE;
  }


  return retval;

}

//#if (defined (_MX233) || defined (_MX28) || (defined (_MX50)))

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_pxp_set_color

DESCRIPTION:        This function set the color for i.MX233 platform.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_pxp_set_color (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  struct v4l2_control ctrl;
  gboolean retval;

  /* Set background color */
  ctrl.id = V4L2_CID_PRIVATE_BASE + 1;
  ctrl.value = 0xFFFFEE;
  if (ioctl (v4l_info->v4l_id, VIDIOC_S_CTRL, &ctrl) < 0) {
    GST_ERROR ("set ctrl %d failed", ctrl.id);
    retval = FALSE;
  }

  /* Set s0 color */
  ctrl.id = V4L2_CID_PRIVATE_BASE + 2;
  ctrl.value = 0xFFFFEE;
  if (ioctl (v4l_info->v4l_id, VIDIOC_S_CTRL, &ctrl) < 0) {
    GST_ERROR ("set ctrl %d failed", ctrl.id);
    retval = FALSE;
  }

  return retval;

}

//#endif




/*=============================================================================
FUNCTION:           mfw_gst_v4l2_output_setup

DESCRIPTION:        This function set up the display device format

ARGUMENTS PASSED:
        fmt            -   pointer to format for the display device
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       TRUE/FALSE( sucess/failure)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static gboolean
mfw_gst_v4l2_output_setup (struct v4l2_format *fmt,
    MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  struct v4l2_requestbuffers buf_req;

  int ret;

  if (ret = ioctl (v4l_info->v4l_id, VIDIOC_S_FMT, fmt) < 0) {
    GST_ERROR ("set format failed %d", ret);
    return FALSE;
  }

  if (ioctl (v4l_info->v4l_id, VIDIOC_G_FMT, fmt) < 0) {
    GST_ERROR ("get format failed");
    return FALSE;
  }
#if 0                           //test code for sw copy render, also need set MIN_BUFFER_NUM 2
  v4l_info->swbuffer_max = v4l_info->buffers_required - 2;
  v4l_info->buffers_required = 2;

#endif

  while (v4l_info->buffers_required >= MIN_BUFFER_NUM) {

    memset (&buf_req, 0, sizeof (buf_req));
    buf_req.count = v4l_info->buffers_required;
    buf_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf_req.memory = V4L2_MEMORY_MMAP;


    if (ioctl (v4l_info->v4l_id, VIDIOC_REQBUFS, &buf_req) >= 0) {
      GST_LOG ("%d hwbuffers sucessfully allocated.",
          v4l_info->buffers_required);
      return TRUE;
    }else{
      GST_ERROR("Try to allocate %d hw buffer failed!!", v4l_info->buffers_required);
    }
#ifdef NO_SWBUFFER
    g_print (RED_STR ("Can not allocate enough v4lbuffer\n", 0));

    return FALSE;
#endif
    GST_WARNING ("Fall down to allocate software buffer");
    if (v4l_info->swbuffer_max == 0)
      v4l_info->swbuffer_max = 2;
    v4l_info->swbuffer_max++;
    v4l_info->buffers_required--;
  }

  v4l_info->buffers_required = v4l_info->swbuffer_max = 0;

  return FALSE;
}



/*=============================================================================
FUNCTION:           mfw_gst_v4l2_mx6q_set_fmt

DESCRIPTION:        This function initialise the display device with the specified parameters.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T
        inp_format     -   the display foramt
        width     -   width to be displayed
        height    -   height to be displayed

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_mx6q_set_fmt (MFW_GST_V4LSINK_INFO_T * v4l_info,
    guint in_fmt, guint width, guint height)
{
  struct v4l2_format fmt;
  gboolean retval = TRUE;
  gint i;

  guint in_width = 0;
  guint in_height = 0;
  guint video_width = 0, video_height = 0;
  gint cr_left = 0;
  gint cr_top = 0;
  gint cr_right = 0;
  gint cr_bottom = 0;

  guint in_width_chroma = 0, in_height_chroma = 0;
  gint crop_left_chroma = 0;
  gint crop_right_chroma = 0;
  gint crop_top_chroma = 0;
  gint crop_bottom_chroma = 0;

  struct v4l2_mxc_offset off;
  struct v4l2_rect icrop;
  /*No need to set input fmt and req buffer again when change para on-the-fly */
  if (v4l_info->init == FALSE) {
    /* set the input cropping parameters */
    in_width = width;
    in_height = height;
    memset (&fmt, 0, sizeof (fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    cr_left = v4l_info->cr_left_bypixel;
    cr_top = v4l_info->cr_top_bypixel;
    cr_right = v4l_info->cr_right_bypixel;
    cr_bottom = v4l_info->cr_bottom_bypixel;

    fmt.fmt.pix.width = in_width + cr_left + cr_right;
    fmt.fmt.pix.height = in_height + cr_top + cr_bottom;
    fmt.fmt.pix.pixelformat = in_fmt;

    icrop.left = cr_left;
    icrop.top = cr_top;
    icrop.width = in_width;
    icrop.height = in_height;
    fmt.fmt.pix.priv = (unsigned int) &icrop;

    retval = mfw_gst_v4l2_output_setup (&fmt, v4l_info);
    if (retval == FALSE) {
      GST_ERROR ("Error in mfw_gst_v4lsink_output_setup");
    }
//#if (defined (_MX233) || defined (_MX28) || (defined (_MX50)))
    if (IS_PXP (v4l_info->chipcode)) {
      fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
      fmt.fmt.win.w.left = cr_left;
      fmt.fmt.win.w.top = cr_top;
      fmt.fmt.win.w.width = in_width;
      fmt.fmt.win.w.height = in_height;
      fmt.fmt.win.global_alpha = 0;
      fmt.fmt.win.chromakey = 0;

      if (ioctl (v4l_info->v4l_id, VIDIOC_S_FMT, &fmt) < 0) {
        perror ("VIDIOC_S_FMT output overlay");
        return FALSE;
      } else {
        GST_ERROR ("Set frame sucessfully");
      }
    }
//#endif
  }
err0:
  return retval;

}

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_set_fmt

DESCRIPTION:        This function initialise the display device with the specified parameters.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T
        inp_format     -   the display foramt
        width     -   width to be displayed
        height    -   height to be displayed

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_set_fmt (MFW_GST_V4LSINK_INFO_T * v4l_info,
    guint in_fmt, guint width, guint height)
{
  struct v4l2_format fmt;
  gboolean retval = TRUE;
  gint i;

  guint in_width = 0;
  guint in_height = 0;
  guint video_width = 0, video_height = 0;
  gint cr_left = 0;
  gint cr_top = 0;
  gint cr_right = 0;
  gint cr_bottom = 0;

  guint in_width_chroma = 0, in_height_chroma = 0;
  gint crop_left_chroma = 0;
  gint crop_right_chroma = 0;
  gint crop_top_chroma = 0;
  gint crop_bottom_chroma = 0;

  struct v4l2_mxc_offset off;

  /*No need to set input fmt and req buffer again when change para on-the-fly */
  if (v4l_info->init == FALSE) {
    /* set the input cropping parameters */
    in_width = width;
    in_height = height;
    memset (&fmt, 0, sizeof (fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    cr_left = v4l_info->cr_left_bypixel;
    cr_top = v4l_info->cr_top_bypixel;
    cr_right = v4l_info->cr_right_bypixel;
    cr_bottom = v4l_info->cr_bottom_bypixel;

//#if ((!defined (_MX233)) && (!defined (_MX28)) && (!defined (_MX50)))
    if (!(IS_PXP (v4l_info->chipcode))) {
      fmt.fmt.pix.width = in_width;
      fmt.fmt.pix.height = in_height;
      fmt.fmt.pix.pixelformat = in_fmt;
    }
//#else
    else {
      fmt.fmt.pix.width = in_width + cr_left + cr_right;
      fmt.fmt.pix.height = in_height + cr_top + cr_bottom;
      fmt.fmt.pix.pixelformat = in_fmt;
    }
//#endif

    in_width_chroma = in_width / 2;
    in_height_chroma = in_height / 2;

    crop_left_chroma = cr_left / 2;
    crop_right_chroma = cr_right / 2;
    crop_top_chroma = cr_top / 2;
    crop_bottom_chroma = cr_bottom / 2;

    if ((cr_left != 0) || (cr_top != 0) || (cr_right != 0)
        || (cr_bottom != 0)) {

      if (in_fmt == V4L2_PIX_FMT_YUV420) {
        off.u_offset =
            ((cr_left + cr_right + in_width) * (in_height +
                cr_bottom)) -
            cr_left +
            (crop_top_chroma *
            (in_width_chroma + crop_left_chroma + crop_right_chroma))
            + crop_left_chroma;
        off.v_offset = off.u_offset +
            (crop_left_chroma + crop_right_chroma + in_width_chroma)
            * (in_height_chroma + crop_bottom_chroma + crop_top_chroma);

        fmt.fmt.pix.bytesperline = in_width + cr_left + cr_right;
        fmt.fmt.pix.priv = (guint32) & off;
        fmt.fmt.pix.sizeimage = (in_width + cr_left + cr_right)
            * (in_height + cr_top + cr_bottom) * 3 / 2;

      } else if (in_fmt == V4L2_PIX_FMT_YUV422P) {
        in_width_chroma = in_width / 2;
        in_height_chroma = in_height;

        crop_left_chroma = cr_left / 2;
        crop_right_chroma = cr_right / 2;
        crop_top_chroma = cr_top;
        crop_bottom_chroma = cr_bottom;

        off.u_offset =
            ((cr_left + cr_right + in_width) * (in_height +
                cr_bottom)) -
            cr_left +
            (crop_top_chroma *
            (in_width_chroma + crop_left_chroma + crop_right_chroma))
            + crop_left_chroma;
        off.v_offset = off.u_offset +
            (crop_left_chroma + crop_right_chroma + in_width_chroma)
            * (in_height_chroma + crop_bottom_chroma + crop_top_chroma);

        fmt.fmt.pix.bytesperline = in_width + cr_left + cr_right;
        fmt.fmt.pix.priv = (guint32) & off;
        fmt.fmt.pix.sizeimage = (in_width + cr_left + cr_right)
            * (in_height + cr_top + cr_bottom) * 2;

      }

      else if (in_fmt == V4L2_PIX_FMT_NV12) {

        off.u_offset = off.v_offset =
            ((cr_left + cr_right + in_width) * (in_height +
                cr_bottom)) -
            cr_left + (crop_top_chroma * (cr_left + cr_right + in_width))
            + cr_left;

        fmt.fmt.pix.bytesperline = in_width + cr_left + cr_right;
        fmt.fmt.pix.priv = (guint32) & off;
        fmt.fmt.pix.sizeimage = (in_width + cr_left + cr_right)
            * (in_height + cr_top + cr_bottom) * 3 / 2;

      } else {
        g_print (RED_STR
            ("unsupport input format 0x%x for non-zero crop\n", in_fmt));
      }
    } else {
      fmt.fmt.pix.bytesperline = in_width;
      fmt.fmt.pix.priv = 0;
      fmt.fmt.pix.sizeimage = 0;
    }

    retval = mfw_gst_v4l2_output_setup (&fmt, v4l_info);
    if (retval == FALSE) {
      GST_ERROR ("Error in mfw_gst_v4lsink_output_setup");
    }
//#if (defined (_MX233) || defined (_MX28) || (defined (_MX50)))
    if (IS_PXP (v4l_info->chipcode)) {
      fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
      fmt.fmt.win.w.left = cr_left;
      fmt.fmt.win.w.top = cr_top;
      fmt.fmt.win.w.width = in_width;
      fmt.fmt.win.w.height = in_height;
      fmt.fmt.win.global_alpha = 0;
      fmt.fmt.win.chromakey = 0;

      if (ioctl (v4l_info->v4l_id, VIDIOC_S_FMT, &fmt) < 0) {
        perror ("VIDIOC_S_FMT output overlay");
        return FALSE;
      } else {
        GST_ERROR ("Set frame sucessfully");
      }
    }
//#endif
  }
err0:
  return retval;

}


/*=============================================================================
FUNCTION:           mfw_gst_v4l2_set_crop

DESCRIPTION:        This function initialise the crop parameters.

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T
        inp_format     -   the display foramt
        disp_width     -   width to be displayed
        disp_height    -   height to be displayed

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_set_crop (MFW_GST_V4LSINK_INFO_T * v4l_info,
    guint display_width, guint display_height)
{
  struct v4l2_control ctrl;
  struct v4l2_framebuffer fb;
  struct v4l2_crop *crop, *prevCrop;
  struct v4l2_crop newcrop;
  struct v4l2_format fmt;
  gboolean retval = TRUE;
  struct v4l2_buffer buf;
  gint i;

  guint in_width = 0;
  guint in_height = 0;
  guint video_width = 0, video_height = 0;
  gint cr_left = 0;
  gint cr_top = 0;
  gint cr_right = 0;
  gint cr_bottom = 0;

  crop = &v4l_info->crop;
  prevCrop = &v4l_info->prevCrop;

  mfw_gst_v4l2_get_crop_cap (v4l_info);
//#if (defined (_MX233) || defined (_MX28) || (defined (_MX50)))
  if (IS_PXP (v4l_info->chipcode)) {
    mfw_gst_v4l2_pxp_crop_init (v4l_info);
  }
//#endif


  /* set the image rectangle of the display by
     setting the appropriate parameters */
//#if ((!defined (_MX233)) && (!defined (_MX28)) && (!defined (_MX50)))
  if (!(IS_PXP (v4l_info->chipcode)))
    crop->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
//#else
  else
    crop->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
//#endif

  crop->c.width = display_width;
  crop->c.height = display_height;
  crop->c.top = v4l_info->axis_top;
  crop->c.left = v4l_info->axis_left;

  if (!(v4l_info->stretch)) {

    if (v4l_info->rotate == IPU_ROTATE_90_LEFT
        || v4l_info->rotate == IPU_ROTATE_90_RIGHT) {
      video_width = v4l_info->height;
      video_height = v4l_info->width;
    } else {
      video_width = v4l_info->width;
      video_height = v4l_info->height;
    }
    if (crop->c.width * video_height > crop->c.height * video_width) {
      int width = video_width * crop->c.height / video_height;
      width = (width >> 3) << 3;
      crop->c.left = crop->c.left + (crop->c.width - width) / 2;
      crop->c.width = width;
    } else if (crop->c.width * video_height < crop->c.height * video_width) {
      int height = video_height * crop->c.width / video_width;
      height = (height >> 3) << 3;
      crop->c.top = crop->c.top + (crop->c.height - height) / 2;
      crop->c.height = height;
    } else {
      /* do nothing */
    }
  }

  /*
   * FixME: If there is a crop and the input resolution is same with
   *  the output display resolution, the display will be distorted.
   *
   */
#if 0
  if ((v4l_info->rotate == 0)
      && ((v4l_info->cr_left_bypixel)
          || (v4l_info->cr_top_bypixel)
          || (v4l_info->cr_right_bypixel)
          || (v4l_info->cr_bottom_bypixel))) {
    if ((v4l_info->width == crop->c.width)
        && (v4l_info->height == crop->c.height)) {
      g_print ("workaround for same resoution of input and output\n");
      if (v4l_info->width > v4l_info->height) {
        crop->c.height -= 8;
      } else {
        crop->c.width -= 8;
      }
    }
  }
#endif

  GST_INFO
      ("[V4L Previous Display]: left=%d, top=%d, width=%d, height=%d",
      prevCrop->c.left, prevCrop->c.top, prevCrop->c.width, prevCrop->c.height);

  /* Same with the previous settings, do nothing */
  GST_INFO ("[V4L Current Display]: left=%d, top=%d, width=%d, height=%d",
      crop->c.left, crop->c.top, crop->c.width, crop->c.height);

  if ((!memcmp (crop, prevCrop, sizeof (struct v4l2_crop)))
      && (v4l_info->rotate == v4l_info->prevRotate)) {
    // mfw_gst_v4l2_streamoff(v4l_info);
    return FALSE;

  } else {
    memcpy (prevCrop, crop, sizeof (struct v4l2_crop));

    g_print (RED_STR
        ("[V4L Update Display]: left=%d, top=%d, width=%d, height=%d\n",
            crop->c.left, crop->c.top, crop->c.width, crop->c.height));
    /* FixME */
    if (v4l_info->chipcode != CC_MX6Q)
      mfw_gst_v4l2_streamoff (v4l_info);

  }


  if (ioctl (v4l_info->v4l_id, VIDIOC_S_CROP, crop) < 0) {
    GST_ERROR ("set crop failed");
    retval = FALSE;
    goto err0;
  } else
    GST_INFO ("Set crop sucessfully");

  newcrop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if (ioctl( v4l_info->v4l_id, VIDIOC_G_CROP, &newcrop) < 0) {
      GST_WARNING("Get crop failed");
      goto err0;
  }
  GST_INFO("Actual crop settings: %d, %d, %d, %d", newcrop.c.left, newcrop.c.top,
          newcrop.c.width, newcrop.c.height);
err0:
  return retval;

}


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

gboolean
mfw_gst_v4l2_display_init (MFW_GST_V4LSINK_INFO_T * v4l_info,
    guint disp_width, guint disp_height)
{

  gboolean retVal = TRUE;
  guint width, height;

  if ((v4l_info->width == -1) || (v4l_info->height == -1)) {
    GST_WARNING ("Still not get the video information");
    v4l_info->setpara |= PARAM_SET_V4L;
    return FALSE;
  }
  retVal = mfw_gst_v4l2_open (v4l_info);
  if (!retVal)
    return FALSE;
  mfw_gst_fb0_get_resolution (v4l_info);

  width = (disp_width >> 3) << 3;
  height = (disp_height >> 3) << 3;
  if (width == 0) {
    GST_WARNING ("Wrong display width information");
    width = v4l_info->fullscreen_width;
  }
  if (height == 0) {
    GST_WARNING ("Wrong display height information");
    height = v4l_info->fullscreen_height;
  }

  return (mfw_gst_v4l2_set_crop (v4l_info, width, height));
}

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

gboolean
mfw_gst_v4l2_input_init (MFW_GST_V4LSINK_INFO_T * v4l_info, guint inp_format)
{

  gboolean retVal = TRUE;

  guint display_width = 0;
  guint display_height = 0;

  if (v4l_info->init == FALSE)
    v4l_info->querybuf_index = 0;

  v4l_info->frame_dropped = 0;

  /* Read the variables passed by the user */

  mfw_gst_v4l2_open (v4l_info);

#ifdef ENABLE_TVOUT
  if (!mfw_gst_v4l2_tv_init (v4l_info))
    return FALSE;
#endif

  if (TRUE == v4l_info->enable_dump)
    dumpfile_open (v4l_info);

  mfw_gst_v4l2_set_rotation (v4l_info);
//#if (defined (_MX233) || defined (_MX28) || (defined (_MX50)))
  if (IS_PXP (v4l_info->chipcode)) {
    mfw_gst_v4l2_pxp_set_color (v4l_info);
  }
//#endif
  if (v4l_info->chipcode == CC_MX6Q)
    mfw_gst_v4l2_mx6q_set_fmt (v4l_info, inp_format,
        v4l_info->width, v4l_info->height);
  else
    mfw_gst_v4l2_set_fmt (v4l_info, inp_format, v4l_info->width,
        v4l_info->height);


  return retVal;

}


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

gboolean
mfw_gst_v4l2_set_field (MFW_GST_V4LSINK_INFO_T * v4l_info, GstCaps * caps)
{
  struct v4l2_format fmt;
  gint err;
  GstStructure *s;
  gint field;

  if (v4l_info->enable_deinterlace==FALSE)
      return FALSE;

  if ((v4l_info->outformat != V4L2_PIX_FMT_YUV420) &&
      (v4l_info->outformat != V4L2_PIX_FMT_NV12))
    return FALSE;

  s = gst_caps_get_structure (caps, 0);

  if (gst_structure_get_int (s, "field", &field)) {
    switch (field) {
      case FIELD_TOP:
        v4l_info->field = V4L2_FIELD_TOP;
        break;
      case FIELD_BOTTOM:
        v4l_info->field = V4L2_FIELD_BOTTOM;
        break;
      case FIELD_INTERLACED_TB:
        v4l_info->field = V4L2_FIELD_INTERLACED_TB;
        break;
      case FIELD_INTERLACED_BT:
        v4l_info->field = V4L2_FIELD_INTERLACED_BT;
        break;
      default:
        v4l_info->field = V4L2_FIELD_NONE;
        GST_DEBUG ("Field is not supported");
        return FALSE;
    }

#ifdef _MX6
    if (v4l_info->qbuff_count == 0) {
#else
    if (v4l_info->qbuff_count == 1) {
#endif
      if ((v4l_info->field == V4L2_FIELD_TOP) ||
          (v4l_info->field == V4L2_FIELD_BOTTOM) ||
          (v4l_info->field == V4L2_FIELD_INTERLACED_TB) ||
          (v4l_info->field == V4L2_FIELD_INTERLACED_BT)) {
        /* FIXME: Set motion to high, how to smart select the motion type  */
        struct v4l2_control ctrl;
#if (defined (_MX51) || defined (_MX6) )
        ctrl.id = V4L2_CID_MXC_MOTION;
        ctrl.value = v4l_info->motion;
        err = ioctl (v4l_info->v4l_id, VIDIOC_S_CTRL, &ctrl);
        if (err < 0) {
          g_print ("VIDIOC_S_CTRL set motion failed\n");
          return FALSE;
        }
#endif
        /* For interlace feature */
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        err = ioctl (v4l_info->v4l_id, VIDIOC_G_FMT, &fmt);
        if (err < 0) {
          g_print ("VIDIOC_G_FMT failed\n");
          return FALSE;
        }
        if ((v4l_info->field == V4L2_FIELD_TOP) ||
            (v4l_info->field == V4L2_FIELD_BOTTOM))
          fmt.fmt.pix.field = V4L2_FIELD_ALTERNATE;
        else
          fmt.fmt.pix.field = v4l_info->field;
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

        GST_INFO("Set deinterlace motion %d field %d", v4l_info->motion, v4l_info->field);

        err = ioctl (v4l_info->v4l_id, VIDIOC_S_FMT, &fmt);
        if (err < 0) {
          g_print ("VIDIOC_S_FMT failed\n");
          return FALSE;
        }
      }
    }

  }

  return TRUE;
}


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

gboolean
mfw_gst_v4l2_streamon (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  gint type;

#if defined (VL4_STREAM_CALLBACK)
  g_signal_emit (G_OBJECT (v4l_info),
      mfw_gst_v4lsink_signals[SIGNAL_V4L_STREAM_CALLBACK],
      0, VS_EVENT_BEFORE_STREAMON);
#endif

  type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if (ioctl (v4l_info->v4l_id, VIDIOC_STREAMON, &type) < 0) {
    GST_ERROR ("Could not stream on");
    return FALSE;
  } else {
    GST_INFO ("Set to Stream ON successfully");
  }

#if defined (VL4_STREAM_CALLBACK)
  g_signal_emit (G_OBJECT (v4l_info),
      mfw_gst_v4lsink_signals[SIGNAL_V4L_STREAM_CALLBACK],
      0, VS_EVENT_AFTER_STREAMON);

#endif

  v4l_info->stream_on = TRUE;
  return TRUE;
}

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

gboolean
mfw_gst_v4l2_streamoff (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  gint type;
  gint err;
  if (v4l_info->stream_on) {
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
#if defined (VL4_STREAM_CALLBACK)
    g_signal_emit (G_OBJECT (v4l_info),
        mfw_gst_v4lsink_signals[SIGNAL_V4L_STREAM_CALLBACK],
        0, VS_EVENT_BEFORE_STREAMOFF);
#endif
    err = ioctl (v4l_info->v4l_id, VIDIOC_STREAMOFF, &type);
    if (err < 0) {
      g_print ("Set VIDIOC_STREAMOFF failed: %d.\n", err);
      return FALSE;
    } else {
      GST_INFO ("Set to Stream off successfully");
    }

#if defined (VL4_STREAM_CALLBACK)
    g_signal_emit (G_OBJECT (v4l_info),
        mfw_gst_v4lsink_signals[SIGNAL_V4L_STREAM_CALLBACK],
        0, VS_EVENT_AFTER_STREAMOFF);
#endif

    v4l_info->qbuff_count = 0;
    v4l_info->stream_on = FALSE;
  }
  return TRUE;
}



/*=============================================================================
FUNCTION:           mfw_gst_v4l2_close

DESCRIPTION:        This function close the v4l device

ARGUMENTS PASSED:
        v4l_info       -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_close (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  if (v4l_info->v4l_id != -1) {
    GST_INFO("--> Close v4l2 device");
    close (v4l_info->v4l_id);
    v4l_info->v4l_id = -1;
  }
#if 0                           //defined(_MX51)

  /*
   *    FixME: The v4l did not clean the resident previous framebuffer
   *  which will cause the next video flash previous images.
   *  Easy way to remove it with "cat /dev/zero > /dev/fb2"
   *  Will remove this after the BSP's fix.
   *
   */

  {
    gint fb;
    guint8 *buf, *temp;
    gint i;
    gint len = (v4l_info->fullscreen_width * v4l_info->fullscreen_height) >> 1;

    /* Create UYVY black color buffer. */
    buf = g_malloc (len << 2);
    if (buf == NULL) {
      g_print ("malloc buffer failed.\n");
      return FALSE;

    }

    temp = buf;

    for (i = 0; i < len; i++) {
      *(temp + 4 * i) = 0x00;
      *(temp + 4 * i + 1) = 0x80;
      *(temp + 4 * i + 2) = 0x00;
      *(temp + 4 * i + 3) = 0x80;

    }

    if ((fb = open ("/dev/fb2", O_RDWR, 0)) < 0) {
      g_print ("Unable to open %s %d\n", "/dev/fb2", fb);
      g_free (buf);
      return FALSE;
    }

    write (fb, buf, len << 2);
    close (fb);

    if (buf)
      g_free (buf);

  }


#endif
  return TRUE;
}

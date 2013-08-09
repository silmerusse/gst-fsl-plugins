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
 * Module Name:    mfw_gst_fb.c
 *
 * Description:    Implementation of FB operations
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
#include <sys/ioctl.h>
//#if ((!defined (_MX233)) && (!defined (_MX28)) && (!defined (_MX50)))
//#include <linux/mxcfb.h>
//#else
//#include <linux/fb.h>
//#endif

#if ((defined (_MX233)) || (defined (_MX28)) )
#include <linux/fb.h>
#else
#include <linux/fb.h>
#include <linux/mxcfb.h>
#endif

#include "mfw_gst_utils.h"
#include "mfw_gst_fb.h"

#include "mfw_gst_v4lsink.h"

/*=============================================================================
                             MACROS
=============================================================================*/

#define FB_DEIVCE "/dev/fb0"





/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/

extern guint mfw_gst_v4lsink_signals[SIGNAL_LAST];

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/


/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

#define COLORKEY_RED       1 
#define COLORKEY_GREEN     2 
#define COLORKEY_BLUE      3 



#define RGB888(r,g,b)\
    ((((guint32)(r))<<16)|(((guint32)(g))<<8)|(((guint32)(b))))
#define RGB888TORGB565(rgb)\
    ((((rgb)<<8)>>27<<11)|(((rgb)<<18)>>26<<5)|(((rgb)<<27)>>27))

#define RGB565TOCOLORKEY(rgb)                              \
      ( ((rgb & 0xf800)<<8)  |  ((rgb & 0xe000)<<3)  |     \
        ((rgb & 0x07e0)<<5)  |  ((rgb & 0x0600)>>1)  |     \
        ((rgb & 0x001f)<<3)  |  ((rgb & 0x001c)>>2)  )


/*=============================================================================
FUNCTION:           mfw_gst_set_gbl_alpha

DESCRIPTION:        This function Set the global alpha value of fb0.

ARGUMENTS PASSED:

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_set_gbl_alpha(gint fb, gint alphaVal)
{

    gboolean ret = TRUE;

#if ((!defined (_MX233)) && (!defined (_MX28)) && (!defined (_MX50)))
    struct mxcfb_gbl_alpha alpha;

    alpha.alpha = alphaVal;
    alpha.enable = 1;
    if (fb == 0)
        g_print("no fb0 device\n");
    if (ioctl(fb, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
        g_print("set global alpha failed.\n");
        ret = FALSE;
    }
#endif
    return ret;
}

/*=============================================================================
FUNCTION:           mfw_gst_set_gbl_alpha

DESCRIPTION:          This function compute the colorkey and return the color
                    value based on color depth.

ARGUMENTS PASSED:

RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_fb0_set_colorkey(gint fb,gulong *colorSrc)
{
    gboolean ret = TRUE;


#if ((!defined (_MX233)) && (!defined (_MX28)) && (!defined (_MX50)))
    struct mxcfb_color_key colorKey;
    struct fb_var_screeninfo fbVar;

    if (ioctl(fb, FBIOGET_VSCREENINFO, &fbVar) < 0) {
        g_print("get vscreen info failed.\n");
        ret = FALSE;
    }

    if (fbVar.bits_per_pixel == 16) {
        *colorSrc = RGB888TORGB565(RGB888(COLORKEY_RED, COLORKEY_GREEN, COLORKEY_BLUE));
        GST_DEBUG("%08X:%08X:%8X",RGB888(COLORKEY_RED, COLORKEY_GREEN, COLORKEY_BLUE),
            RGB888TORGB565(RGB888(COLORKEY_RED, COLORKEY_GREEN, COLORKEY_BLUE)),
            RGB565TOCOLORKEY(RGB888TORGB565(RGB888(COLORKEY_RED, COLORKEY_GREEN, COLORKEY_BLUE))));
        colorKey.color_key = RGB565TOCOLORKEY(*colorSrc);
    }
    else if ((fbVar.bits_per_pixel == 32) || (fbVar.bits_per_pixel == 24)) {
        *colorSrc = RGB888(COLORKEY_RED, COLORKEY_GREEN, COLORKEY_BLUE);
        colorKey.color_key = *colorSrc;

    }
    GST_DEBUG("fbVar.bits_per_pixel:%d",fbVar.bits_per_pixel);

    GST_INFO("color source:0x%08x,set color key:0x%08x.",*colorSrc,colorKey.color_key);

    colorKey.enable = 1;
    if (ioctl(fb, MXCFB_SET_CLR_KEY, &colorKey) < 0) {
        g_print("set color key failed.\n");
        ret = FALSE;
    }
#endif
    return ret;
}

/*=============================================================================
FUNCTION:           mfw_gst_fb0_open

DESCRIPTION:        This function open the fb0 device return the device pointer.

ARGUMENTS PASSED:

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean mfw_gst_fb0_open(gint *fb)
{
    gboolean retval = TRUE;
    gchar fb_device[100] = FB_DEIVCE;

	if ((*fb =
	     open(fb_device, O_RDWR, 0)) < 0) {
	    g_print("Unable to open %s %d\n", fb_device, *fb);
        *fb = 0;
	    retval = FALSE;
    }
    return retval;

}

/*=============================================================================
FUNCTION:           mfw_gst_fb0_get_resolution

DESCRIPTION:        This function get the maximum resolution of fb0 device.

ARGUMENTS PASSED:

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean mfw_gst_fb0_get_resolution(MFW_GST_V4LSINK_INFO_T *v4l)
{
    gint ret;
    struct fb_var_screeninfo fb_var;

    ret = ioctl(v4l->fd_fb, FBIOGET_VSCREENINFO, &fb_var);
    if (ret < 0) {
        g_print("Unable to get resolution value\n");
        v4l->fullscreen_width = 1024;
        v4l->fullscreen_height = 768;
        return FALSE;
    }
    v4l->fullscreen_width = fb_var.xres;
    v4l->fullscreen_height = fb_var.yres;
    g_print("full screen size:%dx%d\n",v4l->fullscreen_width, v4l->fullscreen_height);

    return TRUE;

}

/*=============================================================================
FUNCTION:           mfw_gst_fb0_close

DESCRIPTION:        This function close the fb0 device.

ARGUMENTS PASSED:
        gstXInfo       -   pointer to GstXInfo

RETURN VALUE:       TRUE/FALSE (SUCCESS/FAIL)

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean mfw_gst_fb0_close(gint *fb)
{
    gboolean retval = TRUE;
    if (*fb) {
        close(*fb);
        *fb = 0;
    }
    return retval;

}

#if ((defined (_MX6) || defined (_MX37) || defined (_MX51)) && defined (LOC_ALPHA_SUPPORT))
/*
 *
 *   The Local alpha will set the alpha value in fb2 which only be
 * supported in MX37 and MX51 platform.
 *
 */

gboolean mfw_gst_v4l2_localpha_open(MFW_GST_V4LSINK_INFO_T *v4l)
{
    gchar fb_device2[100] = "/dev/fb2";

    if ((v4l->fd_lalpfb =
         open(fb_device2, O_RDWR, 0)) < 0) {
        g_print("Unable to open %s %d\n", fb_device2, v4l->fd_lalpfb);
        v4l->fd_lalpfb = 0;
        return FALSE;
    }
    return TRUE;
}

void mfw_gst_v4l2_localpha_close(MFW_GST_V4LSINK_INFO_T *v4l)
{
    if (v4l->fd_lalpfb){
        close(v4l->fd_lalpfb);
        v4l->fd_lalpfb = 0;
    }
    return;
}


void mfw_gst_v4l2_disable_local_alpha(MFW_GST_V4LSINK_INFO_T *v4l)
{
    gint ret;
    struct mxcfb_loc_alpha * lalp = &v4l->lalpha;
    gint lalp_buf_size = v4l->crop.c.width*v4l->crop.c.height;

    if (lalp->enable){
        if (v4l->lalp_buf_vaddr[0]){
            munmap(v4l->lalp_buf_vaddr[0], lalp_buf_size);
            v4l->lalp_buf_vaddr[0] = 0;
        }
        if (v4l->lalp_buf_vaddr[1]){
            munmap(v4l->lalp_buf_vaddr[1], lalp_buf_size);
            v4l->lalp_buf_vaddr[1] = 0;
        }


        g_print("emit %p %p\n", v4l->lalp_buf_vaddr[0], v4l->lalp_buf_vaddr[1]);

        g_signal_emit (G_OBJECT (v4l),
        mfw_gst_v4lsink_signals[SIGNAL_LOCALPHA_BUFFER_READY], 0, v4l->lalp_buf_vaddr[0]);

        lalp->enable = 0;

        if (v4l->fd_lalpfb){
            ret = ioctl(v4l->fd_lalpfb, MXCFB_SET_LOC_ALPHA, lalp);
            if (ret<0){
                g_print("Error on MXCFB_SET_LOC_ALPHA ret = %d\n", ret);
                return;
            }
        }

    }
}

void mfw_gst_v4l2_disable_global_alpha(MFW_GST_V4LSINK_INFO_T *v4l)
{
    int ret;
    struct mxcfb_gbl_alpha galpha;

    galpha.alpha = 0;
    galpha.enable = 1;

    ret = ioctl(v4l->fd_lalpfb, MXCFB_SET_GBL_ALPHA, &galpha);

    if (ret<0){
        g_print("set global alpha  failed ret=%d.\n");
    }
}


void mfw_gst_v4l2_enable_local_alpha(MFW_GST_V4LSINK_INFO_T *v4l)
{
    gint ret;
    struct mxcfb_loc_alpha * lalp = &v4l->lalpha;
    struct fb_var_screeninfo scrinfo;
    gint lalp_buf_size = v4l->crop.c.width*v4l->crop.c.height;
    if ((lalp->enable==0) && (lalp_buf_size)){


        if (ioctl(v4l->fd_lalpfb, FBIOGET_VSCREENINFO,
                  &scrinfo) < 0) {
                printf("Get var of fb2 failed\n");
                return;
        }

        scrinfo.xres = v4l->crop.c.width;
        scrinfo.yres = v4l->crop.c.height;
        scrinfo.xres_virtual = v4l->crop.c.width;
        scrinfo.yres_virtual = v4l->crop.c.height*2;
        if (ioctl(v4l->fd_lalpfb, FBIOPUT_VSCREENINFO,
                  &scrinfo) < 0) {
                printf("Put var of fb2 failed\n");
                return;
        }


        lalp->enable = 1;
        lalp->alpha_phy_addr0 = 0;
        lalp->alpha_phy_addr1 = 0;
        if (v4l->fd_lalpfb){
            ret = ioctl(v4l->fd_lalpfb, MXCFB_SET_LOC_ALPHA, lalp);
            if (ret<0){
                g_print("Error on MXCFB_SET_LOC_ALPHA ret = %d\n", ret);
                return;
            }
            v4l->lalp_buf_vaddr[0] = mmap(0, lalp_buf_size, PROT_READ|PROT_WRITE,
                                        MAP_SHARED,v4l->fd_lalpfb, lalp->alpha_phy_addr0);



            v4l->lalp_buf_vaddr[1] = mmap(0, lalp_buf_size, PROT_READ|PROT_WRITE,
                                        MAP_SHARED,v4l->fd_lalpfb, lalp->alpha_phy_addr1);

            if (v4l->lalp_buf_vaddr[0]==-1){
                g_print(RED_STR("can not mmap for 0 %p size %d\n", lalp->alpha_phy_addr0, lalp_buf_size));
                v4l->lalp_buf_vaddr[0] = 0;
            }

            if (v4l->lalp_buf_vaddr[1]==-1){
                v4l->lalp_buf_vaddr[1] = 0;
                g_print(RED_STR("can not mmap for 1 %p\n", lalp->alpha_phy_addr1, lalp_buf_size));
            }
        }

        g_print("emit %p %p\n", v4l->lalp_buf_vaddr[0], v4l->lalp_buf_vaddr[1]);
        //v4l->lalp_buf_cidx = 0;
        g_signal_emit (G_OBJECT (v4l),
        mfw_gst_v4lsink_signals[SIGNAL_LOCALPHA_BUFFER_READY], 0, v4l->lalp_buf_vaddr[0]);
    }
}

void mfw_gst_v4l2_set_local_alpha(MFW_GST_V4LSINK_INFO_T *v4l, gint alpha)
{
    gint ret;
    struct mxcfb_loc_alpha * lalp = &v4l->lalpha;
    gint lalp_buf_size = v4l->crop.c.width*v4l->crop.c.height;
    char * alphabuf = v4l->lalp_buf_vaddr[v4l->lalp_buf_cidx];

    if (v4l->fd_lalpfb==0)
        return;
    g_print("set local alpha %d \n", alpha);

    if (alphabuf){
        if (alpha>=0){
            memset(alphabuf, alpha, lalp_buf_size);
        }
        ret = ioctl(v4l->fd_lalpfb, MXCFB_SET_LOC_ALP_BUF, (v4l->lalp_buf_cidx ? (&lalp->alpha_phy_addr1):((&lalp->alpha_phy_addr0))));
        if (ret<0){
            g_print("Error on MXCFB_SET_LOC_ALP_BUF ret = %d\n", ret);
        }

        if (v4l->lalp_buf_cidx)
            v4l->lalp_buf_cidx = 0;
        else
            v4l->lalp_buf_cidx = 1;
        g_signal_emit (G_OBJECT (v4l), mfw_gst_v4lsink_signals[SIGNAL_LOCALPHA_BUFFER_READY], 0, v4l->lalp_buf_vaddr[v4l->lalp_buf_cidx]);
    }
}

void mfw_gst_v4l2_set_global_alpha(MFW_GST_V4LSINK_INFO_T *v4l, gint alpha)
{
    int ret;
    struct mxcfb_gbl_alpha galpha;

    galpha.alpha = alpha;
    galpha.enable = 1;

    ret = ioctl(v4l->fd_lalpfb, MXCFB_SET_GBL_ALPHA, &galpha);

    if (ret<0){
        g_print("set global alpha  failed ret=%d.\n");
    }
}


void mfw_gst_v4l2_set_alpha(MFW_GST_V4LSINK_INFO_T *v4l)
{
    if (v4l->fd_lalpfb==0)
        return;
    if (v4l->alpha_enable & ALPHA_LOCAL){
        mfw_gst_v4l2_set_local_alpha(v4l, v4l->alpha);
    }else if (v4l->alpha_enable & ALPHA_GLOBAL){
        mfw_gst_v4l2_set_global_alpha(v4l, v4l->alpha);
    }
}



void mfw_gst_v4l2_set_alpha_enable(MFW_GST_V4LSINK_INFO_T *v4l, gint newmask)
{
    gint ret;
    struct mxcfb_loc_alpha * lalp = &v4l->lalpha;

    g_mutex_lock(v4l->flow_lock);

    if (v4l->alpha_enable==newmask){
        g_mutex_unlock(v4l->flow_lock);
        return;
    }

    g_print("switch to mode %d\n", newmask);


    v4l->alpha_enable=newmask;
    if (v4l->stream_on){
        v4l->setpara |= PARAM_SET_V4L;
    }
    g_mutex_unlock(v4l->flow_lock);
}


#endif


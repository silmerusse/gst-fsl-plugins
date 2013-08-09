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
 * Module Name:    mfw_gst_video_surface.c
 *
 * Description:    Implementation for ipu lib based render service.
 *
 * Portability:    This code is written for Linux OS.
 */

/*
 * Changelog: 
 *
 */

/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <semaphore.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>




#include "mfw_gst_video_surface.h"

#include "fsl_debug.h"

#include "mfw_gst_vss_common.h"

/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/
VSLock *gVSlock = NULL;
VideoSurfacesControl *gVSctl = NULL;
static VideoSurface *gvslocal = NULL;


VSFlowReturn _configMasterVideoSurface (void *vshandle, void *config);

VSFlowReturn _configMasterVideoLayer (void *vshandle, void *config);
VSFlowReturn _configDeinterlace (void *vshandle, void *config);
void _initVSIPUTask (VideoSurface * surf);


static ConfigHandleEntry gConfigHandleTable[] = {
  {CONFIG_MASTER_PARAMETER, sizeof (DestinationFmt), _configMasterVideoSurface}
  ,
  {CONFIG_LAYER, 0, _configMasterVideoLayer}
  ,
  {CONFIG_DEINTERLACE_MODE, sizeof (int), _configDeinterlace},
  {CONFIG_INVALID, 0, NULL}
};


/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/


static int
_checkOnDevice (VideoDevice * vd)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);

  int reconfig = ((vd->init == 0) ? 1 : 0);

  if (reconfig) {
    vd->disp.left = 0;
    vd->disp.top = 0;
    vd->disp.right = vd->resX;
    vd->disp.bottom = vd->resY;
  }
  return reconfig;
}

static void
_reconfigAllVideoSurfaces (VideoDevice * vd)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);

  VideoSurface *vs = DEVICE2HEADSURFACE (vd);
  while (vs) {
    _initVSIPUTask (vs);
    vs = NEXTSURFACE (vs);
  }
}

void
_updateSubFrame (VideoSurface * surf)
{
#ifdef IPULIB
  ipu_lib_input_param_t *input = &surf->itask.input;
  ipu_lib_output_param_t *output = &surf->itask.output;
  ipu_lib_overlay_param_t *overlay = &surf->itask.overlay;
  SubFrame *sub = &surf->subframes[0];
  int stride = output->output_win.win_w;
  int pixwidthbyte = fmt2bit (overlay->fmt) / 8;
  double ratio;
  int x, y, offsetx, offsety;
  DBuffer *dbuf = &surf->subframesbuffer.imgbuf;
  DBuffer *adbuf = &surf->subframesbuffer.alphabuf;

  overlay->width = output->output_win.win_w;
  overlay->height = output->output_win.win_h;

  if (overlay->local_alpha_en) {
    _clearRect (adbuf->vaddr + (stride * surf->subframesbuffer.display.top +
            surf->subframesbuffer.display.left),
        RECT_WIDTH (&surf->subframesbuffer.display),
        RECT_HEIGHT (&surf->subframesbuffer.display), stride, 1);
  } else {
    _clearRect (dbuf->vaddr + (stride * surf->subframesbuffer.display.top +
            surf->subframesbuffer.display.left) * pixwidthbyte,
        RECT_WIDTH (&surf->subframesbuffer.display),
        RECT_HEIGHT (&surf->subframesbuffer.display), stride, pixwidthbyte);
  }


  if ((sub->width < stride) && (sub->height < ((int) output->output_win.win_h))) {
    x = sub->width;
    y = sub->height;
    offsetx = (stride - x) / 2;
    offsety = (output->output_win.win_h - output->output_win.win_h / 8);

    if (offsety + y > output->output_win.win_h)
      offsety = output->output_win.win_h - y;
    if (overlay->local_alpha_en) {
      _copyImage2 (sub->image,
          dbuf->vaddr + (offsetx * 2 + offsety * stride * 2),
          adbuf->vaddr + (offsetx + offsety * stride), x, y, stride);
    } else {
      _copyImage (sub->image,
          dbuf->vaddr + (offsetx * 4 + offsety * stride * 4), x, y, stride);
    }


  } else {
    ratio =
        (double) output->output_win.win_w /
        (double) input->input_crop_win.win_w;
    x = (double) sub->width * ratio;
    offsetx = (double) sub->posx * ratio;

    ratio =
        (double) output->output_win.win_h /
        (double) input->input_crop_win.win_h;
    y = (double) sub->height * ratio;
    offsety = (double) sub->posy * ratio;

    if (overlay->local_alpha_en) {
      _resizeImage2 (sub->image, sub->width, sub->height,
          dbuf->vaddr + (offsetx * 2 + offsety * stride * 2),
          adbuf->vaddr + (offsetx + offsety * stride), x, y, stride);
    } else {
      _resizeImage (sub->image, sub->width, sub->height,
          dbuf->vaddr + (offsetx * 4 + offsety * stride * 4), x, y, stride);
    }
  }

  surf->subframesbuffer.display.left = offsetx;
  surf->subframesbuffer.display.right = offsetx + x;
  surf->subframesbuffer.display.top = offsety;
  surf->subframesbuffer.display.bottom = offsety + y;

  overlay->ov_crop_win.win_w = output->output_win.win_w;
  overlay->ov_crop_win.win_h = output->output_win.win_h;

  surf->itask.mode = 1;
#endif

}

void
_reconfigSubFrameBuffer (VideoSurface * surf)
{

#ifdef IPULIB
  ipu_lib_output_param_t *output = &surf->itask.output;
  int width, height;
  SubFrame *sub = &surf->subframes[0];
  DBuffer *dbuf;
  width = output->output_win.win_w;
  height = output->output_win.win_h;

  int size;



  if (fmt2cs (sub->fmt) != fmt2cs (output->fmt)) {

    size = width * height;
    dbuf = &surf->subframesbuffer.alphabuf;

    if ((dbuf->size < size) || (dbuf->size - size > 100000)) {
      _dBufferRealloc (dbuf, size);
      memset (dbuf->vaddr, ALPHA_TRANSPARENT, size);
    }
    surf->itask.overlay.local_alpha_en = 1;
    surf->itask.overlay.user_def_alpha_paddr[0] = dbuf->paddr;
    surf->itask.overlay.fmt = output->fmt;
  } else {
    surf->itask.overlay.local_alpha_en = 0;
    surf->itask.overlay.user_def_alpha_paddr[0] = NULL;
    surf->itask.overlay.fmt = sub->fmt;
  }
  size = width * height * fmt2bit (surf->itask.overlay.fmt) / 8;
  dbuf = &surf->subframesbuffer.imgbuf;


  if ((dbuf->size < size) || (dbuf->size - size > 100000)) {
    _dBufferRealloc (dbuf, size);
    memset (dbuf->vaddr, 0, size);
    memset (&surf->subframesbuffer.display, 0, sizeof (Rect));
  }

  surf->itask.overlay.user_def_paddr[0] = dbuf->paddr;
#endif
}


void
_destroySubFrameBuffer (VideoSurface * surf)
{

  DBuffer *dbuf;

  dbuf = &surf->subframesbuffer.imgbuf;
  _dBufferFree (dbuf);

  dbuf = &surf->subframesbuffer.alphabuf;
  _dBufferFree (dbuf);
}

static int g_vstable[8][2] = {
  {0, 2},                       //ROTATION_NORMAL
  {0, 3},                       //ROTATION_NORMAL_VFLIP
  {1, 2},                       //ROTATION_NORMAL_HFLIP
  {1, 3},                       //ROTATION_180_CLOCKWISE
  {3, 1},                       //ROTATION_90_CLOCKWISE
  {2, 1},                       //ROTATION_90_CLOCKWISE_VFLIP
  {3, 0},                       //ROTATION_90_CLOCKWISE_HFLIP
  {2, 0},                       //ROTATION_270_CLOCKWISE
};

void
_initVSIPUTask (VideoSurface * surf)
{
  VideoDevice *vd = SURFACE2DEVICE (surf);
  IPUTaskOne *t1 = &surf->itask;
  Rect *rect = &(surf->srcfmt.croprect.win), *desrect;

  VS_FLOW ("Fun %s in\n", __FUNCTION__);

  desrect = &(surf->adjustdesrect);

  if (surf->outside & VS_INVISIBLE) {
    surf->status = VS_STATUS_INVISIBLE;
    return;
  }
  surf->status = VS_STATUS_VISIBLE;

  INPUT_FORMAT (t1) = surf->srcfmt.fmt;
  INPUT_WIDTH (t1) = surf->srcfmt.croprect.width;
  INPUT_HEIGHT (t1) = surf->srcfmt.croprect.height;

  INPUT_CROP_X (t1) = rect->left;
  INPUT_CROP_Y (t1) = rect->top;
  INPUT_CROP_WIDTH (t1) = RECT_WIDTH (rect);
  INPUT_CROP_HEIGHT (t1) = RECT_HEIGHT (rect);
  if (surf->outside) {

    /* output outside of screen, need crop in input */
    int xx[4] = { 0, 0, 0, 0 };
    Rect *origrect = &surf->desfmt.rect;
    int x_len, y_len;


    if (surf->desfmt.rot <= ROTATION_180_CLOCKWISE) {
      x_len = RECT_WIDTH (rect);
      y_len = RECT_HEIGHT (rect);
    } else {
      y_len = RECT_WIDTH (rect);
      x_len = RECT_HEIGHT (rect);
    }

    if (surf->outside & VS_LEFT_OUT) {
      xx[0] =
          (DEVICE_LEFT_EDGE - origrect->left) * x_len / RECT_WIDTH (origrect);
      xx[0] = ALIGNLEFT8 (xx[0]);
    }
    if (surf->outside & VS_RIGHT_OUT) {
      xx[1] = (origrect->right - vd->resX) * x_len / RECT_WIDTH (origrect);
      xx[1] = ALIGNLEFT8 (xx[1]);
    }
    if (surf->outside & VS_TOP_OUT) {
      xx[2] =
          (DEVICE_TOP_EDGE - origrect->top) * y_len / RECT_HEIGHT (origrect);
      xx[2] = ALIGNLEFT8 (xx[2]);
    }
    if (surf->outside & VS_BOTTOM_OUT) {
      xx[3] = (origrect->bottom - vd->resY) * y_len / RECT_HEIGHT (origrect);
      xx[3] = ALIGNLEFT8 (xx[3]);
    }

    if (surf->desfmt.rot <= ROTATION_180_CLOCKWISE) {
      INPUT_CROP_WIDTH (t1) -= (xx[0] + xx[1]);
      INPUT_CROP_HEIGHT (t1) -= (xx[2] + xx[3]);
    } else {
      INPUT_CROP_WIDTH (t1) -= (xx[2] + xx[3]);
      INPUT_CROP_HEIGHT (t1) -= (xx[0] + xx[1]);
    }

    INPUT_CROP_X (t1) += xx[g_vstable[surf->desfmt.rot][0]];
    INPUT_CROP_Y (t1) += xx[g_vstable[surf->desfmt.rot][1]];



  }
  OUTPUT_FORMAT (t1) = vd->fmt;
  OUTPUT_WIDTH (t1) = vd->disp.right - vd->disp.left;
  OUTPUT_HEIGHT (t1) = vd->disp.bottom - vd->disp.top;
  OUTPUT_CROP_X (t1) = desrect->left - vd->disp.left;
  OUTPUT_CROP_Y (t1) = desrect->top - vd->disp.top;
  OUTPUT_CROP_WIDTH (t1) = desrect->right - desrect->left;
  OUTPUT_CROP_HEIGHT (t1) = desrect->bottom - desrect->top;
  OUTPUT_ROTATION (t1) = surf->desfmt.rot;
  if ((INPUT_CROP_WIDTH (t1) < 16) || (INPUT_CROP_HEIGHT (t1) < 16)
      || (OUTPUT_CROP_WIDTH (t1) < 16) || (OUTPUT_CROP_HEIGHT (t1) < 16)) {
    surf->status = VS_STATUS_INVISIBLE;
  }


}

static void
_addVideoSurface2Device (VideoDevice * vd, VideoSurface * vs)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);
  VideoSurface *pvs = DEVICE2HEADSURFACE (vd);

  vs->nextid = 0;

  if (pvs) {
    while (NEXTSURFACE (pvs))
      pvs = NEXTSURFACE (pvs);
    SET_NEXTSURFACE (pvs, vs);
  } else {
    SET_DEVICEHEADSURFACE (vd, vs);
  }
}

static void
_removeVideoSurfaceFromDevice (VideoDevice * vd, VideoSurface * vs)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);
  VideoSurface *pvs = DEVICE2HEADSURFACE (vd);
  if (pvs == vs) {
    SET_DEVICEHEADSURFACE (vd, NEXTSURFACE (vs));
  } else {
    while (NEXTSURFACE (pvs) != vs) {
      pvs = NEXTSURFACE (pvs);
    }
    SET_NEXTSURFACE (pvs, NEXTSURFACE (vs));
  }

}


static void
_refreshOnDevice (VideoDevice * vd)
{
  VideoSurface *vs = DEVICE2HEADSURFACE (vd);
  Updated update;
  while (vs) {
    vs->rendmask = 0;
    _renderSuface (vs, vd, &update);
    vs = NEXTSURFACE (vs);
  }
  _FlipOnDevice (vd);
}

int
_adjustDestRect (Rect * rect, VideoDevice * vd)
{
  int outside = 0;

  if ((rect->right <= DEVICE_LEFT_EDGE) || (rect->left >= vd->resX)
      || (rect->bottom <= DEVICE_TOP_EDGE) || (rect->top >= vd->resY)) {
    outside = VS_INVISIBLE;
    return outside;
  }

  if (rect->left < DEVICE_LEFT_EDGE) {
    outside |= VS_LEFT_OUT;
    rect->left = DEVICE_LEFT_EDGE;
  }
  if (rect->top < DEVICE_TOP_EDGE) {
    outside |= VS_TOP_OUT;
    rect->top = DEVICE_TOP_EDGE;
  }
  if (rect->right > vd->resX) {
    outside |= VS_RIGHT_OUT;
    rect->right = vd->resX;
  }
  if (rect->bottom > vd->resY) {
    outside |= VS_BOTTOM_OUT;
    rect->bottom = vd->resY;
  }

  rect->left = ALIGNRIGHT8 (rect->left);
  rect->top = ALIGNRIGHT8 (rect->top);
  rect->right = ALIGNLEFT8 (rect->right);
  rect->bottom = ALIGNLEFT8 (rect->bottom);

  //VS_MESSAGE("adjust win"WIN_FMT"\n", WIN_ARGS(rect));

  return outside;
}

VSFlowReturn
_configMasterVideoLayer (void *vshandle, void *config)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);

  VideoSurface *vs;
  vs = (VideoSurface *) vshandle;

  if (NEXTSURFACE (vs) == NULL)
    return VS_FLOW_OK;

  VideoDevice *vd = SURFACE2DEVICE (vs);


  _removeVideoSurfaceFromDevice (vd, vs);
  _addVideoSurface2Device (vd, vs);

  _refreshOnDevice (vd);

  VS_FLOW ("Fun %s out\n", __FUNCTION__);

  return VS_FLOW_OK;
}


VSFlowReturn
_configDeinterlace (void *vshandle, void *config)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);

  VideoSurface *vs;
  vs = (VideoSurface *) vshandle;
  int *mode = (int *) config;

  VS_LOCK (gVSlock);

  VS_MESSAGE ("set deinterlace mode %d\n", *mode);

  if (*mode) {
    INPUT_ENABLE_DEINTERLACE (&vs->itask);
  } else {
    INPUT_DISABLE_DEINTERLACE (&vs->itask);
  }

  INPUT_DEINTERLACE_MODE (&vs->itask) = *mode;

  VS_UNLOCK (gVSlock);

  VS_FLOW ("Fun %s out\n", __FUNCTION__);

  return VS_FLOW_OK;
}


VSFlowReturn
_configMasterVideoSurface (void *vshandle, void *config)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);
  DestinationFmt *des = (DestinationFmt *) config;

  VideoSurface *vs;
  vs = (VideoSurface *) vshandle;

  VS_MESSAGE ("reconfig win from " WIN_FMT " to " WIN_FMT "\n",
      WIN_ARGS (&vs->desfmt.rect), WIN_ARGS (&des->rect));

  VideoDevice *vd = SURFACE2DEVICE (vs);


  VS_LOCK (gVSlock);
  vs->desfmt = *des;
  vs->outside = _adjustDestRect (&des->rect, vd);
  vs->adjustdesrect = des->rect;

  if (NEXTSURFACE (vs)) {
    _removeVideoSurfaceFromDevice (vd, vs);
    _addVideoSurface2Device (vd, vs);
  }

  vd->cleanmask = 0xffffffff;

  if (_checkOnDevice (vd)) {
    _reconfigAllVideoSurfaces (vd);
    _setDeviceConfig (vd);
  } else {
    _initVSIPUTask (vs);
  }
  if (vd->setalpha)
    _setAlpha (vd);

  vs->mainframeupdate = 1;

  if (vs->itask.mode) {
    _reconfigSubFrameBuffer (vs);
    vs->mainframeupdate = 0;
    _updateSubFrame (vs);
  }
  _refreshOnDevice (vd);

  VS_UNLOCK (gVSlock);
  VS_FLOW ("Fun %s out\n", __FUNCTION__);

  return VS_FLOW_OK;

}



void
_destroyVideoSurface (void *vshandle, int force)
{
  VideoSurface *vs, *vs1;

  VideoDevice *vd;
  vs = (VideoSurface *) vshandle;

  if (vs == NULL)
    return;

  VS_LOCK (gVSlock);

  if (force == 0) {
    vs1 = gvslocal;
    if (vs1 == vs) {
      gvslocal = vs->next;
    } else {
      while (vs1->next != vs)
        vs1 = vs1->next;
      vs1->next = vs->next;
    }
  }

  vd = SURFACE2DEVICE (vs);

  _removeVideoSurfaceFromDevice (vd, vs);
  // _clearBackground(vd, vs);

  _destroySubFrameBuffer (vs);
  vd->cleanmask = 0xffffffff;


  vd->cnt--;


  if (DEVICE2HEADSURFACE (vd) == NULL) {
    _closeDevice (vd);
    vd->init = 0;
  } else {
    if (_checkOnDevice (vd)) {
      _reconfigAllVideoSurfaces (vd);
      _setDeviceConfig (vd);
    }

    if (vd->setalpha)
      _setAlpha (vd);

    _refreshOnDevice (vd);
  }


  VS_MESSAGE ("VS%d destroyed, force=%d!\n", vs->id - 1, force);

  vs->status = VS_STATUS_IDLE;

  VS_UNLOCK (gVSlock);
}

int
_checkSource (SourceFmt * src)
{

  Rect *rect;
  if ((src->croprect.width & 0x7) || (src->croprect.height & 0x7))
    return -1;

  rect = &src->croprect.win;

  rect->left = ALIGNRIGHT8 (rect->left);
  rect->top = ALIGNRIGHT8 (rect->top);
  rect->right = ALIGNLEFT8 (rect->right);
  rect->bottom = ALIGNLEFT8 (rect->bottom);

  return 0;
}

VideoDevice *
_getDevicebyDevID (int devid)
{
  int i;
  for (i = 0; i < VD_MAX; i++) {
    if (gVSctl->devices[i].fbidx == devid) {
      return &gVSctl->devices[i];
    }
  }
  return NULL;
}


int
queryVideoDevice (int idx, VideoDeviceDesc * vd_desc)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);
  VideoDevice *vd;

  if ((idx < 0) || (idx >= VD_MAX) || (vd_desc == NULL))
    goto bail;



  if (gVSctl == NULL) {
    gVSlock = _getAndLockVSLock (VS_IPC_CREATE);
    if (gVSlock == NULL) {
      VS_ERROR ("Can not create/open ipc semphone!\n");
      goto bail;
    }

    gVSctl = _getVSControl (VS_IPC_CREATE);
    if (gVSctl == NULL) {
      VS_ERROR ("Can not create/open ipc sharememory!\n");
      VS_UNLOCK (gVSlock);
      goto bail;
    }
  } else {

    VS_LOCK (gVSlock);
  }


  vd = &gVSctl->devices[idx];

  vd_desc->resx = vd->resX;
  vd_desc->resy = vd->resY;

  if (vd->fbidx >= 0) {

    int i;

    vd_desc->devid = vd->fbidx;
    vd_desc->custom_mode_num = vd->mode_num;
    memcpy (vd_desc->name, vd->name, NAME_LEN);
    for (i = 0; i < vd_desc->custom_mode_num; i++) {
      vd_desc->modes[i] = vd->modes[i];
    }
  } else {
    VS_UNLOCK (gVSlock);
    goto bail;
  }


  VS_UNLOCK (gVSlock);

  return 0;
bail:
  return -1;
}


/*=============================================================================
FUNCTION:           createVideoSurface

DESCRIPTION:        This function create a video surface.
==============================================================================*/
void *
createVideoSurface (int devid, int mode_idx, SourceFmt * src,
    DestinationFmt * des)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);

  VideoSurfacesControl *vc;
  VideoSurface *vs = NULL;
  VideoDevice *vd;
  int i;

  if (_checkSource (src)) {
    VS_ERROR ("source fmt error\n");
    goto err;
  }

  if ((des == NULL) || (src == NULL)) {
    VS_ERROR ("%s: parameters error!\n", __FUNCTION__);
    goto err;
  }

  if (gVSctl == NULL) {
    gVSlock = _getAndLockVSLock (VS_IPC_CREATE);
    if (gVSlock == NULL) {
      VS_ERROR ("Can not create/open ipc semphone!\n");
      goto err;
    }

    gVSctl = _getVSControl (VS_IPC_CREATE);
    if (gVSctl == NULL) {
      VS_ERROR ("Can not create/open ipc sharememory!\n");
      VS_UNLOCK (gVSlock);
      goto err;
    }
  } else {

    VS_LOCK (gVSlock);
  }

  vc = gVSctl;

  if ((vd = _getDevicebyDevID (devid)) == NULL) {
    VS_ERROR ("Can not find dev id %d!\n", devid);
    VS_UNLOCK (gVSlock);
    goto err;
  }

  if (vd->cnt >= vd->vsmax) {
    VS_UNLOCK (gVSlock);
    VS_ERROR ("%s: max surfaces on device support on device%d exceeded!\n",
        __FUNCTION__, devid);
    goto err;
  }

  for (i = 0; i < VS_MAX; i++) {
    if (vc->surfaces[i].status == VS_STATUS_IDLE) {
      break;
    }
  }

  if (i == VS_MAX) {
    VS_UNLOCK (gVSlock);
    VS_ERROR ("%s: max surface support exceeded!\n", __FUNCTION__);
    goto err;
  }

  vs = &vc->surfaces[i];
  vs->status = VS_STATUS_VISIBLE;
  vs->vd_id = vd->id;
  vs->srcfmt = *src;
  vs->desfmt = *des;

  vs->itask.mode = 0;
  vs->mainframeupdate = 1;

  memset (&vs->itask, 0, sizeof (IPUTaskOne));

  if (vd->init == 0) {
    if (_initVideoDevice (vd, mode_idx)) {
      VS_UNLOCK (gVSlock);
      VS_ERROR ("%s: error config!\n", __FUNCTION__);
      goto err;
    }

  }

  vs->outside = _adjustDestRect (&des->rect, vd);
  vs->adjustdesrect = des->rect;

  VS_MESSAGE ("VS%d created. in fmt[" FOURCC_FMT "] win" WIN_FMT " out win"
      WIN_FMT "\n", vs->id - 1, FOURCC_ARGS (src->fmt),
      WIN_ARGS (&src->croprect.win), WIN_ARGS (&vs->desfmt.rect));

  vs->next = gvslocal;
  gvslocal = vs;

  vd->cnt++;

  if (vd->cnt == 1) {
    _openDevice (vd);

  }
  _addVideoSurface2Device (vd, vs);

  if (_checkOnDevice (vd)) {
    _reconfigAllVideoSurfaces (vd);
    _setDeviceConfig (vd);
  }

  vd->init = 1;
  _initVSIPUTask (vs);

  if (vd->setalpha)
    _setAlpha (vd);

  VS_UNLOCK (gVSlock);
  VS_FLOW ("Fun %s out\n", __FUNCTION__);

  return (void *) vs;
err:
  if (vs) {
    vs->status = VS_STATUS_IDLE;
  }
  return NULL;
}



/*=============================================================================
FUNCTION:           destroyVideoSurface

DESCRIPTION:        This function destroy a video surface create before
==============================================================================*/
void
destroyVideoSurface (void *vshandle)
{
  _destroyVideoSurface (vshandle, 0);
}


/*=============================================================================
FUNCTION:           configVideoSurface

DESCRIPTION:        This function reconfig the specific video surface including 
                    output window size, position and rotation.
==============================================================================*/
VSFlowReturn
configVideoSurface (void *vshandle, VSConfigID confid, VSConfig * config)
{
  int ret = VS_FLOW_OK;
  ConfigHandleEntry *configentry = gConfigHandleTable;
  if (vshandle == NULL)
    return VS_FLOW_PARAMETER_ERROR;


  while (configentry->cid != CONFIG_INVALID) {
    if (configentry->cid == confid) {
      if (config->length != configentry->parameterlen) {
        return VS_FLOW_PARAMETER_ERROR;
      }
      if (configentry->handle) {
        ret = (*configentry->handle) (vshandle, config->data);
        return ret;
      }
    }
    configentry++;
  }
  return ret;
}


/*=============================================================================
FUNCTION:           render2VideoSurface

DESCRIPTION:        This function render a new frame on specific video surface
                    It also will refresh other video surfaces in same video device
                    automaticallly.
==============================================================================*/
VSFlowReturn
render2VideoSurface (void *vshandle, SourceFrame * frame, SourceFmt * srcfmt)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);

  VideoDevice *vd;
  VideoSurface *vsurface, *vsurface1;
  Updated updated;

  if ((vshandle == NULL) || (frame == NULL)) {
    VS_ERROR ("%s: parameters error!\n", __FUNCTION__);
    return VS_FLOW_PARAMETER_ERROR;
  }

  vsurface = (VideoSurface *) vshandle;


  vsurface->paddr = frame->paddr;
  vsurface->rendmask = 0;       /*clear mask */

  if (vsurface->status == VS_STATUS_INVISIBLE)  /* no need to render */
    return VS_FLOW_OK;


  vd = SURFACE2DEVICE (vsurface);
#if 0
  if (vsurface->id != vd->headid) {
    if (vsurface->misscnt <= 2) {
      vsurface->misscnt++;
      return VS_FLOW_OK;
    }
    g_print ("slave move\n");

  }

  vsurface->misscnt = 0;
#else

  if (vd->rendering) {
    if (sem_trywait (gVSlock))
      return VS_FLOW_PENDING;
  } else {
    VS_LOCK (gVSlock);
  }

#endif

  vd->rendering = 1;

  //if (sem_trywait(gVSlock))
  //    return VS_FLOW_PENDING;

  vsurface1 = DEVICE2HEADSURFACE (vd);

  memset ((void *) (&updated), 0, sizeof (Updated));
  while (vsurface1) {
    if (_needRender (vsurface1, &updated, vd->renderidx)) {
      _renderSuface (vsurface1, vd, &updated);
    }
    vsurface1 = NEXTSURFACE (vsurface1);
  };

  _FlipOnDevice (vd);

#if 0                           /* no need to sleep anymore */
  if (vd->cnt > 1)
    usleep (10000);
#endif

  vd->rendering = 0;

  VS_UNLOCK (gVSlock);

  VS_FLOW ("Fun %s out\n", __FUNCTION__);
  return VS_FLOW_OK;
}



/*=============================================================================
FUNCTION:           updateSubFrame2VideoSurface

DESCRIPTION:        This function updata a subframe to video surface.
==============================================================================*/
VSFlowReturn
updateSubFrame2VideoSurface (void *vshandle, SubFrame * subframe, int idx)
{
  VideoSurface *vsurface;

  if (vshandle == NULL)
    return VS_FLOW_PARAMETER_ERROR;

  vsurface = (VideoSurface *) vshandle;
  VS_LOCK (gVSlock);

  if (subframe) {
    vsurface->subframes[idx].width = subframe->width;
    vsurface->subframes[idx].height = subframe->height;
    vsurface->subframes[idx].posx = subframe->posx;
    vsurface->subframes[idx].posy = subframe->posy;
    vsurface->subframes[idx].image = subframe->image;
    vsurface->subframes[idx].fmt = SUBFRAME_DEFAULT_FMT;
    vsurface->itask.mode = 1;


    if (vsurface->mainframeupdate) {
      _reconfigSubFrameBuffer (vsurface);
      vsurface->mainframeupdate = 0;
    }
    _updateSubFrame (vsurface);


  } else {
    vsurface->itask.mode = 0;
  }

  VS_UNLOCK (gVSlock);
  return VS_FLOW_OK;
}



/*=============================================================================
FUNCTION:           video_surface_destroy

DESCRIPTION:        This function deconstruct created video surfaces and recycle 
                    all hardware resource.
==============================================================================*/
void __attribute__ ((destructor)) video_surface_destroy (void);
void
video_surface_destroy (void)
{
  VS_FLOW ("Fun %s in\n", __FUNCTION__);
  VideoSurface *vs = gvslocal, *vsnext;
  while (vs) {
    vsnext = vs->next;
    _destroyVideoSurface (vs, 1);
    vs = vsnext;
  }
}

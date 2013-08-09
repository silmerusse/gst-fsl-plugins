/*
 * Copyright (c) 2010-2012, Freescale Semiconductor, Inc. All rights reserved. 
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
 * Module Name:    mfw_gst_vss_common.h
 *
 * Description:    Head file for ipu lib based render service.
 *
 * Portability:    This code is written for Linux OS.
 */

/*
 * Changelog: 
 *
 */

#ifndef __VSS_COMMON_H__
#define __VSS_COMMON_H__
#ifdef IPULIB
#include "mxc_ipu_hl_lib.h"
#else
#include <linux/mxcfb.h>
#include <linux/ipu.h>
#endif


#define VS_ERROR(format,...)    DEBUG_ERROR(format, ##__VA_ARGS__)
#define VS_FLOW(format,...)     DEBUG_FLOW(format, ##__VA_ARGS__)
#define VS_MESSAGE(format,...)  DEBUG_MESSAGE(format, ##__VA_ARGS__)

#define WIN_FMT "(%d,%d-%d,%d:%dx%d)"
#define WIN_ARGS(rect) \
    (rect)->left,(rect)->top,(rect)->right,(rect)->bottom,(rect)->right-(rect)->left,(rect)->bottom-(rect)->top

#define FOURCC_FMT "%c%c%c%c"
#define FOURCC_ARGS(fourcc) (char)(fourcc),(char)((fourcc)>>8),(char)((fourcc)>>16),(char)((fourcc)>>24)

#define VS_MAX 8
#define VS_SUBFRAME_MAX 1

#define ALPHA_TRANSPARENT 0
#define ALPHA_SOLID 255
#define RGB565_BLACK 0

#define MAIN_DEVICE_NAME "/dev/fb0"
#define VS_LOCK_NAME "vss_lock"
#define VS_SHMEM_NAME "vss_shmem"

#define VS_LEFT_OUT     0x1
#define VS_RIGHT_OUT    0x2
#define VS_TOP_OUT      0x4
#define VS_BOTTOM_OUT   0x8
#define VS_INVISIBLE    0x10

#define FB_NUM_BUFFERS (3)


#define VS_IPC_CREATE 0x1
#define VS_IPC_EXCL 0x2

#define DEVICE_LEFT_EDGE 0
#define DEVICE_TOP_EDGE 0

#define SUBFRAME_DEFAULT_FMT IPU_PIX_FMT_ABGR32

#define SUBFRAME_FMT IPU_PIX_FMT_YUYV
#define FB1_FMT IPU_PIX_FMT_UYVY
#define FB2_FMT IPU_PIX_FMT_RGB565


/* FIX ME failed with only csc for 320*240->1920*1080 */
#define CLEAR_SOURCE_WIDTH 640
#define CLEAR_SOURCE_HEIGHT 480
#define CLEAR_SOURCE_FORMAT IPU_PIX_FMT_RGB565



/* function macros */
#define VS_IOCTL(device, request, errorroute, ...)\
    do{\
        int ret;\
        if ((ret = ioctl((device), (request), ##__VA_ARGS__))<0){\
            VS_ERROR("%s:%d ioctl error, return %d\n", __FILE__, __LINE__, ret);\
            goto errorroute;\
        }\
    }while(0)

#define VS_LOCK(lock) \
    do {\
                sem_wait((lock));\
    }while(0)
#define VS_TRY_LOCK(lock) \
    do {\
        sem_trywait((lock));\
    }while(0)
#define VS_UNLOCK(lock) \
    do {\
        sem_post((lock));\
    }while(0)

#define DEVICE2HEADSURFACE(device)\
    (((device)->headid==0)?NULL:(&(gVSctl->surfaces[(device)->headid-1])))

#define SET_DEVICEHEADSURFACE(device, surface)\
    do{\
        if ((surface)){\
            (device)->headid = (surface)->id;\
        }else{\
            (device)->headid = 0;\
        }\
    }while(0)

#define SET_NEXTSURFACE(surfacebefore,surface)\
    do{\
        if ((surface)){\
            (surfacebefore)->nextid = (surface)->id;\
        }else{\
            (surfacebefore)->nextid = 0;\
        }\
    }while(0)

#define NEXTSURFACE(surface)\
    (((surface)->nextid==0)?NULL:(&(gVSctl->surfaces[(surface)->nextid-1])))

#define SURFACE2DEVICE(surface)\
    (&(gVSctl->devices[(surface)->vd_id-1]))

#define NEXT_RENDER_ID(idx)\
    (((idx)+1) % FB_NUM_BUFFERS)

#define ID2INDEX(id) ((id)-1)
#define INDEX2ID(index) ((index)+1)


#define OVERLAPED_RECT(rect1, rect2)\
    (((rect1)->top<(rect2)->bottom)||((rect2)->top<(rect1)->bottom)\
    ||((rect1)->left<(rect2)->right)||((rect2)->left<(rect1)->right))


#ifndef IPULIB
typedef struct
{
  struct ipu_task task;
  int mode;                     /* 0 no overlay; 1: with overlay */
} IPUTaskOne;
#define KICK_IPUTASKONE(ihandle, itask)\
  do{\
      int ret = IPU_CHECK_ERR_INPUT_CROP;\
      while(ret != IPU_CHECK_OK && ret > IPU_CHECK_ERR_MIN) {\
          ret = ioctl((ihandle), IPU_CHECK_TASK, (&((itask)->task)));\
          switch(ret) {\
              case IPU_CHECK_OK:\
                  break;\
              case IPU_CHECK_ERR_SPLIT_INPUTW_OVER:\
                  (itask)->task.input.crop.w -= 8;\
                  break;\
              case IPU_CHECK_ERR_SPLIT_INPUTH_OVER:\
                  (itask)->task.input.crop.h -= 8;\
                  break;\
              case IPU_CHECK_ERR_SPLIT_OUTPUTW_OVER:\
                  (itask)->task.output.crop.w -= 8;\
                  break;\
              case IPU_CHECK_ERR_SPLIT_OUTPUTH_OVER:\
                  (itask)->task.output.crop.h -= 8;\
                  break;\
              default:\
                  break;\
          }\
      }\
      ret = ioctl((ihandle), IPU_QUEUE_TASK, (&((itask)->task)));\
      if(ret < 0) {\
      }\
  }while(0)
#define INPUT_FORMAT(itask) ((itask)->task.input.format)
#define INPUT_WIDTH(itask) ((itask)->task.input.width)
#define INPUT_HEIGHT(itask) ((itask)->task.input.height)
#define INPUT_CROP_X(itask) ((itask)->task.input.crop.pos.x)
#define INPUT_CROP_Y(itask) ((itask)->task.input.crop.pos.y)
#define INPUT_CROP_WIDTH(itask) ((itask)->task.input.crop.w)
#define INPUT_CROP_HEIGHT(itask) ((itask)->task.input.crop.h)
#define INPUT_PADDR(itask) ((itask)->task.input.paddr)
#define INPUT_DEINTERLACE_MODE(itask) ((itask)->task.input.deinterlace.motion)
#define INPUT_ENABLE_DEINTERLACE(itask) ((itask)->task.input.deinterlace.enable = 1)
#define INPUT_DISABLE_DEINTERLACE(itask) ((itask)->task.input.deinterlace.enable = 0)



#define OUTPUT_FORMAT(itask) ((itask)->task.output.format)
#define OUTPUT_WIDTH(itask) ((itask)->task.output.width)
#define OUTPUT_HEIGHT(itask) ((itask)->task.output.height)
#define OUTPUT_CROP_X(itask) ((itask)->task.output.crop.pos.x)
#define OUTPUT_CROP_Y(itask) ((itask)->task.output.crop.pos.y)
#define OUTPUT_CROP_WIDTH(itask) ((itask)->task.output.crop.w)
#define OUTPUT_CROP_HEIGHT(itask) ((itask)->task.output.crop.h)
#define OUTPUT_ROTATION(itask) ((itask)->task.output.rotate)
#define OUTPUT_PADDR(itask) (((itask)->task.output.paddr))



#else

typedef struct
{
  ipu_lib_input_param_t input;
  ipu_lib_output_param_t output;
  ipu_lib_overlay_param_t overlay;
  ipu_lib_handle_t handle;
  int mode;                     /* 0 no overlay; 1: with overlay */
} IPUTaskOne;


#ifdef IPULIB_SPLITMODE_OVERFLOW_FIX

#define KICK_IPUTASKONE(ihandle, itask)\
  do{\
    int ret;\
    do{\
      (itask)->input.motion_sel = 2;\
      int mode = (((itask)->mode==0) ? \
      (((itask)->input.motion_sel)? (TASK_VDI_VF_MODE |OP_NORMAL_MODE):(TASK_PP_MODE|OP_NORMAL_MODE)):(TASK_VF_MODE|OP_NORMAL_MODE)) ;\
      ret =  mxc_ipu_lib_task_init(&(itask)->input, NULL, &(itask)->output, mode, &(itask)->handle);\
      if(ret == IPU_STATE_SPLIT_MODE_WIDTH_OVER){\
          (itask)->output.output_win.pos.x += 8;\
          (itask)->output.output_win.win_w -= 16;\
      }else if(ret == IPU_STATE_SPLIT_MODE_HEIGHT_OVER){\
          (itask)->output.output_win.pos.y += 8;\
          (itask)->output.output_win.win_h -= 16;\
      }\
    }while((ret == IPU_STATE_SPLIT_MODE_WIDTH_OVER)||\
          (ret == IPU_STATE_SPLIT_MODE_HEIGHT_OVER));\
    mxc_ipu_lib_task_buf_update(&(itask)->handle, NULL, NULL, NULL, NULL,NULL);\
    mxc_ipu_lib_task_uninit(&(itask)->handle);\
  }while(0)

#else

#define KICK_IPUTASKONE(ihandle, itask)\
  do{\
    (itask)->input.motion_sel=2;\
    if ((itask)->mode==0){\
        int mode = (((itask)->input.motion_sel)? (TASK_VDI_VF_MODE |OP_NORMAL_MODE):(TASK_PP_MODE|OP_NORMAL_MODE));\
        mxc_ipu_lib_task_init(&(itask)->input, NULL, &(itask)->output, mode, &(itask)->handle);\
        mxc_ipu_lib_task_buf_update(&(itask)->handle, NULL, NULL, NULL, NULL,NULL);\
        mxc_ipu_lib_task_uninit(&(itask)->handle);\
    }else{\
        mxc_ipu_lib_task_init(&(itask)->input, &(itask)->overlay, &(itask)->output, TASK_VF_MODE|OP_NORMAL_MODE, &(itask)->handle);\
        mxc_ipu_lib_task_buf_update(&(itask)->handle, NULL, NULL, NULL, NULL,NULL);\
        mxc_ipu_lib_task_uninit(&(itask)->handle);\
    }\
  }while(0)
#endif

#define INPUT_FORMAT(itask) ((itask)->input.fmt)
#define INPUT_WIDTH(itask) ((itask)->input.width)
#define INPUT_HEIGHT(itask) ((itask)->input.height)
#define INPUT_CROP_X(itask) ((itask)->input.input_crop_win.pos.x)
#define INPUT_CROP_Y(itask) ((itask)->input.input_crop_win.pos.y)
#define INPUT_CROP_WIDTH(itask) ((itask)->input.input_crop_win.win_w)
#define INPUT_CROP_HEIGHT(itask) ((itask)->input.input_crop_win.win_h)
#define INPUT_PADDR(itask) ((itask)->input.user_def_paddr[0])
#define INPUT_DEINTERLACE_MODE(itask) ((itask)->input.motion_sel)
#define INPUT_ENABLE_DEINTERLACE(itask)
#define INPUT_DISABLE_DEINTERLACE(itask)

#define OUTPUT_FORMAT(itask) ((itask)->output.fmt)
#define OUTPUT_WIDTH(itask) ((itask)->output.width)
#define OUTPUT_HEIGHT(itask) ((itask)->output.height)
#define OUTPUT_CROP_X(itask) ((itask)->output.output_win.pos.x)
#define OUTPUT_CROP_Y(itask) ((itask)->output.output_win.pos.y)
#define OUTPUT_CROP_WIDTH(itask) ((itask)->output.output_win.win_w)
#define OUTPUT_CROP_HEIGHT(itask) ((itask)->output.output_win.win_h)
#define OUTPUT_ROTATION(itask) ((itask)->output.rot)
#define OUTPUT_PADDR(itask) ((itask)->output.user_def_paddr[0])

#endif

#define ALIGNLEFT8(value) (((value)>>3)<<3)
#define ALIGNRIGHT8(value) (((value+7)>>3)<<3)


/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/


typedef enum
{
  VS_STATUS_IDLE = 0,
  VS_STATUS_VISIBLE,
  VS_STATUS_INVISIBLE,
} VS_STATUS;

typedef sem_t VSLock;

typedef struct
{
  int updated;
  Rect rect;
} Updated;


typedef struct
{
  int size;
  void *handle;
  void *paddr;
  void *vaddr;
} DBuffer;

typedef struct
{
  DBuffer imgbuf;
  DBuffer alphabuf;
  Rect display;
} SubFrameBuffer;

typedef struct _VideoSurface
{
  int id;
  int nextid;
  int vd_id;

  volatile void *paddr;

  int mainframeupdate;
  SubFrame subframes[VS_SUBFRAME_MAX];
  SubFrameBuffer subframesbuffer;       /* subtitle buffer, dmable */

  volatile unsigned int rendmask;       /* render mask for pingpang buffer */
  VS_STATUS status;
  SourceFmt srcfmt;
  DestinationFmt desfmt;
  Rect adjustdesrect;
  IPUTaskOne itask;
  int outside;                  /* out of screen and need reconfig input */
  struct _VideoSurface *next;
} VideoSurface;

typedef struct _VideoDevice
{
  int headid;
  int fbidx;
  int main_fbidx;
  int renderidx;
  unsigned int cleanmask;
  void *bufaddr[FB_NUM_BUFFERS];
  int fmt;

  Rect disp;
  int resX;
  int resY;

  int id;
  int init;
  int setalpha;

  struct fb_var_screeninfo fbvar;
  int cnt;

#ifdef METHOD2
  IPUTaskOne copytask;
#endif
  struct timeval timestamp;
  int vsmax;

  int current_mode;
  int mode_num;
  int rendering;
  int autoclose;
  VideoMode modes[VM_MAX];

  char name[NAME_LEN];
} VideoDevice;

typedef struct
{
  VideoSurface surfaces[VS_MAX];
  VideoDevice devices[VD_MAX];  /* start from fb1 to fb2 */
  int init;
} VideoSurfacesControl;

typedef VSFlowReturn (*ConfigHandle) (void *, void *);

typedef struct
{
  VSConfigID cid;
  int parameterlen;
  ConfigHandle handle;
} ConfigHandleEntry;

void _dBufferRealloc (DBuffer * dbuf, int size);
void _dBufferFree (DBuffer * dbuf);
int _renderSuface (VideoSurface * surf, VideoDevice * vd, Updated * updated);
int _setDeviceConfig (VideoDevice * vd);
int _setAlpha (VideoDevice * vd);
int _closeDevice (VideoDevice * vd);
VSLock *_getAndLockVSLock (int flag);
int _FlipOnDevice (VideoDevice * vd);
VideoSurfacesControl *_getVSControl (int flag);
int _initVideoDevice (VideoDevice * vd, int mode_idx);
int _openDevice (VideoDevice * vd);
int _needRender (VideoSurface * curSurf, Updated * updated, int renderidx);


#endif

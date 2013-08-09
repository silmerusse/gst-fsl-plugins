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
 * Module Name:    mfw_gst_video_surface.h
 *
 * Description:    Head file for ipu lib based render service.
 *
 * Portability:    This code is written for Linux OS.
 */

/*
 * Changelog: 
 *
 */

#ifndef __VIDEO_SURFACE_H__
#define __VIDEO_SURFACE_H__



#define VM_MAX 3
#define VD_MAX 6
#define NAME_LEN 8


#define RECT_WIDTH(rect) ((rect)->right-(rect)->left)
#define RECT_HEIGHT(rect) ((rect)->bottom-(rect)->top)

typedef enum
{
  CONFIG_INVALID,
  CONFIG_LAYER,
  CONFIG_MASTER_PARAMETER,
  CONFIG_ATTACH_SLAVE,
  CONFIG_DEINTERLACE_MODE,
} VSConfigID;


typedef enum
{
  ROTATION_NORMAL = 0,
  ROTATION_NORMAL_VFLIP = 1,
  ROTATION_NORMAL_HFLIP = 2,
  ROTATION_180_CLOCKWISE = 3,
  ROTATION_90_CLOCKWISE = 4,
  ROTATION_90_CLOCKWISE_VFLIP = 5,
  ROTATION_90_CLOCKWISE_HFLIP = 6,
  ROTATION_270_CLOCKWISE = 7,
} Rotation;


typedef enum
{
  TYPE_VIDEO,
  TYPE_SUBTITLE,
} VSSourceType;

typedef enum
{
  VS_FLOW_NOT_SUPPORT = -10,
  VS_FLOW_PARAMETER_ERROR = -9,
  VS_FLOW_NO_RESOURCE = -2,
  VS_FLOW_ERROR = -1,
  VS_FLOW_OK = 0,
  VS_FLOW_PENDING = 1,
} VSFlowReturn;

typedef struct
{
  int length;
  void *data;
} VSConfig;


typedef struct
{
  int left;
  int right;
  int top;
  int bottom;
} Rect;

typedef struct
{
  Rect win;
  int width;
  int height;
} CropRect;

typedef struct
{
  CropRect croprect;
  int fmt;
  VSSourceType type;
} SourceFmt;

typedef struct
{
  Rect rect;
  int rot;
} DestinationFmt;

typedef struct
{
  int width;
  int height;
  int posx;
  int posy;
  char *image;
  unsigned int fmt;
} SubFrame;

typedef struct _VideoMode
{
  int resx;
  int resy;
  int hz;
  int interleave;
} VideoMode;



typedef struct
{
  char name[NAME_LEN];
  int devid;
  int resx;
  int resy;
  int custom_mode_num;
  VideoMode modes[VM_MAX];
} VideoDeviceDesc;

typedef struct
{
  void *paddr;
} SourceFrame;

int queryVideoDevice (int idx, VideoDeviceDesc * vd_desc);

void *createVideoSurface (int devid, int mode_idx, SourceFmt * src,
    DestinationFmt * des);
void destroyVideoSurface (void *vshandle);

VSFlowReturn configVideoSurface (void *vshandle, VSConfigID confid,
    VSConfig * config);

VSFlowReturn render2VideoSurface (void *vshandle, SourceFrame * frame,
    SourceFmt * srcfmt);

VSFlowReturn updateSubFrame2VideoSurface (void *vshandle, SubFrame * subframe,
    int idx);


#endif

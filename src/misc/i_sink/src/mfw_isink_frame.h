/*
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. All rights reserved. 
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/



/*
 * Module Name:    mfw_isink_frame.h
 *
 * Description:    Header file of isink frame structure.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#ifndef __MFW_ISINK_FRAME_H__
#define __MFW_ISINK_FRAME_H__


#define PLANE_NUM 3
#define ICB_VERSION 1

typedef enum
{
  ICB_RESULT_INIT,
  ICB_RESULT_SUCCESSFUL,
  ICB_RESULT_FAILED,
} ISinkCallbackResult;

typedef struct
{
  ISinkCallbackResult result;
  int version;
  void *data;
} ISinkCallBack;

typedef struct
{
  void *context;
  void *context1;
  int width;
  int height;
  int left;
  int right;
  int top;
  int bottom;
  unsigned int paddr[PLANE_NUM];
  unsigned int vaddr[PLANE_NUM];
} ISinkFrame;


typedef struct
{
  int frame_num;
  unsigned int fmt;
  ISinkFrame *frames[0];
} ISinkFrameAllocInfo;


#endif

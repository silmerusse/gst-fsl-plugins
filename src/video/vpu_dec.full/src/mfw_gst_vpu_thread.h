/*
 * Copyright (C) 2005-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_vpu_thread.h  
 *
 * Description:    Include File for Hardware (VPU) Decoder Plugin 
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
#ifndef __MFW_GST_VPU_THREAD_H__
#define __MFW_GST_VPU_THREAD_H__

#define RELEASE_BUFF_FAILED 1000

#define MAX_NON_I_FRAME_DROPPED 30

#define VPU_PIC_TYPE_AVC_IDR (!(vpu_dec->outputInfo->picType&0x1))
#define VPU_PIC_TYPE_AVC_I_SLICE (1 == (vpu_dec->outputInfo->picType&0x7))
/* 
** Picture Type:
** For H.264,  
**      bit[0]: 0 - IDR; 1 - non-IDR
**      bit[2:1]: picture type if it is non-IDR
** For VC-1 AP, 
**      bit[5:3]: 1st field picture type
**      bit[2:0]: 2nd field picture type
**      bit[2:0] and bit[5:3] should be identical for frame picture
** For others, 
**      bit[2:0]: picture type
*/
#if 0
#define VPU_PIC_TYPE ((vpu_dec->codec == STD_AVC) ? \
                      ((vpu_dec->outputInfo->picType>>1)&0x3) : \
                      (((vpu_dec->codec == STD_VC1) && (vpu_dec->codec_subtype == 1)) ? \
                       ((vpu_dec->outputInfo->picType>>3)&0x7) : \
                       ((vpu_dec->outputInfo->picType)&0x7)))
#define VPU_PIC_TYPE_IDR ( (vpu_dec->codec == STD_AVC) ? (!(vpu_dec->outputInfo->picType&0x1)) : (VPU_PIC_TYPE == 0))
#else
#define VPU_PIC_TYPE ((vpu_dec->codec == STD_AVC) ? \
                      ((picType>>1)&0x3) : \
                      (((vpu_dec->codec == STD_VC1) && (vpu_dec->codec_subtype == 1)) ? \
                       ((picType>>3)&0x7) : \
                       ((picType)&0x7)))
#define VPU_PIC_TYPE_IDR ( (vpu_dec->codec == STD_AVC) ? (!(picType&0x1)) : (VPU_PIC_TYPE == 0))
#endif
#define VPU_PIC_TYPE_I ( VPU_PIC_TYPE_IDR || (VPU_PIC_TYPE == 0))
#define VPU_PIC_TYPE_B (VPU_PIC_TYPE == 2)

#define INTERLACED_FRAME (vpu_dec->outputInfo->interlacedFrame)


#define VPU_THREAD

typedef enum
{
  NO_EVENT,
  VPU_DEC_START_FRAME_GET_OUTPUT_EVENT,
  VPU_DEC_WAIT_EVENT,
  VPU_DEC_CLEAR_FRAME_EVENT,
  VPU_DEC_GET_OUTPUT_EVENT,
  VPU_DEC_EXIT_EVENT
} VPU_EVENTS;

typedef enum
{
  VPU_DEC_START_FRAME_COMPLETE,
  VPU_DEC_GET_OUTPUT_COMPLETE,
  VPU_DEC_WAIT_COMPLETE,
  VPU_DEC_CLEAR_FRAME_COMPLETE,
  VPU_DEC_EXIT_COMPLETE
} VPU_EVENT_SIGNALS;



typedef enum
{
  VPU_DEC_SUCCESS,
  VPU_DEC_CONTINUE,
  VPU_DEC_DONE
} VPU_RET_VALUE;


GstFlowReturn mfw_gst_vpudec_release_buff (MfwGstVPU_Dec * vpu_dec);
gint vpu_thread_begin_decode (MfwGstVPU_Dec * vpu_dec);
gint vpu_dec_thread_decode_completion_wait (MfwGstVPU_Dec * vpu_dec);
gint vpu_thread_check_output (MfwGstVPU_Dec * vpu_dec);
GstFlowReturn mfw_gst_vpu_dec_thread_get_output (MfwGstVPU_Dec * vpu_dec,
    gboolean fContinue);
gpointer mfw_gst_vpu_dec_event_thread (MfwGstVPU_Dec * vpu_dec);
gboolean vpu_thread_mutex_lock (MfwGstVPU_Thread * vpu_thread);
gboolean vpu_thread_mutex_unlock (MfwGstVPU_Thread * vpu_thread);

gint mfw_gst_vpu_dec_thread_init (MfwGstVPU_Dec * vpu_dec);
gint mfw_gst_vpu_dec_thread_free (MfwGstVPU_Dec * vpu_dec);

#endif /* __MFW_GST_VPU_THREAD_H__ */

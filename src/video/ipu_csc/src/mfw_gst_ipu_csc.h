/*
 * Copyright (c) 2009-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_ipu_csc.h
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Jan 07 2010 Guo Yue <B13906@freescale.com>
 * - Initial version
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#ifndef __MFW_GST_IPU_CSC_H__
#define __MFW_GST_IPU_CSC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstbasetransform.h>
#ifdef IPULIB
#include "mxc_ipu_hl_lib.h"
#else
#include <linux/ipu.h>
#endif


/*=============================================================================
                                           CONSTANTS
=============================================================================*/

/*=============================================================================
                                             ENUMS
=============================================================================*/
/* plugin property ID */
enum{
    CSTYPE_YUV = 1,
    CSTYPE_RGB = 2,
    CSTYPE_GRAY = 3
};

enum{
    PROPER_ID_OUTPUT_WIDTH = 1,
    PROPER_ID_OUTPUT_HEIGHT = 2,
    PROPER_ID_OUTPUT_FORMAT = 3,
    PROPER_ID_OUTPUT_CSTYPE = 4
};

/*=============================================================================
                                            MACROS
=============================================================================*/
#ifndef IPULIB
typedef struct {
    struct ipu_task task;
    int mode; /* 0 no overlay; 1: with overlay */
}IPUTaskOne;
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
#define OUTPUT_PADDR(itask) ((itask)->task.output.paddr)


      
#else

typedef struct {
    ipu_lib_input_param_t input;
    ipu_lib_output_param_t output;
    ipu_lib_overlay_param_t overlay;
    ipu_lib_handle_t handle;
    int mode; /* 0 no overlay; 1: with overlay */
}IPUTaskOne;


#ifdef IPULIB_SPLITMODE_OVERFLOW_FIX

#define KICK_IPUTASKONE(ihandle, itask)\
  do{\
    int ret;\
    do{\
      (itask)->input.motion_sel = 0;\
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









G_BEGIN_DECLS

#define MFW_GST_TYPE_IPU_CSC \
    (mfw_gst_ipu_csc_get_type())
#define MFW_GST_IPU_CSC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_IPU_CSC, MfwGstIPUCSC))
#define MFW_GST_IPU_CSC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_IPU_CSC, MfwGstIPUCSCClass))
#define MFW_GST_IS_IPU_CSC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_IPU_CSC))
#define MFW_GST_IS_IPU_CSC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_IPU_CSC))

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

typedef struct _MfwGstIPUCSC
{
    GstBaseTransform element;

    guint input_framesize;
    gint input_width;
    gint input_height;
    guint input_format;
    gint input_cstype;
    
    guint output_framesize;
    gint output_width;
    gint output_height;
    guint output_format;
    gint output_cstype;

    gint insize;
    gint outsize;

    void * hbuf_in;  /* template dmable buffer for input */
    guint hbuf_in_paddr;
    guint hbuf_in_vaddr;
    gint hbuf_in_size;
    void * hbuf_out; /* template dmable buffer for output */
    guint hbuf_out_paddr;
    guint hbuf_out_vaddr;
    gint hbuf_out_size;
    gboolean interlaced;

    int ipufd;
    gboolean bpassthrough;
    IPUTaskOne iputask;

    gchar device_name[10];
}MfwGstIPUCSC;

typedef struct _MfwGstIPUCSCClass 
{
    GstBaseTransformClass parent_class;
}MfwGstIPUCSCClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/
/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/
GType mfw_gst_ipu_csc_get_type (void);

G_END_DECLS

#endif /* __MFW_GST_IPU_CSC_H__ */

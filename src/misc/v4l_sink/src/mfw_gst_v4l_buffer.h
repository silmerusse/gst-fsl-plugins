/*
 *  Copyright (c) 2009-2012, Freescale Semiconductor, Inc.
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
 * Module Name:    mfw_gst_v4l_buffer.h
 *
 * Description:    Gstreamer buffer management for V4L.
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

#ifndef _MFW_GST_V4L_BUFFER_H_
#define _MFW_GST_V4L_BUFFER_H_

#include "mfw_gst_v4lsink.h"

#define BUFFER_NEW_RETRY_MAX        500
#define WAIT_ON_DQUEUE_FAIL_IN_MS   300





#define DQUEUE_MAX_LOOP		200
#define NEXTDQ_WAIT_MSEC	30

#define IS_RESERVED_HWBUFFER_FULL(v4linfo) \
    (g_slist_length((v4linfo)->reservedhwbuffer_list)>=RESERVEDHWBUFFER_DEPTH)

#define PUSH_RESERVED_HWBUFFER(v4linfo, buffer) \
    if (buffer){\
        ((v4linfo)->reservedhwbuffer_list)=g_slist_append(((v4linfo)->reservedhwbuffer_list),\
                                                   (buffer));\
    }

#define POP_RESERVED_HWBUFFER(v4linfo, buffer) \
    if (((v4linfo)->reservedhwbuffer_list)==NULL){\
        (buffer) = NULL;\
    }else{\
        GSList * tmp;\
        tmp = ((v4linfo)->reservedhwbuffer_list);\
        buffer = tmp->data;\
        ((v4linfo)->reservedhwbuffer_list) = \
            g_slist_delete_link(((v4linfo)->reservedhwbuffer_list), tmp);\
    }



G_BEGIN_DECLS

#define MFW_GST_TYPE_V4LSINK_BUFFER (mfw_gst_v4lsink_buffer_get_type())
#define MFW_GST_IS_V4LSINK_BUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MFW_GST_TYPE_V4LSINK_BUFFER))
#define MFW_GST_V4LSINK_BUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MFW_GST_TYPE_V4LSINK_BUFFER, MFWGstV4LSinkBuffer))
#define MFW_GST_V4LSINK_BUFFER_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MFW_GST_TYPE_V4LSINK_BUFFER, MFWGstV4LSinkBufferClass))

typedef struct _MFWGstV4LSinkBuffer MFWGstV4LSinkBuffer;
typedef struct _MFWGstV4LSinkBufferClass MFWGstV4LSinkBufferClass;

/*use buf state to control drop frame and avoid memory leak*/
/*ENGR33442:No picture out when doing state switch between FS and RS continuously */

typedef enum {
    BUF_STATE_ILLEGAL,  
    BUF_STATE_ALLOCATED,/* buffer occured by codec or pp */
    BUF_STATE_SHOWED,   /* buffer is successlly showed */
    BUF_STATE_SHOWING,  /* buffer is showing(in v4l queue) */
    BUF_STATE_IDLE,     /* buffer is idle, can be allocated to codec or pp */
    BUF_STATE_FREE,     /* buffer need to be freed, the acctually free precedure will happen when future unref */
    BUF_STATE_IGNORED,
} BUF_STATE;

struct _MFWGstV4LSinkBuffer {
    GstBuffer buffer;
    MFW_GST_V4LSINK_INFO_T *v4lsinkcontext;
    struct v4l2_buffer v4l_buf;

    /*use buf state to control drop frame and avoid memory leak*/
 	/*ENGR33442:No picture out when doing state switch between FS and RS continuously */
    BUF_STATE bufstate;
    gint showcnt; /* count for show times, increase when queue to v4l, decrese when dqueue */

};

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_new_hwbuffer

DESCRIPTION:        This function allocated a new hardware V4L  buffer.

ARGUMENTS PASSED:
        v4l_info    -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       returns the pointer to the hardware V4L buffer

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
MFWGstV4LSinkBuffer *mfw_gst_v4l2_new_hwbuffer(MFW_GST_V4LSINK_INFO_T* v4l_info);

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_new_buffer

DESCRIPTION:        This function gets a  v4l buffer, hardware or software.

ARGUMENTS PASSED:
        v4l_info -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       returns the pointer to the v4l buffer

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
MFWGstV4LSinkBuffer *mfw_gst_v4l2_new_buffer(MFW_GST_V4LSINK_INFO_T* v4l_info);

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_clear_showingbuf

DESCRIPTION:        This function Clear the V4L framebuffer in queue.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4l2_clear_showingbuf(MFW_GST_V4LSINK_INFO_T* v4l_info);

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_free_buffers

DESCRIPTION:        This function free all the buffer allocated.

ARGUMENTS PASSED:
        v4l_info    -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       returns the pointer to the hardware V4L buffer

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void mfw_gst_v4l2_free_buffers(MFW_GST_V4LSINK_INFO_T *v4l_info);


G_END_DECLS

#endif				/* _MFW_GST_V4L_BUFFER_H_ */

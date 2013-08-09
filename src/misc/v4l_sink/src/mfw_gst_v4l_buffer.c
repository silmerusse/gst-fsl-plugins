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
 * Module Name:    mfw_gst_v4l_buffer.c
 *
 * Description:    Implementation of V4L Buffer management for Gstreamer
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

#include "mfw_gst_fb.h"
#include "mfw_gst_v4l_buffer.h"
#include "mfw_gst_v4lsink.h"
#include "gstbufmeta.h"

GST_DEBUG_CATEGORY_EXTERN (mfw_gst_v4lsink_debug);
#define GST_CAT_DEFAULT mfw_gst_v4lsink_debug


/*=============================================================================
                             BUFFER MANAGEMENT
=============================================================================*/
static void
mfw_gst_v4lsink_buffer_finalize (MFWGstV4LSinkBuffer * v4lsink_buffer_released)
{
  MFW_GST_V4LSINK_INFO_T *v4l_info;

  /*use buf state to control drop frame */
  g_return_if_fail (v4lsink_buffer_released != NULL);
  v4l_info = v4lsink_buffer_released->v4lsinkcontext;

  switch (v4lsink_buffer_released->bufstate) {
    case BUF_STATE_ALLOCATED:
      GST_INFO ("Buffer %d maybe dropped.",
          v4lsink_buffer_released->v4l_buf.index);
      v4l_info->frame_dropped++;
      if (!(v4l_info->frame_dropped & 0x3f)) {
        GST_ERROR ("%d dropped while %d showed!", v4l_info->frame_dropped,
            v4l_info->qbuff_count);
      }
    case BUF_STATE_SHOWED:
      g_mutex_lock (v4l_info->pool_lock);

      v4lsink_buffer_released->bufstate = BUF_STATE_IDLE;
      if (GST_BUFFER_FLAG_IS_SET (v4lsink_buffer_released,
              GST_BUFFER_FLAG_LAST)) {
        /* hwbuffer, put on the head of free pool. */
        if (!IS_RESERVED_HWBUFFER_FULL (v4l_info)) {
          PUSH_RESERVED_HWBUFFER (v4l_info, v4lsink_buffer_released);
          GST_LOG ("Push to reserved buffer pool:%d",
              g_slist_length ((v4l_info)->reservedhwbuffer_list));
        } else {
          v4l_info->free_pool =
              g_slist_append (v4l_info->free_pool, v4lsink_buffer_released);
          GST_LOG ("Push to free buffer pool:%d",
              g_slist_length ((v4l_info)->free_pool));

        }
        gst_buffer_ref (GST_BUFFER_CAST (v4lsink_buffer_released));

      } else {
        /* swbuffer, free the object. */
        GST_DEBUG (RED_STR ("%s: free software buffer", __FUNCTION__));
        g_free (GST_BUFFER_DATA (v4lsink_buffer_released));
        GST_BUFFER_DATA (v4lsink_buffer_released) = NULL;
      }
      g_mutex_unlock (v4l_info->pool_lock);
      break;

    case BUF_STATE_FREE:
      /*free it,do not need ref. */
      if (GST_BUFFER_DATA (v4lsink_buffer_released)) {
        if (GST_BUFFER_FLAG_IS_SET (v4lsink_buffer_released,
                GST_BUFFER_FLAG_LAST)) {
          munmap (GST_BUFFER_DATA (v4lsink_buffer_released),
              v4lsink_buffer_released->v4l_buf.length);
          v4l_info->all_buffer_pool[v4lsink_buffer_released->v4l_buf.index] =
              NULL;
          v4l_info->querybuf_index--;
          GST_LOG ("Free buffer %d",
              v4lsink_buffer_released->v4l_buf.index);

        } else {
          GST_DEBUG ("Software Buffer %p is freed.",
              GST_BUFFER_DATA (v4lsink_buffer_released));
          g_free (GST_BUFFER_DATA (v4lsink_buffer_released));

        }
      }
      {
        gint index =
            G_N_ELEMENTS (v4lsink_buffer_released->buffer._gst_reserved) - 1;
        if (GST_IS_BUFFER_META (v4lsink_buffer_released->
                buffer._gst_reserved[index])) {
          GST_INFO ("Free buffer meta context");
          gst_buffer_meta_free (v4lsink_buffer_released->
              buffer._gst_reserved[index]);
        }
      }

      GST_BUFFER_DATA (v4lsink_buffer_released) = NULL;


      if (v4l_info->querybuf_index == 0) {
        /* close the v4l driver */
        g_free (v4l_info->all_buffer_pool);
        v4l_info->all_buffer_pool = NULL;
        GST_INFO ("--> All v4l2 buffer freed.");
#if 0
        system ("cat /dev/zero > /dev/fb2\n");
        close (v4l_info->v4l_id);
#else

#endif
      }

      if (v4l_info) {
        gst_object_unref (GST_OBJECT (v4l_info));
        v4lsink_buffer_released->v4lsinkcontext = NULL;
      }
      break;

    case BUF_STATE_ILLEGAL:
      /* DO NOTHING */
      break;

    default:
      gst_buffer_ref (GST_BUFFER_CAST (v4lsink_buffer_released));
      GST_ERROR ("Buffer %d:%p is unref with error state %d!",
          v4lsink_buffer_released->v4l_buf.index, v4lsink_buffer_released,
          v4lsink_buffer_released->bufstate);
  }

  return;

}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_buffer_init

DESCRIPTION:        This funtion initialises the buffer class of the V4lsink
                    plug-in

ARGUMENTS PASSED:
        v4lsink_buffer -   pointer to V4Lsink buffer class
        g_class        -   global pointer


RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/


static void
mfw_gst_v4lsink_buffer_init (MFWGstV4LSinkBuffer * v4lsink_buffer,
    gpointer g_class)
{

  memset (&v4lsink_buffer->v4l_buf, 0, sizeof (struct v4l2_buffer));
  v4lsink_buffer->showcnt = 0;
  return;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_buffer_class_init

DESCRIPTION:        This funtion registers the  funtions used by the
                    buffer class of the V4lsink plug-in

ARGUMENTS PASSED:
        g_class        -   class from which the mini objext is derived
        class_data     -   global class data

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static void
mfw_gst_v4lsink_buffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);
  mini_object_class->finalize =
      (GstMiniObjectFinalizeFunction) mfw_gst_v4lsink_buffer_finalize;
  return;

}

/*=============================================================================
FUNCTION:           mfw_gst_v4lsink_buffer_get_type

DESCRIPTION:        This funtion registers the  buffer class
                    on to the V4L sink plugin

ARGUMENTS PASSED:   None

RETURN VALUE:       return the registered buffer class

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

GType
mfw_gst_v4lsink_buffer_get_type (void)
{

  static GType _mfw_gst_v4lsink_buffer_type;

  if (G_UNLIKELY (_mfw_gst_v4lsink_buffer_type == 0)) {
    static const GTypeInfo v4lsink_buffer_info = {
      sizeof (GstBufferClass),
      NULL,
      NULL,
      mfw_gst_v4lsink_buffer_class_init,
      NULL,
      NULL,
      sizeof (MFWGstV4LSinkBuffer),
      0,
      (GInstanceInitFunc) mfw_gst_v4lsink_buffer_init,
      NULL
    };
    _mfw_gst_v4lsink_buffer_type =
        g_type_register_static (GST_TYPE_BUFFER, "MFWGstV4LSinkBuffer",
        &v4lsink_buffer_info, 0);
  }

  return _mfw_gst_v4lsink_buffer_type;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4l2_new_swbuffer

DESCRIPTION:        This function allocate a new software display buffer.

ARGUMENTS PASSED:
        v4l_info    -   pointer to MFW_GST_V4LSINK_INFO_T

RETURN VALUE:       returns the pointer to the software display buffer

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
MFWGstV4LSinkBuffer *
mfw_gst_v4l2_new_swbuffer (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  MFWGstV4LSinkBuffer *v4lsink_buffer;
  struct v4l2_buffer *v4lbuf;
  void *pdata;
  gint buf_size;

  v4lsink_buffer =
      (MFWGstV4LSinkBuffer *) gst_mini_object_new (MFW_GST_TYPE_V4LSINK_BUFFER);

  v4lsink_buffer->bufstate = BUF_STATE_ILLEGAL;

  /* try to allocate data buffer for swbuffer */
  switch (v4l_info->outformat) {
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
      buf_size =
        (v4l_info->width + v4l_info->cr_left_bypixel +
         v4l_info->cr_right_bypixel) * (v4l_info->height +
           v4l_info->cr_left_bypixel + v4l_info->cr_right_bypixel) * 2;
      break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_NV12:
      buf_size =
        (v4l_info->width + v4l_info->cr_left_bypixel +
         v4l_info->cr_right_bypixel) * (v4l_info->height +
           v4l_info->cr_left_bypixel + v4l_info->cr_right_bypixel) * 3 / 2;
      break;
    case IPU_PIX_FMT_YUV444P:
      buf_size =
        (v4l_info->width + v4l_info->cr_left_bypixel +
         v4l_info->cr_right_bypixel) * (v4l_info->height +
           v4l_info->cr_left_bypixel + v4l_info->cr_right_bypixel) * 3;
      break;
    case V4L2_PIX_FMT_RGB24:
      buf_size =
        (v4l_info->width + v4l_info->cr_left_bypixel +
         v4l_info->cr_right_bypixel) * (v4l_info->height +
           v4l_info->cr_left_bypixel + v4l_info->cr_right_bypixel) * 3;
      break;
    case V4L2_PIX_FMT_RGB32:
      buf_size =
          (v4l_info->width + v4l_info->cr_left_bypixel +
          v4l_info->cr_right_bypixel) * (v4l_info->height +
          v4l_info->cr_left_bypixel + v4l_info->cr_right_bypixel) * 4;
      break;
    default:
      GST_ERROR ("Unsupport format:%x", v4l_info->outformat);
      goto bail;
      break;

  };

  pdata = g_malloc (buf_size);

  if (pdata == NULL) {
    GST_ERROR ("Can not allocate data buffer for swbuffer!");
    goto bail;
  }

  GST_BUFFER_DATA (v4lsink_buffer) = pdata;
  GST_BUFFER_OFFSET (v4lsink_buffer) = 0;

  v4lsink_buffer->v4lsinkcontext = gst_object_ref (GST_OBJECT (v4l_info));

  v4lsink_buffer->bufstate = BUF_STATE_IDLE;

  return v4lsink_buffer;

bail:
  gst_buffer_unref (GST_BUFFER_CAST (v4lsink_buffer));
  return NULL;
}


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

MFWGstV4LSinkBuffer *
mfw_gst_v4l2_new_buffer (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  gint type = 0;
  MFWGstV4LSinkBuffer *v4lsink_buffer = NULL;
  struct v4l2_buffer *v4lbuf = NULL;
  {
    int loopcount = 0;
    GSList *tmp;
    int ret;
    struct v4l2_buffer v4l2buf;

    while (v4l_info->v4lqueued > MIN_QUEUE_NUM) {
      mfw_gst_v4l2_dq_buffer (v4l_info);
    }

    g_mutex_lock (v4l_info->pool_lock);

    if ((IS_RESERVED_HWBUFFER_FULL (v4l_info)) && (tmp = v4l_info->free_pool)) {
      v4lsink_buffer = (MFWGstV4LSinkBuffer *) tmp->data;
      v4lsink_buffer->bufstate = BUF_STATE_ALLOCATED;
      v4l_info->free_pool = g_slist_delete_link (v4l_info->free_pool, tmp);
      g_mutex_unlock (v4l_info->pool_lock);
      GST_LOG ("Assign a buffer from queue, available :%d.",
          g_slist_length (v4l_info->free_pool));
      return v4lsink_buffer;
    }

    while ((loopcount++) < BUFFER_NEW_RETRY_MAX) {
      ret = 0;

      if (v4l_info->v4lqueued > 1) {
        g_mutex_unlock (v4l_info->pool_lock);
        ret = mfw_gst_v4l2_dq_buffer (v4l_info);
        g_mutex_lock (v4l_info->pool_lock);

      }

      if (tmp = v4l_info->free_pool) {
        v4lsink_buffer = (MFWGstV4LSinkBuffer *) tmp->data;
        v4lsink_buffer->bufstate = BUF_STATE_ALLOCATED;
        v4l_info->free_pool = g_slist_delete_link (v4l_info->free_pool, tmp);
        g_mutex_unlock (v4l_info->pool_lock);
        GST_DEBUG
            ("After DQ, assign a hw buffer from queue,queued:%d, left:%d.",
            v4l_info->v4lqueued, g_slist_length (v4l_info->free_pool));
        return v4lsink_buffer;
      }
      if (ret < 0) {
        g_mutex_unlock (v4l_info->pool_lock);
        usleep (WAIT_ON_DQUEUE_FAIL_IN_MS);
        g_mutex_lock (v4l_info->pool_lock);
      }
    }


    GST_WARNING ("Try new buffer failed, ret %d %s queued %d",
        errno, strerror (errno), v4l_info->v4lqueued);

    v4lsink_buffer = mfw_gst_v4l2_new_swbuffer (v4l_info);
    v4lsink_buffer->bufstate = BUF_STATE_ALLOCATED;
    GST_DEBUG ("Finally assign a sw buffer from queue, left:%d.",
        g_slist_length (v4l_info->free_pool));

    g_mutex_unlock (v4l_info->pool_lock);
    return v4lsink_buffer;
  }
}



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
MFWGstV4LSinkBuffer *
mfw_gst_v4l2_new_hwbuffer (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  MFWGstV4LSinkBuffer *v4lsink_buffer = NULL;
  guint image_width = 0;
  gint extra_pixel = 0;
  struct v4l2_buffer *v4lbuf;
  gint cr_left = 0, cr_right = 0, cr_top = 0;


  v4lsink_buffer =
      (MFWGstV4LSinkBuffer *) gst_mini_object_new (MFW_GST_TYPE_V4LSINK_BUFFER);

  v4lsink_buffer->bufstate = BUF_STATE_ILLEGAL;

  memset (&v4lsink_buffer->v4l_buf, 0, sizeof (struct v4l2_buffer));

  v4lbuf = &v4lsink_buffer->v4l_buf;
  v4lbuf->index = v4l_info->querybuf_index;
  v4lbuf->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  v4lbuf->memory = V4L2_MEMORY_MMAP;

  /* Buffer queried from the /V4L driver */

  if (ioctl (v4l_info->v4l_id, VIDIOC_QUERYBUF, v4lbuf) < 0) {
    GST_ERROR ("VIDIOC_QUERYBUF failed %d, device id:%d,  err:%s",
        v4l_info->querybuf_index, v4l_info->v4l_id, strerror (errno));
    gst_buffer_unref (v4lsink_buffer);
    v4lsink_buffer = NULL;
    goto queryret;
  }

  GST_BUFFER_DATA (v4lsink_buffer) =
      mmap (NULL, v4lbuf->length,
      PROT_READ | PROT_WRITE, MAP_SHARED, v4l_info->v4l_id, v4lbuf->m.offset);

  /* Walkaround for V4L, it need QUERYBUF twice to get the hardware address */
  if (v4l_info->chipcode == CC_MX6Q)
    if (ioctl (v4l_info->v4l_id, VIDIOC_QUERYBUF, v4lbuf) < 0) {
      GST_ERROR ("VIDIOC_QUERYBUF failed %d, device id:%d,  err:%s",
          v4l_info->querybuf_index, v4l_info->v4l_id, strerror (errno));
      gst_buffer_unref (v4lsink_buffer);
      v4lsink_buffer = NULL;
      goto queryret;
    }
  if (v4l_info->outformat_flags & FMT_FLAG_GRAY8){
    memset(GST_BUFFER_DATA (v4lsink_buffer), 128, v4lbuf->length);
  }

  /* Buffer queried for is mapped from the /V4L driver space */
  GST_BUFFER_OFFSET (v4lsink_buffer) = (size_t) v4lbuf->m.offset;
  // Added by Guo Yue
  v4lsink_buffer->buffer._gst_reserved[0] = (gpointer) (v4lbuf->m.offset);
  /* Set the physical address */
  {
    gint index;
    GstBufferMeta *bufmeta;
    index = G_N_ELEMENTS (v4lsink_buffer->buffer._gst_reserved) - 1;
    bufmeta = gst_buffer_meta_new ();
    bufmeta->physical_data = (gpointer) (v4lbuf->m.offset);
    v4lsink_buffer->buffer._gst_reserved[index] = bufmeta;
  }

  if (GST_BUFFER_DATA (v4lsink_buffer) == NULL) {
    GST_ERROR ("v4l2_out test: mmap failed");
    gst_buffer_unref (v4lsink_buffer);
    v4lsink_buffer = NULL;
    goto queryret;
  }

  cr_left = v4l_info->cr_left_bypixel;
  cr_right = v4l_info->cr_right_bypixel;
  cr_top = v4l_info->cr_top_bypixel;

  /* The input cropping is set here */
  if ((cr_left != 0) || (cr_right != 0) || (cr_top != 0)) {
    image_width = v4l_info->width;
    v4lbuf->m.offset = v4lbuf->m.offset
        + (cr_top * (image_width + cr_left + cr_right))
        + cr_left;
  }
  v4lsink_buffer->bufstate = BUF_STATE_IDLE;
  GST_BUFFER_FLAG_SET (v4lsink_buffer, GST_BUFFER_FLAG_LAST);
  v4lsink_buffer->v4lsinkcontext = gst_object_ref (GST_OBJECT (v4l_info));

  v4l_info->all_buffer_pool[v4l_info->querybuf_index++] = v4lsink_buffer;

queryret:
  return v4lsink_buffer;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_clear_showingbuf

DESCRIPTION:        This function Clear the V4L framebuffer in queue.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_clear_showingbuf (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  g_mutex_lock (v4l_info->pool_lock);
  {
    MFWGstV4LSinkBuffer *v4lsinkbuffer;
    int i;
    for (i = 0; i < v4l_info->buffers_required; i++) {
      v4lsinkbuffer = v4l_info->all_buffer_pool[i];
      if (v4lsinkbuffer) {
        if (v4lsinkbuffer->bufstate == BUF_STATE_SHOWING) {
          GST_WARNING ("Buffer %p state changed from SHOWING to SHOWED",
              v4lsinkbuffer);

          v4lsinkbuffer->bufstate = BUF_STATE_SHOWED;
          g_mutex_unlock (v4l_info->pool_lock);
          /* Release the mutex to avoid conflict with finalization function */
          while (v4lsinkbuffer->showcnt > 0) {
            gst_buffer_unref (GST_BUFFER_CAST (v4lsinkbuffer));
            v4lsinkbuffer->showcnt--;
          };

          g_mutex_lock (v4l_info->pool_lock);
          v4l_info->v4lqueued--;
        }
      }
    }
  }
  g_mutex_unlock (v4l_info->pool_lock);
  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_dq_buffer

DESCRIPTION:        This function try to dqueue buffer from v4l device.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_dq_buffer (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  struct v4l2_buffer v4l2buf;
  gboolean ret = FALSE;
  memset (&v4l2buf, 0, sizeof (struct v4l2_buffer));
  v4l2buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  v4l2buf.memory = V4L2_MEMORY_MMAP;
  g_mutex_lock (v4l_info->pool_lock);

  if (!((ioctl (v4l_info->v4l_id, VIDIOC_DQBUF, &v4l2buf)) < 0)) {

    MFWGstV4LSinkBuffer *v4lsinkbuffer;
    v4l_info->v4lqueued--;
    v4l_info->v4llist =
        g_list_remove (v4l_info->v4llist, (gpointer) (v4l2buf.index));
    if (g_list_find (v4l_info->v4llist, (gpointer) (v4l2buf.index)))
      g_print ("something wrong here, line:%d\n", __LINE__);
    v4lsinkbuffer =
        (MFWGstV4LSinkBuffer *) (v4l_info->all_buffer_pool[v4l2buf.index]);
    if ((v4lsinkbuffer) && (v4lsinkbuffer->bufstate == BUF_STATE_SHOWING)) {
      v4lsinkbuffer->showcnt--;
      if (v4lsinkbuffer->showcnt == 0)
        v4lsinkbuffer->bufstate = BUF_STATE_SHOWED;
      /* Release the mutex to avoid conflict with finalization function */

      g_mutex_unlock (v4l_info->pool_lock);
      gst_buffer_unref (GST_BUFFER_CAST (v4lsinkbuffer));
      g_mutex_lock (v4l_info->pool_lock);

    } else {
      GST_INFO ("DQ buffer state:%d", v4lsinkbuffer->bufstate);
    }
    ret = TRUE;
  }
  g_mutex_unlock (v4l_info->pool_lock);

  return ret;
}

/*=============================================================================
FUNCTION:           mfw_gst_v4l2_queue_buffer

DESCRIPTION:        This function try to dqueue buffer from v4l device.

ARGUMENTS PASSED:

RETURN VALUE:       

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

gboolean
mfw_gst_v4l2_queue_buffer (MFW_GST_V4LSINK_INFO_T * v4l_info,
    struct v4l2_buffer * v4l2buf)
{
  gboolean ret = FALSE;
  return ret;
}


GstFlowReturn
mfw_gst_v4l2_buffer_init (MFW_GST_V4LSINK_INFO_T * v4l_info)
{

  if (v4l_info->all_buffer_pool) {
    g_free (v4l_info->all_buffer_pool);
    v4l_info->all_buffer_pool = NULL;
  }

  v4l_info->all_buffer_pool = g_malloc (sizeof (GstBuffer *) *
      (v4l_info->buffers_required));

  if (v4l_info->all_buffer_pool == NULL) {
    GST_ERROR ("Failed to allocate buffer pool container");
    return GST_FLOW_ERROR;
  }

  /* no software buffer at all, no reserved needed */

  v4l_info->swbuffer_count = 0;

  memset (v4l_info->all_buffer_pool, 0, (sizeof (GstBuffer *) *
          (v4l_info->buffers_required)));

  {
    MFWGstV4LSinkBuffer *tmpbuffer;

    g_mutex_lock (v4l_info->pool_lock);

    while (!IS_RESERVED_HWBUFFER_FULL (v4l_info)) {
      tmpbuffer = mfw_gst_v4l2_new_hwbuffer (v4l_info);
      if (tmpbuffer) {
        PUSH_RESERVED_HWBUFFER (v4l_info, tmpbuffer);
      } else {
        break;
      }
    }

    while (v4l_info->querybuf_index < v4l_info->buffers_required) {
      tmpbuffer = mfw_gst_v4l2_new_hwbuffer (v4l_info);
      if (tmpbuffer) {
        v4l_info->free_pool = g_slist_prepend (v4l_info->free_pool, tmpbuffer);
      } else {
        break;
      }
    }

    g_mutex_unlock (v4l_info->pool_lock);

  }
  return GST_FLOW_OK;

}


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
void
mfw_gst_v4l2_free_buffers (MFW_GST_V4LSINK_INFO_T * v4l_info)
{
  gint totalbuffernum = (v4l_info->buffers_required);
  gint i;
  MFWGstV4LSinkBuffer *v4lsink_buffer = NULL;

  if (v4l_info->all_buffer_pool) {
    /* try to unref all buffer in pool */
    for (i = 0; i < totalbuffernum; i++) {
      v4lsink_buffer = (MFWGstV4LSinkBuffer *) (v4l_info->all_buffer_pool[i]);

      /* for buffers in IDLE and SHOWING state, no unref outside, explicit unref it */
      if (v4lsink_buffer) {
        GST_WARNING ("try to free buffer %d at state %d",
            v4lsink_buffer->v4l_buf.index, v4lsink_buffer->bufstate);

        if ((v4lsink_buffer->bufstate == BUF_STATE_IDLE) ||
            (v4lsink_buffer->bufstate == BUF_STATE_SHOWING)) {
          if (v4lsink_buffer->bufstate == BUF_STATE_IDLE) {
            v4l_info->free_pool =
                g_slist_remove (v4l_info->free_pool, v4lsink_buffer);
          }
          v4lsink_buffer->bufstate = BUF_STATE_FREE;
          gst_buffer_unref (GST_BUFFER_CAST (v4lsink_buffer));
        } else {
          v4lsink_buffer->bufstate = BUF_STATE_FREE;
        }
      }
    }

    if (v4l_info->v4llist){
      g_list_free (v4l_info->v4llist);
      v4l_info->v4llist = NULL;
    }
  }

}

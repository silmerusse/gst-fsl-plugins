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
 * Module Name:    mfw_gst_vpu_thread.c
 *
 * Description:    Implementation of Hardware (VPU) Decoder Plugin for Gstreamer.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */


/*======================================================================================
                            INCLUDE FILES
=======================================================================================*/
#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <string.h>
#include <fcntl.h>              /* fcntl */
#include <sys/mman.h>           /* mmap */
#include <sys/ioctl.h>          /* fopen/fread */
#include "vpu_io.h"
#include "vpu_lib.h"
#include "mfw_gst_vpu_decoder.h"
#include "mfw_gst_utils.h"
#include "mfw_gst_vpu_thread.h"


/*======================================================================================
                                        LOCAL MACROS
=======================================================================================*/


/*======================================================================================
                                 STATIC FUNCTION PROTOTYPES
=======================================================================================*/

static gint thread_mutex_cnt = 0;


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_release_buff

DESCRIPTION:        Release buffers that are already displayed

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_release_buff (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  gint i = 0;
  int numFreeBufs = 0;
  int numBusyBufs = 0;

  if (vpu_dec->codec == STD_MJPG || (vpu_dec->handle == NULL)) {
    //GST_DEBUG (">>VPU_DEC: Release buf do nothing because of flush state or MJPEG");
    return GST_FLOW_OK;
  }
  // Loop through and release any buffers that are pending to be release or recently flipped

  // In this case we want to make sure that there is at least 1 buffer available before
  // we start decoding - preferably at least 2 but why start decoding when none are
  // that should be rare.   This loop is not infinite as VPU will not decode if none
  // are free but the looping allows the downstream v4lsink to display and release buffers
  // so VPU can now use them for decoding.  It is a throttle to force this plugin to wait
  // for more display buffers to be released and used for decoding.
  for (i = 0; i < vpu_dec->numframebufs; i++) {
    if (vpu_dec->fb_state_plugin[i] == FB_STATE_ALLOCATED) {
      numFreeBufs++;
      //GST_FRAMEDBG (">>VPU_DEC: already free %d free buf %d busy bufs %d\n", i, numFreeBufs, numBusyBufs);
    } else if (vpu_dec->outbuffers[i] &&
        gst_buffer_is_metadata_writable (vpu_dec->outbuffers[i])) {
      if ((vpu_dec->fb_state_plugin[i] == FB_STATE_PENDING) ||
          (vpu_dec->fb_state_plugin[i] == FB_STATE_FREE)) {
        GST_FRAMEDBG (">>VPU_DEC: clearing PENDING %d\n", i);   // use for debugging VPU buffer flow
        vpu_ret = vpu_DecClrDispFlag (*(vpu_dec->handle), i);
        if (vpu_ret != RETCODE_SUCCESS) {
          GST_ERROR
              (">>VPU_DEC: vpu_DecClrDispFlag failed Error is %d pending buff idx=%d handle=0x%x",
              vpu_ret, i, vpu_dec->handle);
          //return (GST_FLOW_ERROR);
        }
        vpu_dec->fb_state_plugin[i] = FB_STATE_ALLOCATED;
        numFreeBufs++;
        //GST_FRAMEDBG (">>VPU_DEC: pending release %d free buf %d busy bufs %d", i, numFreeBufs, numBusyBufs);
      }
      if (vpu_dec->fb_state_plugin[i] == FB_STATE_DISPLAY) {
        // if VC-1 and last frame is display - do not release
        if (!((i == vpu_dec->outputInfo->indexFrameDisplay)
                && (vpu_dec->codec == STD_VC1))) {
          GST_FRAMEDBG (">>VPU_DEC: clearing DISPLAY %d\n", i); // use for debugging VPU buffer flow

          vpu_ret = vpu_DecClrDispFlag (*(vpu_dec->handle), i);
          if (vpu_ret != RETCODE_SUCCESS) {
            GST_ERROR
                (">>VPU_DEC: vpu_DecClrDispFlag failed Error is %d disp buff idx=%d handle=0x%x",
                vpu_ret, i, vpu_dec->handle);
            //return (GST_FLOW_ERROR);
          }
          vpu_dec->fb_state_plugin[i] = FB_STATE_ALLOCATED;
          numFreeBufs++;
          //GST_FRAMEDBG (">>VPU_DEC: release %d free buf %d busy bufs %d\n", i, numFreeBufs, numBusyBufs);
        } else {
          numBusyBufs++;
          //GST_FRAMEDBG (">>VPU_DEC: No release - previous displayed buffer %d free buf %d busy bufs %d\n", i, numFreeBufs, numBusyBufs);
        }
      } else if (vpu_dec->fb_state_plugin[i] == FB_STATE_DECODED) {
        numBusyBufs++;
        //GST_FRAMEDBG (">>VPU_DEC: No release - DECODED %d  free buf %d busy bufs %d\n", i, numFreeBufs, numBusyBufs);
      }
    } else {
      // This is a special case usually only needed after flushing/seeking where the downstream is not unrefing the buffers
      // after a seek so we will do it now - otherwise VPU will be stuck waiting for buffers to decode into
      if ((vpu_dec->fb_state_plugin[i] == FB_STATE_FREE)
          && vpu_dec->direct_render && (vpu_dec->fb_type[i] == FB_TYPE_GST)) {
        gst_buffer_unref (vpu_dec->outbuffers[i]);
        vpu_ret = vpu_DecClrDispFlag (*(vpu_dec->handle), i);
        if (vpu_ret != RETCODE_SUCCESS) {
          GST_ERROR
              (">>VPU_DEC: vpu_DecClrDispFlag failed Error is %d disp buff idx=%d handle=0x%x",
              vpu_ret, i, vpu_dec->handle);
        }

        vpu_dec->fb_state_plugin[i] = FB_STATE_ALLOCATED;
        numFreeBufs++;
        GST_FRAMEDBG (">>VPU_DEC: Releasing and unreffing FLUSHED  %d\n", i);   // use for debugging VPU buffer flow

      } else {
        numBusyBufs++;
        //GST_FRAMEDBG (">>VPU_DEC: No release - busy buffer %d  free buf %d busy bufs %d\n", i, numFreeBufs, numBusyBufs);
      }
    }
  }

  //GST_FRAMEDBG (">>VPU_DEC: can decode now - %d buffers are free %d are busy\n", numFreeBufs, numBusyBufs);
  return GST_FLOW_OK;
}

/*=============================================================================
FUNCTION:           vpu_thread_mutex_lock

DESCRIPTION:        This function locks thread mutex

ARGUMENTS PASSED:
        vpu_thread  -  Pointer to MfwGstVPU_Thread


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
vpu_thread_mutex_lock (MfwGstVPU_Thread * vpu_thread)
{
  g_mutex_lock (vpu_thread->mutex);
  thread_mutex_cnt++;
  //GST_DEBUG(">>VPU_THREAD LOCK mutex_cnt = %d req=%d", thread_mutex_cnt, thread_mutex_req);
}



/*=============================================================================
FUNCTION:           vpu_thread_mutex_unlock

DESCRIPTION:        This function unlocks thread mutex

ARGUMENTS PASSED:
        vpu_thread  -  Pointer to MfwGstVPU_Thread


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
vpu_thread_mutex_unlock (MfwGstVPU_Thread * vpu_thread)
{
  g_mutex_unlock (vpu_thread->mutex);
  thread_mutex_cnt--;
  //GST_DEBUG(">>VPU_THREAD UNLOCK mutex_cnt = %d req=%d", thread_mutex_cnt, thread_mutex_req);
}


/*=============================================================================
FUNCTION:           vpu_thread_begin_decode

DESCRIPTION:        This function starts a decode with VPU using startoneframe

ARGUMENTS PASSED:
        vpu_dec  -  Pointer to MfwGstVPU_Dec


RETURN VALUE:       vpu_ret

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gint
vpu_thread_begin_decode (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;

  GST_FRAMEDBG (">>VPU_DEC: Start frame decoding\n");

#ifdef VPU_THREAD
  vpu_thread_mutex_lock (vpu_dec->vpu_thread);

  // push the begin thread with the cmd and wait for it to complete */
  g_async_queue_push (vpu_dec->vpu_thread->async_q,
      (gpointer) VPU_DEC_START_FRAME_GET_OUTPUT_EVENT);

  g_cond_wait (vpu_dec->vpu_thread->
      cond_event[VPU_DEC_START_FRAME_COMPLETE], vpu_dec->vpu_thread->mutex);

  vpu_ret = vpu_dec->vpu_thread->retval;

  vpu_thread_mutex_unlock (vpu_dec->vpu_thread);
#else
  vpu_dec->decoding_completed = FALSE;

  // Release buffers back to VPU before starting next decode
  if (mfw_gst_vpudec_release_buff (vpu_dec) != GST_FLOW_OK) {
    // Error in clearing VPU buffers
    return RELEASE_BUFF_FAILED;
  }

  vpu_dec->outputInfo->indexFrameDisplay = -1;
  vpu_dec->outputInfo->indexFrameDecoded = -1;

  vpu_ret = vpu_DecStartOneFrame (*(vpu_dec->handle), vpu_dec->decParam);
  if (vpu_ret == RETCODE_SUCCESS) {
    vpu_dec->is_frame_started = TRUE;
    vpu_dec->num_timeouts = 0;
  }
#endif
  return vpu_ret;
}


/*=============================================================================
FUNCTION:           vpu_dec_thread_decode_completion_wait

DESCRIPTION:        This function waits on VPU for decode to complete

ARGUMENTS PASSED:
        vpu_dec  -  Pointer to MfwGstVPU_Dec


RETURN VALUE:       retval

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gint
vpu_dec_thread_decode_completion_wait (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  gboolean fBusy = vpu_IsBusy ();


  if (vpu_dec->is_frame_started == FALSE)
    return VPU_DEC_DONE;

  if (G_UNLIKELY (vpu_dec->profiling)) {
    gettimeofday (&vpu_dec->tv_prof, 0);
  }


  while (fBusy) {
    if ((vpu_dec->loop_cnt > 1) && !vpu_dec->must_copy_data &&
        !vpu_dec->file_play_mode && !vpu_dec->loopback && !vpu_dec->eos) {
      return VPU_DEC_DONE;
    }
    if (vpu_dec->in_cleanup || vpu_dec->flushing) {
      return VPU_DEC_DONE;
    }


    if (!vpu_dec->file_play_mode) {
      if (vpu_dec->must_copy_data) {
        vpu_dec->retval =
            mfw_gst_vpudec_copy_sink_input (vpu_dec, vpu_dec->gst_buffer);
        if (vpu_dec->retval != GST_FLOW_OK) {
          GST_ERROR
              (">>VPU_DEC: mfw_gst_vpudec_copy_sink_input failed - Error %d ",
              vpu_dec->retval);
          return VPU_DEC_DONE;
        }
      }
      if (!vpu_dec->must_copy_data && (vpu_dec->loop_cnt > 1) && (!vpu_dec->eos)
          && !vpu_dec->loopback) {
        return VPU_DEC_DONE;
      }
    }
#ifdef VPU_THREAD
    vpu_ret = vpu_WaitForInt (33);
#else

    vpu_mutex_unlock (vpu_dec->vpu_mutex);
    GST_MUTEX (">>VPU_DEC: unlock mutex before vpu wait\n");
    vpu_ret = vpu_WaitForInt (33);

    if (vpu_dec->in_cleanup || !vpu_dec->vpu_init)
      return VPU_DEC_DONE;

    if (vpu_mutex_lock (vpu_dec->vpu_mutex, TRUE) == FALSE) {
      vpu_dec->trymutex = FALSE;
      GST_MUTEX (">>VPU_DEC: after VPU Wait  - no mutex lock\n");
      return VPU_DEC_DONE;      // no mutex so exit - must be flushing or cleaning up
    } else {
      GST_MUTEX (">>VPU_DEC: after VPU Wait - mutex lock\n");
      if (!vpu_dec->is_frame_started || !vpu_dec->vpu_init)     // can happen after flush after mutex is reacquired
        return VPU_DEC_DONE;
    }
#endif
    // avoid an infinite loop and make sure that exit and try later
    fBusy = vpu_IsBusy ();
    if (fBusy) {
      vpu_dec->num_timeouts++;
      GST_FRAMEDBG
          (">>VPU_DEC: VPU is timing out past 1 frame time timeouts=%d loop_cnt% d vpu_ret=%d\n",
          vpu_dec->num_timeouts, vpu_dec->loop_cnt, vpu_ret);
      if (vpu_dec->num_timeouts > 100) {
        vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle), 0);
        mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
            "VPU Decoder Failed - too many time outs");
        vpu_dec->retval = GST_FLOW_ERROR;
        return VPU_DEC_DONE;
      }
      if ((vpu_dec->num_timeouts > 1) && !vpu_dec->loopback) {
        return VPU_DEC_CONTINUE;
      }
    }                           /* if VPU busy */
  }                             /* while VPU busy */

  vpu_dec->num_timeouts = 0;

  if (G_UNLIKELY (vpu_dec->profiling)) {
    long time_before = 0, time_after = 0;
    gettimeofday (&vpu_dec->tv_prof1, 0);
    time_before =
        (vpu_dec->tv_prof.tv_sec * 1000000) + vpu_dec->tv_prof.tv_usec;
    time_after =
        (vpu_dec->tv_prof1.tv_sec * 1000000) + vpu_dec->tv_prof1.tv_usec;
    vpu_dec->decode_wait_time += time_after - time_before;
  }
  // Get the VPU output from decoding
  vpu_ret = vpu_DecGetOutputInfo (*(vpu_dec->handle), vpu_dec->outputInfo);


  if (vpu_ret != RETCODE_SUCCESS) {
    // in some demuxers we get headers separately which will cause VPU to fail
    // so in those cases just reset our error and wait for more data
    if (vpu_ret == RETCODE_FAILURE) {
      vpu_dec->retval = GST_FLOW_OK;
      vpu_dec->is_frame_started = FALSE;

    } else if (vpu_ret == RETCODE_FRAME_NOT_COMPLETE) {
      vpu_dec->retval = GST_FLOW_OK;
    } else {
      vpu_dec->retval = GST_FLOW_ERROR;
      vpu_dec->is_frame_started = FALSE;
      GST_ERROR
          (">>VPU_DEC: vpu_DecGetOutputInfo failed. Error code is %d ",
          vpu_ret);
    }
    if (vpu_dec->must_copy_data)
      return VPU_DEC_CONTINUE;
    else
      return VPU_DEC_DONE;
  }

  vpu_dec->is_frame_started = FALSE;
  return 0;

}


/*=============================================================================
FUNCTION:           vpu_thread_check_output

DESCRIPTION:        This function checks the output of VPU

ARGUMENTS PASSED:
        vpu_dec  -  Pointer to MfwGstVPU_Dec


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gint
vpu_thread_check_output (MfwGstVPU_Dec * vpu_dec)
{
  gint picType = -1;
  //GST_DEBUG(">>VPU_DEC: output pictype=%d",vpu_dec->outputInfo->picType);

  if ((vpu_dec->decParam->prescanEnable == 1) &&
      (vpu_dec->outputInfo->prescanresult == 0)) {
    //vpu_dec->allow_parallelization = FALSE;
    if ((vpu_dec->loop_cnt == 1)
        && (vpu_dec->min_data_in_vpu < (256 * 1024)))
      vpu_dec->min_data_in_vpu += 2048;

    GST_FRAMEDBG
        (">>VPU_DEC: The prescan result is zero, VPU needs more input - data in vpu=%d\n",
        vpu_dec->data_in_vpu);
    // Return for more data as this is incomplete - but do not process as an error
    vpu_dec->retval = GST_FLOW_OK;
    if (vpu_dec->must_copy_data)
      return VPU_DEC_CONTINUE;
    else
      return VPU_DEC_DONE;
  }
  // Use this for debugging VPU issues with buffer indexes
  GST_FRAMEDBG
      (">>VPU_DEC:disp=%d decode=%d pic_type %d output resolution:%dx%d,loopcnt=%d datain vpu %d min %d\n",
      vpu_dec->outputInfo->indexFrameDisplay,
      vpu_dec->outputInfo->indexFrameDecoded, vpu_dec->outputInfo->picType,
      vpu_dec->outputInfo->decPicWidth, vpu_dec->outputInfo->decPicHeight,
      vpu_dec->loop_cnt, vpu_dec->data_in_vpu, vpu_dec->min_data_in_vpu);

  if ((vpu_dec->decParam->skipframeMode == 1) &&
      (vpu_dec->decParam->skipframeNum > 0)) {
    /* 1 = skip frame enabled (skip frames but I (IDR) frame) */
    if ((vpu_dec->outputInfo->indexFrameDecoded >= 0) ||
        (vpu_dec->non_iframes_dropped >= MAX_NON_I_FRAME_DROPPED)) {
      /* we have a valid decoded frame index, or we have too many frames 
       * dropped, disable skip frame mode */
      vpu_dec->decParam->skipframeMode = 0;
    } else if (vpu_dec->outputInfo->indexFrameDecoded == -2) {
      GST_FRAMEDROP ("Skip frame mode: skip one non-I frame : %d\n",
          vpu_dec->decParam->skipframeNum);
      vpu_dec->non_iframes_dropped++;
      mfw_gst_vpudec_no_display (vpu_dec);
      return VPU_DEC_CONTINUE;
    } else if (vpu_dec->outputInfo->indexFrameDecoded == -1) {
      /* eos */
      GST_DEBUG (">>VPU_DEC: VPU hits EOS must reset on next buffer");
      vpu_dec->eos = TRUE;
      return VPU_DEC_CONTINUE;
    }
  }

  if (vpu_dec->outputInfo->indexFrameDecoded >= 0) {
    vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDecoded] =
        FB_STATE_DECODED;
    vpu_dec->fb_pic_type[vpu_dec->outputInfo->indexFrameDecoded] =
        vpu_dec->outputInfo->picType;
  }

  // Check for cases a frame not to display
  if (G_UNLIKELY (vpu_dec->outputInfo->indexFrameDisplay < 0)) {
    /* no display frame in this round */
    // if eos marked by VPU then save the eos state
    if ((vpu_dec->outputInfo->indexFrameDisplay == -1) &&
        (vpu_dec->outputInfo->indexFrameDecoded < 0)) {
      GST_DEBUG (">>VPU_DEC: VPU hits EOS must reset on next buffer");
      vpu_dec->eos = TRUE;
      return VPU_DEC_CONTINUE;
    }

    if (vpu_dec->outputInfo->mp4PackedPBframe == 1) {
      GST_FRAMEDBG (">>VPU_DEC: Packed Frame set with negative display idx\n");
      vpu_dec->allow_parallelization = FALSE;
      return VPU_DEC_CONTINUE;
    }
    // loop again unless our decoded was also negative
    if (vpu_dec->outputInfo->indexFrameDecoded < 0) {
      vpu_dec->retval = GST_FLOW_OK;
    }
    return VPU_DEC_CONTINUE;
  }

  /* (vpu_dec->outputInfo->indexFrameDisplay >= 0) */
  vpu_dec->frames_decoded++;
  picType = vpu_dec->fb_pic_type[vpu_dec->outputInfo->indexFrameDisplay];
  if (vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDisplay] ==
      FB_STATE_DISPLAY) {
    GST_ERROR (">>VPU_DEC: Display frame is same as last so ignore");
    return VPU_DEC_CONTINUE;
  }

  if (vpu_dec->frames_decoded == 1) {
    mfw_vpudec_get_field (vpu_dec);
  }

  if ((vpu_dec->check_for_iframe) || (vpu_dec->just_flushed)) {
    if (!VPU_PIC_TYPE_IDR) {
      if (vpu_dec->non_iframes_dropped < MAX_NON_I_FRAME_DROPPED) {
        GST_FRAMEDROP
            (">>VPU_DEC: No I frame don't display yet pictype= %d \n",
            vpu_dec->outputInfo->picType);
        mfw_gst_vpudec_no_display (vpu_dec);
        vpu_dec->non_iframes_dropped++;
        return VPU_DEC_CONTINUE;
      }
    }

    vpu_dec->check_for_iframe = FALSE;
    vpu_dec->just_flushed = FALSE;
    vpu_dec->non_iframes_dropped = 0;
  }
  // To improve latency it helps to tweak our threshold in antipation of I frames especially with VBR input
  // can guess when an I frame is coming up and enable prescan for I frames and limit the minimim
  // data required in bitstream before starting a decode
  if (VPU_PIC_TYPE_IDR) {
    vpu_dec->num_gops++;
    if ((vpu_dec->frames_rendered > 0)) {
      guint new_gop_size =
          (guint) (vpu_dec->frames_decoded - vpu_dec->idx_last_gop);
      // if we are not having same gop sizes then turn off the predict gop flag
      // we use for frame dropping
      if (new_gop_size != vpu_dec->gop_size) {
        vpu_dec->predict_gop = FALSE;
      } else {
        vpu_dec->predict_gop = TRUE;
      }
    } else {
      //GST_LATENCY (">>VPU_DEC: frames since last gop %d gop_size %d\n",
      //               (guint)(vpu_dec->frames_rendered - vpu_dec->idx_last_gop), vpu_dec->gop_size);
    }
    vpu_dec->gop_size =
        (guint) (vpu_dec->frames_decoded - vpu_dec->idx_last_gop);
    vpu_dec->idx_last_gop = vpu_dec->frames_decoded;
    vpu_dec->gop_is_next = FALSE;
    GST_LATENCY ("\n \n >>VPU_DEC: numgops %d gopsize %d \n",
        vpu_dec->num_gops, vpu_dec->gop_size);
  } else {
    guint frames_since_gop =
        vpu_dec->frames_decoded - vpu_dec->idx_last_gop + 1;
    if (vpu_dec->predict_gop && vpu_dec->gop_size
        && (frames_since_gop >= vpu_dec->gop_size)) {
      vpu_dec->gop_is_next = TRUE;
      //GST_LATENCY (">>VPU_DEC: Delta -  frames since gop %d  INCREASING MINIMUM  to %d \n",
      //             (guint)(vpu_dec->frames_rendered - vpu_dec->idx_last_gop), vpu_dec->min_data_in_vpu);
    } else {
      //GST_LATENCY (">>VPU_DEC: Delta -  frames since gop %d  loop %d\n",
      //       (guint)(vpu_dec->frames_decoded - vpu_dec->idx_last_gop), loop_cnt);
    }
  }

  vpu_dec->decoding_completed = TRUE;
  return 0;
}



/*=============================================================================
FUNCTION:           mfw_gst_vpu_dec_thread_get_output

DESCRIPTION:        This function handles the event thread talking to VPU

ARGUMENTS PASSED:
        vpu_dec  -  Pointer to MfwGstVPU_Dec


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GstFlowReturn
mfw_gst_vpu_dec_thread_get_output (MfwGstVPU_Dec * vpu_dec, gboolean fContinue)
{
  GstFlowReturn retval = GST_FLOW_OK;

#ifdef VPU_THREAD

  if (vpu_dec->vpu_thread == NULL) {
    GST_ERROR ("error: instance has been released");
    return VPU_DEC_DONE;
  }
  if (vpu_dec->vpu_thread->getoutput_pending) {
    vpu_thread_mutex_lock (vpu_dec->vpu_thread);
    g_async_queue_push (vpu_dec->vpu_thread->async_q,
        (gpointer) VPU_DEC_WAIT_EVENT);
    g_cond_wait (vpu_dec->vpu_thread->cond_event[VPU_DEC_WAIT_COMPLETE],
        vpu_dec->vpu_thread->mutex);
    if (fContinue)
      retval = vpu_dec->vpu_thread->retval;
    else
      retval = vpu_dec->retval;
    vpu_thread_mutex_unlock (vpu_dec->vpu_thread);
  }
  // in some case the startframe get output does not get the output
  // in this case it should try again since the queue is clear of a pending get output
  if (vpu_dec->is_frame_started) {
    vpu_thread_mutex_lock (vpu_dec->vpu_thread);
    g_async_queue_push (vpu_dec->vpu_thread->async_q,
        (gpointer) VPU_DEC_GET_OUTPUT_EVENT);
    g_cond_wait (vpu_dec->vpu_thread->
        cond_event[VPU_DEC_GET_OUTPUT_COMPLETE], vpu_dec->vpu_thread->mutex);
    if (fContinue)
      retval = vpu_dec->vpu_thread->retval;
    else
      retval = vpu_dec->retval;
    vpu_thread_mutex_unlock (vpu_dec->vpu_thread);
  }
#else
  if (fContinue) {
    // wait for decode to complete before calling get output
    retval = vpu_dec_thread_decode_completion_wait (vpu_dec);

    if (retval == VPU_DEC_DONE)
      return VPU_DEC_DONE;
    if (retval == VPU_DEC_CONTINUE)
      return VPU_DEC_CONTINUE;

    // call get output and check the results
    retval = vpu_thread_check_output (vpu_dec);
  } else
    retval = vpu_DecGetOutputInfo (*(vpu_dec->handle), vpu_dec->outputInfo);
#endif
  return retval;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpu_dec_event_thread

DESCRIPTION:        This function handles the event thread talking to VPU

ARGUMENTS PASSED:
        vpu_dec  -  Pointer to MfwGstVPU_Dec


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gpointer
mfw_gst_vpu_dec_event_thread (MfwGstVPU_Dec * vpu_dec)
{
  gboolean bExit = FALSE;
  MfwGstVPU_Thread *vpu_thread = vpu_dec->vpu_thread;
  gint cmd = 0;
  GstFlowReturn retval = GST_FLOW_OK;
  RetCode vpu_ret = RETCODE_SUCCESS;

  // this mutex is important - it must be locked now but unlocked before queue pop
  // if the caller does not have lock first then it is possible that queue could get popped and event
  // completed and signalled done before the caller ever calls wait on the signal event -> thus causing deadlock
  //
  // so what happens now is thread has lock until it waits for a event - queue will just hold if empty
  // caller gets lock before posting to the queue which will unlock the queue but then it has to wait for the lock
  // caller calls conditional wait which unlocks so that this thread can continue
  // when this thread is done it posts the event complete that caller is waiting on
  // after thread below loops around it will unlock and caller gets the lock which it can release again

  vpu_thread_mutex_lock (vpu_dec->vpu_thread);

  while (!bExit) {
    if (!vpu_thread || !vpu_thread->mutex)
      return NULL;

    // thread was destroyed while we were waiting for lock
    if (!vpu_dec->vpu_thread || !vpu_thread->async_q)
      return NULL;

    vpu_thread_mutex_unlock (vpu_dec->vpu_thread);

    cmd = (gint) g_async_queue_pop (vpu_thread->async_q);

    // get a lock now
    vpu_thread_mutex_lock (vpu_dec->vpu_thread);

    switch (cmd) {
      case VPU_DEC_START_FRAME_GET_OUTPUT_EVENT:
        //GST_DEBUG(">>VPU_DEC: Event thread VPU_DEC_START_FRAME_GET_OUTPUT_EVENT");

        vpu_dec->decoding_completed = FALSE;
        vpu_thread->retval = VPU_DEC_SUCCESS;

        if (mfw_gst_vpudec_release_buff (vpu_dec) != GST_FLOW_OK) {
          vpu_thread->retval = RELEASE_BUFF_FAILED;
          g_cond_broadcast (vpu_thread->
              cond_event[VPU_DEC_START_FRAME_COMPLETE]);
        } else {                // release okay now do start frame
          vpu_dec->retval =
              vpu_DecStartOneFrame (*(vpu_dec->handle), vpu_dec->decParam);

          // start frame okay so kick off get output event
          if (vpu_dec->retval == RETCODE_SUCCESS) {
            vpu_thread->getoutput_pending = TRUE;
            vpu_dec->is_frame_started = TRUE;
            vpu_dec->num_timeouts = 0;
            g_cond_broadcast (vpu_thread->
                cond_event[VPU_DEC_START_FRAME_COMPLETE]);

            // used in cases where previous startframe get output was not able to do a get output
            // in this case it will do a get output
            vpu_thread->retval =
                vpu_dec_thread_decode_completion_wait (vpu_dec);

            // no get output done - so don't check results
            if ((vpu_thread->retval != VPU_DEC_DONE) &&
                (vpu_thread->retval != VPU_DEC_CONTINUE)) {
              vpu_thread->retval = vpu_thread_check_output (vpu_dec);
            }
            vpu_thread->getoutput_pending = FALSE;
          } else
            g_cond_broadcast (vpu_thread->
                cond_event[VPU_DEC_START_FRAME_COMPLETE]);
        }

        break;

      case VPU_DEC_GET_OUTPUT_EVENT:
        //GST_DEBUG(">>VPU_DEC: Event thread VPU_DEC_GET_OUTPUT_EVENT");
        if (vpu_dec->is_frame_started == FALSE) {
          if (!vpu_thread->getoutput_pending)
            g_cond_broadcast (vpu_thread->
                cond_event[VPU_DEC_GET_OUTPUT_COMPLETE]);
          else
            vpu_thread->getoutput_pending = FALSE;
          continue;;
        }
        // used in cases where previous startframe get output was not able to do a get output
        // in this case it will do a get output
        vpu_thread->retval = vpu_dec_thread_decode_completion_wait (vpu_dec);

        // no get output done - so don't check results
        if ((vpu_thread->retval != VPU_DEC_DONE) &&
            (vpu_thread->retval != VPU_DEC_CONTINUE)) {
          vpu_thread->retval = vpu_thread_check_output (vpu_dec);
        }
        vpu_thread->getoutput_pending = FALSE;
        if (!vpu_thread->getoutput_pending)
          g_cond_broadcast (vpu_thread->
              cond_event[VPU_DEC_GET_OUTPUT_COMPLETE]);
        else
          vpu_thread->getoutput_pending = FALSE;

        break;

      case VPU_DEC_WAIT_EVENT:
        //GST_DEBUG(">>VPU_DEC: Event thread VPU_DEC_WAIT_EVENT getoutputpending=%d",vpu_thread->getoutput_pending);

        // this is a dummy event just ot make sure the queue is clear of a pending get output
        g_cond_broadcast (vpu_thread->cond_event[VPU_DEC_WAIT_COMPLETE]);

        break;

      case VPU_DEC_CLEAR_FRAME_EVENT:
        //GST_DEBUG(">>VPU_DEC: Event thread VPU_DEC_CLEAR_FRAME_EVENT");
        // used to clear frames in the vpu reset case only
        vpu_thread->retval = mfw_gst_vpudec_release_buff (vpu_dec);
        g_cond_broadcast (vpu_thread->cond_event[VPU_DEC_CLEAR_FRAME_COMPLETE]);

        break;

      case VPU_DEC_EXIT_EVENT:
        //GST_DEBUG(">>VPU_DEC: Event thread VPU_DEC_EXIT_EVENT");
        g_cond_broadcast (vpu_thread->cond_event[VPU_DEC_EXIT_COMPLETE]);
        bExit = TRUE;
        break;
      default:
        break;
    }                           /* end switch */
  }                             /* end while */

  vpu_thread_mutex_unlock (vpu_dec->vpu_thread);


  GST_DEBUG ("vpu_thread event thread exit.");

  g_thread_exit (0);
  return NULL;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpu_dec_thread_init

DESCRIPTION:        This function initializes the event thread talking to VPU

ARGUMENTS PASSED:
        vpu_dec  -  Pointer to MfwGstVPU_Dec


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gint
mfw_gst_vpu_dec_thread_init (MfwGstVPU_Dec * vpu_dec)
{
  MfwGstVPU_Thread *vpu_thread = vpu_dec->vpu_thread;
  if (vpu_thread)
    return -1;

  vpu_thread = MM_MALLOC (sizeof (MfwGstVPU_Thread));
  if (vpu_thread == NULL)
    return -1;

  vpu_dec->vpu_thread = vpu_thread;

  memset (vpu_thread, 0, sizeof (MfwGstVPU_Thread));

#ifdef VPU_THREAD
  vpu_thread->mutex = g_mutex_new ();

  vpu_thread->async_q = g_async_queue_new ();

  vpu_thread->event_thread =
      g_thread_create ((GThreadFunc) mfw_gst_vpu_dec_event_thread,
      (gpointer) vpu_dec, FALSE, NULL);

  vpu_thread->cond_event[VPU_DEC_START_FRAME_COMPLETE] = g_cond_new ();
  vpu_thread->cond_event[VPU_DEC_GET_OUTPUT_COMPLETE] = g_cond_new ();
  vpu_thread->cond_event[VPU_DEC_WAIT_COMPLETE] = g_cond_new ();
  vpu_thread->cond_event[VPU_DEC_CLEAR_FRAME_COMPLETE] = g_cond_new ();
  vpu_thread->cond_event[VPU_DEC_EXIT_COMPLETE] = g_cond_new ();

#endif
}


/*=============================================================================
FUNCTION:           mfw_gst_vpu_dec_thread_free

DESCRIPTION:        This function frees the event thread talking to VPU

ARGUMENTS PASSED:
        vpu_dec  -  Pointer to MfwGstVPU_Dec


RETURN VALUE:

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gint
mfw_gst_vpu_dec_thread_free (MfwGstVPU_Dec * vpu_dec)
{
  MfwGstVPU_Thread *vpu_thread = vpu_dec->vpu_thread;
  if (vpu_thread == NULL)
    return -1;

#ifdef VPU_THREAD
  vpu_thread_mutex_lock (vpu_thread);
  g_async_queue_push (vpu_thread->async_q, (gpointer) VPU_DEC_EXIT_EVENT);
  g_cond_wait (vpu_thread->cond_event[VPU_DEC_EXIT_COMPLETE],
      vpu_thread->mutex);
  vpu_thread_mutex_unlock (vpu_dec->vpu_thread);

  g_cond_free (vpu_thread->cond_event[VPU_DEC_START_FRAME_COMPLETE]);
  g_cond_free (vpu_thread->cond_event[VPU_DEC_GET_OUTPUT_COMPLETE]);
  g_cond_free (vpu_thread->cond_event[VPU_DEC_WAIT_COMPLETE]);
  g_cond_free (vpu_thread->cond_event[VPU_DEC_CLEAR_FRAME_COMPLETE]);
  g_cond_free (vpu_thread->cond_event[VPU_DEC_EXIT_COMPLETE]);


  while (g_async_queue_length_unlocked (vpu_thread->async_q)) {
    g_async_queue_pop (vpu_thread->async_q);
  }
  g_async_queue_unref (vpu_thread->async_q);

  if (vpu_thread->mutex) {
    g_mutex_free (vpu_thread->mutex);
    vpu_thread->mutex = 0;
  }
#endif

  MM_FREE (vpu_dec->vpu_thread);
  vpu_dec->vpu_thread = NULL;
}

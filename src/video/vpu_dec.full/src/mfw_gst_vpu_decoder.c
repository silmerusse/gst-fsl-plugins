/*
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_vpu_decoder.c
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
#include "mfw_gst_vpu_thread.h"

#include "../../../misc/i_sink/src/mfw_isink_frame.h"

#if GST_CHECK_VERSION(0, 10, 30)
#define GST_VIDEO_FORMAT_VPU_GRAY_8 (GST_VIDEO_FORMAT_Y800)
#elif GST_CHECK_VERSION(0, 10, 29)
#define GST_VIDEO_FORMAT_VPU_GRAY_8 (GST_VIDEO_FORMAT_GRAY8)
#else
#define GST_VIDEO_FORMAT_VPU_GRAY_8 GST_MAKE_FOURCC('Y','8','0','0')
#endif




/*======================================================================================
                                        LOCAL MACROS
=======================================================================================*/

#define DEFAULT_FRAME_BUFFER_ALIGNMENT_H 16
#define DEFAULT_FRAME_BUFFER_ALIGNMENT_V 16

MfwGstVPU_Dec *vpudec_global_ptr = NULL;
static gint mutex_cnt = 0;
/* table with framerates expressed as fractions */
static const gint fpss[][2] = { {24000, 1001},
{24, 1}, {25, 1}, {30000, 1001},
{30, 1}, {50, 1}, {60000, 1001},
{60, 1}, {0, 1}
};

static const GstVideoFormat g_video_formats[] = { GST_VIDEO_FORMAT_I420,
  GST_VIDEO_FORMAT_NV12,
  GST_VIDEO_FORMAT_Y42B,
  GST_VIDEO_FORMAT_Y444,
  GST_VIDEO_FORMAT_VPU_GRAY_8,
  GST_VIDEO_FORMAT_UNKNOWN
};

/*======================================================================================
                                      STATIC VARIABLES
=======================================================================================*/
extern int vpu_fd;


/*======================================================================================
                                 STATIC FUNCTION PROTOTYPES
=======================================================================================*/
GST_DEBUG_CATEGORY (mfw_gst_vpudec_debug);

static void mfw_gst_vpudec_class_init (MfwGstVPU_DecClass *);
static void mfw_gst_vpudec_base_init (MfwGstVPU_DecClass *);
static void mfw_gst_vpudec_init (MfwGstVPU_Dec *, MfwGstVPU_DecClass *);
static GstFlowReturn mfw_gst_vpudec_chain (GstPad *, GstBuffer *);
static GstStateChangeReturn mfw_gst_vpudec_change_state (GstElement *,
    GstStateChange);
static void mfw_gst_vpudec_set_property (GObject *, guint, const GValue *,
    GParamSpec *);
static void mfw_gst_vpudec_get_property (GObject *, guint, GValue *,
    GParamSpec *);
static gboolean mfw_gst_vpudec_sink_event (GstPad *, GstEvent *);
static gboolean mfw_gst_vpudec_src_event (GstPad *, GstEvent *);
static gboolean mfw_gst_vpudec_setcaps (GstPad *, GstCaps *);
static GstBuffer *mfw_gst_VC1_Create_RCVheader (MfwGstVPU_Dec *, GstBuffer *);
static gboolean vpudec_preparevc1apheader (unsigned char *pDestBuffer,
    unsigned char *pDataBuffer,
    unsigned char *pAucHdrBuffer, int aucHdrLen, int *pLen);
void mfw_vpudec_set_field (MfwGstVPU_Dec * vpu_dec, GstBuffer * buf);
void mfw_gst_vpudec_vpu_finalize (void);
/*======================================================================================
                                     GLOBAL VARIABLES
=======================================================================================*/
/*======================================================================================
                                     LOCAL FUNCTIONS
=======================================================================================*/
gboolean vpu_mutex_lock (GMutex * mutex, gboolean ftry);
void vpu_mutex_unlock (GMutex * mutex);
gint mfw_gst_vpudec_FrameBufferInit (MfwGstVPU_Dec *, FrameBuffer *, gint);

void mfw_gst_vpudec_FrameBufferRelease (MfwGstVPU_Dec *);
gboolean mfw_gst_get_timestamp (MfwGstVPU_Dec *, GstClockTime *);
GstFlowReturn mfw_gst_vpudec_vpu_open (MfwGstVPU_Dec * vpu_dec);
GstFlowReturn mfw_gst_vpudec_copy_sink_input (MfwGstVPU_Dec *, GstBuffer *);
GstFlowReturn mfw_gst_vpudec_vpu_init (MfwGstVPU_Dec *);
GstFlowReturn mfw_gst_vpudec_render (MfwGstVPU_Dec *);
void mfw_gst_vpudec_cleanup (MfwGstVPU_Dec * vpu_dec);

/* used	in change state	function */
static GstElementClass *parent_class = NULL;

static guint32
mfw_vpu_video_format_to_fourcc(gint fmt)
{
  guint32 ret;
#if GST_CHECK_VERSION(0, 10, 29)
  ret =  gst_video_format_to_fourcc(fmt);
#else
  switch (fmt){
    case GST_VIDEO_FORMAT_VPU_GRAY_8:
      ret = GST_MAKE_FOURCC('Y','8','0','0');
      break;
    default:
      ret =  gst_video_format_to_fourcc(fmt);
      break;
  };
#endif
  return ret;
}

/*=============================================================================
FUNCTION:           mfw_gst_vpudec_post_fatal_error_msg

DESCRIPTION:        This function is used to post a fatal error message and
                    terminate the pipeline during an unrecoverable error.

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context error_msg message to be posted

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
mfw_gst_vpudec_post_fatal_error_msg (MfwGstVPU_Dec * vpu_dec, gchar * error_msg)
{
  GError *error = NULL;
  GQuark domain;
  domain = g_quark_from_string ("mfw_vpudecoder");
  error = g_error_new (domain, 10, "fatal error");
  gst_element_post_message (GST_ELEMENT (vpu_dec),
      gst_message_new_error (GST_OBJECT (vpu_dec), error, error_msg));
  g_error_free (error);
}

/*=============================================================================
FUNCTION:           vpu_mutex_lock

DESCRIPTION:        Locks the VPU mutex

ARGUMENTS PASSED:   mutex
                    fTry - if set means use trylock so it can fail if already locked

RETURN VALUE:       result - TRUE if lock succeeded, FALSE if failed
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
vpu_mutex_lock (GMutex * mutex, gboolean ftry)
{
  gboolean result = TRUE;
  if (!mutex || !vpudec_global_ptr) {
    GST_DEBUG (">>VPU_DEC: lock mutex is NULL cnt=%d", mutex_cnt);
    return FALSE;
  }
  if (ftry) {
    result = g_mutex_trylock (mutex);
    if (result == FALSE) {
    } else {
      mutex_cnt++;
    }
  } else {
    g_mutex_lock (mutex);
    mutex_cnt++;
  }
  return result;
}

/*=============================================================================
FUNCTION:           vpu_mutex_unlock

DESCRIPTION:        Unlocks the mutex

ARGUMENTS PASSED:   mutex

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
vpu_mutex_unlock (GMutex * mutex)
{
  if (!mutex || !vpudec_global_ptr) {
    GST_DEBUG (">>VPU_DEC: unlock mutex is NULL cnt=%d", mutex_cnt);
    return;
  }
  mutex_cnt--;
  g_mutex_unlock (mutex);
  return;
}


/*=============================================================================
FUNCTION:           vpu_dis_skipframe

DESCRIPTION:        Disable the skip frame mode.

ARGUMENTS PASSED:   mutex

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
vpu_dis_skipframe (MfwGstVPU_Dec * vpu_dec)
{
  if (vpu_dec->decParam->skipframeMode == 0)
    return;

  if (vpu_dec->decParam) {
    vpu_dec->decParam->skipframeMode = 0;
    vpu_dec->decParam->skipframeNum = 0;
  }
  GST_DEBUG ("disable skip mode.");
  return;
}


/*=============================================================================
FUNCTION:           vpu_ena_skipframe

DESCRIPTION:        Enable the skip frame mode: B/BP mode.
                    In this mode, the VPU will invoke GetOutputInfo to sync
                    timestamp outside.

ARGUMENTS PASSED:   mutex

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
vpu_ena_skipframe (MfwGstVPU_Dec * vpu_dec, gint level)
{
  if (level >= SKIP_MAX_VAL)
    return;

  if (vpu_dec->decParam->skipframeMode == level)
    return;


  GST_DEBUG ("set skip mode:%d: origin mode %d", level,
      vpu_dec->decParam->skipframeMode);

  if ((vpu_dec->decParam)) {
    vpu_dis_skipframe (vpu_dec);
    vpu_dec->decParam->skipframeMode = level;
    vpu_dec->decParam->skipframeNum = 1;
  }
  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_vpudec_mjpeg_is_supported

DESCRIPTION:        Check the MJPEG output format is supported or not.

ARGUMENTS PASSED:   mutex

RETURN VALUE:       TRUE: For MJPEG support 4:2:0 and 4:2:2 horizontal format.
                    FALSE: MJPEG not supported format.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
mfw_gst_vpudec_mjpeg_is_supported (MfwGstVPU_Dec * vpu_dec)
{
  if (vpu_dec->codec == STD_MJPG) {
    /*
     * 0 C 4:2:0,
     * 1 C 4:2:2 horizontal,
     * 2 C 4:2:2 vertical,
     * 3 C 4:4:4,
     * 4 C 4:0:0
     */
#define JPEG_SUPPORT_MASK  (0x1b)

    if ((1 << vpu_dec->initialInfo->mjpg_sourceFormat) & JPEG_SUPPORT_MASK) {
      return TRUE;
    } else {
      return FALSE;
    }
  } else
    return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_vpudec_is_mjpeg_422h

DESCRIPTION:        Check the MJPEG 4:2:2H format.

ARGUMENTS PASSED:   mutex

RETURN VALUE:       TRUE: This is MJPEG 4:2:2H format, FALSE: not this format.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
mfw_gst_vpudec_is_mjpeg_422h (MfwGstVPU_Dec * vpu_dec)
{
  if ((vpu_dec->codec == STD_MJPG)
      && (vpu_dec->initialInfo->mjpg_sourceFormat == 1))
    return TRUE;
  else
    return FALSE;
}

gboolean
mfw_gst_vpudec_is_mjpeg_444 (MfwGstVPU_Dec * vpu_dec)
{
  if ((vpu_dec->codec == STD_MJPG)
      && (vpu_dec->initialInfo->mjpg_sourceFormat == 3))
    return TRUE;
  else
    return FALSE;
}

gboolean
mfw_gst_vpudec_is_mjpeg_400 (MfwGstVPU_Dec * vpu_dec)
{
  if ((vpu_dec->codec == STD_MJPG)
      && (vpu_dec->initialInfo->mjpg_sourceFormat == 4))
    return TRUE;
  else
    return FALSE;
}


/*=============================================================================
FUNCTION:           vpudec_preparevc1apheader

DESCRIPTION:        prepare header for VC1 AP,
                    reference from Zhujun's implementation.

ARGUMENTS PASSED:   mutex

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gboolean
vpudec_preparevc1apheader (unsigned char *pDestBuffer,
    unsigned char *pDataBuffer,
    unsigned char *pAucHdrBuffer, int aucHdrLen, int *pLen)
{
  int i;
  unsigned char *temp;

  if ((pDestBuffer == NULL) || (pDataBuffer == NULL)
      || (pAucHdrBuffer == NULL) || (pLen == NULL) || (aucHdrLen < 4))
    return FALSE;

  i = *pLen;
  temp = pDestBuffer;

  //Find SeqHdrStartCode in AucHdrBuffer
  unsigned char *pSeqHdrPtr = pAucHdrBuffer;
  int iSeqHdrHdrLen = aucHdrLen;

  gboolean bScanSeqHdr = TRUE;
  while (bScanSeqHdr && (pSeqHdrPtr < pAucHdrBuffer + aucHdrLen)) {
    if ((0x00 == pSeqHdrPtr[0]) && (0x00 == pSeqHdrPtr[1])
        && (0x01 == pSeqHdrPtr[2]) && (0x0F == pSeqHdrPtr[3])) {
      iSeqHdrHdrLen -= (pSeqHdrPtr - pAucHdrBuffer);
      bScanSeqHdr = FALSE;
      break;
    } else {
      pSeqHdrPtr++;
    }
  }
  if (bScanSeqHdr) {
    return FALSE;
  }


  if ((0x00 == pDataBuffer[0]) && (0x00 == pDataBuffer[1])
      && (0x01 == pDataBuffer[2])) {
    if (0x0E == pDataBuffer[3]) // Entry pointer
    {
      gboolean loop = TRUE;
      unsigned char *pHdrScan = pSeqHdrPtr;
      while (loop && (pHdrScan < pSeqHdrPtr + iSeqHdrHdrLen)) {
        if ((0x00 == pHdrScan[0]) && (0x00 == pHdrScan[1])
            && (0x01 == pHdrScan[2]) && (0x0E == pHdrScan[3])) {
          loop = FALSE;
          break;
        } else {
          temp[i++] = pHdrScan[0];
          pHdrScan++;
        }
      }
    } else if (0x0D == pDataBuffer[3])  // Frame StartCode
    {
      // copy Auc Hdr
      memcpy (temp + i, pSeqHdrPtr, iSeqHdrHdrLen);
      i += iSeqHdrHdrLen;
    }
  } else {
    // copy Auc Hdr
    memcpy (temp + i, pSeqHdrPtr, iSeqHdrHdrLen);
    i += iSeqHdrHdrLen;

    temp[i++] = 0x00;
    temp[i++] = 0x00;
    temp[i++] = 0x01;
    temp[i++] = 0x0D;
  }
  *pLen = i;

  return TRUE;
}

/*=============================================================================
FUNCTION:           mfw_gst_VC1_Create_RCVheader

DESCRIPTION:        This function is used to create the RCV header
                    for integration with the ASF demuxer using the width,height and the
                    Header Extension data recived through caps negotiation.

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstBuffer
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstBuffer *
mfw_gst_VC1_Create_RCVheader (MfwGstVPU_Dec * vpu_dec, GstBuffer * inbuffer)
{
  GstBuffer *RCVHeader = NULL;
  unsigned char *RCVHeaderData;
  unsigned int value = 0;
  int i = 0;

#define RCV_HEADER_LEN  256
  RCVHeader = gst_buffer_new_and_alloc (RCV_HEADER_LEN);

  if (RCVHeader == NULL) {
    GST_DEBUG ("RVCHeader not allocated");
    return NULL;
  }
  RCVHeaderData = GST_BUFFER_DATA (RCVHeader);
  if (RCVHeaderData == NULL) {
    GST_DEBUG ("RVCHeader not allocated");
    return NULL;
  }

  if (vpu_dec->codec_subtype == 1) {
    GST_DEBUG ("preparing header for AP!");
    if (vpudec_preparevc1apheader
        (RCVHeaderData, GST_BUFFER_DATA (inbuffer),
            GST_BUFFER_DATA (vpu_dec->codec_data), vpu_dec->codec_data_len,
            &i) == FALSE) {
      GST_DEBUG ("Failed in prepare VC1 AP header!");
    }
    GST_DEBUG ("preparing header for AP successful!");

  } else {
    gchar *codec_data = GST_BUFFER_DATA (vpu_dec->codec_data);
    gint codec_data_len = vpu_dec->codec_data_len;
    gchar profile = -1;
    gint idx = 0;

    //Number of Frames, Header Extension Bit, Codec Version
    value = NUM_FRAMES | SET_HDR_EXT | CODEC_VERSION;
    RCVHeaderData[i++] = (unsigned char) value;
    RCVHeaderData[i++] = (unsigned char) (value >> 8);
    RCVHeaderData[i++] = (unsigned char) (value >> 16);
    RCVHeaderData[i++] = (unsigned char) (value >> 24);
    //Header Extension Size
    //ASF Parser gives 5 bytes whereas the VPU expects only 4 bytes, so limiting it
    if (vpu_dec->codec_data_len > 4)
      vpu_dec->codec_data_len = 4;
    RCVHeaderData[i++] = (unsigned char) vpu_dec->codec_data_len;
    RCVHeaderData[i++] = 0;     //(unsigned char)(vpu_dec->codec_data_len >> 8);
    RCVHeaderData[i++] = 0;     //(unsigned char)(vpu_dec->codec_data_len >> 16);
    RCVHeaderData[i++] = 0;     //(unsigned char)(vpu_dec->codec_data_len >> 24);

    //Header Extension bytes obtained during negotiation
    while (codec_data_len - idx >= 4) {
      profile = (codec_data[idx] >> 4) & 0x0F;
      if (((0 == profile) || (4 == profile)) &&
          ((codec_data[idx + 1] & 0x05) == 1) &&
          ((codec_data[idx + 2] & 0x04) == 0) &&
          ((codec_data[idx + 3] & 0x01) == 1)) {
        /* it is simple profile or main profile as expected */
        break;
      }
      idx++;
    }

    if (codec_data_len - idx < 4) {
      GST_ERROR (">>VPU_DEC: Invalid codec data for RCV header.");
      /* FIXME: generate codec data here. refer to Table 263 in VC-1 spec */
      idx = 0;
    }

    memcpy (RCVHeaderData + i, codec_data + idx, vpu_dec->codec_data_len);
    i += vpu_dec->codec_data_len;

    //Height
    RCVHeaderData[i++] = (unsigned char) vpu_dec->picHeight;
    RCVHeaderData[i++] = (unsigned char) (((vpu_dec->picHeight >> 8) & 0xff));
    RCVHeaderData[i++] = (unsigned char) (((vpu_dec->picHeight >> 16) & 0xff));
    RCVHeaderData[i++] = (unsigned char) (((vpu_dec->picHeight >> 24) & 0xff));
    //Width
    RCVHeaderData[i++] = (unsigned char) vpu_dec->picWidth;
    RCVHeaderData[i++] = (unsigned char) (((vpu_dec->picWidth >> 8) & 0xff));
    RCVHeaderData[i++] = (unsigned char) (((vpu_dec->picWidth >> 16) & 0xff));
    RCVHeaderData[i++] = (unsigned char) (((vpu_dec->picWidth >> 24) & 0xff));
    //Frame Size
    RCVHeaderData[i++] = (unsigned char) GST_BUFFER_SIZE (inbuffer);
    RCVHeaderData[i++] = (unsigned char) (GST_BUFFER_SIZE (inbuffer) >> 8);
    RCVHeaderData[i++] = (unsigned char) (GST_BUFFER_SIZE (inbuffer) >> 16);
    RCVHeaderData[i++] =
        (unsigned char) ((GST_BUFFER_SIZE (inbuffer) >> 24) | 0x80);
  }

  GST_BUFFER_SIZE (RCVHeader) = i;
  return RCVHeader;
}

static gboolean
mfw_gst_vpudec_prePareVC1Header (MfwGstVPU_Dec * vpu_dec, GstBuffer * buffer)
{
  // The Size of the input stream is appended with the input stream
  // for integration with ASF
  unsigned char *tempChars;
  GstBuffer *tempBuf;
  if (vpu_dec->codec_subtype == 0) {
    tempBuf = gst_buffer_new_and_alloc (4);
    tempChars = GST_BUFFER_DATA (tempBuf);
    tempChars[0] = (unsigned char) GST_BUFFER_SIZE (buffer);
    tempChars[1] = (unsigned char) (GST_BUFFER_SIZE (buffer) >> 8);
    tempChars[2] = (unsigned char) (GST_BUFFER_SIZE (buffer) >> 16);
    tempChars[3] = (unsigned char) (GST_BUFFER_SIZE (buffer) >> 24);
    vpu_dec->gst_buffer = gst_buffer_join (tempBuf, buffer);
  } else {
    unsigned char *pSourceBuffer = GST_BUFFER_DATA (buffer);
    if ((0x00 == pSourceBuffer[0])
        && (0x00 == pSourceBuffer[1])
        && (0x01 == pSourceBuffer[2])
        && (0x0D == pSourceBuffer[3])) {
    } else if ((0x00 == pSourceBuffer[0])
        && (0x00 == pSourceBuffer[1])
        && (0x01 == pSourceBuffer[2])
        && (0x0E == pSourceBuffer[3])) {
    } else {
      //start code -- 0x0d010000;
      tempBuf = gst_buffer_new_and_alloc (4);
      tempChars = GST_BUFFER_DATA (tempBuf);
      tempChars[0] = 0x00;
      tempChars[1] = 0x00;
      tempChars[2] = 0x01;
      tempChars[3] = 0x0D;
      vpu_dec->gst_buffer = gst_buffer_join (tempBuf, buffer);
    }
  }
}

static void
vpudec_free_gst_buf_meta (GstBuffer * buf)
{
  /* does not care the actual physical buffers since it
     will be freed in mfw_gst_vpudec_FrameBufferRelease */

  gint index = G_N_ELEMENTS (buf->_gst_reserved) - 1;
  GST_DEBUG ("vpu free buffer");
  gst_buffer_meta_free (buf->_gst_reserved[index]);
}


/*=============================================================================
FUNCTION:           mfw_gst_vpudec_FrameBufferInit

DESCRIPTION:        This function allocates the outbut buffer for the decoder

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context
                    frameBuf - VPU's Output Frame Buffer to be
                                   allocated.

                    num_buffers number of frame buffers to be allocated

RETURN VALUE:       0 (SUCCESS)/ -1 (FAILURE)
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gint
mfw_gst_vpudec_FrameBufferInit (MfwGstVPU_Dec * vpu_dec,
    FrameBuffer * frameBuf, gint num_buffers)
{
  gint i = 0;
  GstFlowReturn retval = GST_FLOW_OK;
  GstBuffer *outbuffer = NULL;
  guint strideY = vpu_dec->initialInfo->picWidth;
  guint height = vpu_dec->initialInfo->picHeight;
  guint img_size = strideY * height;
  guint cr_offset;
  gint mvsize = 0;
  gint sw_cnt = 0, hw_cnt = 0;

  vpu_dec->numframebufs = num_buffers;

  if (mfw_gst_vpudec_is_mjpeg_422h (vpu_dec))
    cr_offset = (strideY * height) >> 1;
  else if (mfw_gst_vpudec_is_mjpeg_444 (vpu_dec))
    cr_offset = (strideY * height);
  else
    cr_offset = (strideY * height) >> 2;


  mvsize = strideY * height / 4;

  vpu_dec->direct_render = TRUE;

  if (!vpu_dec->use_internal_buffer) {
    /* try to allocate one frame from v4lsink */
    gint limit = 300;
    /* FixME: why the videosink is in the wrong state */
    retval = gst_pad_alloc_buffer_and_set_caps (vpu_dec->srcpad, 0,
        vpu_dec->yuv_frame_size, GST_PAD_CAPS (vpu_dec->srcpad), &outbuffer);
    if (retval != GST_FLOW_OK) {
      GST_ERROR
          (">>VPU_DEC: Error %d in allocating the Framebuffer[%d]", retval, i);

    }

    /* FixME: why the videosink is in the wrong state */

    while (((limit--) > 0)
        && ((retval != GST_FLOW_OK)
            || (!(IS_DMABLE_BUFFER (outbuffer))))) {
      usleep (30000);
      if (retval == GST_FLOW_OK) {
        gst_buffer_unref (outbuffer);
      }
      outbuffer = NULL;
      retval = gst_pad_alloc_buffer_and_set_caps (vpu_dec->srcpad, 0,
          vpu_dec->yuv_frame_size, GST_PAD_CAPS (vpu_dec->srcpad), &outbuffer);
      if (retval != GST_FLOW_OK) {
        GST_ERROR
            (">>VPU_DEC: Error %d in allocating the Framebuffer[%d]",
            retval, i);
        continue;
      }

    }

    if (outbuffer == NULL) {
      GST_ERROR ("Could not allocate Framebuffer");
      return -1;
    }
  }

  for (i = 0; i < num_buffers; i++) {
    if ((!vpu_dec->use_internal_buffer) && (outbuffer == NULL)) {
      retval = gst_pad_alloc_buffer_and_set_caps (vpu_dec->srcpad, 0,
          vpu_dec->yuv_frame_size, GST_PAD_CAPS (vpu_dec->srcpad), &outbuffer);

      if (retval != GST_FLOW_OK) {
        GST_ERROR (">>VPU_DEC: Error %d in allocating the \
	                Framebuffer[%d]", retval, i);
        return -1;
      }
    }
    /* if the buffer allocated is the Hardware Buffer use it as it is */
    if ((outbuffer) && (IS_DMABLE_BUFFER (outbuffer))) {
      gpointer meta;
      /* Get the MV memory from VPU library */
      vpu_dec->frame_mem[i].size = mvsize;
      IOGetPhyMem (&vpu_dec->frame_mem[i]);
      frameBuf[i].bufMvCol = vpu_dec->frame_mem[i].phy_addr;
      vpu_dec->outbuffers[i] = outbuffer;
      GST_BUFFER_SIZE (vpu_dec->outbuffers[i]) = vpu_dec->yuv_frame_size;
      GST_BUFFER_OFFSET_END (vpu_dec->outbuffers[i]) = i;
      vpu_dec->fb_state_plugin[i] = FB_STATE_ALLOCATED;
      vpu_dec->fb_type[i] = FB_TYPE_GST;
      sw_cnt++;
      meta =
          outbuffer->_gst_reserved[G_N_ELEMENTS (outbuffer->_gst_reserved) - 1];

      {
        frameBuf[i].bufY = (PhysicalAddress) DMABLE_BUFFER_PHY_ADDR (outbuffer);
        frameBuf[i].bufCb = frameBuf[i].bufY + img_size;
        frameBuf[i].bufCr = frameBuf[i].bufCb + cr_offset;
      }
    } else {
      // else allocate The Hardware buffer through IOGetPhyMem
      // Note this to support writing the output to a file in case of
      // File Sink
      if (outbuffer != NULL) {
        gst_buffer_unref (outbuffer);
        outbuffer = NULL;
      }
      vpu_dec->frame_mem[i].size = vpu_dec->yuv_frame_size + mvsize;
      IOGetPhyMem (&vpu_dec->frame_mem[i]);

      // if not enough physical memory free all we allocated and exit
      if (vpu_dec->frame_mem[i].phy_addr == 0) {
        gint j;
        /* Free all the buffers */
        for (j = 0; j < i; j++) {
          IOFreeVirtMem (&vpu_dec->frame_mem[j]);
          IOFreePhyMem (&vpu_dec->frame_mem[j]);
          vpu_dec->frame_mem[j].phy_addr = 0;
          vpu_dec->frame_virt[j] = NULL;
          if (vpu_dec->fb_type[j] == FB_TYPE_GST) {
            gst_buffer_unref (vpu_dec->outbuffers[j]);
            vpu_dec->outbuffers[j] = NULL;
          }
        }
        GST_ERROR (">>VPU_DEC: Not enough memory for framebuffer!");
        return -1;
      }
      IOGetVirtMem (&vpu_dec->frame_mem[i]);

      {
        GstBufferMeta *bufmeta = gst_buffer_meta_new ();;
        outbuffer = gst_buffer_new ();
        int index;

        gst_buffer_set_caps (outbuffer, GST_PAD_CAPS (vpu_dec->srcpad));

        GST_BUFFER_SIZE (outbuffer) = vpu_dec->yuv_frame_size;
        GST_BUFFER_DATA (outbuffer) = vpu_dec->frame_mem[i].virt_uaddr;

        index = G_N_ELEMENTS (outbuffer->_gst_reserved) - 1;
        bufmeta->physical_data = vpu_dec->frame_mem[i].phy_addr;
        bufmeta->priv = NULL;
        outbuffer->_gst_reserved[index] = bufmeta;


        GST_BUFFER_MALLOCDATA (outbuffer) = outbuffer;
        GST_BUFFER_FREE_FUNC (outbuffer) = vpudec_free_gst_buf_meta;

        vpu_dec->fb_state_plugin[i] = FB_STATE_ALLOCATED;
        vpu_dec->fb_type[i] = FB_TYPE_GST;

        frameBuf[i].bufY = vpu_dec->frame_mem[i].phy_addr;
        frameBuf[i].bufCb = frameBuf[i].bufY + img_size;
        frameBuf[i].bufCr = frameBuf[i].bufCb + cr_offset;
        frameBuf[i].bufMvCol = frameBuf[i].bufCr + cr_offset;

        vpu_dec->outbuffers[i] = outbuffer;

      }

    }
    outbuffer = NULL;
  }

  if (vpu_dec->direct_render)
    GST_INFO (">>VPU_DEC: Use DR mode, GST buffer count:%d, \
                  HW buffer count:%d.", sw_cnt, hw_cnt);
  else
    GST_INFO (">>VPU_DEC: Use Non-DR mode.");

  for (i = 0; i < num_buffers; i++) {
    GST_INFO (">>VPU_DEC: buffer:%d, type:%s", i,
        (vpu_dec->fb_type[i] == FB_TYPE_HW) ? "FB_TYPE_HW" : "FB_TYPE_GST");
  }
  return 0;
}

/*=============================================================================
FUNCTION:           mfw_gst_vpudec_FrameBufferRelease

DESCRIPTION:        This function frees the allocated frame buffers

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
mfw_gst_vpudec_FrameBufferRelease (MfwGstVPU_Dec * vpu_dec)
{
  gint i;
  for (i = 0; i < vpu_dec->numframebufs; i++) {
    if (vpu_dec->frame_mem[i].phy_addr != 0) {
      IOFreeVirtMem (&vpu_dec->frame_mem[i]);
      IOFreePhyMem (&vpu_dec->frame_mem[i]);
      vpu_dec->frame_mem[i].phy_addr = 0;
      vpu_dec->frame_virt[i] = NULL;
    }
    if (vpu_dec->outbuffers[i]) {
      gst_buffer_unref (vpu_dec->outbuffers[i]);
      vpu_dec->outbuffers[i] = NULL;
    }
  }
  return;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_vpu_open

DESCRIPTION:        Open VPU

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context
                    buffer - pointer to the input buffer which has the video data.

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_vpu_open (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  guint8 *virt_bit_stream_buf = NULL;

  GST_DEBUG (">>VPU_DEC: codec=%d", vpu_dec->codec);
  vpu_dec->bit_stream_buf.size = vpu_dec->buffer_fill_size;
  if (vpu_dec->bit_stream_buf.phy_addr == NULL) {
    if (IOGetPhyMem (&vpu_dec->bit_stream_buf) < 0) {
      GST_ERROR ("Could not allocate bitstream buffer");
      return GST_FLOW_ERROR;
    }
  }
  virt_bit_stream_buf = (guint8 *) IOGetVirtMem (&vpu_dec->bit_stream_buf);
  vpu_dec->start_addr = vpu_dec->base_addr = virt_bit_stream_buf;
  vpu_dec->end_addr = virt_bit_stream_buf + vpu_dec->buffer_fill_size;
  vpu_dec->decOP->bitstreamBuffer = vpu_dec->bit_stream_buf.phy_addr;
  vpu_dec->base_write = vpu_dec->bit_stream_buf.phy_addr;
  vpu_dec->end_write =
      vpu_dec->bit_stream_buf.phy_addr + vpu_dec->buffer_fill_size;


  if (vpu_dec->codec == STD_AVC) {
    vpu_dec->ps_mem_desc.size = PS_SAVE_SIZE;
    if (vpu_dec->ps_mem_desc.phy_addr == NULL) {
      if (IOGetPhyMem (&vpu_dec->ps_mem_desc) < 0) {
        GST_ERROR ("Could not allocate ps buffer");
        if (vpu_dec->bit_stream_buf.phy_addr) {
          IOFreeVirtMem (&(vpu_dec->bit_stream_buf));
          IOFreePhyMem (&(vpu_dec->bit_stream_buf));

          vpu_dec->bit_stream_buf.phy_addr = 0;
        }
        return GST_FLOW_ERROR;
      }
    }
    vpu_dec->decOP->psSaveBuffer = vpu_dec->ps_mem_desc.phy_addr;
    vpu_dec->decOP->psSaveBufferSize = PS_SAVE_SIZE;

    vpu_dec->slice_mem_desc.size = SLICE_SAVE_SIZE;
    if (vpu_dec->slice_mem_desc.phy_addr == NULL) {
      if (IOGetPhyMem (&vpu_dec->slice_mem_desc) < 0) {
        GST_ERROR ("Could not allocate slice buffer");
        if (vpu_dec->bit_stream_buf.phy_addr) {
          IOFreeVirtMem (&(vpu_dec->bit_stream_buf));
          IOFreePhyMem (&(vpu_dec->bit_stream_buf));
          vpu_dec->bit_stream_buf.phy_addr = 0;
        }
        if (vpu_dec->ps_mem_desc.phy_addr) {
          IOFreeVirtMem (&(vpu_dec->ps_mem_desc));
          IOFreePhyMem (&(vpu_dec->ps_mem_desc));
          vpu_dec->ps_mem_desc.phy_addr = 0;
        }
        return GST_FLOW_ERROR;

      }
    }
  }

  vpu_dec->decOP->bitstreamBufferSize = vpu_dec->buffer_fill_size;

  if (vpu_dec->fmt == 0) {
    GST_DEBUG (">>VPU_DEC: set chromainterleave");
    vpu_dec->decOP->chromaInterleave = 1;
  } else {
    vpu_dec->decOP->chromaInterleave = 0;
  }

  if ((vpu_dec->codec == STD_AVC) || (vpu_dec->codec == STD_RV)) {
    vpu_dec->decOP->reorderEnable = 1;
  } else {
    vpu_dec->decOP->reorderEnable = 0;
  }

  // For MX37 and beyond VPU handles file play mode well and is more efficient
  // however MPEG2 requires streaming mode on all chips
  // If you are using the VPU plugins outside of a parser then you must disable
  // file play mode on these chips also
  if (vpu_dec->codec == STD_MPEG2) {
    // note that some mpeg2 files can play in file play mode but it is hard to know
    // exactly which ones so for now assume all streaming mode (which does not play as well sometimes)
    vpu_dec->file_play_mode = FALSE;
  } else {
    vpu_dec->file_play_mode = TRUE;
  }

  vpu_dec->decOP->mp4Class = vpu_dec->mp4Class;

  // if caps weren't set time_per_frame was not set - so no parser input
  // if parser_input not set then assume streaming mode
  if (!vpu_dec->parser_input || (vpu_dec->time_per_frame == 0))
    vpu_dec->file_play_mode = FALSE;
  if (vpu_dec->min_latency)
    vpu_dec->set_ts_manually = TRUE;

  vpu_dec->decOP->filePlayEnable = (vpu_dec->file_play_mode) ? 1 : 0;
  GST_DEBUG (">>VPU_DEC: Setting file play mode to %d",
      vpu_dec->decOP->filePlayEnable);

  vpu_dec->decOP->bitstreamFormat = vpu_dec->codec;
  vpu_dec->decOP->picWidth = vpu_dec->picWidth;
  vpu_dec->decOP->picHeight = vpu_dec->picHeight;


  /* open a VPU's decoder instance */
  vpu_ret = vpu_DecOpen (vpu_dec->handle, vpu_dec->decOP);
  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR (">>VPU_DEC: vpu_DecOpen failed. Error code is %d ", vpu_ret);
    return GST_FLOW_ERROR;
  }
  vpu_dec->vpu_opened = TRUE;
  return GST_FLOW_OK;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_copy_data

DESCRIPTION:        Copies data to the VPU input

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_copy_data (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  guint gst_buffer_size = GST_BUFFER_SIZE (vpu_dec->gst_buffer);
  guint size_to_copy = gst_buffer_size - vpu_dec->buff_consumed;
  guint8 *buffer_to_copy = GST_BUFFER_DATA (vpu_dec->gst_buffer);
  PhysicalAddress p1, p2;
  Uint32 space = vpu_dec->buffer_fill_size;
  gchar *info;

  if (!vpu_dec->vpu_init || !vpu_dec->file_play_mode) {
    vpu_ret = vpu_DecGetBitstreamBuffer (*(vpu_dec->handle), &p1, &p2, &space);
    if (vpu_ret != RETCODE_SUCCESS) {
      GST_ERROR
          (">>VPU_DEC: vpu_DecGetBitstreamBuffer failed. Error is %d ",
          vpu_ret);
      return GST_FLOW_ERROR;
    }
    if ((space <= 0) && (vpu_dec->num_timeouts == 0)) {
      if (vpu_dec->vpu_init) {
        GST_ERROR ("vpu_DecGetBitstreamBuffer returned zero space do a reset ");
        /* in this case our bitstream buffer is full
         *  - VPU is not able to decode into our buffers
         */
        mfw_gst_vpudec_reset (vpu_dec);
        /*
         * can't do copy yet since we need that first frame
         * which is done in copy_sink
         */
        return GST_FLOW_OK;
      } else {
        GST_ERROR
            ("vpu_DecGetBitstreamBuffer returned zero space flush buffer ");
        vpu_ret = vpu_DecBitBufferFlush (*vpu_dec->handle);
        resyncTSManager (vpu_dec->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
        vpu_ret =
            vpu_DecGetBitstreamBuffer (*(vpu_dec->handle), &p1, &p2, &space);
        if (space <= 0)
          return GST_FLOW_ERROR;
      }
    }
    vpu_dec->data_in_vpu = vpu_dec->buffer_fill_size - space;
  }
  // For File Play mode - copy over data from start of buffer and update VPU first time
  if (vpu_dec->file_play_mode == TRUE) {
    if ((vpu_dec->vpu_init == FALSE) && (space < size_to_copy)) {
      GST_ERROR
          ("copy_data failed space %d less then size_to_copy %d",
          space, size_to_copy);
      return GST_FLOW_ERROR;
    }

    if (vpu_dec->profiling) {
      info = g_strdup_printf ("copy data:%d ", size_to_copy);
      TIME_BEGIN ();
    }

    memcpy (vpu_dec->start_addr, buffer_to_copy, size_to_copy);

    if (vpu_dec->profiling) {
      if ((gint) size_to_copy > 1000000) {
        TIME_END (info);
      }
      g_free (info);
    }

    vpu_dec->decParam->chunkSize = size_to_copy;

    if (!vpu_dec->vpu_init) {
      /* Update Bitstream buffer just at init time */
      vpu_ret = vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle),
          (space <= 0) ? vpu_dec->buffer_fill_size : space);
      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR
            ("vpu_DecUpdateBitstreamBuffer failed. Error is %d ", vpu_ret);
        return (GST_FLOW_ERROR);
      }
    }
    GST_FRAMEDBG ("copy data size_to_copy %d  space %d\n", size_to_copy, space);

  } else {                      // handle streaming mode
    guint8 *buffer_to_copy =
        GST_BUFFER_DATA (vpu_dec->gst_buffer) + vpu_dec->buff_consumed;

    // Update min_data_in_vpu according to the input buffer size
    if (0 == vpu_dec->buff_consumed) {
      if (vpu_dec->min_data_in_vpu < gst_buffer_size) {
        vpu_dec->min_data_in_vpu = (gst_buffer_size + 511) & ~511;
      }
    }
    // check if we can only copy part of the buffer if so copy what what we can this time
    if (space <= size_to_copy) {
      size_to_copy = space - 4;
    }
    // check if we can do just one memcpy
    if ((vpu_dec->start_addr + size_to_copy) <= vpu_dec->end_addr) {
      memcpy (vpu_dec->start_addr, buffer_to_copy, size_to_copy);
      vpu_dec->start_addr += size_to_copy;
    } else                      // otherwise split it into two copies - one at end and one from beginning
    {
      guint residue = (vpu_dec->end_addr - vpu_dec->start_addr);
      memcpy (vpu_dec->start_addr, buffer_to_copy, residue);
      memcpy (vpu_dec->base_addr, buffer_to_copy + residue,
          size_to_copy - residue);
      vpu_dec->start_addr = vpu_dec->base_addr + size_to_copy - residue;
    }

    // Now update the bitstream buffer with the amount we just copied
    vpu_ret = vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle), size_to_copy);
    if (vpu_ret != RETCODE_SUCCESS) {
      GST_ERROR
          ("vpu_DecUpdateBitstreamBuffer failed. Error code is %d ", vpu_ret);
      return (GST_FLOW_ERROR);
    }

    vpu_dec->data_in_vpu += size_to_copy;
    GST_FRAMEDBG
        (">>VPU_DEC: copy data data_in_vpu %d min_data_in_vpu %d space %d\n",
        vpu_dec->data_in_vpu, vpu_dec->min_data_in_vpu, space);
  }

  // add our totals and see if buffer is now consumed - if so unref the input buffer
  vpu_dec->buff_consumed += size_to_copy;
  if (vpu_dec->buff_consumed >= gst_buffer_size) {
    gst_buffer_unref (vpu_dec->gst_buffer);
    vpu_dec->gst_buffer = NULL;
    vpu_dec->buff_consumed = 0;
    vpu_dec->must_copy_data = FALSE;
  } else {
    vpu_dec->must_copy_data = TRUE;
  }

  return GST_FLOW_OK;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_passthru

DESCRIPTION:        Copy undecoded data downstream

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_passthru (MfwGstVPU_Dec * vpu_dec)
{
  GstFlowReturn retval = GST_FLOW_OK;
  if (!vpu_dec->vpu_init) {
    GstCaps *caps = gst_caps_new_simple ("video/x-raw-yuv", NULL);

    if (!(gst_pad_set_caps (vpu_dec->srcpad, caps))) {
      GST_ERROR (">>VPU_DEC: Passthru could not set the caps for src pad");
    }
    gst_caps_unref (caps);
    caps = NULL;
    vpu_dec->vpu_init = TRUE;
  }
  GST_DEBUG (">>VPU_DEC: Passthru pushing %d downstream ",
      GST_BUFFER_SIZE (vpu_dec->gst_buffer));
  retval = gst_pad_alloc_buffer_and_set_caps (vpu_dec->srcpad, 0,
      GST_BUFFER_SIZE (vpu_dec->
          gst_buffer), GST_PAD_CAPS (vpu_dec->srcpad), &vpu_dec->pushbuff);
  if (retval != GST_FLOW_OK) {
    GST_ERROR (">>VPU_DEC: Error %d allocating the pass thru buffer", retval);
    return retval;
  }
  memcpy (GST_BUFFER_DATA (vpu_dec->pushbuff),
      GST_BUFFER_DATA (vpu_dec->gst_buffer),
      GST_BUFFER_SIZE (vpu_dec->gst_buffer));
  gst_buffer_unref (vpu_dec->gst_buffer);
  vpu_dec->gst_buffer = NULL;
  retval = gst_pad_push (vpu_dec->srcpad, vpu_dec->pushbuff);
  if (retval != GST_FLOW_OK) {
    GST_ERROR (">>VPU_DEC: Error %d pushing buffer downstream ", retval);
    return retval;
  }
  return GST_FLOW_OK;
}

void
ModifyAVC_offset (guint32 * jump, guint32 NALLengthFieldSize)
{
  static guint32 mask_value[4] =
      { 0xff000000, 0xffff0000, 0xffffff00, 0xffffffff };

  *jump = ((*jump) & (mask_value[NALLengthFieldSize - 1]));
  *jump >>= ((NAL_START_CODE_SIZE - NALLengthFieldSize) * 8);
}

/*======================================================================================
FUNCTION:           nal_scan_replace

DESCRIPTION:     * no nal header found, then we need to scan all the buffer
                 * 1. find whether it have jump length greater than buffer length.
                 *    if does not then replace the code with nal header until find it
                 *    if found large length, then do nothing, and jump out
                 ****************************************************************

ARGUMENTS PASSED:
                    buffer  - input buffer
                    end - the end address of current buffer

=======================================================================================*/

guint32
nal_scan_replace (unsigned char *buf, int end,
    guint32 NALLengthFieldSize, guint32 Ori_buf_size)
{
  unsigned char *temp_buf1, *temp_buf2;
  guint32 jump, data_size, unlegal_flag, counter, buf_size;
  unlegal_flag = 0;
  data_size = 0;
  counter = 0;
  buf_size = 0;
  temp_buf1 = buf + NAL_START_DEFAULT_LENGTH;

  while (((guint32) temp_buf1) < end) {
    jump = (temp_buf1[0] << 24) | (temp_buf1[1] << 16) | (temp_buf1[2] << 8)
        | (temp_buf1[3]);
    if (jump == 0x00000001) {
      break;
    } else {
      data_size = end - (guint32) temp_buf1;
      ModifyAVC_offset (&jump, NALLengthFieldSize);
      if (jump > data_size) {
        unlegal_flag = 1;
        break;
      }
    }
    counter++;
    temp_buf1 += (jump + NALLengthFieldSize);
  }
  /* nal header replace procedure, need copy to 100 byte ahead */
  temp_buf1 = buf + NAL_START_DEFAULT_LENGTH;
  temp_buf2 = buf;

  if (!unlegal_flag) {
    while (counter) {
      jump =
          (temp_buf1[0] << 24) | (temp_buf1[1] << 16) | (temp_buf1[2] <<
          8) | (temp_buf1[3]);
      ModifyAVC_offset (&jump, NALLengthFieldSize);

      temp_buf2[2] = temp_buf2[1] = temp_buf2[0] = 0;
      temp_buf2[3] = 0x01;
      memmove (temp_buf2 + 4, temp_buf1 + NALLengthFieldSize, jump);
      temp_buf1 += (jump + NALLengthFieldSize);
      temp_buf2 += (jump + 4);
      buf_size += (jump + 4);
      counter--;
    }
  } else {
    memmove (temp_buf2, temp_buf1, Ori_buf_size);
    buf_size = Ori_buf_size;
  }
  return buf_size;
}


/*======================================================================================
FUNCTION:           scan_data_found_nal_header

DESCRIPTION:     * no nal header found, then we need to scan all the buffer
                 * 1. find whether it have jump length greater than buffer length.
                 *    if does not then replace the code with nal header until find it
                 *    if found large length, then do nothing, and jump out
                 ****************************************************************

ARGUMENTS PASSED:
                    buffer  - input buffer
                    end - the end address of current buffer

RETURN VALUE:       GstFlowReturn - Success of Failure.

=======================================================================================*/

void
scan_data_found_nal_header (unsigned char *buf, int end)
{
  unsigned char *temp_buf;
  guint32 jump, data_size, unlegal_flag, counter;
  unlegal_flag = 0;
  data_size = 0;
  counter = 0;
  temp_buf = buf;
  while (((guint32) buf) < end) {
    jump = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
    if (jump == 0x00000001) {
      break;
    } else {
      data_size = end - (guint32) buf;
      if (jump > data_size) {
        unlegal_flag = 1;
        break;
      }
    }
    counter++;
    buf += (jump + 4);
  }
  /* nal header replace procedure */
  if (!unlegal_flag) {
    while (counter) {
      jump = (temp_buf[0] << 24) | (temp_buf[1] << 16) | (temp_buf[2] << 8)
          | (temp_buf[3]);
      temp_buf[2] = temp_buf[1] = temp_buf[0] = 0;
      temp_buf[3] = 0x01;
      temp_buf += (jump + 4);
      counter--;
    }
  }
}

/*======================================================================================
FUNCTION:           mfw_gst_avc_handle_specificdata

DESCRIPTION:        Create new codec specific data and put it into inputbuffer,
                    it is used to combine with the data buffer

ARGUMENTS PASSED:   GstBuffer *hdrbuffer           output  for header buffer
                    GstBuffer *codecdata          original codec data buffer

RETURN VALUE:      TRUE or FALSE
=======================================================================================*/

gboolean
mfw_gst_avc_handle_specificdata (GstBuffer * hdrbuffer, GstBuffer * codecdata,
    guint32 * NALLengthFieldSize)
{
  gboolean err = TRUE;
  guint8 *inputData;
  guint32 inputDataSize;
  /* H264 video, wrap decoder info in NAL units. The parameter NAL length field size
     is always 2 bytes long, different from that of data NAL units (1, 2 or 4 bytes) */
  guint32 i, j, k;
  guint32 info_size = 0;        /* size of SPS&PPS in NALs */
  gchar *data = NULL;           /* temp buffer */
  guint8 lengthSizeMinusOne;
  guint8 numOfSequenceParameterSets;
  guint8 numOfPictureParameterSets;
  guint16 NALLength;

  inputData = GST_BUFFER_DATA (codecdata);
  inputDataSize = GST_BUFFER_SIZE (codecdata);
  data = GST_BUFFER_DATA (hdrbuffer);


  /*
     start code to check for the begining of the H264 object header.
     Might have to change for other streams if required
     Amanda: ENGR57903
     Format of 'avcC' does not strictly follow that of 'jvtC'in 14496-15(2003),
     sec 4.1.5 "AVCConfigurationRecord".
     The actual format seems to be:
     aligned(8) class AVCDecoderConfigurationRecord {
     unsigned int(8) configurationVersion = 1;
     unsigned int(8) AVCProfileIndication;
     unsigned int(8) AVCLevelIndication;
     unsigned int(8) AVCCompatibleProfiles;
     bit(6) reserved = '111111'b;
     unsigned int(2) lengthSizeMinusOne;
     bit(3) reserved = '111'b;
     unsigned int(5) numOfSequenceParameterSets;
     for (i=0; i< numOfSequenceParameterSets;  i++) {
     unsigned int(16) sequenceParameterSetLength ;
     bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
     }
     unsigned int(8) numOfPictureParameterSets;
     for (i=0; i< numOfPictureParameterSets;  i++) {
     unsigned int(16) pictureParameterSetLength;
     bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
     }
     }
     TODO: test more files to change the following code
   */

  lengthSizeMinusOne = inputData[4];
  lengthSizeMinusOne &= 0x03;   /* nal length max is 4, min is 1 byte */

  /* lengthSizeMinusOne = 0x11, 0b1111 1111 */
  *NALLengthFieldSize = (guint32) lengthSizeMinusOne + 1;
  GST_INFO ("AVC NAL length size %d bytes", *NALLengthFieldSize);

  numOfSequenceParameterSets = inputData[5] & 0x1f;


  k = 6;
  for (i = 0; i < numOfSequenceParameterSets; i++) {
    if (k >= inputDataSize) {
      GST_ERROR ("Invalid Sequence parameter NAL length: %d", NALLength);
      err = FALSE;
      goto bail;
    }
    NALLength = inputData[k];
    NALLength = (NALLength << 8) + inputData[k + 1];
    //printf("Sequence parameter NAL length: %d\n",  NALLength);
    k += (NALLength + 2);
    info_size += (NALLength + NAL_START_CODE_SIZE);
  }
  numOfPictureParameterSets = inputData[k];
  k++;


  for (i = 0; i < numOfPictureParameterSets; i++) {
    if (k >= inputDataSize) {
      GST_ERROR ("Invalid picture parameter NAL length: %d", NALLength);
      err = FALSE;
      goto bail;
    }
    NALLength = inputData[k];
    NALLength = (NALLength << 8) + inputData[k + 1];
    //printf("Picture parameter NAL length: %d\n",  NALLength);
    k += (NALLength + 2);
    info_size += (NALLength + NAL_START_CODE_SIZE);
  }

  //printf("AVC decoder specific info size: %d\n", info_size);

  /* wrap SPS + PPS into the temp buffer "data" */

  k = 6;
  j = 0;
  for (i = 0; i < numOfSequenceParameterSets; i++) {
    NALLength = inputData[k];
    NALLength = (NALLength << 8) + inputData[k + 1];
    //printf("Sequence parameter NAL length: %d\n",  NALLength);
    *(data + j) = 0;
    *(data + j + 1) = 0;
    *(data + j + 2) = 0;
    *(data + j + 3) = 1;
    j += 4;

    memmove (data + j, inputData + k + 2, NALLength);
    k += (NALLength + 2);
    j += NALLength;

  }
  k++;                          /* number of picture parameter sets */
  for (i = 0; i < numOfPictureParameterSets; i++) {
    NALLength = inputData[k];
    NALLength = (NALLength << 8) + inputData[k + 1];
    //printf("Picture parameter NAL length: %d\n",  NALLength);
    *(data + j) = 0;
    *(data + j + 1) = 0;
    *(data + j + 2) = 0;
    *(data + j + 3) = 1;
    j += 4;

    memmove (data + j, inputData + k + 2, NALLength);
    k += (NALLength + 2);
    j += NALLength;
  }

  /* write back size to the original buffer */
  GST_BUFFER_SIZE (hdrbuffer) = info_size;
  //g_print("AVC NAL header length size %d bytes\n",  info_size);

bail:
  return err;

}

/*======================================================================================
FUNCTION:           mfw_gst_avc_fix_nalheader

DESCRIPTION:        Modify the buffer so that the length 4 bytes is changed to start code
                    open source demuxers like qt and ffmpeg_mp4 generate length+payload
                    instead of startcode plus payload which VPU can't support

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context
                    buffer - pointer to the input buffer which has the video data.

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_avc_fix_nalheader (MfwGstVPU_Dec * vpu_dec)
{
  GstBuffer *buffer = vpu_dec->gst_buffer;
  guint32 jump;
  gboolean ret;
  guint32 NALLengthFieldSize;
  unsigned char *buf = GST_BUFFER_DATA (buffer);
  guint32 end =
      (guint32) (GST_BUFFER_DATA (buffer) + GST_BUFFER_SIZE (buffer) - 4);
  GstBuffer *hdrBuf = NULL;
  ret = TRUE;
  NALLengthFieldSize = vpu_dec->NALLengthFieldSize;
  if (vpu_dec->firstFrameProcessed == FALSE) {
    if (vpu_dec->codec_data_len >= 8) {
      unsigned char *codec_data = GST_BUFFER_DATA (vpu_dec->codec_data);
      guint32 startcode =
          (codec_data[0] << 24) | (codec_data[1] << 16) | (codec_data[2] << 8) |
          (codec_data[3]);
      if ((startcode == 0x00000001) || ((startcode & 0xFFFFFF00) == 0x00000100)) {      /* FSL parser already encapsulate the codec data */
        GST_WARNING ("FSL parser, no necessary to convert data");
        hdrBuf = gst_buffer_copy (vpu_dec->codec_data);
      } else {

        hdrBuf =
            gst_buffer_new_and_alloc (vpu_dec->codec_data_len +
            NAL_START_DEFAULT_LENGTH);
        if (hdrBuf == NULL)
          return GST_FLOW_OK;
        /* successful allocate hdrBuf, then use function to refill it. */
        ret =
            mfw_gst_avc_handle_specificdata (hdrBuf, vpu_dec->codec_data,
            &NALLengthFieldSize);
        vpu_dec->NALLengthFieldSize = NALLengthFieldSize;
        if (!ret) {
          gst_buffer_unref (hdrBuf);
          hdrBuf = NULL;
          return GST_FLOW_OK;
        }
      }
      /* success create hdr codec specific buffer, then join the header buffer and the data buffer  */
    } else {
#if 0                           /* The AVC type is more complicate than 0x67 and 0x27, disable it. */
      guint32 startcode =
          (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
      if ((startcode == 0x00000001)
          && (vpu_dec->file_play_mode == FALSE)) {
        if ((buf[4] != 0x67) && (buf[4] != 0x27))
          return GST_FLOW_ERROR;
        else {
          return GST_FLOW_OK;
        }
      }
#else
      if (vpu_dec->file_play_mode == FALSE)
        return GST_FLOW_OK;
#endif
    }

  }
  if (vpu_dec->accumulate_hdr) {
    guint32 startcode =
        (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
    if (startcode == 0x00000001) {
      if ((buf[4] == 0x67) || (buf[4] == 0x68)) {       // no decoding of just headers
        vpu_dec->hdr_received = TRUE;
        return GST_FLOW_OK;
      }
    }

  }


  jump = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);

  if (jump == 0x00000001) {
    GST_DEBUG ("nal found in avc data");
  } else {
    /* judge whether hdr received for the first time */
    if (((buf[4] == 0x67) || (buf[4] == 0x68)))
      vpu_dec->hdr_received = TRUE;


    /* no nal header found, then we need to scan all the buffer
     * 1. find whether it have jump length greater than buffer length.
     *    if does not then replace the code with nal header until find it
     *    if found large length, then do nothing, and jump out
     ****************************************************************/
    {
      if (NALLengthFieldSize != NAL_START_CODE_SIZE) {
        /* offset of nal length is not same with start code, so we have to enlarge buffer
         *  to accormidate the added data, 100 as a default adding value for it. */
        GstBuffer *temp = NULL;
        guint32 buf_size, tmp_buf_size;
        temp = gst_buffer_new_and_alloc (NAL_START_DEFAULT_LENGTH);
        if (temp == NULL)
          return GST_FLOW_OK;
        tmp_buf_size = GST_BUFFER_SIZE (vpu_dec->gst_buffer);
        vpu_dec->gst_buffer = gst_buffer_join (temp, vpu_dec->gst_buffer);
        buf = GST_BUFFER_DATA (vpu_dec->gst_buffer);
        end =
            (guint32) (GST_BUFFER_DATA (vpu_dec->gst_buffer) +
            GST_BUFFER_SIZE (vpu_dec->gst_buffer));
        /*  write new function handle special case, need to copy data  */
        buf_size =
            nal_scan_replace (buf, end, NALLengthFieldSize, tmp_buf_size);
        if (buf_size != 0)
          GST_BUFFER_SIZE (vpu_dec->gst_buffer) = buf_size;
      } else {
        /* same with start code, normal process */
        scan_data_found_nal_header (buf, end);
      }
    }

  }
  if (hdrBuf)
    vpu_dec->gst_buffer = gst_buffer_join (hdrBuf, vpu_dec->gst_buffer);

  return GST_FLOW_OK;
}



/*======================================================================================
FUNCTION:           decoder_close

DESCRIPTION:        Closes VPU

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void
decoder_close (MfwGstVPU_Dec * vpu_dec)
{
  DecOutputInfo outinfo = { 0 };
  RetCode ret;

  ret = vpu_DecClose (*vpu_dec->handle);
  if (ret == RETCODE_FRAME_NOT_COMPLETE) {
    mfw_gst_vpu_dec_thread_get_output (vpu_dec, FALSE);
    ret = vpu_DecClose (*vpu_dec->handle);
  }
  // try sending EOS which will get VPU out of busy state
  if (ret != RETCODE_SUCCESS) {
    ret = vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle), 0);
    mfw_gst_vpu_dec_thread_get_output (vpu_dec, FALSE);
    ret = vpu_DecClose (*vpu_dec->handle);
  }
  // if all this fails then log out an error
  if (ret != RETCODE_SUCCESS) {
    GST_ERROR (">>>VPU_DEC: vpu_DecClose failed");
  }
}


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_reset

DESCRIPTION:        Resets VPU

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_reset (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  guint i = 0;
  GST_DEBUG
      (">>VPU_DEC: Reset close and reopen using same previously allocated memory ");
  for (i = 0; i < vpu_dec->numframebufs; i++) {
    if (vpu_dec->fb_state_plugin[i] != FB_STATE_DISPLAY)
      vpu_dec->fb_state_plugin[i] = FB_STATE_FREE;
  }

#ifdef VPU_THREAD
  vpu_thread_mutex_lock (vpu_dec->vpu_thread);
  g_async_queue_push (vpu_dec->vpu_thread->async_q,
      (gpointer) VPU_DEC_CLEAR_FRAME_EVENT);
  g_cond_wait (vpu_dec->vpu_thread->
      cond_event[VPU_DEC_CLEAR_FRAME_COMPLETE], vpu_dec->vpu_thread->mutex);
  vpu_thread_mutex_unlock (vpu_dec->vpu_thread);
#else
  mfw_gst_vpudec_release_buff (vpu_dec);
#endif

  // if VPU is busy you can't cleanup and close will hang so be sure to set
  // bitstream to 0 so reset can happen
  if (vpu_IsBusy ())
    vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle), 0);

  i = 0;
  while (vpu_IsBusy ()) {
    //GST_DEBUG (">>VPU_DEC: waiting for VPU to get out of busy state \n");
    usleep (500000);
    i++;
    if (i > 100)
      return GST_FLOW_ERROR;    // avoid infinite loop but VPU will not be closed
  }

  vpu_dec->data_in_vpu = 0;
  if (vpu_dec->is_frame_started) {
    vpu_ret = mfw_gst_vpu_dec_thread_get_output (vpu_dec, FALSE);

    if (vpu_ret != RETCODE_SUCCESS)
      GST_ERROR ("vpu_DecGetOutputInfo failed. Error code is %d ", vpu_ret);
  }
  decoder_close (vpu_dec);


  /*
   * FIXME:Release the frame buffer cause memory leakage.
   * This reset function does not release the frame buffer
   * which already requested from outside, but VPU need register
   * these buffer again.
   */
  vpu_dec->vpu_init = FALSE;
  vpu_dec->firstFrameProcessed = FALSE;
  memset (vpu_dec->handle, 0, sizeof (DecHandle));
  vpu_ret = vpu_DecOpen (vpu_dec->handle, vpu_dec->decOP);
  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR ("vpu_DecOpen failed. Error code is %d ", vpu_ret);
  }
  vpu_dec->is_frame_started = FALSE;
  vpu_dec->start_addr = vpu_dec->base_addr;
  vpu_dec->eos = FALSE;
  vpu_dec->frames_decoded = 0;
  resyncTSManager (vpu_dec->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
  vpu_dec->num_timeouts = 0;

  if (vpu_dec->codec == STD_MPEG2) {
    vpu_dec->firstFrameProcessed = TRUE;
    memcpy (vpu_dec->start_addr, GST_BUFFER_DATA (vpu_dec->codec_data),
        vpu_dec->codec_data_len);
    vpu_dec->start_addr += vpu_dec->codec_data_len;
    // Now update the bitstream buffer with the amount we just copied
    vpu_ret =
        vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle),
        vpu_dec->codec_data_len);
    if (vpu_ret != RETCODE_SUCCESS) {
      GST_ERROR
          ("vpu_DecUpdateBitstreamBuffer failed. Error code is %d ", vpu_ret);
      return (GST_FLOW_ERROR);
    }
    vpu_dec->data_in_vpu += vpu_dec->codec_data_len;
    GST_DEBUG (">>VPU_DEC: Merging the codec data over size %d",
        vpu_dec->codec_data_len);
  }

  return GST_FLOW_OK;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_copy_sink_input

DESCRIPTION:        Update bitstream buffer in streaming mode which means we might not
                    be getting a full frame from upstream as we are in file play mode

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context
                    buffer - pointer to the input buffer which has the video data.

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_copy_sink_input (MfwGstVPU_Dec * vpu_dec, GstBuffer * buffer)
{
  RetCode vpu_ret = RETCODE_SUCCESS;

  // save the time stamp and see how late we are
  if (buffer && vpu_dec->new_ts && ((vpu_dec->outputInfo->indexFrameDecoded >= 0) ||    /* in file-play mode, we only push pts after vpu decoded a frame successfully */
          !vpu_dec->file_play_mode || !vpu_dec->vpu_init)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (vpu_dec->new_segment) {
      resyncTSManager (vpu_dec->pTS_Mgr, timestamp, vpu_dec->tsm_mode);
      vpu_dec->new_segment = 0;
    }

    TSManagerReceive (vpu_dec->pTS_Mgr, timestamp);
    vpu_dec->new_ts = FALSE;
  }

  vpu_dec->hdr_received = FALSE;

  // Check EOS and reset VPU accordingly
  if (vpu_dec->vpu_init && (G_UNLIKELY (buffer == NULL || vpu_dec->eos))) {
    // check if a non-null buffer was passed - in this case we were in EOS but are getting
    // more data so we need to close and reopen VPU to reset VPU before copying data

    vpu_ret = vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle), 0);
    if (vpu_ret != RETCODE_SUCCESS) {
      GST_ERROR
          (">>VPU_DEC: vpu_DecUpdateBitstreamBuffer failed. Error is %d ",
          vpu_ret);
      return GST_FLOW_ERROR;
    }
    if (buffer) {
      mfw_gst_vpudec_reset (vpu_dec);
    } else {
      /* Normal case received NULL buffer means :
       * normal EOS case - mark so we can decode/display remaining data in VPU
       */
      if (vpu_dec->decOP->reorderEnable == 1) {
        // Make display reorder buffer TRUE to read the decoded frames which are pending with VPU
        vpu_dec->decParam->dispReorderBuf = 1;
      }
      vpu_dec->eos = TRUE;
      vpu_dec->must_copy_data = FALSE;
      GST_DEBUG (">>VPU_DEC: After EOS set reorder enable");
      return GST_FLOW_OK;       // nothing else left to do since buffer is null
    }

    vpu_dec->check_for_bframe = FALSE;
    vpu_dec->just_flushed = FALSE;
    vpu_dec->decParam->iframeSearchEnable = 0;
    GST_DEBUG ("%s:before skip frame mode:%d", __FUNCTION__,
        vpu_dec->decParam->skipframeMode);
    vpu_dec->decParam->skipframeMode = 0;
    vpu_dec->decParam->skipframeNum = 0;
  }
  /* VPU not initiliaztion or buffer != NULL, !EOS case */
  if (vpu_dec->is_frame_started && !vpu_dec->num_timeouts)
    return GST_FLOW_OK;

  if (buffer == NULL)
    return GST_FLOW_OK;

  // Check for first frame and copy codec specific data needed
  // before copying input buffer to VPU
  if (G_UNLIKELY (vpu_dec->firstFrameProcessed == FALSE)) {     //first packet
    if ((vpu_dec->codec == STD_VC1) && (vpu_dec->picWidth != 0)) {
      // Creation of RCV Header is done in case of ASF Playback pf VC-1 streams
      //   from the parameters like width height and Header Extension Data */
      GstBuffer *tempBuf = mfw_gst_VC1_Create_RCVheader (vpu_dec, buffer);
      if (tempBuf == NULL) {
        GST_DEBUG ("error in create rcv");
        return GST_FLOW_ERROR;
      }
      vpu_dec->gst_buffer = gst_buffer_join (tempBuf, vpu_dec->gst_buffer);
    }
    if (((vpu_dec->codec == STD_MPEG2) || (vpu_dec->codec == STD_MPEG4)
            || (vpu_dec->codec == STD_RV)) && vpu_dec->codec_data_len) {
      // or for open source demux (qtdemux), it also needs the initial codec data
      GstBuffer *tempBuf = gst_buffer_new_and_alloc (vpu_dec->codec_data_len);
      memcpy (GST_BUFFER_DATA (tempBuf),
          GST_BUFFER_DATA (vpu_dec->codec_data), vpu_dec->codec_data_len);
      vpu_dec->gst_buffer = gst_buffer_join (tempBuf, buffer);
      if (vpu_dec->check_for_iframe) {
        vpu_dec->just_flushed = TRUE;
        vpu_dec->check_for_iframe = FALSE;      // not needed now with other settings
      }
    }
    if (vpu_dec->codec == STD_AVC) {
      // this routine checks and makes sure we are starting on an I frame.  The VPU encoder
      // in streaming mode will always send headers with an I frame so checking for headers
      // helps us avoid starting VPU on a non-I frame which it does not like!
      if (vpu_dec->check_for_iframe) {
        vpu_dec->just_flushed = TRUE;
        vpu_dec->check_for_iframe = FALSE;      // not needed now with other settings
      }

      if ((vpu_dec->nal_check || !vpu_dec->vpu_init)
          && mfw_gst_avc_fix_nalheader (vpu_dec) != GST_FLOW_OK) {
        if (vpu_dec->passthru)
          goto passthru;
        return GST_FLOW_OK;     // ignore until we get an I frame
      }
    }
    if ((vpu_dec->codec == STD_MPEG2) && !vpu_dec->codec_data) {
      // save this header in repeat mode since it is not always sent down again
      vpu_dec->codec_data_len = GST_BUFFER_SIZE (buffer);
      GST_FRAMEDBG (">>VPU_DEC: Initializing codec_data size=%d\n",
          vpu_dec->codec_data_len);
      vpu_dec->codec_data = gst_buffer_new_and_alloc (vpu_dec->codec_data_len);
      memcpy (GST_BUFFER_DATA (vpu_dec->codec_data),
          GST_BUFFER_DATA (buffer), vpu_dec->codec_data_len);
    }

    vpu_dec->decParam->skipframeMode = 1;
    vpu_dec->decParam->skipframeNum = 1;

    vpu_dec->firstFrameProcessed = TRUE;
  } else if ((vpu_dec->codec == STD_VC1) && (vpu_dec->picWidth != 0)) {
    mfw_gst_vpudec_prePareVC1Header (vpu_dec, buffer);
  } else if (vpu_dec->codec == STD_AVC) {
    if (vpu_dec->nal_check)     // playing from file we might not have nal boundaries
    {
      mfw_gst_avc_fix_nalheader (vpu_dec);
    }
  }
  // vpu_dec->buff_consumed = 0;

passthru:
  // if passthru just pass it down to our pipeline - no decode
  // Incase of the Filesink the output in the hardware buffer is copied onto the
  // buffer allocated by filesink
  if (vpu_dec->passthru) {
    return (mfw_gst_vpudec_passthru (vpu_dec));
  }

  return (mfw_gst_vpudec_copy_data (vpu_dec));
}


void
float2fractions (double startx, long maxden, int *denom, int *num)
{
  double x;
  long m[2][2];
  long ai;
  x = startx;

  /*  initialize matrix */
  m[0][0] = m[1][1] = 1;
  m[0][1] = m[1][0] = 0;

  /*  loop finding terms until denom gets too big */
  while (m[1][0] * (ai = (long) x) + m[1][1] <= maxden) {
    long t;
    t = m[0][0] * ai + m[0][1];
    m[0][1] = m[0][0];
    m[0][0] = t;
    t = m[1][0] * ai + m[1][1];
    m[1][1] = m[1][0];
    m[1][0] = t;
    if (x == (double) ai)
      break;                    // AF: division by zero
    x = 1 / (x - (double) ai);
    if (x > (double) 0x7FFFFFFF)
      break;                    // AF: representation failure
  }

  GST_INFO ("%ld/%ld, tolerance = %e\n", m[0][0], m[1][0],
      startx - ((double) m[0][0] / (double) m[1][0]));
  *denom = m[1][0];
  *num = m[0][0];


}


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_get_par

DESCRIPTION:        extract pixel aspect ratio info from VPU initial info

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void
mfw_gst_vpudec_get_par (MfwGstVPU_Dec * vpu_dec)
{
  vpu_dec->par_width = DEFAULT_PAR_WIDTH;
  vpu_dec->par_height = DEFAULT_PAR_HEIGHT;
  if (vpu_dec->codec == STD_AVC) {
    if (0 == (vpu_dec->initialInfo->aspectRateInfo & 0xFFFF0000)) {
      guint index = (vpu_dec->initialInfo->aspectRateInfo & 0xFF);
      if (16 >= index) {
        /* get from table E-1 in AVC spec */
        guint a_par_width[] =
            { DEFAULT_PAR_WIDTH, 1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64,
              160, 4, 3, 2 };
        guint a_par_height[] =
            { DEFAULT_PAR_HEIGHT, 1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33,
              99, 3, 2, 1 };
        vpu_dec->par_width = a_par_width[index];
        vpu_dec->par_height = a_par_height[index];
      }
    } else {
      /* | 31 ----- 16 | 15 ----- 0 |
       *   pixel width  pixel height */
      vpu_dec->par_width = vpu_dec->initialInfo->aspectRateInfo >> 16;
      vpu_dec->par_height = vpu_dec->initialInfo->aspectRateInfo & 0xFFFF;
    }
  } else if (vpu_dec->codec == STD_VC1) {
    /* | 31 ----- 16 | 15 ----- 0 |
     *   pixel width  pixel height */
    vpu_dec->par_width = vpu_dec->initialInfo->aspectRateInfo >> 16;
    vpu_dec->par_height = vpu_dec->initialInfo->aspectRateInfo & 0xFFFF;
  } else if (vpu_dec->codec == STD_MPEG4) {
    if (5 >= (vpu_dec->initialInfo->aspectRateInfo & 0xFF)) {
      /* get from table 6-12 in MPEG-4 video spec */
      guint a_par_width[] = { DEFAULT_PAR_WIDTH, 1, 12, 10, 16, 40 };
      guint a_par_height[] = { DEFAULT_PAR_HEIGHT, 1, 11, 11, 11, 33 };
      vpu_dec->par_width = a_par_width[vpu_dec->initialInfo->aspectRateInfo];
      vpu_dec->par_height = a_par_height[vpu_dec->initialInfo->aspectRateInfo];
    } else if (15 == (vpu_dec->initialInfo->aspectRateInfo & 0xFF)) {
      /* | 31 ----- 24 | 23 ----- 16 | 15 ------- 8 | 7 ------- 0 |
       *   0x00         pixel height  pixel width    0x0F         */
      vpu_dec->par_width = (vpu_dec->initialInfo->aspectRateInfo >> 8) & 0xFF;
      vpu_dec->par_height = (vpu_dec->initialInfo->aspectRateInfo >> 16) & 0xFF;
    }
  } else if (vpu_dec->codec == STD_MPEG2) {
    if ((vpu_dec->initialInfo->profile == 0)
        && (vpu_dec->initialInfo->level == 0)) {
      gfloat dar =
          vpu_dec->initialInfo->aspectRateInfo *
          (vpu_dec->initialInfo->picHeight / vpu_dec->initialInfo->picWidth);
      float2fractions (dar, 16, &vpu_dec->par_width, &vpu_dec->par_height);
    } else {
      if (vpu_dec->initialInfo->aspectRateInfo < 5) {
        guint a_par_width[] = { DEFAULT_PAR_WIDTH, 1, 4, 16, 221 };
        guint a_par_height[] = { DEFAULT_PAR_HEIGHT, 1, 3, 9, 100 };
        vpu_dec->par_width = a_par_width[vpu_dec->initialInfo->aspectRateInfo];
        vpu_dec->par_height =
            a_par_height[vpu_dec->initialInfo->aspectRateInfo];
      }
    }
    GST_INFO ("mpeg2 aspect ratio:%d,%d", vpu_dec->par_width,
        vpu_dec->par_height);
  }

  if ((vpu_dec->par_width == 0) || (vpu_dec->par_height == 0)) {
    vpu_dec->par_width = DEFAULT_PAR_WIDTH;
    vpu_dec->par_height = DEFAULT_PAR_HEIGHT;
  }
}



/*======================================================================================
FUNCTION:           mfw_gst_vpudec_check_allowed_caps

DESCRIPTION:        get the output YUV format fourcc

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       YUV format fourcc
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstVideoFormat
mfw_gst_vpudec_get_format (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

  /* by default, the format is 4:2:0. check if the UV is interleaved */
  if (vpu_dec->fmt == 0)
    format = GST_VIDEO_FORMAT_NV12;
  else
    format = GST_VIDEO_FORMAT_I420;

  if (mfw_gst_vpudec_is_mjpeg_422h (vpu_dec)) {
    format = GST_VIDEO_FORMAT_Y42B;
    GST_WARNING ("get the mjpg output format:%d.",
        vpu_dec->initialInfo->mjpg_sourceFormat);
  } else if (mfw_gst_vpudec_is_mjpeg_444 (vpu_dec)) {
    format = GST_VIDEO_FORMAT_Y444;
    GST_WARNING ("get the mjpg output format:%d.",
        vpu_dec->initialInfo->mjpg_sourceFormat);
  } else if (mfw_gst_vpudec_is_mjpeg_400 (vpu_dec)) {
    format = GST_VIDEO_FORMAT_VPU_GRAY_8;
    GST_WARNING ("get the mjpg output format:%d.",
        vpu_dec->initialInfo->mjpg_sourceFormat);
  }

  GST_INFO ("VPU output format is %d", format);

  return format;
}



/*======================================================================================
FUNCTION:           mfw_gst_vpudec_check_allowed_caps

DESCRIPTION:        get allowed caps from src pad and its peer, and get some fields

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
gint
mfw_gst_vpudec_check_allowed_caps (MfwGstVPU_Dec * vpu_dec)
{
  GstCaps *allowed_caps = NULL;
  GstStructure *structure = NULL;
  guint32 format_fourcc, i, j = 0;
  GstVideoFormat peer_format, vpu_format;
  gint ret = TRUE;

  if ((vpu_dec->buf_alignment_h > 0) && (vpu_dec->buf_alignment_v > 0)) {
    goto exit;
  }

  vpu_dec->video_format = mfw_gst_vpudec_get_format (vpu_dec);

  /* get allowed media types that can flow through pad and its peer. */
  allowed_caps = gst_pad_get_allowed_caps (vpu_dec->srcpad);
  if (NULL == allowed_caps) {
    goto exit;
  }

#if GST_CHECK_VERSION(0, 10, 35)
  GST_INFO ("allowed_caps : %" GST_PTR_FORMAT, allowed_caps);
#else
  {
    gchar * caps_str = gst_caps_to_string(allowed_caps);
    if (caps_str){
      GST_INFO ("allowed_caps : %s", allowed_caps);
      g_free(caps_str);
    }
  }
#endif

  /* check if the peer support current output format of VPU */
  for (i = 0; i < gst_caps_get_size (allowed_caps); i++) {
    structure = gst_caps_get_structure (allowed_caps, i);
    gst_structure_get_fourcc (structure, "format", &format_fourcc);
    peer_format = gst_video_format_from_fourcc (format_fourcc);
    if (peer_format == vpu_dec->video_format)
      break;
    structure = NULL;
  }

  if (NULL == structure) {
    /* no matched format, then try to find another format which is supported
     * by both VPU and peer. (need to reopen VPU) */
    if (GST_VIDEO_FORMAT_I420 == vpu_dec->video_format) {
      GST_WARNING ("video sink does not support I420, try NV12");
      vpu_dec->video_format = GST_VIDEO_FORMAT_NV12;
      vpu_dec->fmt = 0;
    } else if (GST_VIDEO_FORMAT_NV12 == vpu_dec->video_format) {
      GST_WARNING ("video sink does not support NV12, try I420");
      vpu_dec->video_format = GST_VIDEO_FORMAT_I420;
      vpu_dec->fmt = 1;
    } else {
      GST_ERROR ("video sink does not support Y42B, Y444 or Y800");
      /* no allowed caps for 4:2:2, 4:4:4 or 4:0:0, fatal error */
      goto exit;
    }

    for (i = 0; i < gst_caps_get_size (allowed_caps); i++) {
      structure = gst_caps_get_structure (allowed_caps, i);
      gst_structure_get_fourcc (structure, "format", &format_fourcc);
      peer_format = gst_video_format_from_fourcc (format_fourcc);
      if (peer_format == vpu_dec->video_format)
        break;
      structure = NULL;
    }

    if (NULL == structure) {
      GST_ERROR ("neither NV12 nor I420 is supported by video sink");
      /* no allowed caps for NV12 or I420, fatal error */
      goto exit;
    }
  }



  if (gst_structure_has_field (structure, "width_align")) {
    gst_structure_get_int (structure, "width_align", &vpu_dec->buf_alignment_h);
  }
  if (gst_structure_has_field (structure, "height_align")) {
    gst_structure_get_int (structure, "height_align",
        &vpu_dec->buf_alignment_v);
  }

exit:
  if (0 == vpu_dec->buf_alignment_h) {
    vpu_dec->buf_alignment_h = DEFAULT_FRAME_BUFFER_ALIGNMENT_H;
  }
  if (0 == vpu_dec->buf_alignment_v) {
    vpu_dec->buf_alignment_v = DEFAULT_FRAME_BUFFER_ALIGNMENT_V;
  }

  GST_INFO ("width_align = %d, height_align = %d",
      vpu_dec->buf_alignment_h, vpu_dec->buf_alignment_v);

  if (NULL != allowed_caps) {
    gst_caps_unref (allowed_caps);
  }

  return TRUE;
}


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_vpu_init

DESCRIPTION:        Initialize VPU and register allocated buffers

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_vpu_init (MfwGstVPU_Dec * vpu_dec)
{

  RetCode vpu_ret = RETCODE_SUCCESS;
  DecBufInfo bufinfo;
  guint needFrameBufCount = 0;
  guint32 format_fourcc;

  vpu_DecSetEscSeqInit (*(vpu_dec->handle), 1);

  memset (vpu_dec->initialInfo, 0, sizeof (DecInitialInfo));
  vpu_ret = vpu_DecGetInitialInfo (*(vpu_dec->handle), vpu_dec->initialInfo);


  // Release the VPU for initialization regardless of error
  vpu_DecSetEscSeqInit (*(vpu_dec->handle), 0);


  // In some cases of streaming mode - might not be enough data so return OK
  // this is not an error - just signifies more input buffer is needed
  // should not happen in file play mode where a parser is giving a full frame
  // each time
  if (vpu_ret == RETCODE_FRAME_NOT_COMPLETE) {
    GST_DEBUG
        (">>VPU_DEC: Exiting vpu_init for more data - init is not complete ");
    return GST_FLOW_OK;
  }

  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR
        (">>VPU_DEC: vpu_DecGetInitialInfo failed. Error code is %d handle %d ",
        vpu_ret, vpu_dec->handle);

    /* drop this timestamp */
    vpu_dec->init_fail_cnt++;
    TSManagerSend (vpu_dec->pTS_Mgr);
    vpu_ret = vpu_DecClose (*vpu_dec->handle);
    if (vpu_ret != RETCODE_SUCCESS) {
      GST_ERROR (">>>VPU_DEC: vpu_DecClose failed");
      /* fatal error, no more retry */
      vpu_dec->num_timeouts = 100;
    } else {
      vpu_dec->vpu_opened = FALSE;
    }
    return GST_FLOW_ERROR;
  }

  GST_DEBUG (">>VPU_DEC: min buffer count= %d",
      vpu_dec->initialInfo->minFrameBufferCount);
  GST_DEBUG
      (">>VPU_DEC: InitialInfo picWidth: %u, picHeight: %u, frameRate: %u",
      vpu_dec->initialInfo->picWidth, vpu_dec->initialInfo->picHeight,
      (unsigned int) vpu_dec->initialInfo->frameRateInfo);

  /* Check: Minimum resolution limitation */
  if (G_UNLIKELY (vpu_dec->initialInfo->picWidth < MIN_WIDTH ||
          vpu_dec->initialInfo->picHeight < MIN_HEIGHT)) {
    GstMessage *message = NULL;
    GError *gerror = NULL;
    gchar *text_msg = "unsupported video resolution.";
    gerror = g_error_new_literal (1, 0, text_msg);
    message = gst_message_new_error (GST_OBJECT (GST_ELEMENT (vpu_dec)),
        gerror, "debug none");
    gst_element_post_message (GST_ELEMENT (vpu_dec), message);
    g_error_free (gerror);
    return (GST_FLOW_ERROR);
  }
  // Is VPU asking for more buffers than our plugin can allocated?
  if (G_UNLIKELY
      (vpu_dec->initialInfo->minFrameBufferCount > NUM_MAX_VPU_REQUIRED)) {
    GST_ERROR
        (">>VPU_DEC: required frames number exceed max limitation, required %d.",
        vpu_dec->initialInfo->minFrameBufferCount);
    return (GST_FLOW_ERROR);
  }

  if (!mfw_gst_vpudec_mjpeg_is_supported (vpu_dec)) {
    GST_ERROR (">>VPU_DEC: Not supported MJPEG output format:%d.",
        vpu_dec->initialInfo->mjpg_sourceFormat);
    return GST_FLOW_ERROR;
  }

  needFrameBufCount =
      vpu_dec->initialInfo->minFrameBufferCount + EXT_BUFFER_NUM + 2;

  if (STD_MJPG == vpu_dec->codec) {
    if (FALSE == mfw_gst_vpudec_check_allowed_caps (vpu_dec)) {
      vpu_dec->init_fail_cnt = 100;
      GST_ERROR ("Caps negotiation failed");
      return (GST_FLOW_NOT_NEGOTIATED);
    }

    /* close VPU and will be reopen later */
    /* since IPU does not support NV16 or NV24, try to reopen the VPU as I420 */
    if ((GST_VIDEO_FORMAT_Y42B == vpu_dec->video_format) ||
        (GST_VIDEO_FORMAT_Y444 == vpu_dec->video_format)) {
      if (vpu_dec->fmt == 0) {
        GST_WARNING ("Do not support NV16 and ?NV24? mode");
        vpu_dec->fmt = 1;
        vpu_ret = vpu_DecClose (*vpu_dec->handle);
        if (vpu_ret != RETCODE_SUCCESS) {
          GST_ERROR (">>>VPU_DEC: vpu_DecClose failed");
          /* fatal error, no more retry */
          vpu_dec->init_fail_cnt = 100;
        } else {
          vpu_dec->vpu_opened = FALSE;
        }
        return (GST_FLOW_ERROR);
      }
    }
  }

  /* Padding the width and height to 16 */
  if (!vpu_dec->eos) {
    GstCaps *caps = NULL;
    gint crop_top_len, crop_left_len;
    gint crop_right_len, crop_bottom_len;
    gint orgPicW = vpu_dec->initialInfo->picWidth;
    gint orgPicH = vpu_dec->initialInfo->picHeight;
    guint numBufs = (vpu_dec->rotation_angle || vpu_dec->mirror_dir
        || (vpu_dec->codec == STD_MJPG)) ? 2 : needFrameBufCount;
    guint align_h = 0 == vpu_dec->buf_alignment_h ?
        DEFAULT_FRAME_BUFFER_ALIGNMENT_H : vpu_dec->buf_alignment_h;
    guint align_v = 0 == vpu_dec->buf_alignment_v ?
        DEFAULT_FRAME_BUFFER_ALIGNMENT_V : vpu_dec->buf_alignment_v;
    guint align_v_field = align_v * 2;

    vpu_dec->initialInfo->picWidth =
        (vpu_dec->initialInfo->picWidth + align_h - 1) / align_h * align_h;

    if ((vpu_dec->initialInfo->interlace)
        && ((vpu_dec->codec == STD_MPEG2) || (vpu_dec->codec == STD_VC1)
            || (vpu_dec->codec == STD_AVC))) {
      vpu_dec->initialInfo->picHeight =
          (vpu_dec->initialInfo->picHeight + align_v_field - 1) /
          align_v_field * align_v_field;
    } else {
      vpu_dec->initialInfo->picHeight =
          (vpu_dec->initialInfo->picHeight + align_v - 1) / align_v * align_v;
    }

    GST_DEBUG ("crop info:%d,%d,%d,%d", vpu_dec->initialInfo->picCropRect.left,
        vpu_dec->initialInfo->picCropRect.right,
        vpu_dec->initialInfo->picCropRect.top,
        vpu_dec->initialInfo->picCropRect.bottom);

    if (vpu_dec->codec == STD_AVC) {

      crop_top_len = vpu_dec->initialInfo->picCropRect.top;
      crop_left_len = vpu_dec->initialInfo->picCropRect.left;

      if (vpu_dec->initialInfo->picCropRect.right > 0) {
        crop_right_len = vpu_dec->initialInfo->picWidth -
            vpu_dec->initialInfo->picCropRect.right;
      } else {
        crop_right_len = vpu_dec->initialInfo->picWidth - orgPicW;
      }

      if (vpu_dec->initialInfo->picCropRect.bottom > 0) {
        crop_bottom_len = vpu_dec->initialInfo->picHeight -
            vpu_dec->initialInfo->picCropRect.bottom;
      } else {
        crop_bottom_len = vpu_dec->initialInfo->picHeight - orgPicH;
      }
    } else {
      crop_top_len = 0;
      crop_left_len = 0;
      crop_right_len = vpu_dec->initialInfo->picWidth - orgPicW;
      crop_bottom_len = vpu_dec->initialInfo->picHeight - orgPicH;

    }

    if ((crop_left_len + crop_right_len > vpu_dec->initialInfo->picWidth - 2) ||
        (crop_top_len + crop_bottom_len >
            vpu_dec->initialInfo->picHeight - 2)) {
      GST_WARNING
          ("Wrong cropping parameters : top(%d), left(%d), right(%d), bottom (%d). Set all to 0",
          crop_top_len, crop_left_len, crop_right_len, crop_bottom_len);
      crop_top_len = 0;
      crop_left_len = 0;
      crop_right_len = 0;
      crop_bottom_len = 0;
    }


    /*
     * FIXME: Force to apply IC-bypass method
     *  which could cause mismatch issue.
     */
    if (vpu_dec->initialInfo->picHeight == 1088)
      crop_bottom_len = 8;

    if (GST_VIDEO_FORMAT_Y42B == vpu_dec->video_format) {
      vpu_dec->yuv_frame_size =
          vpu_dec->initialInfo->picWidth * vpu_dec->initialInfo->picHeight * 2;
    } else if (GST_VIDEO_FORMAT_Y444 == vpu_dec->video_format) {
      vpu_dec->yuv_frame_size =
          vpu_dec->initialInfo->picWidth * vpu_dec->initialInfo->picHeight * 3;
    } else
      vpu_dec->yuv_frame_size = (vpu_dec->initialInfo->picWidth *
          vpu_dec->initialInfo->picHeight * 3) / 2;



    vpu_dec->width = ((vpu_dec->rotation_angle == 90)
        || (vpu_dec->rotation_angle ==
            270)) ? vpu_dec->initialInfo->picHeight : vpu_dec->initialInfo->
        picWidth;

    vpu_dec->height = ((vpu_dec->rotation_angle == 90)
        || (vpu_dec->rotation_angle ==
            270)) ? vpu_dec->initialInfo->picWidth : vpu_dec->initialInfo->
        picHeight;

    /* calculate pixel aspect ratio */
    mfw_gst_vpudec_get_par (vpu_dec);
    /* set the capabilites on the source pad */
    format_fourcc = mfw_vpu_video_format_to_fourcc (vpu_dec->video_format);
    caps = gst_caps_new_simple ("video/x-raw-yuv",
        "format", GST_TYPE_FOURCC,
        format_fourcc,
        "width", G_TYPE_INT,
        vpu_dec->width,
        "height", G_TYPE_INT,
        vpu_dec->height,
        "width_align", G_TYPE_INT,
        vpu_dec->buf_alignment_h,
        "height_align", G_TYPE_INT,
        vpu_dec->buf_alignment_v,
        "framerate", GST_TYPE_FRACTION,
        vpu_dec->frame_rate_nu,
        vpu_dec->frame_rate_de,
        "pixel-aspect-ratio", GST_TYPE_FRACTION,
        vpu_dec->par_width,
        vpu_dec->par_height,
        CAPS_FIELD_CROP_TOP, G_TYPE_INT, (((vpu_dec->rotation_angle == 90)
                || (vpu_dec->rotation_angle ==
                    270)) ? (crop_left_len) :
            (crop_top_len)),
        CAPS_FIELD_CROP_LEFT, G_TYPE_INT, (((vpu_dec->rotation_angle == 90)
                || (vpu_dec->rotation_angle ==
                    270)) ? (crop_top_len) :
            (crop_left_len)),
        CAPS_FIELD_CROP_RIGHT, G_TYPE_INT, (((vpu_dec->rotation_angle == 90)
                || (vpu_dec->rotation_angle ==
                    270)) ? (crop_bottom_len) :
            (crop_right_len)),
        CAPS_FIELD_CROP_BOTTOM, G_TYPE_INT, (((vpu_dec->rotation_angle == 90)
                || (vpu_dec->rotation_angle ==
                    270)) ? (crop_right_len) :
            (crop_bottom_len)),
        CAPS_FIELD_REQUIRED_BUFFER_NUMBER, G_TYPE_INT,
        needFrameBufCount, "field", G_TYPE_INT, 0, NULL);

#if GST_CHECK_VERSION(0, 10, 35)
    GST_INFO("Try set downstream caps %" GST_PTR_FORMAT, caps);
#else
    {
      gchar * caps_str = gst_caps_to_string(caps);
      if (caps_str) {
        GST_INFO("Try set downstream caps %s", caps_str);
        g_free(caps_str);
      }
    }
#endif

#if 0
    if (vpu_dec->frame_drop_allowed) {  // set the max lateness to a higher number
      gst_caps_set_simple (caps, "sfd", G_TYPE_INT, 0, NULL);
    }
#endif
    if (!(gst_pad_set_caps (vpu_dec->srcpad, caps))) {
      GST_ERROR (">>VPU_DEC: Could not set the caps for src pad");
    }
    gst_caps_unref (caps);
    caps = NULL;
  }



  /* Allocate the Frame buffers requested by the Decoder if not done already */

  if (vpu_dec->framebufinit_done == FALSE) {
    if ((mfw_gst_vpudec_FrameBufferInit (vpu_dec, vpu_dec->frameBuf,
                needFrameBufCount)) < 0) {
      GST_ERROR
          (">>VPU_DEC: Frame Buffer Init Memory system allocation failed!");
      mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
          "Allocation of the Frame Buffers Failed");

      return (GST_FLOW_ERROR);
    }
    vpu_dec->framebufinit_done = TRUE;
  } else {
    GST_WARNING ("Init framebuffer");
  }

  // This is needed for H.264 only
  memset (&bufinfo, 0, sizeof (bufinfo));

#if (VPU_LIB_VERSION_CODE>=VPU_LIB_VERSION(5, 3, 0))
  bufinfo.avcSliceBufInfo.bufferBase = vpu_dec->slice_mem_desc.phy_addr;
  bufinfo.avcSliceBufInfo.bufferSize = SLICE_SAVE_SIZE;
#else
  bufinfo.avcSliceBufInfo.sliceSaveBuffer = vpu_dec->slice_mem_desc.phy_addr;
  bufinfo.avcSliceBufInfo.sliceSaveBufferSize = SLICE_SAVE_SIZE;
#endif

  // Updated since firmware 1.4.14
  bufinfo.maxDecFrmInfo.maxMbX = vpu_dec->initialInfo->picWidth / 16;
  bufinfo.maxDecFrmInfo.maxMbY = vpu_dec->initialInfo->picHeight / 16;
  bufinfo.maxDecFrmInfo.maxMbNum =
      vpu_dec->initialInfo->picHeight * vpu_dec->initialInfo->picWidth / 256;
  // Register the Allocated Frame buffers with the decoder
  // for rotation - only register minimum as extra two buffers
  //                will be used for display separately outside of VPU

  vpu_ret = vpu_DecRegisterFrameBuffer (*(vpu_dec->handle),
      vpu_dec->frameBuf,
      (vpu_dec->rotation_angle
          || vpu_dec->mirror_dir
          || (vpu_dec->codec ==
              STD_MJPG)) ?
      vpu_dec->initialInfo->
      minFrameBufferCount : vpu_dec->
      numframebufs, vpu_dec->initialInfo->picWidth, &bufinfo);
  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR
        (">>VPU_DEC: vpu_DecRegisterFrameBuffer failed. Error code is %d ",
        vpu_ret);
    mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
        "Registration of the Allocated Frame Buffers Failed ");
    return (GST_FLOW_ERROR);
  }
  // Setup rotation or mirroring which will be output to separate buffers for display
  if (vpu_dec->rotation_angle || vpu_dec->mirror_dir
      || (vpu_dec->codec == STD_MJPG)) {
    int rotStride = vpu_dec->initialInfo->picWidth;
    vpu_dec->allow_parallelization = FALSE;

    if (vpu_dec->rotation_angle) {
      // must set angle before rotator stride since the stride uses angle
      // or error checking
      vpu_ret = vpu_DecGiveCommand (*(vpu_dec->handle),
          SET_ROTATION_ANGLE, &vpu_dec->rotation_angle);

      // Do a 90 degree rotation - buffer is allocated in FrameBufferInit at end
      // for rotation, set stride, rotation angle and initial buffer output
      if ((vpu_dec->rotation_angle == 90)
          || (vpu_dec->rotation_angle == 270))
        rotStride = vpu_dec->initialInfo->picHeight;
    }
    if (vpu_dec->mirror_dir) {
      vpu_ret = vpu_DecGiveCommand (*(vpu_dec->handle),
          SET_MIRROR_DIRECTION, &vpu_dec->mirror_dir);
    }
    vpu_ret =
        vpu_DecGiveCommand (*(vpu_dec->handle), SET_ROTATOR_STRIDE, &rotStride);
    if (vpu_ret != RETCODE_SUCCESS) {
      GST_ERROR (">>VPU_DEC: SET_ROTATOR_STRIDE failed. ret=%d ", vpu_ret);
      mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
          "VPU SET_ROTATOR_STRIDE failed ");
      return (GST_FLOW_ERROR);
    }
    vpu_dec->rot_buff_idx = vpu_dec->initialInfo->minFrameBufferCount;
    vpu_ret = vpu_DecGiveCommand (*(vpu_dec->handle), SET_ROTATOR_OUTPUT,
        &vpu_dec->frameBuf[vpu_dec->rot_buff_idx]);
    if (vpu_dec->rotation_angle)
      vpu_ret = vpu_DecGiveCommand (*(vpu_dec->handle), ENABLE_ROTATION, 0);
    if (vpu_dec->mirror_dir)
      vpu_ret = vpu_DecGiveCommand (*(vpu_dec->handle), ENABLE_MIRRORING, 0);
  }
  /* end rotation and mirroring setup */
  /* Always disable preScan mode in the default */
  vpu_dec->decParam->prescanEnable = 0;
  if (vpu_dec->parser_input == FALSE)
    vpu_dec->decParam->prescanEnable = 1;

  if (vpu_dec->initialInfo->picWidth >= 720)
    vpu_dec->min_data_in_vpu = MIN_DATA_IN_VPU_720P;
  else if (vpu_dec->initialInfo->picWidth >= 640)
    vpu_dec->min_data_in_vpu = MIN_DATA_IN_VPU_VGA;
  else if (vpu_dec->initialInfo->picWidth >= 320)
    vpu_dec->min_data_in_vpu = MIN_DATA_IN_VPU_QVGA;
  else
    vpu_dec->min_data_in_vpu = MIN_DATA_IN_VPU_QCIF;

  if (vpu_dec->dbk_enabled) {
    DbkOffset dbkoffset;
    dbkoffset.DbkOffsetEnable = 1;
    dbkoffset.DbkOffsetA = vpu_dec->dbk_offset_a;
    dbkoffset.DbkOffsetB = vpu_dec->dbk_offset_b;

    vpu_DecGiveCommand (*(vpu_dec->handle), SET_DBK_OFFSET, &dbkoffset);
  } else {
    DbkOffset dbkoffset;
    dbkoffset.DbkOffsetEnable = 0;
    dbkoffset.DbkOffsetA = 0;
    dbkoffset.DbkOffsetB = 0;

    vpu_DecGiveCommand (*(vpu_dec->handle), SET_DBK_OFFSET, &dbkoffset);
  }

  GST_DEBUG (">>VPU_DEC: VPU Initialization complete ");
  vpu_dec->vpu_init = TRUE;
  vpu_dec->flushing = FALSE;
  return GST_FLOW_OK;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_prep_gstbuf

DESCRIPTION:        Prepare gstbuffer for push purpose.

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       gboolean - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
gboolean
mfw_gst_vpudec_prep_gstbuf (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  GstFlowReturn retval = TRUE;


  if (vpu_dec->outputInfo->indexFrameDisplay < 0)
    return FALSE;

  if (vpu_dec->flushing) {
    vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDisplay] =
        FB_STATE_PENDING;
    return FALSE;
  }

  if (G_LIKELY (vpu_dec->direct_render == TRUE)) {
    if (vpu_dec->rotation_angle || vpu_dec->mirror_dir
        || (vpu_dec->codec == STD_MJPG)) {
      vpu_dec->pushbuff = vpu_dec->outbuffers[vpu_dec->rot_buff_idx];
      gst_buffer_ref (vpu_dec->pushbuff);
      // switch output buffer for every other frame so we don't overwrite display
      // data in v4lsink this way VPU can still decode while v4l sink is displaying
      if (vpu_dec->rot_buff_idx == vpu_dec->initialInfo->minFrameBufferCount)
        vpu_dec->rot_buff_idx = vpu_dec->initialInfo->minFrameBufferCount + 1;
      else
        vpu_dec->rot_buff_idx = vpu_dec->initialInfo->minFrameBufferCount;
      vpu_DecGiveCommand (*(vpu_dec->handle), SET_ROTATOR_OUTPUT,
          &vpu_dec->frameBuf[vpu_dec->rot_buff_idx]);
      vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDisplay] =
          FB_STATE_PENDING;
    } else {
      // The HW case in DR mode
      if (vpu_dec->fb_type[vpu_dec->outputInfo->indexFrameDisplay] ==
          FB_TYPE_HW) {
        retval =
            gst_pad_alloc_buffer_and_set_caps (vpu_dec->srcpad, 0,
            vpu_dec->
            yuv_frame_size, GST_PAD_CAPS (vpu_dec->srcpad), &vpu_dec->pushbuff);
        if (retval != GST_FLOW_OK) {
          GST_ERROR
              (">>VPU_DEC: Error %d in allocating the Framebuffer[%d]",
              retval, vpu_dec->outputInfo->indexFrameDisplay);
          return retval;
        }
        if (GST_BUFFER_FLAG_IS_SET
            (vpu_dec->pushbuff, GST_BUFFER_FLAG_LAST) == TRUE) {
          GST_DEBUG (">>VPU_DEC: wrong here, should not be a HW buffer?");
        } else {
          GST_DEBUG (">>VPU_DEC: Get a software buffer from V4L sink.");
          memcpy (GST_BUFFER_DATA (vpu_dec->pushbuff),
              vpu_dec->frame_virt[vpu_dec->outputInfo->
                  indexFrameDisplay], vpu_dec->yuv_frame_size);
        }
        vpu_dec->fb_state_plugin[vpu_dec->outputInfo->
            indexFrameDisplay] = FB_STATE_PENDING;
      } else {
        vpu_dec->pushbuff =
            vpu_dec->outbuffers[vpu_dec->outputInfo->indexFrameDisplay];
        if (vpu_dec->pushbuff) {
          gst_buffer_set_caps (vpu_dec->pushbuff,
              GST_PAD_CAPS (vpu_dec->srcpad));


          gst_buffer_ref (vpu_dec->pushbuff);

          vpu_dec->fb_state_plugin[vpu_dec->outputInfo->
              indexFrameDisplay] = FB_STATE_DISPLAY;
        }
      }
    }
  } else {
    // Incase of the Filesink the output in the hardware buffer is copied onto the
    // buffer allocated by filesink
    retval = gst_pad_alloc_buffer_and_set_caps (vpu_dec->srcpad, 0,
        vpu_dec->yuv_frame_size,
        GST_PAD_CAPS (vpu_dec->srcpad), &vpu_dec->pushbuff);
    if (retval != GST_FLOW_OK) {
      GST_ERROR (">>VPU_DEC: Error %d allocating the Framebuffer[%d]",
          retval, vpu_dec->outputInfo->indexFrameDisplay);
      return retval;
    }
    retval = TRUE;
    memcpy (GST_BUFFER_DATA (vpu_dec->pushbuff),
        GST_BUFFER_DATA(vpu_dec->outbuffers[vpu_dec->outputInfo->indexFrameDisplay]),
        vpu_dec->yuv_frame_size);
    vpu_ret =
        vpu_DecClrDispFlag (*(vpu_dec->handle),
        vpu_dec->outputInfo->indexFrameDisplay);
    vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDisplay] =
        FB_STATE_ALLOCATED;
  }

  // Update the time stamp base on the frame-rate
  GST_BUFFER_SIZE (vpu_dec->pushbuff) = vpu_dec->yuv_frame_size;
  GST_BUFFER_DURATION (vpu_dec->pushbuff) = vpu_dec->time_per_frame;

  {
    GstClockTime ts;
#if 0
    if ((vpu_dec->rate >= 0) && (vpu_dec->codec != STD_MPEG2)
        && (vpu_dec->codec != STD_RV)) {
      mfw_gst_get_timestamp (vpu_dec, &ts);
    } else
#endif
    {
      ts = TSManagerSend (vpu_dec->pTS_Mgr);
    }
    GST_BUFFER_TIMESTAMP (vpu_dec->pushbuff) = ts;
  }
#if 0
// only reference the gstreamer buffers that we allocated through GST
  if (vpu_dec->direct_render &&
      (vpu_dec->fb_type[vpu_dec->outputInfo->indexFrameDisplay] ==
          FB_TYPE_GST)) {
    g_print ("ref some %p \n", vpu_dec->pushbuff);
    gst_buffer_ref (vpu_dec->pushbuff);
  }
#endif
  // Push the buffer downstream
  mfw_vpudec_set_field (vpu_dec, vpu_dec->pushbuff);

  return retval;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_render

DESCRIPTION:        Render one frame if conditions allow

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpudec_render (MfwGstVPU_Dec * vpu_dec)
{
  GstFlowReturn retval = GST_FLOW_OK;
  if (vpudec_global_ptr == NULL)
    return GST_FLOW_OK;

  if ((vpu_dec->frame_drop_allowed) && (vpu_dec->skipmode > SKIP_NONE)) {
    RetCode vpu_ret = RETCODE_SUCCESS;
    gint factor = 0;
    gint idx;

    switch (vpu_dec->skipmode) {
      case SKIP_L2:
      case SKIP_BP:
        factor = 0x3;
        break;
      default:
        factor = 0x7;
        break;

    }
    if (factor < 0x1) {
      GST_WARNING ("something wrong for the factor.");
    } else
      GST_DEBUG ("frame dropping factor is %d.", factor);

    if ((vpu_dec->frames_decoded & factor) == factor) {
      idx = vpu_dec->outputInfo->indexFrameDisplay;

      if ((vpu_dec->fb_state_plugin[idx] == FB_STATE_PENDING) ||
          (vpu_dec->fb_state_plugin[idx] == FB_STATE_FREE) ||
          ((vpu_dec->fb_state_plugin[idx] = FB_STATE_DISPLAY)
              && (vpu_dec->codec != STD_VC1))) {
        /* Drop the frame before render it. */
        GST_DEBUG ("Drop the frame before render it:%d.status:%d",
            vpu_dec->outputInfo->indexFrameDisplay,
            vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDisplay]);
        vpu_ret =
            vpu_DecClrDispFlag (*(vpu_dec->handle),
            vpu_dec->outputInfo->indexFrameDisplay);
        if (vpu_ret != RETCODE_SUCCESS) {
          GST_ERROR
              (">>VPU_DEC: vpu_DecClrDispFlag failed Error is %d disp buff idx=%d handle=0x%x",
              vpu_ret, vpu_dec->outputInfo->indexFrameDisplay, vpu_dec->handle);
        }
        vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDisplay] =
            FB_STATE_ALLOCATED;
      }

      gst_buffer_unref (vpu_dec->pushbuff);

      return GST_FLOW_OK;
    }

  }


  retval = gst_pad_push (vpu_dec->srcpad, vpu_dec->pushbuff);

  if (retval != GST_FLOW_OK) {
    GST_ERROR
        (">>VPU_DEC: Error %d Pushing Output onto the Source Pad, idx=%d ",
        retval, vpu_dec->outputInfo->indexFrameDisplay);

  } else {
    GST_DEBUG (">>VPU_DEC: Render buff %d, total:%d",
        vpu_dec->outputInfo->indexFrameDisplay, vpu_dec->frames_rendered);
    vpu_dec->frames_rendered++;
  }
  return retval;
}



/*======================================================================================
FUNCTION:           mfw_gst_vpudec_continue_looping

DESCRIPTION:        This routine checks various parameters to see if we should
                    keep looping

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context
                    frame_started - if chain was entered with a decode pending
                          then we might want to loop to start a new frame before exiting
                          since all we got was output of last frame and no new decode started

RETURN VALUE:       gboolean - continue looping is TRUE otherwise return FALSE .
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
gboolean
mfw_gst_vpudec_continue_looping (MfwGstVPU_Dec * vpu_dec, guint loop_cnt)
{
  gboolean fContinue = FALSE;
  RetCode vpu_ret = RETCODE_SUCCESS;
  if (vpudec_global_ptr == NULL)
    return FALSE;

  vpu_dec->decParam->chunkSize = 0;

  if ((vpu_dec->file_play_mode == FALSE) && (loop_cnt < 3))
    fContinue = TRUE;

  // keep looping on EOS until VPU fails with EOS
  if (vpu_dec->outputInfo->mp4PackedPBframe == 1) {
    fContinue = TRUE;
  } else if (vpu_dec->flushing) {
    return FALSE;
  } else if (G_UNLIKELY (vpu_dec->eos)) {
    if ((vpu_dec->outputInfo->indexFrameDisplay == -1) &&
        (vpu_dec->outputInfo->indexFrameDecoded == -2)) {
      return FALSE;
    }
    // in some cases at eos VPU can get in an infinite loop
    if ((vpu_dec->outputInfo->indexFrameDisplay == -1) ||
        (vpu_dec->outputInfo->indexFrameDecoded == -1) &&
        (loop_cnt > vpu_dec->numframebufs)) {
      return FALSE;
    }

    if ((vpu_dec->outputInfo->indexFrameDisplay == -3) &&
        (vpu_dec->outputInfo->indexFrameDecoded == -2) &&
        (vpu_dec->decParam->skipframeMode == 0) &&
        (loop_cnt > vpu_dec->numframebufs)) {
      return FALSE;
    }

    fContinue = TRUE;
  }
#ifndef VPU_THREAD
  else if (vpu_dec->outputInfo->indexFrameDisplay == -3
      && (vpu_dec->outputInfo->indexFrameDecoded >= 0)
      && (vpu_dec->codec == STD_VC1)) {
    fContinue = FALSE;
  } else {
    // for file play mode after seeking or interlaced sometimes need a 2nd decode to get
    // positive display index to turn off iframesearchenable
    // happens most often with VC1 which will return -3 for disp and positive decode index after seek sometimes
    // add vc-1 advance profile support, if it is vc-1, indexFrameDisplay = -3  indexFrameDecoded = 0, return false
    if ((loop_cnt < 3) &&       // avoid infinite loop - usually only one loop is needed
        (vpu_dec->outputInfo->indexFrameDisplay < 0) &&
        ((vpu_dec->outputInfo->indexFrameDecoded >= 0)
            || (vpu_dec->outputInfo->indexFrameDecoded == -2))) {
      fContinue = TRUE;
    }
  }
#endif
  if (vpu_dec->loopback) {
    fContinue = FALSE;
  }
  // this is to avoid infinite loops where it might keep looping when it should not or flush is waiting
  if ((vpu_dec->outputInfo->indexFrameDisplay < 0) && !vpu_dec->eos
      && (loop_cnt > vpu_dec->initialInfo->minFrameBufferCount)) {
    fContinue = FALSE;
  }

  GST_FRAMEDBG (">>VPU_DEC: Continue Loop returns %d\n", fContinue);
  return fContinue;
}


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_no_display

DESCRIPTION:        Releases a buffer back to VPU since it will not be displayed

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context
                    idx - index of buffer to be released

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void
mfw_gst_vpudec_no_display (MfwGstVPU_Dec * vpu_dec)
{
#if  0
  if (vpu_dec->TimeStamp_Object.ts_tx != vpu_dec->TimeStamp_Object.ts_rx)
    vpu_dec->TimeStamp_Object.ts_tx =
        (vpu_dec->TimeStamp_Object.ts_tx + 1) % MAX_STREAM_BUF;

  if (vpu_dec->frames_rendered)
    vpu_dec->frames_dropped++;

  if (vpu_dec->just_flushed)
    vpu_dec->last_ts_sent += vpu_dec->time_per_frame;
#else
  TSManagerSend (vpu_dec->pTS_Mgr);
  if (vpu_dec->frames_rendered)
    vpu_dec->frames_dropped++;
  if (vpu_dec->just_flushed) {
    // GstClockTime ts;
    // ts = _mfw_gst_get_last_ts(vpu_dec->pTS_Mgr);
    // _mfw_gst_set_last_ts(vpu_dec->pTS_Mgr, (ts+_mfw_gst_get_ts_tpf(vpu_dec->pTS_Mgr)));
  }
#endif
  if ((vpu_dec->outputInfo->indexFrameDisplay >= 0) &&
      (vpu_dec->outputInfo->indexFrameDisplay < vpu_dec->numframebufs)) {
    vpu_dec->fb_state_plugin[vpu_dec->outputInfo->indexFrameDisplay] =
        FB_STATE_PENDING;
  }
}

/*======================================================================================
FUNCTION:           mfw_vpudec_set_field

DESCRIPTION:        Set the field info for interlaced content

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void
mfw_vpudec_get_field (MfwGstVPU_Dec * vpu_dec)
{
  vpu_dec->field = FIELD_NONE;

  /* Set the field information */
  if (vpu_dec->codec == STD_VC1) {
    GST_INFO ("FRAME_INTERLACE:%d", vpu_dec->outputInfo->pictureStructure);

    if (vpu_dec->outputInfo->pictureStructure == 2)
      GST_INFO ("FRAME_INTERLACE");
    else if (vpu_dec->outputInfo->pictureStructure == 3) {
      if (vpu_dec->outputInfo->topFieldFirst)
        vpu_dec->field = FIELD_INTERLACED_TB;
      else
        vpu_dec->field = FIELD_INTERLACED_BT;

    }
    if (vpu_dec->outputInfo->vc1_repeatFrame)
      GST_INFO ("dec_idx %d : VC1 RPTFRM [%1d]",
          vpu_dec->frames_decoded, vpu_dec->outputInfo->vc1_repeatFrame);
  } else if (vpu_dec->codec == STD_AVC) {
    if (vpu_dec->outputInfo->interlacedFrame) {
      if (vpu_dec->outputInfo->topFieldFirst)
        vpu_dec->field = FIELD_INTERLACED_TB;
      else
        vpu_dec->field = FIELD_INTERLACED_BT;
      GST_INFO ("Top Field First flag: %d, dec_idx %d",
          vpu_dec->outputInfo->topFieldFirst, vpu_dec->frames_decoded);
    }
  } else if ((vpu_dec->codec != STD_MPEG4) && (vpu_dec->codec != STD_RV)) {
    if (vpu_dec->outputInfo->interlacedFrame
        || !vpu_dec->outputInfo->progressiveFrame) {
      if (vpu_dec->outputInfo->pictureStructure == 1)
        vpu_dec->field = FIELD_TOP;
      else if (vpu_dec->outputInfo->pictureStructure == 2)
        vpu_dec->field = FIELD_BOTTOM;
      else if (vpu_dec->outputInfo->pictureStructure == 3) {
        if (vpu_dec->outputInfo->topFieldFirst)
          vpu_dec->field = FIELD_INTERLACED_TB;
        else
          vpu_dec->field = FIELD_INTERLACED_BT;
      }
    }
    if (vpu_dec->outputInfo->repeatFirstField)
      GST_INFO ("frame_idx %d : Repeat First Field", vpu_dec->frames_decoded);

  }

  return;
}

/*======================================================================================
FUNCTION:           mfw_vpudec_set_field

DESCRIPTION:        Set the field info for interlaced content

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void
mfw_vpudec_set_field (MfwGstVPU_Dec * vpu_dec, GstBuffer * buf)
{
  if (!buf)
    return;

  GstCaps *caps = GST_PAD_CAPS (vpu_dec->srcpad);
  GstCaps *newcaps;
  GstStructure *stru;
  gint field;
  stru = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (stru, "field", &field);
  if (field != vpu_dec->field) {
    newcaps = gst_caps_copy (caps);
    gst_caps_set_simple (newcaps, "field", G_TYPE_INT, vpu_dec->field, NULL);
    gst_buffer_set_caps (buf, newcaps);
    gst_caps_unref (newcaps);
    GST_INFO ("field:%d,%d", field, vpu_dec->field);
  }

  return;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_chain

DESCRIPTION:        The main processing function where the data comes in as buffer. This
                    data is decoded, and then pushed onto the next element for further
                    processing.

ARGUMENTS PASSED:   pad - pointer to the sinkpad of this element
                    buffer - pointer to the input buffer which has the H.264 data.

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static GstFlowReturn
mfw_gst_vpudec_chain (GstPad * pad, GstBuffer * buffer)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  GstFlowReturn retval = GST_FLOW_OK;
  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (GST_PAD_PARENT (pad));
  gboolean frame_started = vpu_dec->is_frame_started;
  gboolean fContinue = FALSE;
  gfloat time_val = 0;
  long time_before = 0, time_after = 0;
  guint gst_buffer_size = 0;
  gint ret = VPU_DEC_SUCCESS;

  // handle exit conditions
  if ((vpu_dec == NULL) || (vpudec_global_ptr == NULL))
    return GST_FLOW_OK;

  if (vpu_dec->flushing || vpu_dec->in_cleanup)
    return GST_FLOW_OK;

  /*
   *   Try lock works so that if it fails some other thread has it
   * and in that case we should not
   * proceed with any decodes - either flushing or cleaning up
   */
  if (vpu_mutex_lock (vpu_dec->vpu_mutex, TRUE) == FALSE) {
    GST_MUTEX (">>VPU_DEC: In chain but exiting since mutex is locked \
            by other thread or null mutex 0x%x\n", vpu_dec->vpu_mutex);
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  } else {
    GST_MUTEX (">>VPU_DEC: In chain mutex acquired cnt=%d\n", mutex_cnt);
  }

  vpu_dec->trymutex = TRUE;

  /*
   * Only drop non-i frames after vpu initialization.
   */
  if ((vpu_dec->vpu_init) && (buffer
          && GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT))) {
    if (vpu_dec->file_play_mode && (!GST_BUFFER_SIZE (buffer))) {
      GST_FRAMEDROP (">>VPU_DEC: Ignore delta before before I frame \n");
      vpu_dec->frames_dropped++;
      gst_buffer_unref (buffer);
      goto done;
    }
  }
  if (vpu_dec->flushing) {
    gst_buffer_unref (buffer);
    goto done;
  }

  vpu_dec->num_timeouts = 0;
  vpu_dec->loop_cnt = 0;
  vpu_dec->must_copy_data = TRUE;
  vpu_dec->buff_consumed = 0;

  MFW_WEAK_ASSERT (vpu_dec->gst_buffer == NULL);

  vpu_dec->gst_buffer = buffer;
  vpu_dec->new_ts = TRUE;

  // Update Profiling timestamps
  if (G_UNLIKELY (vpu_dec->profiling)) {
    gettimeofday (&vpu_dec->tv_prof2, 0);
  }
  // Open VPU if not already opened
  if (G_UNLIKELY (!vpu_dec->vpu_opened)) {
    if (STD_MJPG != vpu_dec->codec) {
      if (FALSE == mfw_gst_vpudec_check_allowed_caps (vpu_dec)) {
        mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
            "Caps negotiation failed");
        return (GST_FLOW_NOT_NEGOTIATED);
      }
    }

    retval = mfw_gst_vpudec_vpu_open (vpu_dec);
    if (retval != GST_FLOW_OK) {
      GST_ERROR
          (">>VPU_DEC: mfw_gst_vpudec_vpu_open failed. Error code is %d ",
          retval);
      goto done;
    }
  }
  // Initialize VPU and copy data
  if (G_UNLIKELY (vpu_dec->vpu_init == FALSE)) {

    // Write input bitstream to VPU - special for streaming mode

    retval = mfw_gst_vpudec_copy_sink_input (vpu_dec, buffer);
    if (retval != GST_FLOW_OK) {
      GST_ERROR
          (">>VPU_DEC: mfw_gst_vpudec_copy_sink_input failed - Error %d ",
          retval);
      goto done;
    }
    // if we have not processed our first frame than return
    // In networking mode we might check for headers before initializing VPU
    if (vpu_dec->firstFrameProcessed == FALSE) {
      GST_DEBUG (">>VPU_DEC: Error with first frame - return for more data");
      retval = GST_FLOW_OK;
      goto done;
    }
    // this is used to dump what we send to VPU to a file to analyze if problems in bitstream.
    // it does not pass anything to VPU but exits after copy was done above in copy_sink_input
    if (vpu_dec->passthru) {
      retval = GST_FLOW_OK;
      goto done;
    }
    if (!vpu_dec->file_play_mode &&
        (vpu_dec->data_in_vpu < 2048) && (vpu_dec->num_timeouts == 0)) {
      GST_DEBUG (">>VPU_DEC: init needs more data data_in_vpu=%d",
          vpu_dec->data_in_vpu);
      retval = GST_FLOW_OK;
      goto done;
    }
    retval = mfw_gst_vpudec_vpu_init (vpu_dec);
    if (retval != GST_FLOW_OK) {
      // in the case of MPEG2 keep trying for at least 100 times
      if ((vpu_dec->codec != STD_VC1) && (vpu_dec->init_fail_cnt < 100)) {
        GST_DEBUG
            (">>VPU_DEC: mfw_gst_vpudec_vpu_init failed but with MPEG2 keep asking for more data timeouts=%d, init fail cnt=%d",
            vpu_dec->num_timeouts, vpu_dec->init_fail_cnt);
        vpu_dec->num_timeouts++;
        retval = GST_FLOW_OK;
        goto done;
      }
      GST_ERROR (">>VPU_DEC: mfw_gst_vpudec_vpu_init failed ");
      mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
          "VPU Decoder Initialization failed ");
      goto done;
    }
    vpu_dec->num_timeouts = 0;
    fContinue = TRUE;
    vpu_dec->init_fail_cnt = 0;
  }
  // wait for headers
  if (vpu_dec->accumulate_hdr && vpu_dec->hdr_received) {
    if (vpu_dec->check_for_iframe || vpu_dec->just_flushed) {
      vpu_dec->check_for_iframe = FALSE;
      vpu_dec->just_flushed = FALSE;
      vpu_dec->decParam->iframeSearchEnable = 0;        // turn off iframesearch - i frame found
      vpu_dec->decParam->skipframeMode = 0;     // turn off skipframe used in networking
      vpu_dec->decParam->skipframeNum = 0;      // turn off skipframe used in networking
    }
  }

  // Keep looping while there is enough in bitstream to decode
  do {
    vpu_dec->loop_cnt++;

    if (vpudec_global_ptr == NULL)
      return GST_FLOW_OK;

    if (vpu_dec->flushing || vpu_dec->in_cleanup)
      goto done;

    // render a frame if decoding is completed
    if (vpu_dec->decoding_completed) {
      // render a frame
      if (mfw_gst_vpudec_prep_gstbuf (vpu_dec)) {

        vpu_mutex_unlock (vpu_dec->vpu_mutex);
        GST_MUTEX (">>VPU_DEC: unlock mutex before render cnt=%d\n", mutex_cnt);

        retval = mfw_gst_vpudec_render (vpu_dec);

        if (vpu_mutex_lock (vpu_dec->vpu_mutex, TRUE) == FALSE) {
          vpu_dec->trymutex = FALSE;
          GST_MUTEX
              (">>VPU_DEC: after render - no mutex lock cnt=%d\n", mutex_cnt);
          retval = GST_FLOW_ERROR;
        } else {
          GST_MUTEX
              (">>VPU_DEC: after render - mutex lock cnt=%d\n", mutex_cnt);
        }

      } else
        retval = GST_FLOW_OK;

      vpu_dec->decoding_completed = FALSE;

      // a pause could have locked the pipeline so could exit and vpu is dead so exit
      if (vpudec_global_ptr == NULL)
        return GST_FLOW_OK;

      if (retval != GST_FLOW_OK) {
        //GST_ERROR (">>VPU_DEC: Render frame failed \n");
        retval = GST_FLOW_OK;
        if (mutex_cnt && vpu_dec->must_copy_data && !vpu_dec->flushing
            && !vpu_dec->in_cleanup)
          goto check_continue;
        else
          goto done;
      }
    }

    // Start Decoding One frame in VPU if not already started
    if (!vpu_dec->is_frame_started && !vpu_dec->decoding_completed) {
      // copy data if still there or process eos
      if (vpu_dec->must_copy_data) {
        retval = mfw_gst_vpudec_copy_sink_input (vpu_dec, buffer);
        if (retval != GST_FLOW_OK) {
          GST_ERROR
              (">>VPU_DEC: mfw_gst_vpudec_copy_sink_input failed - Error %d ",
              retval);
          goto done;
        }
        if (vpu_dec->passthru) {
          goto done;
        }
      } else if (!vpu_dec->eos) {       // check if it is okay to start another frame before exiting
        if (vpu_dec->file_play_mode) {
          if (!fContinue)
            goto done;          // exit if continue flag was turned off
        } else {                // if streaming mode check if enough data to start another decode
          PhysicalAddress p1, p2;
          Uint32 space = vpu_dec->buffer_fill_size;
          vpu_ret =
              vpu_DecGetBitstreamBuffer (*(vpu_dec->handle), &p1, &p2, &space);
          vpu_dec->data_in_vpu = vpu_dec->buffer_fill_size - space;
          if (space == 0) {     // sometimes the buffer goes empty - in that case make sure to exit out
            retval = GST_FLOW_OK;
            goto done;
          }
        }
      }
      // Don't bother decoding in VPU if we don't have enough data and are not in eos
      if (!vpu_dec->file_play_mode && !vpu_dec->must_copy_data
          && !vpu_dec->loopback && !vpu_dec->eos
          && (vpu_dec->data_in_vpu < vpu_dec->min_data_in_vpu)) {
        GST_FRAMEDBG (">>VPU_DEC: Exiting chain to get more data %d\n",
            vpu_dec->data_in_vpu);
        retval = GST_FLOW_OK;
        goto done;
      }
      // this happens after reset - must re-init VPU again
      if (vpu_dec->vpu_init == FALSE) {
        retval = mfw_gst_vpudec_vpu_init (vpu_dec);
        if (retval != GST_FLOW_OK) {
          // in the case of MPEG2 keep trying for at least 100 times
          if ((vpu_dec->codec != STD_VC1) && (vpu_dec->init_fail_cnt < 100)) {
            GST_DEBUG
                (">>VPU_DEC: mfw_gst_vpudec_vpu_init failed but with MPEG2 keep asking for more data timeouts=%d",
                vpu_dec->num_timeouts);
            vpu_dec->num_timeouts++;
            retval = GST_FLOW_OK;
            goto done;
          }
          GST_ERROR (">>VPU_DEC: mfw_gst_vpudec_vpu_init failed ");
          mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
              "VPU Decoder Initialization failed ");
          goto done;
        }
        vpu_dec->num_timeouts = 0;
        vpu_dec->init_fail_cnt = 0;
      }
      // Begin Decoding of a Frame
      ret = vpu_thread_begin_decode (vpu_dec);
      retval = vpu_dec->retval;
      if ((ret == RELEASE_BUFF_FAILED) ||
          (retval == RETCODE_FRAME_NOT_COMPLETE)) {
        retval = GST_FLOW_OK;
        GST_WARNING ("No frame buffer is available");
        goto done;
      } else if ((retval != RETCODE_SUCCESS) || (retval != GST_FLOW_OK)) {
        GST_ERROR
            (">>VPU_DEC: vpu_DecStartOneFrame failed. Error code is %d ",
            retval);
        retval = GST_FLOW_ERROR;
        goto done;
      }
      // This is the parallelization hook - but it should not exit in the following cases
      // eos, unconsumed buffer, interlaced frame, packed frame
      //GST_FRAMEDBG(">>VPU_DEC: eos %d must_copy %d just_flushed %d\n", vpu_dec->eos, vpu_dec->must_copy_data,vpu_dec->just_flushed);
      if (vpu_dec->allow_parallelization && vpu_dec->is_frame_started &&
          !vpu_dec->eos && !fContinue && !vpu_dec->just_flushed &&
          !vpu_dec->must_copy_data &&
          /*   vpu_dec->state_playing &&  */// if paused we have to get the output
          (vpu_dec->outputInfo->mp4PackedPBframe == 0) && !INTERLACED_FRAME) {
        GST_FRAMEDBG (">>VPU_DEC: Parallelization exit \n");
        retval = GST_FLOW_OK;
        goto done;
      }
    }

    // check for output
    if (G_LIKELY (vpu_dec->is_frame_started == TRUE)) {
      gint ret = mfw_gst_vpu_dec_thread_get_output (vpu_dec, TRUE);
      retval = vpu_dec->retval;
      if (ret == VPU_DEC_DONE)
        goto done;
      if (ret == VPU_DEC_CONTINUE)
        goto check_continue;
    }
    // render a frame if decoding is completed
    if (vpu_dec->decoding_completed) {
      // render a frame
      if (mfw_gst_vpudec_prep_gstbuf (vpu_dec)) {

        vpu_mutex_unlock (vpu_dec->vpu_mutex);
        GST_MUTEX (">>VPU_DEC: unlock mutex before render cnt=%d\n", mutex_cnt);

        retval = mfw_gst_vpudec_render (vpu_dec);

        if (vpu_mutex_lock (vpu_dec->vpu_mutex, TRUE) == FALSE) {
          vpu_dec->trymutex = FALSE;
          GST_MUTEX
              (">>VPU_DEC: after render - no mutex lock cnt=%d\n", mutex_cnt);
          retval = GST_FLOW_ERROR;
        } else {
          GST_MUTEX
              (">>VPU_DEC: after render - mutex lock cnt=%d\n", mutex_cnt);
        }

      }

      vpu_dec->decoding_completed = FALSE;

      // a pause could have locked the pipeline so could exit and vpu is dead so exit
      if (vpudec_global_ptr == NULL)
        return GST_FLOW_OK;

      if (retval != GST_FLOW_OK) {

        retval = GST_FLOW_OK;
        if (mutex_cnt && vpu_dec->must_copy_data && !vpu_dec->flushing
            && !vpu_dec->in_cleanup)
          goto check_continue;
        else
          goto done;
      }
    }

  check_continue:
    retval = GST_FLOW_OK;
    fContinue = mfw_gst_vpudec_continue_looping (vpu_dec, vpu_dec->loop_cnt);
  } while (fContinue || vpu_dec->must_copy_data);

done:
#if 0
  if (!vpudec_global_ptr || vpu_dec->in_cleanup)
    return GST_FLOW_OK;
#endif
  if (vpu_dec->trymutex) {
    vpu_mutex_unlock (vpu_dec->vpu_mutex);
    GST_MUTEX (">>VPU_DEC: Unlocking mutex at end of chain cnt=%d\n",
        mutex_cnt);
  } else {
    GST_MUTEX
        (">>VPU_DEC: chain does not unlock mutex - might be cleaning up or flushing mutex_cnt=%d cleanup=%d\n",
        mutex_cnt, vpu_dec->in_cleanup);
  }

  if (G_UNLIKELY (vpu_dec->profiling)) {
    gettimeofday (&vpu_dec->tv_prof3, 0);
    time_before =
        (vpu_dec->tv_prof2.tv_sec * 1000000) + vpu_dec->tv_prof2.tv_usec;
    time_after =
        (vpu_dec->tv_prof3.tv_sec * 1000000) + vpu_dec->tv_prof3.tv_usec;
    vpu_dec->chain_Time += time_after - time_before;
  }
  // vc1 buffer join does an unref so can't do it in this case
  if (vpu_dec->gst_buffer != NULL) {
    gst_buffer_unref (vpu_dec->gst_buffer);
    vpu_dec->gst_buffer = NULL;
  }

  return retval;
}

/*=============================================================================
FUNCTION:   mfw_gst_vpudec_src_event

DESCRIPTION:

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -    event is handled properly
        FALSE      -    event is not handled properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_vpudec_src_event (GstPad * src_pad, GstEvent * event)
{

  gboolean res = TRUE;

  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (gst_pad_get_parent (src_pad));


  if ((vpudec_global_ptr == NULL) || vpu_dec->in_cleanup) {
    goto bail;
  }

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      res = gst_pad_push_event (vpu_dec->sinkpad, event);

      if (TRUE != res) {
        GST_DEBUG ("Error in pushing the event,result   is %d", res);
      }
      break;
    }
      /* Qos Event handle, trigger the strategy of frame dropping. */
    case GST_EVENT_QOS:
    {

      if (vpu_dec->frame_drop_allowed) {
        gdouble proportion;
        GstClockTimeDiff diff;
        GstClockTime timestamp;

        gst_event_parse_qos (event, &proportion, &diff, &timestamp);

        GST_VPU_QOS_EVENT_HANDLE (vpu_dec->skipmode, diff);
        /* Drop B strategy could cause playback not smooth, disable it */
        if ( /* (vpu_dec->skipmode == SKIP_B) || */ (vpu_dec->skipmode ==
                SKIP_BP))
          vpu_ena_skipframe (vpu_dec, vpu_dec->skipmode);
        else
          vpu_dis_skipframe (vpu_dec);

      }
      res = gst_pad_push_event (vpu_dec->sinkpad, event);
    }
      break;

    default:
      res = gst_pad_push_event (vpu_dec->sinkpad, event);
      break;
  }
bail:
  gst_object_unref (vpu_dec);
  return res;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_sink_event

DESCRIPTION:        This function handles the events the occur on the sink pad
                    Like EOS

ARGUMENTS PASSED:   pad - pointer to the sinkpad of this element
                    event - event generated.

RETURN VALUE:       TRUE   event handled success fully.
                    FALSE .event not handled properly

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static gboolean
mfw_gst_vpudec_sink_event (GstPad * pad, GstEvent * event)
{
  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (GST_PAD_PARENT (pad));
  gboolean result = TRUE;
  gint i = 0;
  guint height = 0, width = 0;
  RetCode vpu_ret = RETCODE_SUCCESS;
  gulong size = 0;
  gint flushsize = 0;
  guint offset = 0;

  if ((vpudec_global_ptr == NULL) || vpu_dec->in_cleanup)
    return GST_FLOW_OK;

  width = vpu_dec->initialInfo->picWidth;
  height = vpu_dec->initialInfo->picHeight;

  switch (GST_EVENT_TYPE (event)) {

    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gint64 start, stop, position;
      gdouble rate;
      gint i;

      gst_event_parse_new_segment (event, NULL, &rate, &format,
          &start, &stop, &position);
      GST_DEBUG (">>VPU_DEC: Receiving new seg: start = %" GST_TIME_FORMAT
          " stop = %" GST_TIME_FORMAT " position =% " GST_TIME_FORMAT,
          GST_TIME_ARGS (start), GST_TIME_ARGS (stop),
          GST_TIME_ARGS (position));

      vpu_dec->new_segment = 1;
      if ((rate <= 2.0) && (rate >= 0.0)) {
        vpu_dec->tsm_mode = MODE_AI;
      } else {
        vpu_dec->tsm_mode = MODE_FIFO;
      }
      vpu_dec->frames_decoded = 0;
      vpu_dec->frames_rendered = 0;
      //vpu_dec->just_flushed = FALSE;
      //vpu_dec->check_for_bframe = FALSE;

      if (GST_FORMAT_TIME == format) {
        result = gst_pad_push_event (vpu_dec->srcpad, event);
        if (TRUE != result) {
          GST_ERROR (">>VPU_DEC: Error in pushing the event %d", event);
        }
      } else
        GST_DEBUG
            (">>VPU_DEC: new seg event not pushed format = %d", (guint) format);
      GST_DEBUG (">>VPU_DEC: End New Seg event ");

      break;
    }

    case GST_EVENT_FLUSH_START:
    {
      GST_DEBUG (">>VPU_DEC: Receiving Flush start event ");
      if (vpu_dec->in_cleanup)
        return GST_FLOW_OK;

      GST_MUTEX (">>VPU_DEC: flush start before mutex_lock cnt=%d\n",
          mutex_cnt);
      vpu_mutex_lock (vpu_dec->vpu_mutex, FALSE);
      vpu_dec->flushing = TRUE;
      vpu_mutex_unlock (vpu_dec->vpu_mutex);
      GST_MUTEX (">>VPU_DEC: flush start after mutex_lock cnt=%d\n", mutex_cnt);

      if (!vpu_dec->vpu_init) {
        GST_DEBUG (">>VPU_DEC: Ignore flush since vpu not init ");
        vpu_mutex_lock (vpu_dec->vpu_mutex, FALSE);
        vpu_dec->flushing = FALSE;
        vpu_mutex_unlock (vpu_dec->vpu_mutex);
        result = gst_pad_push_event (vpu_dec->srcpad, event);
        return GST_FLOW_OK;
      }


      result = gst_pad_push_event (vpu_dec->srcpad, event);
      if (TRUE != result) {
        GST_ERROR (">>VPU_DEC: Error in pushing the event %d", event);
      }
      GST_DEBUG (">>VPU_DEC: End Flush start event ");
    }
      break;

    case GST_EVENT_FLUSH_STOP:
    {
      GST_DEBUG (">>VPU_DEC: Receiving Flush stop event ");
      gboolean fdoReset = FALSE;

      vpu_dec->data_in_vpu = 0;
      resyncTSManager (vpu_dec->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
      vpu_dec->num_timeouts = 0;

      if (!vpu_dec->vpu_init) {
        GST_DEBUG (">>VPU_DEC: Ignore flush stop since vpu not init ");
        vpu_dec->flushing = FALSE;
        result = gst_pad_push_event (vpu_dec->srcpad, event);
        return GST_FLOW_OK;
      }

      if (vpu_dec->is_frame_started) {
        gint ret;

        ret = mfw_gst_vpu_dec_thread_get_output (vpu_dec, FALSE);
        if (ret == VPU_DEC_DONE) {
          vpu_dec->is_frame_started = FALSE;
        } else {
          GST_WARNING ("vpu_DecGetOutputInfo() failed, fill zero to bistream");
          vpu_ret = vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle), 0);
          i = 0;
          fdoReset = TRUE;
          while (vpu_IsBusy ()) {
            //GST_DEBUG (">>VPU_DEC: waiting for VPU to get out of busy state \n");
            usleep (500000);
            i++;
            if (i > 10)
              break;
          }
          vpu_ret = mfw_gst_vpu_dec_thread_get_output (vpu_dec, FALSE);
          vpu_dec->is_frame_started = FALSE;
          GST_ERROR
              (">>VPU_DEC: Error in Flush getting output from vpu so flush bitstream buffer");
        }
      }
      // turn on i frame search so VPU will not return display buffers until i frame
      // this is very slow so we only use it on flushes and turn it off as soon as i frame apears
      // this helps us prevent garbage from being displayed after seeking
      //vpu_dec->check_for_bframe = TRUE;  // this is only used after flushing

      // put all buffers in pending free state to free later

      for (i = 0; i < vpu_dec->numframebufs; i++) {
        // this is because for display cases, in some cases the buffers are not unreffed
        // later we must unref if these show up as locked in release_buf - if we do not do
        // this eventually after a bunch of seeks VPU just stays locked up constantly returning
        // -3 for disp and -1 for decode (usually for VC1).
        if (vpu_dec->fb_state_plugin[i] == FB_STATE_DISPLAY);   //vpu_dec->fb_state_plugin[i] = FB_STATE_FREE;
        if (vpu_dec->fb_state_plugin[i] == FB_STATE_DECODED)
          vpu_dec->fb_state_plugin[i] = FB_STATE_PENDING;
      }

      if (!vpu_dec->eos) {
        vpu_dec->check_for_bframe = TRUE;       // this is only used after flushing
        vpu_dec->just_flushed = TRUE;
      }

      if (vpu_dec->file_play_mode == FALSE) {
        mfw_gst_vpudec_reset (vpu_dec);
      }

      /* use skip frame mode instead of i frame search to avoid
       * timestamp mismatch issue */
      //vpu_dec->decParam->iframeSearchEnable = 1;
      vpu_dec->decParam->skipframeMode = 1;     /* skip frames but I (IDR) frame */
      vpu_dec->decParam->skipframeNum = 1;      /* set to max */

      vpu_dec->outputInfo->indexFrameDecoded = 0;

      result = gst_pad_push_event (vpu_dec->srcpad, event);
      if (TRUE != result) {
        GST_ERROR (">>VPU_DEC: Error in pushing the event %d", event);
      }
      vpu_mutex_lock (vpu_dec->vpu_mutex, FALSE);
      vpu_dec->flushing = FALSE;
      vpu_mutex_unlock (vpu_dec->vpu_mutex);
      GST_DEBUG (">>VPU_DEC: End Flush stop event ");

      break;
    }
    case GST_EVENT_EOS:
    {
      GST_DEBUG (">>VPU_DEC: receiving EOS event ");
      /* Enable prescan mode in STREAM mode */
      vpu_dec->decParam->prescanEnable = (vpu_dec->file_play_mode) ? 0 : 1;
      if (!vpu_dec->flushing && (vpu_dec->codec != STD_MJPG)) {
        mfw_gst_vpudec_chain (vpu_dec->sinkpad, NULL);
      }
      result = gst_pad_push_event (vpu_dec->srcpad, event);
      if (result != TRUE) {
        GST_ERROR (">>VPU_DEC: Error in pushing EOS event %d", event);
      }
      break;
    }
    default:
    {

      GST_DEBUG (">>VPU_DEC: Event unhandled %d", event);

      result = gst_pad_event_default (pad, event);
      break;
    }

  }
  return result;

}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_cleanup

DESCRIPTION:        cleans up allocated memory and resources

ARGUMENTS PASSED:   vpu_dec  - VPU decoder plugins context

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void
mfw_gst_vpudec_cleanup (MfwGstVPU_Dec * vpu_dec)
{
  RetCode vpu_ret = RETCODE_SUCCESS;

  if (vpudec_global_ptr == NULL)
    return;

  vpu_dec->in_cleanup = TRUE;
  vpu_dec->start_addr = NULL;
  vpu_dec->end_addr = NULL;
  vpu_dec->base_addr = NULL;

  GST_DEBUG (">>>>VPU_DEC: Cleanup frame started %d",
      vpu_dec->is_frame_started);

  if (vpu_dec->is_frame_started) {
    // need this to avoid hangs on vpu close which will not close if outstanding
    // decode - best way to kill an oustanding decode is flush the bitstream buffer
    vpu_ret = vpu_DecUpdateBitstreamBuffer (*(vpu_dec->handle), 0);

    // if VPU is busy you can't cleanup and close will hang so be sure to set
    // bitstream to 0 so reset can happen
    while (vpu_IsBusy ()) {
      //GST_DEBUG (">>VPU_DEC: waiting for VPU to get out of busy state ");
      usleep (500000);
    }

    vpu_ret = mfw_gst_vpu_dec_thread_get_output (vpu_dec, FALSE);
    if (vpu_ret == RETCODE_SUCCESS) {
      vpu_dec->is_frame_started = FALSE;
    }
  }
#ifdef VPU_THREAD
  if (vpu_dec->vpu_thread) {
    mfw_gst_vpu_dec_thread_free (vpu_dec);
  }
#endif


  mfw_gst_vpudec_FrameBufferRelease (vpu_dec);

  if (vpu_dec->direct_render) {
    /* release framebuffers hold by vpu */
    int cnt;
    for (cnt = 0; cnt < vpu_dec->numframebufs; cnt++) {
#if 0                           //no need to unref, since vpu always hold the buffer reference.
      if (vpu_dec->outbuffers[cnt])
        gst_buffer_unref (vpu_dec->outbuffers[cnt]);
#endif
    }
  }

  vpu_dec->firstFrameProcessed = FALSE;
  resyncTSManager (vpu_dec->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
  vpu_dec->yuv_frame_size = 0;
  vpu_dec->direct_render = FALSE;
  vpu_dec->framebufinit_done = FALSE;

  if (vpu_dec->vpu_opened) {
    GST_DEBUG (">>>>VPU_DEC: Before close frame_started=%d vpu busy %d",
        vpu_dec->is_frame_started, vpu_IsBusy ());
    // if vpu is still in busy state this might hang but if you don't
    // do close then can't open another file after this.

    decoder_close (vpu_dec);
    vpu_dec->vpu_opened = FALSE;
    GST_DEBUG (">>>>VPU_DEC: After close");
  }

  if (vpu_dec->bit_stream_buf.phy_addr) {
    IOFreeVirtMem (&(vpu_dec->bit_stream_buf));
    IOFreePhyMem (&(vpu_dec->bit_stream_buf));

    vpu_dec->bit_stream_buf.phy_addr = 0;
  }
  if (vpu_dec->ps_mem_desc.phy_addr) {

    IOFreeVirtMem (&(vpu_dec->ps_mem_desc));
    IOFreePhyMem (&(vpu_dec->ps_mem_desc));
    vpu_dec->ps_mem_desc.phy_addr = 0;
  }

  if (vpu_dec->slice_mem_desc.phy_addr) {
    IOFreeVirtMem (&(vpu_dec->slice_mem_desc));
    IOFreePhyMem (&(vpu_dec->slice_mem_desc));
    vpu_dec->ps_mem_desc.phy_addr = 0;
  }
  if (vpu_dec->decOP != NULL) {
    MM_FREE (vpu_dec->decOP);
    vpu_dec->decOP = NULL;
  }

  if (vpu_dec->initialInfo != NULL) {
    MM_FREE (vpu_dec->initialInfo);
    vpu_dec->initialInfo = NULL;
  }
  if (vpu_dec->decParam != NULL) {
    MM_FREE (vpu_dec->decParam);
    vpu_dec->decParam = NULL;
  }
  if (vpu_dec->outputInfo != NULL) {
    MM_FREE (vpu_dec->outputInfo);
    vpu_dec->outputInfo = NULL;
  }
  if (vpu_dec->handle != NULL) {
    MM_FREE (vpu_dec->handle);
    vpu_dec->handle = NULL;
  }

  vpu_dec->clock_base = 0;
  GST_DEBUG (">>>>VPU_DEC: End Cleanup ");

}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_change_state

DESCRIPTION:        This function keeps track of different states of pipeline.

ARGUMENTS PASSED:
                element     -   pointer to element
                transition  -   state of the pipeline

RETURN VALUE:
                GST_STATE_CHANGE_FAILURE    - the state change failed
                GST_STATE_CHANGE_SUCCESS    - the state change succeeded
                GST_STATE_CHANGE_ASYNC      - the state change will happen asynchronously
                GST_STATE_CHANGE_NO_PREROLL - the state change cannot be prerolled

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static GstStateChangeReturn mfw_gst_vpudec_change_state
    (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (element);
  gint vpu_ret = RETCODE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    {
      vpu_versioninfo ver;
      MM_INIT_DBG_MEM ("vpu_dec");
      GST_DEBUG (">>VPU_DEC: State: Null to Ready");
      if (vpu_dec->vpu_chipinit == FALSE) {
        vpu_ret = vpu_Init (NULL);
        if (vpu_ret < 0) {
          GST_DEBUG
              (">>VPU_DEC: Error in initializing the VPU, error is %d",
              vpu_ret);
          return GST_STATE_CHANGE_FAILURE;
        }
      }

      vpu_dec->vpu_chipinit = TRUE;
      vpu_dec->vpu_mutex = g_mutex_new ();

      vpu_ret = vpu_GetVersionInfo (&ver);
      if (vpu_ret) {
        GST_DEBUG
            (">>VPU_DEC: Error in geting the VPU version, error is %d",
            vpu_ret);
        vpu_UnInit ();
        vpu_dec->vpu_chipinit = FALSE;
        return GST_STATE_CHANGE_FAILURE;
      }

      vpu_dec->tsm_mode = MODE_AI;

      g_print (YELLOW_STR
          ("VPU Version: firmware %d.%d.%d; libvpu: %d.%d.%d \n",
              ver.fw_major, ver.fw_minor, ver.fw_release,
              ver.lib_major, ver.lib_minor, ver.lib_release));

      if (VPU_LIB_VERSION (ver.lib_major, ver.lib_minor,
              ver.lib_release) != VPU_LIB_VERSION_CODE) {
        g_print (RED_STR
            ("Vpu library version mismatch, please recompile the plugin with running correct head file!!\n"));
      }
#define MFW_GST_VPU_DECODER_PLUGIN VERSION
      PRINT_PLUGIN_VERSION (MFW_GST_VPU_DECODER_PLUGIN);
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      GST_DEBUG (">>VPU_DEC: State: Ready to Paused");
      if (vpudec_global_ptr == NULL)
        return GST_STATE_CHANGE_SUCCESS;

      vpu_dec->vpu_init = FALSE;
      vpu_dec->vpu_opened = FALSE;
      vpu_dec->just_flushed = FALSE;
      vpu_dec->flushing = FALSE;
      vpu_dec->data_in_vpu = 0;
      vpu_dec->start_addr = NULL;
      vpu_dec->end_addr = NULL;
      vpu_dec->base_addr = NULL;
      vpu_dec->yuv_frame_size = 0;
      vpu_dec->decode_wait_time = 0;
      vpu_dec->chain_Time = 0;
      vpu_dec->num_timeouts = 0;
      vpu_dec->codec_data = 0;
      vpu_dec->codec_data_len = 0;
      vpu_dec->buf_alignment_h = 0;
      vpu_dec->buf_alignment_v = 0;

      // used for latency
      vpu_dec->num_gops = 0;
      vpu_dec->gop_size = 0;
      vpu_dec->idx_last_gop = 0;
      vpu_dec->gop_is_next = FALSE;


      vpu_dec->frames_rendered = 0;
      vpu_dec->frames_decoded = 0;
      vpu_dec->avg_fps_decoding = 0.0;
      vpu_dec->non_iframes_dropped = 0;
      vpu_dec->frames_dropped = 0;
      vpu_dec->direct_render = FALSE;
      vpu_dec->firstFrameProcessed = FALSE;
      resyncTSManager (vpu_dec->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
      vpu_dec->clock_base = 0;
      vpu_dec->state_playing = FALSE;;
      vpu_dec->framebufinit_done = FALSE;
      vpu_dec->file_play_mode = FALSE;
      vpu_dec->decoding_completed = FALSE;
      vpu_dec->eos = FALSE;
      vpu_dec->just_flushed = FALSE;
      vpu_dec->mp4Class = MP4_MPEG4;
      vpu_dec->accumulate_hdr = FALSE;
      vpu_dec->nal_check = FALSE;
      vpu_dec->par_width = DEFAULT_PAR_WIDTH;
      vpu_dec->par_height = DEFAULT_PAR_HEIGHT;
      vpu_dec->init_fail_cnt = 0;
      vpu_dec->NALLengthFieldSize = NAL_START_CODE_SIZE;
      {
        int cnt = 0;
        for (cnt = 0; cnt < NUM_FRAME_BUF; cnt++)
          vpu_dec->outbuffers[cnt] = NULL;
      }
      memset (&vpu_dec->bit_stream_buf, 0, sizeof (vpu_mem_desc));
      memset (&vpu_dec->frameBuf[0], 0, NUM_FRAME_BUF * sizeof (FrameBuffer));
      memset (&vpu_dec->frame_mem[0], 0, NUM_FRAME_BUF * sizeof (vpu_mem_desc));
      /* Handle the decoder Initialization over here. */
      if (NULL == vpu_dec->decOP)
        vpu_dec->decOP = MM_MALLOC (sizeof (DecOpenParam));
      if (NULL == vpu_dec->initialInfo)
        vpu_dec->initialInfo = MM_MALLOC (sizeof (DecInitialInfo));
      if (NULL == vpu_dec->decParam)
        vpu_dec->decParam = MM_MALLOC (sizeof (DecParam));
      if (NULL == vpu_dec->handle)
        vpu_dec->handle = MM_MALLOC (sizeof (DecHandle));
      if (NULL == vpu_dec->outputInfo)
        vpu_dec->outputInfo = MM_MALLOC (sizeof (DecOutputInfo));
      memset (vpu_dec->decOP, 0, sizeof (DecOpenParam));
      memset (vpu_dec->handle, 0, sizeof (DecHandle));
      memset (vpu_dec->decParam, 0, sizeof (DecParam));
      memset (vpu_dec->outputInfo, 0, sizeof (DecOutputInfo));
      memset (&vpu_dec->ps_mem_desc, 0, sizeof (vpu_mem_desc));
      memset (&vpu_dec->slice_mem_desc, 0, sizeof (vpu_mem_desc));
      memset (vpu_dec->initialInfo, 0, sizeof (DecInitialInfo));
      memset (vpu_dec->outputInfo, 0, sizeof (DecOutputInfo));
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    case 0x24:                 /* playing to playing - get this after first frame pushed downstream */
    {
      if (vpudec_global_ptr == NULL)
        return GST_STATE_CHANGE_SUCCESS;

      if (GST_ELEMENT (vpu_dec)->clock) {
        vpu_dec->clock_base = gst_clock_get_time (GST_ELEMENT (vpu_dec)->clock);
        vpu_dec->clock_base -=
            gst_element_get_base_time (GST_ELEMENT (vpu_dec));
        GST_DEBUG
            (">>VPU_DEC: State: Transition to Playing new clock %d",
            (guint) vpu_dec->clock_base);
      }
      vpu_dec->state_playing = TRUE;
      break;
    }
    default:
      GST_DEBUG (">>VPU_DEC: State unhandled 0x%x", transition);
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  GST_DEBUG (">>VPU_DEC: State Change 0x%x for VPU returned %d",
      transition, ret);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
      if (vpudec_global_ptr == NULL)
        return GST_STATE_CHANGE_SUCCESS;

      GST_DEBUG (">>VPU_DEC: State: Playing to Paused");
      vpu_dec->state_playing = FALSE;;
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      gfloat avg_mcps = 0, avg_plugin_time = 0, avg_dec_time = 0;

      GST_DEBUG (">>VPU_DEC: State: Paused to Ready");

      if (vpu_dec->profiling) {
        g_print (">>VPU_DEC: PROFILE FIGURES OF VPU DECODER PLUGIN");
        g_print ("\nTotal decode wait time is            %lldus",
            vpu_dec->decode_wait_time);
        g_print ("\nTotal plugin time is                 %lldus",
            vpu_dec->chain_Time);
        g_print ("\nTotal number of frames decoded is    %ld",
            vpu_dec->frames_decoded);
        g_print ("\nTotal number of frames dropped is    %ld\n",
            vpu_dec->frames_dropped);
        if (vpu_dec->frame_rate != 0) {
          avg_mcps =
              ((float) vpu_dec->decode_wait_time * PROCESSOR_CLOCK /
              (1000000 * (vpu_dec->frames_rendered - vpu_dec->frames_dropped)))
              * vpu_dec->frame_rate;
          g_print ("\nAverage decode WAIT MCPS is          %f", avg_mcps);

          avg_mcps =
              ((float) vpu_dec->chain_Time * PROCESSOR_CLOCK /
              (1000000 * (vpu_dec->frames_decoded - vpu_dec->frames_dropped)))
              * vpu_dec->frame_rate;
          g_print ("\nAverage plug-in MCPS is              %f", avg_mcps);
        } else {
          GST_DEBUG
              ("enable the Frame Rate property of the decoder to get the MCPS \
                        ...  ! mfw_vpudecoder framerate=value ! .... \
                         Note: value denotes the framerate to be set");
        }
        avg_dec_time =
            ((float) vpu_dec->decode_wait_time) / vpu_dec->frames_decoded;
        g_print ("\nAverage decoding Wait time is        %fus", avg_dec_time);
        avg_plugin_time =
            ((float) vpu_dec->chain_Time) / vpu_dec->frames_decoded;
        g_print ("\nAverage plugin time is               %fus\n",
            avg_plugin_time);

        vpu_dec->decode_wait_time = 0;
        vpu_dec->chain_Time = 0;
        vpu_dec->avg_fps_decoding = 0.0;
        vpu_dec->frames_decoded = 0;
        vpu_dec->frames_rendered = 0;
        vpu_dec->frames_dropped = 0;
        vpu_dec->init_fail_cnt = 0;

      }

      if (vpu_dec->codec_data) {
        gst_buffer_unref (vpu_dec->codec_data);
        vpu_dec->codec_data_len = 0;
        vpu_dec->codec_data = 0;
      }

      if (vpu_dec->gst_buffer) {
        gst_buffer_unref (vpu_dec->gst_buffer);
        vpu_dec->gst_buffer = NULL;
      }

      if (vpudec_global_ptr == NULL || vpu_dec->in_cleanup)
        return GST_STATE_CHANGE_SUCCESS;

      GST_MUTEX
          (">>VPU_DEC: Before mutex lock in Paused to ready cnt=%d\n",
          mutex_cnt);
#if 0
      if (vpu_dec->vpu_mutex) {
        vpu_mutex_lock (vpu_dec->vpu_mutex, FALSE);

        // sometimes there was error in opening and/or init so still need cleanup here since pipeline was never started
        mfw_gst_vpudec_cleanup (vpu_dec);
        /* Unlock the mutex to free the mutex
         * in case of date terminated.
         */
        vpu_mutex_unlock (vpu_dec->vpu_mutex);
        mutex_cnt = 0;
        g_mutex_free (vpu_dec->vpu_mutex);
        vpu_dec->vpu_mutex = NULL;
      }
#endif
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
      g_print ("\n>>VPU_DEC: State: Ready to Null\n");
      if (vpudec_global_ptr == NULL)
        return GST_STATE_CHANGE_SUCCESS;

      GST_MUTEX
          (">>VPU_DEC: Before mutex lock in Ready to NULL cnt=%d\n", mutex_cnt);
#if 0
      if (vpu_dec->vpu_mutex) {
        vpu_mutex_lock (vpu_dec->vpu_mutex, FALSE);

        // sometimes there was error in opening and/or init so still need cleanup here since pipeline was never started
        mfw_gst_vpudec_cleanup (vpu_dec);
        /* Unlock the mutex to free the mutex
         * in case of date terminated.
         */
        vpu_mutex_unlock (vpu_dec->vpu_mutex);
        mutex_cnt = 0;
        g_mutex_free (vpu_dec->vpu_mutex);
        vpu_dec->vpu_mutex = NULL;
      }
      if (vpu_dec->vpu_chipinit) {
        GST_WARNING ("vpu_uninit\n");
        vpu_UnInit ();
        vpu_dec->vpu_chipinit = FALSE;

      }
#endif
      break;
    }
    default:
      GST_DEBUG (">>VPU_DEC: State unhandled next 0x%x", transition);
      break;
  }

  return ret;

}

/*=============================================================================
FUNCTION:           mfw_gst_vpudec_set_property

DESCRIPTION:        Sets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property set by the application
        pspec      - pointer to the attributes of the property

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_vpudec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (object);
  if (vpudec_global_ptr == NULL)
    return;

  switch (prop_id) {
    case MFW_GST_VPU_PROF_ENABLE:
      vpu_dec->profiling = g_value_get_boolean (value);
      GST_DEBUG (">>VPU_DEC: profiling=%d", vpu_dec->profiling);
      break;

    case MFW_GST_VPU_CODEC_TYPE:
      vpu_dec->codec = g_value_get_enum (value);
      GST_DEBUG (">>VPU_DEC: codec=%d", vpu_dec->codec);
      break;

    case MFW_GST_VPU_LOOPBACK:
      vpu_dec->loopback = g_value_get_boolean (value);
      if (vpu_dec->loopback)
        vpu_dec->allow_parallelization = FALSE;
      break;

    case MFW_GST_VPU_PASSTHRU:
      vpu_dec->passthru = g_value_get_boolean (value);
      break;

    case MFW_GST_VPU_LATENCY:
      vpu_dec->min_latency = g_value_get_boolean (value);
      break;

    case MFW_GST_VPU_PARSER:   // in case of no parser providing frame by frame - disable nal checking
      vpu_dec->parser_input = g_value_get_boolean (value);
      break;

    case MFW_GST_VPU_FRAMEDROP:
      vpu_dec->frame_drop_allowed = g_value_get_boolean (value);
      break;

    case MFW_GST_VPU_DBK_ENABLE:
      vpu_dec->dbk_enabled = g_value_get_boolean (value);
      break;

    case MFW_GST_VPU_DBK_OFFSETA:
      vpu_dec->dbk_offset_a = g_value_get_int (value);
      break;

    case MFW_GST_VPU_DBK_OFFSETB:
      vpu_dec->dbk_offset_b = g_value_get_int (value);
      break;

    case MFW_GST_VPU_USE_INTERNAL_BUFFER:
      vpu_dec->use_internal_buffer = g_value_get_boolean (value);
      break;

    case MFW_GST_VPU_MIRROR:
      vpu_dec->mirror_dir = g_value_get_enum (value);
      break;

    case MFW_GST_VPU_ROTATION:
      vpu_dec->rotation_angle = g_value_get_uint (value);
      switch (vpu_dec->rotation_angle) {
        case 0:
        case 90:
        case 180:
        case 270:
          break;
        default:
          vpu_dec->rotation_angle = 0;
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
      }
      break;

    case MFW_GST_VPU_OUTPUT_FMT:
      vpu_dec->fmt = g_value_get_int (value);
      break;

    case MFW_GST_VPU_FRAMERATE_NU:
      vpu_dec->frame_rate_nu = g_value_get_int (value);
      break;

    case MFW_GST_VPU_FRAMERATE_DE:
      vpu_dec->frame_rate_de = g_value_get_int (value);
      break;

    default:                   // else rotation will fall through with invalid parameter
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}

/*=============================================================================
FUNCTION:           mfw_gst_vpudec_set_property

DESCRIPTION:        Gets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property set by the application
        pspec      - pointer to the attributes of the property

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_vpudec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{

  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (object);
  if (vpudec_global_ptr == NULL)
    return;

  switch (prop_id) {
    case MFW_GST_VPU_PROF_ENABLE:
      g_value_set_boolean (value, vpu_dec->profiling);
      break;
    case MFW_GST_VPU_CODEC_TYPE:
      g_value_set_enum (value, vpu_dec->codec);
      break;
    case MFW_GST_VPU_LOOPBACK:
      g_value_set_boolean (value, vpu_dec->loopback);
      break;
    case MFW_GST_VPU_PASSTHRU:
      g_value_set_boolean (value, vpu_dec->passthru);
      break;
    case MFW_GST_VPU_LATENCY:
      g_value_set_boolean (value, vpu_dec->min_latency);
      break;
    case MFW_GST_VPU_PARSER:
      g_value_set_boolean (value, vpu_dec->parser_input);
      break;
    case MFW_GST_VPU_FRAMEDROP:
      g_value_set_boolean (value, vpu_dec->frame_drop_allowed);
      break;
    case MFW_GST_VPU_DBK_ENABLE:
      g_value_set_boolean (value, vpu_dec->dbk_enabled);
      break;
    case MFW_GST_VPU_DBK_OFFSETA:
      g_value_set_int (value, vpu_dec->dbk_offset_a);
      break;
    case MFW_GST_VPU_DBK_OFFSETB:
      g_value_set_int (value, vpu_dec->dbk_offset_b);
      break;
    case MFW_GST_VPU_USE_INTERNAL_BUFFER:
      g_value_set_boolean (value, vpu_dec->use_internal_buffer);
      break;
    case MFW_GST_VPU_MIRROR:
      g_value_set_enum (value, vpu_dec->mirror_dir);
      break;
    case MFW_GST_VPU_ROTATION:
      g_value_set_uint (value, vpu_dec->rotation_angle);
      break;
    case MFW_GST_VPU_OUTPUT_FMT:
      g_value_set_int (value, vpu_dec->fmt);
      break;
    case MFW_GST_VPU_FRAMERATE_NU:
      g_value_set_int (value, vpu_dec->frame_rate_nu);
      break;
    case MFW_GST_VPU_FRAMERATE_DE:
      g_value_set_int (value, vpu_dec->frame_rate_de);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}


/*=============================================================================
FUNCTION:           src_templ

DESCRIPTION:        Template to create a srcpad for the decoder

ARGUMENTS PASSED:   None

RETURN VALUE:       GstPadTemplate
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static GstPadTemplate *
src_templ (void)
{

  static GstPadTemplate *source_template = NULL;

  if (source_template == NULL) {

    GstCaps *caps, *newcaps;
    GstStructure *structure;
    GValue list = { 0 }
    , fps = {
    0}
    , fmt = {
    0};
    gint i = 0;

    caps = gst_caps_new_simple ("video/x-raw-yuv",
        "format", GST_TYPE_FOURCC,
        mfw_vpu_video_format_to_fourcc (g_video_formats[i++]),
        "width", GST_TYPE_INT_RANGE, 16, 4096,
        "height", GST_TYPE_INT_RANGE, 16, 4096, NULL);

    while (GST_VIDEO_FORMAT_UNKNOWN != g_video_formats[i]) {
      newcaps = gst_caps_new_simple ("video/x-raw-yuv",
          "format", GST_TYPE_FOURCC,
          mfw_vpu_video_format_to_fourcc (g_video_formats[i]),
          "width", GST_TYPE_INT_RANGE, 16, 4096,
          "height", GST_TYPE_INT_RANGE, 16, 4096, NULL);
      gst_caps_append (caps, newcaps);
      i++;
    }
    source_template =
        gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  }

  return source_template;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_setcaps

DESCRIPTION:        This function negoatiates the caps set on the sink pad

ARGUMENTS PASSED:
                pad   -   pointer to the sinkpad of this element
                caps  -     pointer to the caps set

RETURN VALUE:
               TRUE         negotiation success full
               FALSE        negotiation Failed

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static gboolean
mfw_gst_vpudec_setcaps (GstPad * pad, GstCaps * caps)
{
  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (gst_pad_get_parent (pad));
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  const gchar *mime = gst_structure_get_name (structure);
  GValue *codec_data_buf = NULL;
  gint i = 0;
  gint wmvversion, wmvprofile = 0;

  vpu_dec->mp4Class = MP4_MPEG4;        /* Set the default value. */

  if (strcmp (mime, "video/x-h264") == 0)
    vpu_dec->codec = STD_AVC;
  else if (strcmp (mime, "video/mpeg") == 0) {
    gint mpegversion;
    gst_structure_get_int (structure, "mpegversion", &mpegversion);
    if (mpegversion == 4)
      vpu_dec->codec = STD_MPEG4;
    else
      vpu_dec->codec = STD_MPEG2;

    vpu_dec->mp4Class = MP4_MPEG4;
  } else if (strcmp (mime, "video/mpegts") == 0)
    vpu_dec->codec = STD_MPEG2;
  else if (strcmp (mime, "video/x-h263") == 0)
    vpu_dec->codec = STD_H263;
  else if ((strcmp (mime, "video/x-flash-video") == 0) &&
      HAS_SORENSON_DECODER (vpu_dec->chip_code))
    vpu_dec->codec = STD_H263;
  else if (strcmp (mime, "video/x-wmv") == 0)
    vpu_dec->codec = STD_VC1;
  else if (strcmp (mime, "video/mp2v") == 0)
    vpu_dec->codec = STD_MPEG2;
  else if ((strcmp (mime, "video/x-pn-realvideo") == 0) &&
      HAS_RV_DECODER (vpu_dec->chip_code))
    vpu_dec->codec = STD_RV;
  else if ((strcmp (mime, "video/x-divx") == 0) &&
      HAS_DIVX_DECODER (vpu_dec->chip_code)) {
    gint divx_version;
    gst_structure_get_int (structure, "divxversion", &divx_version);
    if (divx_version == 3)
      vpu_dec->codec = STD_DIV3;

    else
      vpu_dec->codec = STD_MPEG4;
    if (divx_version >= 5)
      vpu_dec->mp4Class = MP4_DIVX5_HIGHER;
    else
      vpu_dec->mp4Class = MP4_DIVX4;
  }else if ( strcmp (mime, "video/x-xvid") == 0) {
      vpu_dec->codec = STD_MPEG4;
      vpu_dec->mp4Class = MP4_XVID;
  }else if ((strcmp (mime, "image/jpeg") == 0) &&
      HAS_MJPEG_DECODER (vpu_dec->chip_code))
    vpu_dec->codec = STD_MJPG;
  else {
    GST_ERROR (">>VPU_DEC: Codec Standard not supporded ");
    gst_object_unref (vpu_dec);
    return FALSE;
  }

  {
    gint intvalue0, intvalue1;
    if (gst_structure_get_fraction(structure, "framerate",&intvalue0, &intvalue1)){
      if ((intvalue0>0) && (intvalue1>0)){
        vpu_dec->frame_rate_nu = intvalue0;
        vpu_dec->frame_rate_de = intvalue1;
      }
    }

    vpu_dec->frame_rate =
        (gfloat) (vpu_dec->frame_rate_nu) / vpu_dec->frame_rate_de;

    GST_DEBUG (">>VPU_DEC: set framerate nu %d de %d",
        vpu_dec->frame_rate_nu, vpu_dec->frame_rate_de);
    vpu_dec->time_per_frame =
        gst_util_uint64_scale_int (GST_SECOND, vpu_dec->frame_rate_de,
        vpu_dec->frame_rate_nu);
    setTSManagerFrameRate (vpu_dec->pTS_Mgr, vpu_dec->frame_rate_nu,
        vpu_dec->frame_rate_de);
  }

  vpu_dec->set_ts_manually = FALSE;
  vpu_dec->nal_check = TRUE;

  GST_DEBUG (">>VPU_DEC: Frame Rate = %d ", (guint) vpu_dec->frame_rate);
  gst_structure_get_int (structure, "width", &vpu_dec->picWidth);
  GST_DEBUG (">>VPU_DEC: Input Width is %d", vpu_dec->picWidth);
  gst_structure_get_int (structure, "height", &vpu_dec->picHeight);
  GST_DEBUG (">>VPU_DEC: Input Height is %d", vpu_dec->picHeight);

  // if resolution is not set then most likely we are getting input from an RTP
  // depayloader which does not give our first frame as I frame.  Even if we wait
  // for first I frame we might display garbage video
  if ((vpu_dec->picHeight == 0) && (vpu_dec->picWidth == 0)) {
    vpu_dec->accumulate_hdr = TRUE;
    vpu_dec->check_for_iframe = TRUE;

    //vpu_dec->min_latency = TRUE; // rtp depayers give strange timestamps
    if (vpu_dec->loopback == FALSE)
      vpu_dec->parser_input = FALSE;    // no parser input
    vpu_dec->set_ts_manually = TRUE;
    GST_DEBUG (">>VPU_DEC: Assuming no parser input so using streaming mode ");
  } else {
    vpu_dec->accumulate_hdr = TRUE;
    vpu_dec->check_for_iframe = TRUE;

    if (vpu_dec->codec != STD_MPEG2) {
      GST_DEBUG (">>VPU_DEC: Assuming parser input using file play mode ");
    }
  }

  if (vpu_dec->codec == STD_VC1) {
    guint fourcc = 0;
    gst_structure_get_int (structure, "wmvversion", &wmvversion);
    if (wmvversion != 3) {
      mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
          "WMV Version error:This is a VC1 decoder supports "
          "only WMV 9 Simple,Main and Advance Profile decode (WMV3 or WVC1)");
      gst_object_unref (vpu_dec);
      return FALSE;
    }
    gst_structure_get_int (structure, "wmvprofile", &wmvprofile);
    gst_structure_get_int (structure, "format", &fourcc);
    vpu_dec->codec_subtype = 0;
    if ((wmvprofile == 2) || (fourcc==GST_STR_FOURCC("WVC1"))){
      vpu_dec->codec_subtype = 1;
    }
    codec_data_buf =
        (GValue *) gst_structure_get_value (structure, "codec_data");

    if (NULL != codec_data_buf) {
      vpu_dec->codec_data =
          gst_buffer_ref (gst_value_get_buffer (codec_data_buf));
      vpu_dec->codec_data_len = GST_BUFFER_SIZE (vpu_dec->codec_data);
      GST_DEBUG (">>VPU_DEC: VC1 Codec specific data length is %d",
          vpu_dec->codec_data_len);
    } else {
      GST_ERROR
          (">>VPU_DEC: No Header Extension Data found during Caps Negotiation ");
      mfw_gst_vpudec_post_fatal_error_msg (vpu_dec,
          "No Extension Header Data Received from the Demuxer");
      gst_object_unref (vpu_dec);
      return FALSE;
    }
  }
  if ((vpu_dec->codec == STD_MPEG2) || (vpu_dec->codec == STD_MPEG4)
      || (vpu_dec->codec == STD_RV)) {
    codec_data_buf =
        (GValue *) gst_structure_get_value (structure, "codec_data");
    if (NULL != codec_data_buf) {
      vpu_dec->codec_data =
          gst_buffer_ref (gst_value_get_buffer (codec_data_buf));
      vpu_dec->codec_data_len = GST_BUFFER_SIZE (vpu_dec->codec_data);
      GST_DEBUG
          (">>VPU_DEC: MPEG4 Codec specific data length is %d",
          vpu_dec->codec_data_len);
    }
  }
  if (vpu_dec->codec == STD_AVC) {

    codec_data_buf =
        (GValue *) gst_structure_get_value (structure, "codec_data");
    if (NULL != codec_data_buf) {
      guint8 *hdrextdata;
      vpu_dec->codec_data =
          gst_buffer_ref (gst_value_get_buffer (codec_data_buf));
      GST_DEBUG ("H.264 SET CAPS check for codec data ");
      vpu_dec->codec_data_len = GST_BUFFER_SIZE (vpu_dec->codec_data);
      GST_DEBUG (">>VPU_DEC: AVC Codec specific data length is %d",
          vpu_dec->codec_data_len);
      GST_DEBUG ("AVC codec data is :");
      hdrextdata = GST_BUFFER_DATA (vpu_dec->codec_data);
      for (i = 0; i < vpu_dec->codec_data_len; i++)
        GST_DEBUG ("%x ", hdrextdata[i]);

    }

  }

  gst_object_unref (vpu_dec);
  return TRUE;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_gen_sink_pad_caps

DESCRIPTION:        Generates sink pad caps

ARGUMENTS PASSED:   None

RETURN VALUE:       sink pad caps

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GstCaps *
mfw_gst_vpudec_gen_sink_pad_caps (CHIP_CODE chip_code)
{
  GstCaps *caps = NULL;
  guint max_width = 0, max_height = 0;
  gint i = 0;
  gchar *caps_list[16];
  gchar *resolution_str = NULL;
  gchar *caps_str = NULL;

  /* set the caps list for different chips */
  switch (chip_code) {
    case CC_MX53:
      caps_list[i++] = "video/x-flash-video, %s;";

    case CC_MX51:
      caps_list[i++] = "video/x-pn-realvideo, %s;";

    case CC_MX37:
      caps_list[i++] = "video/x-xvid, %s;";

      caps_list[i++] = "video/x-divx, %s, " "divxversion = (int)[3, 6];";

      caps_list[i++] = "video/x-wmv, %s, " "wmvversion = (int)3;";

      caps_list[i++] = "video/mp2v, %s;";

      caps_list[i++] = "video/mpeg, %s, "
          "mpegversion = (int)[1,2], "
          "systemstream = (boolean)false, " "parsed = (boolean)true;";

    case CC_MX27:
      caps_list[i++] = "video/x-h264,  %s, " "parsed = (boolean)true;";

      caps_list[i++] = "video/x-h263, %s;";

      caps_list[i++] = "video/mpeg, %s, "
          "mpegversion = (int)4, " "parsed = (boolean)true;";
      break;
  }

  /* set the max resolution for different chips */
  if ((CC_MX51 == chip_code) || (CC_MX53 == chip_code)) {
    max_width = 1920;
    max_height = 1088;
  } else if (CC_MX37 == chip_code) {
    max_width = 1280;
    max_height = 720;
  } else {
    max_width = 720;
    max_height = 576;
  }

  /* loop the list and generate caps */
  caps = gst_caps_new_empty ();
  if (caps) {
    resolution_str = g_strdup_printf ("width = (int)[64, %d], "
        "height = (int)[64, %d]", max_width, max_height);
    for (i = i - 1; i >= 0; i--) {
      GstCaps *newcaps = NULL;
      caps_str = g_strdup_printf (caps_list[i], resolution_str);
      newcaps = gst_caps_from_string (caps_str);
      if (newcaps)
        gst_caps_append (caps, newcaps);
      g_free (caps_str);
    }
  }

  g_free (resolution_str);

  /* set caps of MJPEG for MX51 and MX53 */
  if (HAS_MJPEG_DECODER (chip_code)) {
    GstCaps *newcaps = gst_caps_from_string ("image/jpeg, "
        "width = (int)[16, 4096], " "height = (int)[16, 4096];");
    if (newcaps)
      gst_caps_append (caps, newcaps);
  }

  return caps;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_sink_pad_template

DESCRIPTION:        Gets sink pad template

ARGUMENTS PASSED:   None

RETURN VALUE:       sink pad template

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GstPadTemplate *
mfw_gst_vpudec_sink_pad_template (CHIP_CODE chip_code)
{
  FILE *fp = NULL;
  GstCaps *caps = NULL;
  static GstPadTemplate *sink_template = NULL;

  if (sink_template == NULL) {

    if (fp = fopen ("/dev/mxc_vpu", "r")) {
      caps = mfw_gst_vpudec_gen_sink_pad_caps (chip_code);
      fclose (fp);
    }

    if (NULL == caps) {
      sink_template = gst_pad_template_new ("sink", GST_PAD_SINK,
          GST_PAD_ALWAYS, GST_CAPS_NONE);
    } else {
      sink_template = gst_pad_template_new ("sink", GST_PAD_SINK,
          GST_PAD_ALWAYS, caps);
    }
  }

  return sink_template;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_base_init

DESCRIPTION:        Element details are registered with the plugin during
                    _base_init ,This function will initialise the class and child
                    class properties during each new child class creation

ARGUMENTS PASSED:   klass - void pointer

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/

static void
mfw_gst_vpudec_base_init (MfwGstVPU_DecClass * klass)
{

  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  CHIP_CODE chip_code = getChipCode ();
  const gchar *description = "";

  gst_element_class_add_pad_template (element_class, src_templ ());
  gst_element_class_add_pad_template (element_class,
      mfw_gst_vpudec_sink_pad_template (chip_code));

  switch (chip_code) {
    case CC_MX53:
      description = "Decodes DivX, Xvid, RealVideo, "
          "MPEG-2, MPEG-4, Sorenson, H263++, H.264 and VC-1 "
          "elementary data into YUV 4:2:0 data; "
          "Decodes MJPEG elementary data into YUV 4:2:0, "
          "4:2:2 horizontal, 4:2:2 vertical, 4:4:4 or 4:0:0 data";
      break;

    case CC_MX51:
      description = "Decodes DivX, Xvid, RealVideo, "
          "MPEG-2, MPEG-4, H263++, H.264 and VC-1 "
          "elementary data into YUV 4:2:0 data; "
          "Decodes MJPEG elementary data into YUV 4:2:0, "
          "4:2:2 horizontal, 4:2:2 vertical, 4:4:4 or 4:0:0 data";
      break;

    case CC_MX37:
      description = "Decodes DivX, Xvid, MPEG-2, MPEG-4, H263++, H.264 and "
          "VC-1 elementary data into YUV 4:2:0 data";
      break;

    case CC_MX27:
      description = "Decodes MPEG-4, H263++ and H.264 "
          "elementary data into YUV 4:2:0 data";
      break;
  }

  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "VPU-based video decoder",
      "Codec/Decoder/Video", "Decode compressed video to raw data by using VPU");

  GST_DEBUG_CATEGORY_INIT (mfw_gst_vpudec_debug,
      "mfw_vpudecoder", 0, "FreeScale's VPU  Decoder's Log");

}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_codec_get_type

DESCRIPTION:        Gets an enumeration for the different
                    codec standars supported by the decoder

ARGUMENTS PASSED:   None

RETURN VALUE:       enumerated type of the codec standards
                    supported by the decoder

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
GType
mfw_gst_vpudec_codec_get_type (void)
{
  CHIP_CODE chip_code = getChipCode ();
  const gchar *name = "MfwGstVpuDecCodecs";

  if ((CC_MX51 == chip_code) || (CC_MX53 == chip_code)) {
    static GEnumValue vpudec_codecs[] = {
      {STD_MPEG4, STR (STD_MPEG4), "std_mpeg4"},
      {STD_H263, STR (STD_H263), "std_h263"},
      {STD_AVC, STR (STD_AVC), "std_avc"},
      {STD_VC1, STR (STD_VC1), "std_vc1"},
      {STD_MPEG2, STR (STD_MPEG2), "std_mpeg2"},
      {STD_DIV3, STR (STD_DIV3), "std_div3"},
      {STD_RV, STR (STD_RV), "std_rv"},
      {STD_MJPG, STR (STD_MJPG), "std_mjpg"},
      {0, NULL, NULL}
    };

    return (g_enum_register_static (name, vpudec_codecs));
  } else if (CC_MX37 == chip_code) {
    static GEnumValue vpudec_codecs[] = {
      {STD_MPEG4, STR (STD_MPEG4), "std_mpeg4"},
      {STD_H263, STR (STD_H263), "std_h263"},
      {STD_AVC, STR (STD_AVC), "std_avc"},
      {STD_VC1, STR (STD_VC1), "std_vc1"},
      {STD_MPEG2, STR (STD_MPEG2), "std_mpeg2"},
      {STD_DIV3, STR (STD_DIV3), "std_div3"},
      {0, NULL, NULL}
    };

    return (g_enum_register_static (name, vpudec_codecs));
  } else {                      /* MX27 */
    static GEnumValue vpudec_codecs[] = {
      {STD_MPEG4, STR (STD_MPEG4), "std_mpeg4"},
      {STD_H263, STR (STD_H263), "std_h263"},
      {STD_AVC, STR (STD_AVC), "std_avc"},
      {0, NULL, NULL}
    };

    return (g_enum_register_static (name, vpudec_codecs));
  }
}


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_finalize

DESCRIPTION:        Class finalized

ARGUMENTS PASSED:   object     - pointer to the elements object

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static void
mfw_gst_vpudec_finalize (GObject * object)
{
  MfwGstVPU_Dec *vpu_dec = MFW_GST_VPU_DEC (object);
  if (vpu_dec->pTS_Mgr) {
    destroyTSManager (vpu_dec->pTS_Mgr);
    (vpu_dec->pTS_Mgr) = NULL;
  }
  mfw_gst_vpudec_vpu_finalize ();

  PRINT_FINALIZE ("vpu_dec");

  MM_DEINIT_DBG_MEM ();

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_mirror_get_type

DESCRIPTION:        Gets an enumeration for mirror directions

ARGUMENTS PASSED:   None

RETURN VALUE:       Enumerated type of the mirror directions supported by VPU

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
GType
mfw_gst_vpudec_mirror_get_type (void)
{
  static GEnumValue vpudec_mirror[] = {
    {MIRDIR_NONE, STR (MIRDIR_NONE), "none"},
    {MIRDIR_VER, STR (MIRDIR_VER), "ver"},
    {MIRDIR_HOR, STR (MIRDIR_HOR), "hor"},
    {MIRDIR_HOR_VER, STR (MIRDIR_HOR_VER), "hor_ver"},
    {0, NULL, NULL},
  };
  return (g_enum_register_static ("MfwGstVpuDecMirror", vpudec_mirror));
}

/*======================================================================================
FUNCTION:           mfw_gst_vpudec_class_init

DESCRIPTION:        Initialise the class.(specifying what signals,
                    arguments and virtual functions the class has and setting up
                    global states)

ARGUMENTS PASSED:
                klass - pointer to H.264Decoder element class

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static void
mfw_gst_vpudec_class_init (MfwGstVPU_DecClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstelement_class->change_state = mfw_gst_vpudec_change_state;
  gobject_class->set_property = mfw_gst_vpudec_set_property;
  gobject_class->get_property = mfw_gst_vpudec_get_property;
  gobject_class->finalize = mfw_gst_vpudec_finalize;

  parent_class = g_type_class_peek_parent (klass);

  g_object_class_install_property (gobject_class, MFW_GST_VPU_PROF_ENABLE,
      g_param_spec_boolean ("profiling",
          "Profiling",
          "enable time profiling of the vpu decoder plug-in",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_CODEC_TYPE,
      g_param_spec_enum ("codec-type",
          "codec_type",
          "selects the codec type for decoding",
          MFW_GST_TYPE_VPU_DEC_CODEC, STD_AVC, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_LOOPBACK,
      g_param_spec_boolean ("loopback",
          "LoopBack",
          "enables the decoder plug-in to operate"
          "in loopback mode with encoder ", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_ROTATION,
      g_param_spec_uint ("rotation",
          "Rotation",
          "Rotation Angle should be 0, 90, 180 or 270.",
          0, 270, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_MIRROR,
      g_param_spec_enum ("mirror-dir",
          "mirror_dir",
          "specifies mirror direction",
          MFW_GST_TYPE_VPU_DEC_MIRROR, MIRDIR_NONE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_DBK_ENABLE,
      g_param_spec_boolean ("dbkenable",
          "dbkenable",
          "enables the decoder plug-in deblock", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_DBK_OFFSETA,
      g_param_spec_int ("dbk-offseta",
          "dbk_offseta",
          "set the deblock offset a",
          G_MININT, G_MAXINT, 5, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_DBK_OFFSETB,
      g_param_spec_int ("dbk-offsetb",
          "dbk_offsetb",
          "set the deblock offset b",
          G_MININT, G_MAXINT, 5, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      MFW_GST_VPU_USE_INTERNAL_BUFFER,
      g_param_spec_boolean ("use-internal-buffer", "use internal buffer",
          "use internal buffer instead allocate from downstream", FALSE,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_PASSTHRU,
      g_param_spec_boolean ("passthru",
          "passthru",
          "No decode but passes data to be sent to VPU."
          "true:passthru, false: no pass thru - full decode",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_LATENCY,
      g_param_spec_boolean ("min_latency",
          "minimum latency",
          "Minimizes latency through plugin"
          "true: minimize latency, false: full path",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_PARSER,
      g_param_spec_boolean ("parser",
          "parser providing input in frame boundaries",
          "parser on is default - used to turn off when playing elementary video files or streaming "
          "true: parser input , false: no parser - playing from elementary ",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_FRAMEDROP,
      g_param_spec_boolean ("framedrop",
          "enable frame dropping before decode default is on",
          "Used to disable frame dropping before decode which might affect a/v sync "
          "true: frame drop , false: disable frame dropping",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_OUTPUT_FMT,
      g_param_spec_int ("fmt", "output_fmt",
          "set the format of output(0 for NV12, 1 for I420",
          0, 1, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_FRAMERATE_NU,
      g_param_spec_int ("framerate-nu", "framerate numerator",
        "set framerate numerator",
          1, G_MAXINT, DEFAULT_FRAME_RATE_NUMERATOR, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_FRAMERATE_DE,
      g_param_spec_int ("framerate-de", "framerate denominator",
        "set framerate denominator",
          1, G_MAXINT, DEFAULT_FRAME_RATE_DENOMINATOR, G_PARAM_READWRITE));


}


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_init

DESCRIPTION:        Create the pad template that has been registered with the
                    element class in the _base_init

ARGUMENTS PASSED:
                vpu_dec -    pointer to vpu_decoder element structure

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static void
mfw_gst_vpudec_init (MfwGstVPU_Dec * vpu_dec, MfwGstVPU_DecClass * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (vpu_dec);

  CHIP_CODE chip_code = getChipCode ();

  vpudec_global_ptr = vpu_dec;

  /* create the sink and src pads */
  // vpu_dec->sinkpad =
  //     gst_pad_new_from_template (gst_element_class_get_pad_template
  //                                (klass, "sink"), "sink");

  vpu_dec->sinkpad =
      gst_pad_new_from_template (mfw_gst_vpudec_sink_pad_template (chip_code),
      "sink");


  vpu_dec->srcpad = gst_pad_new_from_template (src_templ (), "src");
  gst_element_add_pad (GST_ELEMENT (vpu_dec), vpu_dec->sinkpad);
  gst_element_add_pad (GST_ELEMENT (vpu_dec), vpu_dec->srcpad);

  gst_pad_set_chain_function (vpu_dec->sinkpad, mfw_gst_vpudec_chain);

  gst_pad_set_setcaps_function (vpu_dec->sinkpad, mfw_gst_vpudec_setcaps);

  gst_pad_set_event_function (vpu_dec->sinkpad,
      GST_DEBUG_FUNCPTR (mfw_gst_vpudec_sink_event));
  gst_pad_set_event_function (vpu_dec->srcpad,
      GST_DEBUG_FUNCPTR (mfw_gst_vpudec_src_event));

  vpu_dec->chip_code = getChipCode ();

  vpu_dec->vpu_mutex = NULL;
  vpu_dec->rotation_angle = 0;
  vpu_dec->mirror_dir = MIRDIR_NONE;
  vpu_dec->codec = STD_AVC;
  vpu_dec->loopback = FALSE;
  vpu_dec->vpu_chipinit = FALSE;

  vpu_dec->lastframedropped = FALSE;
  vpu_dec->frame_drop_allowed = TRUE;
  vpu_dec->parser_input = TRUE;
  vpu_dec->predict_gop = FALSE;
  vpu_dec->min_latency = FALSE;
  vpu_dec->is_frame_started = FALSE;
  /* Deblock parameters */
  vpu_dec->dbk_enabled = FALSE;
  vpu_dec->dbk_offset_a = vpu_dec->dbk_offset_b = DEFAULT_DBK_OFFSET_VALUE;

  vpu_dec->pTS_Mgr = createTSManager (0);

  vpu_dec->fmt = 0;

#ifdef VPU_PARALLELIZATION
  vpu_dec->allow_parallelization = TRUE;
#else
  vpu_dec->allow_parallelization = FALSE;
#endif
  vpu_dec->field = FIELD_NONE;
  vpu_dec->in_cleanup = FALSE;
  vpu_dec->skipmode = SKIP_NONE;
  vpu_dec->vpu_thread = NULL;
  vpu_dec->width = 0;
  vpu_dec->height = 0;

  if ((CC_MX51 == vpu_dec->chip_code) || (CC_MX53 == vpu_dec->chip_code)) {
    vpu_dec->buffer_fill_size = BUFF_FILL_SIZE_LARGE;
  } else {
    vpu_dec->buffer_fill_size = BUFF_FILL_SIZE_SMALL;
  }

  vpu_dec->frame_rate_nu = DEFAULT_FRAME_RATE_NUMERATOR;
  vpu_dec->frame_rate_de = DEFAULT_FRAME_RATE_DENOMINATOR;

  INIT_SFD_INFO (&vpu_dec->sfd_info);

#ifdef VPU_THREAD
  mfw_gst_vpu_dec_thread_init (vpu_dec);
#endif
}

/*======================================================================================
FUNCTION:           plugin_init

DESCRIPTION:        Special function , which is called as soon as the plugin or
                    element is loaded and information returned by this function
                    will be cached in central registry

ARGUMENTS PASSED:
                plugin - pointer to container that contains features loaded
                        from shared object module

RETURN VALUE:
                return TRUE or FALSE depending on whether it loaded initialized any
                dependency correctly

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static gboolean
plugin_init (GstPlugin * plugin)
{
  FILE *fp = NULL;
  gboolean ret = FALSE;

  fp = fopen ("/dev/mxc_vpu", "r");

  /* register vpu decoder plugin only for the chips with VPU */
  if (fp != NULL) {
    ret = gst_element_register (plugin, "mfw_vpudecoder",
        FSL_GST_RANK_HIGH, MFW_GST_TYPE_VPU_DEC);
    fclose (fp);
  }

  return ret;
}



/*======================================================================================
FUNCTION:           mfw_gst_type_vpu_dec_get_type

DESCRIPTION:        Interfaces are initiated in this function.you can register one
                    or more interfaces after having registered the type itself.

ARGUMENTS PASSED:   None

RETURN VALUE:       A numerical value ,which represents the unique identifier
                    of this element.

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
     GType mfw_gst_type_vpu_dec_get_type (void)
{
  static GType vpu_dec_type = 0;
  if (!vpu_dec_type) {
    static const GTypeInfo vpu_dec_info = {
      sizeof (MfwGstVPU_DecClass),
      (GBaseInitFunc) mfw_gst_vpudec_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_vpudec_class_init,
      NULL,
      NULL,
      sizeof (MfwGstVPU_Dec),
      0,
      (GInstanceInitFunc) mfw_gst_vpudec_init,
    };
    vpu_dec_type = g_type_register_static (GST_TYPE_ELEMENT,
        "MfwGstVPU_Dec", &vpu_dec_info, 0);
  }
  return vpu_dec_type;
}


/*======================================================================================
FUNCTION:           mfw_gst_vpudec_vpu_finalize

DESCRIPTION:        Handles cleanup of any unreleased memory if player is closed

ARGUMENTS PASSED:   None
RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
void __attribute__ ((destructor)) mfw_gst_vpudec_vpu_finalize (void);

void
mfw_gst_vpudec_vpu_finalize (void)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  MfwGstVPU_Dec *vpu_dec = vpudec_global_ptr;
  if (vpu_dec == NULL) {
    GST_WARNING (">>VPU_DEC: vpu_dec is null,exit no clean up needed");
    return;
  }
  GST_DEBUG (">>VPU_DEC: Destructor - final cleanup ");

  if (vpu_dec->vpu_mutex) {
    GST_MUTEX (">>VPU_DEC: Before cleanup mutex lock cnt=%d,vpu:%p,%p\n",
        mutex_cnt, vpu_dec, vpu_dec->vpu_mutex);

    vpu_mutex_lock (vpu_dec->vpu_mutex, FALSE);
    vpu_dec->in_cleanup = TRUE;
    mfw_gst_vpudec_cleanup (vpu_dec);
    /* Unlock the mutex to free the mutex
     * in case of date terminated.
     */
    vpu_mutex_unlock (vpu_dec->vpu_mutex);
    mutex_cnt = 0;
    g_mutex_free (vpu_dec->vpu_mutex);
    vpu_dec->vpu_mutex = NULL;
  } else {
    GST_DEBUG (">>VPU_DEC: Cleanup already done before destructor time ");
  }

  if (vpu_dec->vpu_chipinit) {
    vpu_UnInit ();
    vpu_dec->vpu_chipinit = FALSE;
  }

  GST_DEBUG (">>VPU_DEC: vpu instance 0x%x destroyed.", vpudec_global_ptr);
  vpudec_global_ptr = NULL;
  return;

}

FSL_GST_PLUGIN_DEFINE("mfwvpudec", "VPU-based video decoder", plugin_init);



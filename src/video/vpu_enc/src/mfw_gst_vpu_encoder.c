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
 * Module Name:    mfw_gst_vpu_encoder.c
 *
 * Description:    Implementation of Hardware (VPU) Encoder Plugin for Gstreamer.
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
#include <string.h>
#include <fcntl.h>              /* fcntl */
#include <sys/mman.h>           /* mmap */
#include <sys/ioctl.h>          /* fopen/fread */
#include "vpu_io.h"
#include "vpu_lib.h"
#include "mfw_gst_utils.h"
#include "mfw_gst_vpu_encoder.h"
#include "vpu_jpegtable.h"

#include "gstbufmeta.h"
#include "mfw_gst_ts.h"

//#define GST_DEBUG g_print
//#define GST_FRAMEDBG g_print
#ifndef GST_FRAMEDBG
#define GST_FRAMEDBG
#endif
//#define GST_LOGTIME g_print
#ifndef GST_LOGTIME
#define GST_LOGTIME
#endif

//#define GST_ERROR g_print

/*======================================================================================
                                        LOCAL MACROS
=======================================================================================*/
#define	GST_CAT_DEFAULT	mfw_gst_vpuenc_debug
static MfwGstVPU_Enc *vpuenc_global_ptr = NULL;

#define IS_DMABLE_BUFFER(buffer) ( (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1])) \
                                 || ( GST_IS_BUFFER(buffer) \
                                 &&  GST_BUFFER_FLAG_IS_SET((buffer),GST_BUFFER_FLAG_LAST)))
#define DMABLE_BUFFER_PHY_ADDR(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]) ? \
                                        ((GstBufferMeta *)(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))->physical_data : \
                                        GST_BUFFER_OFFSET(buffer))


#define VPU_PIC_TYPE ((vpu_enc->codec == STD_AVC) ? ((vpu_enc->outputInfo->picType>>1)&0x3) : ((vpu_enc->outputInfo->picType)&0x3) )
#define VPU_PIC_TYPE_IDR ( (vpu_enc->codec == STD_AVC) ? (!(vpu_enc->outputInfo->picType&0x1)) : (VPU_PIC_TYPE == 0))
#define VPU_PIC_TYPE_I ( VPU_PIC_TYPE_IDR || (VPU_PIC_TYPE == 0))
#define NALU_HEADER_SIZE    5
#define AVC_NALU_START_CODE 0x00000001

#ifdef SWAP
#undef SWAP
#endif
#define SWAP(a, b) \
{ \
    gint temp = a; \
    a = b; \
    b = temp; \
}


/*======================================================================================
                                 STATIC FUNCTION PROTOTYPES
=======================================================================================*/

GST_DEBUG_CATEGORY_STATIC (mfw_gst_vpuenc_debug);

static GstElementClass *parent_class = NULL;


static void mfw_gst_vpuenc_set_property (GObject *, guint, const GValue *,
    GParamSpec *);
static void mfw_gst_vpuenc_get_property (GObject *, guint, GValue *,
    GParamSpec *);
static GstFlowReturn mfw_gst_vpuenc_chain (GstPad * pad, GstBuffer * buffer);
static GstStateChangeReturn mfw_gst_vpuenc_change_state (GstElement * element,
    GstStateChange transition);
static gboolean mfw_gst_vpuenc_sink_event (GstPad * pad, GstEvent * event);
static gboolean mfw_gst_vpuenc_setcaps (GstPad * pad, GstCaps * caps);
static void mfw_gst_vpuenc_base_init (MfwGstVPU_EncClass * klass);
static GType mfw_gst_vpuenc_codec_get_type (void);
static GType mfw_gst_type_vpu_enc_get_type (void);
static void mfw_gst_vpuenc_class_init (MfwGstVPU_EncClass *);
static void mfw_gst_vpuenc_init (MfwGstVPU_Enc *, MfwGstVPU_EncClass *);
void mfw_gst_vpuenc_vpu_finalize (void);


/*======================================================================================
                                     LOCAL FUNCTIONS
=======================================================================================*/
GstFlowReturn mfw_gst_vpuenc_vpuinitialize (MfwGstVPU_Enc * vpu_enc);
RetCode mfw_gst_vpuenc_configure (MfwGstVPU_Enc * vpu_enc);
void mfw_gst_vpuenc_free_framebuffer (MfwGstVPU_Enc * vpu_enc);
gboolean mfw_gst_vpuenc_alloc_framebuffer (MfwGstVPU_Enc * vpu_enc);
void mfw_gst_vpuenc_cleanup (MfwGstVPU_Enc * vpu_enc);
int mfw_gst_vpuenc_fill_headers (MfwGstVPU_Enc * vpu_enc);
GstFlowReturn mfw_gst_vpuenc_send_buffer (MfwGstVPU_Enc * vpu_enc,
    GstBuffer * buffer);
GstFlowReturn mfw_gst_vpuenc_vpuinitialize (MfwGstVPU_Enc * vpu_enc);
gboolean mfw_gst_vpudec_generate_jpeg_tables (MfwGstVPU_Enc * vpu_enc,
    Uint8 * *pphuftable, Uint8 * *ppqmattable);


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_post_fatal_error_msg

DESCRIPTION:        This function is used to post a fatal error message and 
                    terminate the pipeline during an unrecoverable error.
                        
ARGUMENTS PASSED:   vpu_dec  - VPU encoder plugins context error_msg message to be posted 
        
RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
mfw_gst_vpuenc_post_fatal_error_msg (MfwGstVPU_Enc * vpu_enc, gchar * error_msg)
{
  GError *error = NULL;
  GQuark domain;
  domain = g_quark_from_string ("mfw_vpuencoder");
  error = g_error_new (domain, 10, "fatal error");
  gst_element_post_message (GST_ELEMENT (vpu_enc),
      gst_message_new_error (GST_OBJECT (vpu_enc), error, error_msg));
  g_error_free (error);
}

/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_property

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
mfw_gst_vpuenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  MfwGstVPU_Enc *vpu_enc = MFW_GST_VPU_ENC (object);

  switch (prop_id) {
    case MFW_GST_VPU_PROF_ENABLE:
      vpu_enc->profile = g_value_get_boolean (value);
      GST_DEBUG (">>VPU_ENC: profile=%d", vpu_enc->profile);
      break;

    case MFW_GST_VPU_CODEC_TYPE:
      vpu_enc->codec = g_value_get_enum (value);
      GST_DEBUG (">>VPU_ENC: codec=%d vpu_enc 0x%x", vpu_enc->codec, vpu_enc);
      break;

    case MFW_GST_VPUENC_WIDTH:
      vpu_enc->encWidth = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: encoded width=%d", vpu_enc->encWidth);
      break;

    case MFW_GST_VPUENC_HEIGHT:
      vpu_enc->encHeight = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: encoded height=%d", vpu_enc->encHeight);
      break;

    case MFW_GST_VPUENC_BITRATE:
      vpu_enc->bitrate = g_value_get_uint (value);
      if (vpu_enc->bitrate > MAX_BITRATE) {
        GST_WARNING (">>VPU_ENC: too large bit rate (%d), set to the max (%d)",
            vpu_enc->bitrate, MAX_BITRATE);
        vpu_enc->bitrate = MAX_BITRATE;
      }
      GST_OBJECT_LOCK (vpu_enc);
      vpu_enc->setbitrate = TRUE;
      GST_OBJECT_UNLOCK (vpu_enc);
      GST_DEBUG (">>VPU_ENC: bitrate=%d", vpu_enc->bitrate);
      break;

    case MFW_GST_VPUENC_QP:
      vpu_enc->qp = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: Quantization Parameter=%d", vpu_enc->qp);
      break;

    case MFW_GST_VPUENC_MAX_QP:
      vpu_enc->max_qp = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: CBR Max Quantization Parameter=%d",
          vpu_enc->max_qp);
      break;

    case MFW_GST_VPUENC_MIN_QP:
      vpu_enc->min_qp = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: CBR Min Quantization Parameter=%d",
          vpu_enc->min_qp);
      break;

    case MFW_GST_VPUENC_INTRA_QP:
      vpu_enc->intra_qp = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: CBR IntraQP Parameter=%d", vpu_enc->intra_qp);
      break;


    case MFW_GST_VPUENC_GAMMA:
      vpu_enc->gamma = g_value_get_uint (value);
      if (vpu_enc->gamma <= MAX_GAMMA) {
        GST_DEBUG (">>VPU_ENC: CBR Gamma=%d", vpu_enc->gamma);
      } else {
        GST_WARNING (">>VPU_ENC: too large gamma (%d), set to the max (%d)",
            vpu_enc->gamma, MAX_GAMMA);
        vpu_enc->gamma = MAX_GAMMA;
      }
      break;

    case MFW_GST_VPUENC_GOP:
      vpu_enc->gopsize = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: gopsize=%d", vpu_enc->gopsize);
      break;

    case MFW_GST_VPUENC_INTRAREFRESH:
      vpu_enc->intraRefresh = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: intraRefresh=%d", vpu_enc->intraRefresh);
      break;

    case MFW_GST_VPUENC_H263PROFILE0:
      vpu_enc->h263profile0 = g_value_get_boolean (value);
      GST_DEBUG (">>VPU_ENC: H263 Profile 0=%d", vpu_enc->h263profile0);
      break;

    case MFW_GST_VPUENC_LOOPBACK:
      vpu_enc->loopback = g_value_get_boolean (value);
      GST_DEBUG (">>VPU_ENC: Loopback=%d", vpu_enc->loopback);
      break;

    case MFW_GST_VPUENC_CROP_LEFT:
      vpu_enc->crop_left = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: crop_left = %d", vpu_enc->crop_left);
      break;

    case MFW_GST_VPUENC_CROP_TOP:
      vpu_enc->crop_top = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: crop_top =%d", vpu_enc->crop_top);
      break;

    case MFW_GST_VPUENC_CROP_RIGHT:
      vpu_enc->crop_right = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: crop_right = %d", vpu_enc->crop_right);
      break;

    case MFW_GST_VPUENC_CROP_BOTTOM:
      vpu_enc->crop_bottom = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: crop_bottom =%d", vpu_enc->crop_bottom);
      break;

    case MFW_GST_VPUENC_ROTATION_ANGLE:
      vpu_enc->rotation = g_value_get_uint (value);
      GST_DEBUG (">>VPU_ENC: rotation =%d", vpu_enc->rotation);
      break;

    case MFW_GST_VPUENC_MIRROR_DIRECTION:
      vpu_enc->mirror = g_value_get_enum (value);
      GST_DEBUG (">>VPU_ENC: mirror = %d", vpu_enc->mirror);
      break;

    case MFW_GST_VPUENC_AVC_BYTE_STREAM:
      vpu_enc->avc_byte_stream = g_value_get_boolean (value);
      GST_DEBUG (">>VPU_ENC: H.264 byte stream = %d", vpu_enc->avc_byte_stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_property

DESCRIPTION:        Gets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property set by the application
        pspec      - pointer to the attributes of the property

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void
mfw_gst_vpuenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  MfwGstVPU_Enc *vpu_enc = MFW_GST_VPU_ENC (object);

  switch (prop_id) {
    case MFW_GST_VPU_PROF_ENABLE:
      g_value_set_boolean (value, vpu_enc->profile);
      break;

    case MFW_GST_VPU_CODEC_TYPE:
      g_value_set_enum (value, vpu_enc->codec);
      break;

    case MFW_GST_VPUENC_WIDTH:
      g_value_set_uint (value, vpu_enc->encWidth);
      break;

    case MFW_GST_VPUENC_HEIGHT:
      g_value_set_uint (value, vpu_enc->encHeight);
      break;

    case MFW_GST_VPUENC_BITRATE:
      g_value_set_uint (value, vpu_enc->bitrate);
      break;

    case MFW_GST_VPUENC_QP:
      g_value_set_uint (value, vpu_enc->qp);
      break;

    case MFW_GST_VPUENC_MAX_QP:
      g_value_set_uint (value, vpu_enc->max_qp);
      break;

    case MFW_GST_VPUENC_MIN_QP:
      g_value_set_uint (value, vpu_enc->min_qp);
      break;

    case MFW_GST_VPUENC_INTRA_QP:
      g_value_set_uint (value, vpu_enc->intra_qp);
      break;

    case MFW_GST_VPUENC_GAMMA:
      g_value_set_uint (value, vpu_enc->gamma);
      break;

    case MFW_GST_VPUENC_GOP:
      g_value_set_uint (value, vpu_enc->gopsize);
      break;

    case MFW_GST_VPUENC_INTRAREFRESH:
      g_value_set_uint (value, vpu_enc->intraRefresh);
      break;

    case MFW_GST_VPUENC_H263PROFILE0:
      g_value_set_boolean (value, vpu_enc->h263profile0);
      break;

    case MFW_GST_VPUENC_LOOPBACK:
      g_value_set_boolean (value, vpu_enc->loopback);
      break;

    case MFW_GST_VPUENC_CROP_LEFT:
      g_value_set_uint (value, vpu_enc->crop_left);
      break;

    case MFW_GST_VPUENC_CROP_TOP:
      g_value_set_uint (value, vpu_enc->crop_top);
      break;

    case MFW_GST_VPUENC_CROP_RIGHT:
      g_value_set_uint (value, vpu_enc->crop_right);
      break;

    case MFW_GST_VPUENC_CROP_BOTTOM:
      g_value_set_uint (value, vpu_enc->crop_bottom);
      break;

    case MFW_GST_VPUENC_ROTATION_ANGLE:
      g_value_set_uint (value, vpu_enc->rotation);
      break;

    case MFW_GST_VPUENC_MIRROR_DIRECTION:
      g_value_set_enum (value, vpu_enc->mirror);
      break;

    case MFW_GST_VPUENC_AVC_BYTE_STREAM:
      g_value_set_boolean (value, vpu_enc->avc_byte_stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_configure

DESCRIPTION:        Configure the IRAM memory

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_configure (MfwGstVPU_Enc * vpu_enc)
{
  SearchRamParam search_pa = { 0 };
  RetCode vpu_ret = RETCODE_SUCCESS;
  if ((CC_MX51 == vpu_enc->chip_code) || (CC_MX53 == vpu_enc->chip_code)) {
    iram_t iram;
    int ram_size;

    memset (&iram, 0, sizeof (iram_t));
    ram_size = ((vpu_enc->width + 15) & ~15) * 36 + 2048;
    IOGetIramBase (&iram);
    if ((iram.end - iram.start) < ram_size)
      ram_size = iram.end - iram.start;
    search_pa.searchRamAddr = iram.start;
    search_pa.SearchRamSize = ram_size;
  } else {                      /* CC_MX27 == vpu_enc->chip_code */

#define DEFAULT_SEARCHRAM_ADDR  (0xFFFF4C00)
    search_pa.searchRamAddr = DEFAULT_SEARCHRAM_ADDR;
  }

  vpu_ret =
      vpu_EncGiveCommand (vpu_enc->handle, ENC_SET_SEARCHRAM_PARAM, &search_pa);
  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR (">>VPU_ENC: SET_SEARCHRAM_PARAM failed");
  }
  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_rotation

DESCRIPTION:        Configure the rotation angle and mirror direction

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_rotation (MfwGstVPU_Enc * vpu_enc)
{
  guint rotation = vpu_enc->rotation;
  guint mirror = vpu_enc->mirror;
  RetCode vpu_ret = RETCODE_SUCCESS;

  do {
    if ((rotation != 0) && (rotation != 90) &&
        (rotation != 180) && (rotation != 270)) {
      if ((rotation > 0) && (rotation < 90)) {
        rotation = 0;
      } else if ((rotation > 90) && (rotation < 180)) {
        rotation = 90;
      } else if ((rotation > 180) && (rotation < 270)) {
        rotation = 180;
      } else {
        rotation = 270;
      }

      GST_ERROR (">>VPU_ENC: Invalid rotation angle (%d), set to %d",
          vpu_enc->rotation, rotation);
      vpu_enc->rotation = rotation;
    }

    if (rotation > 0) {
      vpu_ret = vpu_EncGiveCommand (vpu_enc->handle, ENABLE_ROTATION, 0);
      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR (">>VPU_ENC: ENABLE_ROTATION failed : %d", vpu_ret);
        break;
      }
      vpu_ret =
          vpu_EncGiveCommand (vpu_enc->handle, SET_ROTATION_ANGLE, &rotation);
      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR (">>VPU_ENC: SET_ROTATION_ANGLE failed : %d", vpu_ret);
        break;
      }
    } else {
      vpu_ret = vpu_EncGiveCommand (vpu_enc->handle, DISABLE_ROTATION, 0);
      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR (">>VPU_ENC: DISABLE_ROTATION failed : %d", vpu_ret);
        break;
      }
    }

    if (mirror != MIRDIR_NONE) {
      vpu_ret = vpu_EncGiveCommand (vpu_enc->handle, ENABLE_MIRRORING, 0);
      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR (">>VPU_ENC: ENABLE_MIRRORING failed : %d", vpu_ret);
        break;
      }
      vpu_ret =
          vpu_EncGiveCommand (vpu_enc->handle, SET_MIRROR_DIRECTION, &mirror);
      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR (">>VPU_ENC: SET_MIRROR_DIRECTION failed : %d", vpu_ret);
        break;
      }
    } else {
      vpu_ret = vpu_EncGiveCommand (vpu_enc->handle, DISABLE_MIRRORING, 0);
      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR (">>VPU_ENC: DISABLE_MIRRORING failed : %d", vpu_ret);
        break;
      }
    }
  } while (0);

  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_qp

DESCRIPTION:        Configure the qp 

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_qp (MfwGstVPU_Enc * vpu_enc)
{
  guint rotation = vpu_enc->rotation;
  guint mirror = vpu_enc->mirror;
  guint maxQP, minQP, defaultQP;
  RetCode vpu_ret = RETCODE_SUCCESS;


  if (vpu_enc->codec == STD_AVC) {
    maxQP = H264_QP_MAX;
    minQP = H264_QP_MIN;
    defaultQP = VPU_DEFAULT_H264_QP;
  } else {
    maxQP = MPEG4_QP_MAX;
    minQP = MPEG4_QP_MIN;
    defaultQP = VPU_DEFAULT_MPEG4_QP;
  }

  // if neither QP or Bitrate is set - use the default per codec
  if (0 == vpu_enc->bitrate) {  /* VBR, set QP */
    if (vpu_enc->qp == VPU_INVALID_QP) {
      vpu_enc->qp = defaultQP;
    } else {                    /* clip qp to [min, max] */
      vpu_enc->qp = CLAMP (vpu_enc->qp, minQP, maxQP);
    }
    vpu_enc->encParam->quantParam = vpu_enc->qp;

    GST_DEBUG (">>VPU_ENC: QP quantparam = %d", vpu_enc->encParam->quantParam);
  } else {                      /* if RC is used, set the intra QP and max/min QP */
    if (vpu_enc->intra_qp != VPU_INVALID_QP) {
      /* crop intra qp to [min, max] */
      vpu_enc->intra_qp = CLAMP (vpu_enc->intra_qp, minQP, maxQP);
    }
    /* else ... default intra QP should be -1 and VPU decides the value */
    vpu_enc->encOP->rcIntraQp = vpu_enc->intra_qp;

    if ((CC_MX51 == vpu_enc->chip_code) || (CC_MX53 == vpu_enc->chip_code)) {
      if (vpu_enc->max_qp < maxQP) {
        vpu_enc->encOP->userQpMaxEnable = TRUE;
        vpu_enc->encOP->userQpMax = vpu_enc->max_qp;
      } else {                  /* disable max QP */
        vpu_enc->encOP->userQpMaxEnable = FALSE;
        vpu_enc->max_qp = maxQP;
      }

      if (vpu_enc->min_qp != VPU_INVALID_QP) {
        vpu_enc->min_qp = CLAMP (vpu_enc->min_qp, minQP, vpu_enc->max_qp);
        vpu_enc->encOP->userQpMinEnable = TRUE;
        vpu_enc->encOP->userQpMin = vpu_enc->min_qp;
      } else {                  /* disable min QP */
        vpu_enc->encOP->userQpMinEnable = FALSE;
        vpu_enc->min_qp = minQP;
      }

      vpu_enc->encOP->userGamma = vpu_enc->gamma;
    }
#if 0                           /* FIXME : no header file of mx27 vpu file */
    else {                      /* CC_MX27 == vpu_enc->chip_code */

      if (vpu_enc->max_qp != VPU_INVALID_QP)
        vpu_enc->encOP->maxQp = vpu_enc->max_qp;

      vpu_enc->encOP->Gamma = vpu_enc->gamma;
    }
#endif
    GST_DEBUG (">>VPU_ENC: intra QP =%d, min QP =%d, max QP = %d",
        vpu_enc->encOP->rcIntraQp, vpu_enc->min_qp, vpu_enc->max_qp);
  }

  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_bitrate

DESCRIPTION:        Configure default bitrate

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_bitrate (MfwGstVPU_Enc * vpu_enc)
{
  guint encWidth = vpu_enc->encOP->picWidth;
  guint encHeight = vpu_enc->encOP->picHeight;
  guint encSize = 0;
  RetCode vpu_ret = RETCODE_SUCCESS;

  if ((vpu_enc->bitrate >= INVALID_BITRATE) &&
      (vpu_enc->bitrate <= MAX_BITRATE)) {
    /* return if it is VBR or bit rate is set by application */
    return vpu_ret;
  }

  switch (vpu_enc->yuv_proportion) {
    case MFW_GST_VPUENC_YUV_420:
      encSize = encWidth * encHeight * 3 / 2;
      break;
    case MFW_GST_VPUENC_YUV_422H:
    case MFW_GST_VPUENC_YUV_422V:
      encSize = encWidth * encHeight * 2;
      break;
    case MFW_GST_VPUENC_YUV_444:
      encSize = encWidth * encHeight * 3;
      break;
    case MFW_GST_VPUENC_YUV_400:
      encSize = encWidth * encHeight;
      break;
  }

  if (vpu_enc->codec == STD_MJPG) {
    vpu_enc->bitrate = encSize * vpu_enc->tgt_framerate;
  } else {
    vpu_enc->bitrate = encSize * vpu_enc->tgt_framerate / 10;
  }

  vpu_enc->bitrate = vpu_enc->bitrate >> 10;
  vpu_enc->encOP->bitRate = vpu_enc->bitrate;
  GST_INFO ("bir rate %d kbps, size %d, frame rate %f fps, yuv_proportion %d",
      vpu_enc->encOP->bitRate, encSize, vpu_enc->tgt_framerate,
      vpu_enc->yuv_proportion);
  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_format

DESCRIPTION:        Configure source image format

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_format (MfwGstVPU_Enc * vpu_enc)
{
  RetCode vpu_ret = RETCODE_SUCCESS;

  do {
    switch (vpu_enc->format) {
      case GST_VIDEO_FORMAT_I420:
        vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_420;
        vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y_U_V;
        break;
      case GST_VIDEO_FORMAT_YV12:
        vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_420;
        vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y_V_U;
        break;
      case GST_VIDEO_FORMAT_NV12:
        vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_420;
        vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y_UV;
        break;
      case GST_VIDEO_FORMAT_Y42B:
        vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_422H;
        vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y_U_V;
        break;
        //case GST_MAKE_FOURCC('Y', 'V', '1', '6'):
        //    vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_422V;
        //    vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y_V_U;
        //    break;
        //case GST_MAKE_FOURCC('N', 'V', '1', '6'):
        //    vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_422V;
        //    vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y_UV;
        //    break;
      case GST_VIDEO_FORMAT_Y444:
        vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_444;
        vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y_U_V;
        break;
#if GST_CHECK_VERSION(0, 10, 29)
      case GST_VIDEO_FORMAT_GRAY8:
#if GST_CHECK_VERSION(0, 10, 30)
      case GST_VIDEO_FORMAT_Y800:
#endif
        vpu_enc->yuv_proportion = MFW_GST_VPUENC_YUV_400;
        vpu_enc->yuv_planar = MFW_GST_VPUENC_PLANAR_Y;
        break;
#endif
      default:
        vpu_ret = RETCODE_INVALID_PARAM;
    }

    if (RETCODE_SUCCESS != vpu_ret)
      break;

    if (vpu_enc->codec != STD_MJPG) {
      if (vpu_enc->yuv_proportion != MFW_GST_VPUENC_YUV_420) {
        /* the source image format is not 4:2:0, post error message */
        GST_ERROR
            (">>VPU_ENC: Wrong YUV format of source image for non-MJPEG encoder.");
        vpu_ret = RETCODE_INVALID_PARAM;
        break;
      }
      /* for cbr, check bit rate, resolution and gop size. 
         limit gop size for low bit rate */
    }

    if (MFW_GST_VPUENC_PLANAR_Y_UV == vpu_enc->yuv_planar) {
      vpu_enc->encOP->chromaInterleave = 1;
      GST_DEBUG ("Input format is UV interleaved.");
    }
  } while (0);

  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_std_param

DESCRIPTION:        Configure vpu encoder standard parameters

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_std_param (MfwGstVPU_Enc * vpu_enc)
{
  RetCode vpu_ret = RETCODE_SUCCESS;

  if (vpu_enc->codec == STD_MPEG4) {
    vpu_enc->encOP->EncStdParam.mp4Param.mp4_verid = 2;
  } else if (vpu_enc->codec == STD_H263) {
    if (vpu_enc->h263profile0 == FALSE) {
      /* H.263v3 profile 3 (Annex I is not enabled in VPU encoder) */
      vpu_enc->encOP->EncStdParam.h263Param.h263_annexJEnable = 1;
      vpu_enc->encOP->EncStdParam.h263Param.h263_annexKEnable = 1;
      vpu_enc->encOP->EncStdParam.h263Param.h263_annexTEnable = 1;
    }

  } else if (vpu_enc->codec == STD_MJPG) {
    Uint8 **mjpg_hufTable =
        &(vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_hufTable);
    Uint8 **mjpg_qMatTable =
        &(vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_qMatTable);
    if (FALSE == mfw_gst_vpudec_generate_jpeg_tables (vpu_enc, mjpg_hufTable,
            mjpg_qMatTable)) {
      GST_ERROR (">>VPU_ENC: Failed to generate jpeg tables.");
      vpu_ret = RETCODE_FAILURE;
    }
    vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_sourceFormat = vpu_enc->yuv_proportion;  /* encConfig.mjpgChromaFormat */
    vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_restartInterval = 60;    /* FIXME: set restart interval according to resolution */
  } else if (vpu_enc->codec != STD_AVC) {
    GST_ERROR (">>VPU_ENC: Invalid codec standard mode");
    vpu_ret = RETCODE_INVALID_PARAM;
  }

  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_enc_resolution

DESCRIPTION:        Set encoded picture resolution.
             
ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_enc_resolution (MfwGstVPU_Enc * vpu_enc)
{
  gint encWidth = 0, encHeight = 0;
  gint alignCropRight = 0, alignCropBottom = 0;
  gint maxWidth = vpu_enc->max_width;
  gint maxHeight = vpu_enc->max_height;
  gint minWidth = MIN_WIDTH, minHeight = MIN_HEIGHT;
  gboolean rotate_90_270;

  if (vpu_enc->codec == STD_MJPG) {
    maxWidth = MAX_MJPEG_WIDTH;
    maxHeight = MAX_MJPEG_HEIGHT;
  }

  encWidth = vpu_enc->encWidth;
  encHeight = vpu_enc->encHeight;

  rotate_90_270 = (90 == vpu_enc->rotation) || (270 == vpu_enc->rotation);
  if (rotate_90_270) {
    SWAP (encWidth, encHeight);
  }

  /* check the encWidth set by property */
  if (INVALID_WIDTH == encWidth) {      /* encWidth is not set */
    encWidth = vpu_enc->width - vpu_enc->crop_left - vpu_enc->crop_right;
  } else if (encWidth < vpu_enc->width) {
    if (vpu_enc->crop_left + encWidth > vpu_enc->width) {
      vpu_enc->crop_left = vpu_enc->width - encWidth;
    }
  } else {                      /* encWidth is larger than source width */
    GST_ERROR (">>VPU_ENC: Target picture width is too big : %d", encWidth);
    encWidth = vpu_enc->width;
    vpu_enc->crop_left = 0;
  }

  /* check the encHeight set by property */
  if (INVALID_HEIGHT == encHeight) {    /* encHeight is not set */
    encHeight = vpu_enc->height - vpu_enc->crop_top - vpu_enc->crop_bottom;
  } else if (encHeight < vpu_enc->height) {
    if (vpu_enc->crop_top + encHeight > vpu_enc->height) {
      vpu_enc->crop_top = vpu_enc->height - encHeight;
    }
  } else {                      /* encHeight is larger than source height */
    GST_ERROR (">>VPU_ENC: Target picture height is too big : %d", encHeight);
    encHeight = vpu_enc->height;
    vpu_enc->crop_top = 0;
  }

  if (rotate_90_270) {
    SWAP (vpu_enc->width, vpu_enc->height);
    SWAP (vpu_enc->crop_left, vpu_enc->crop_top);
    SWAP (encWidth, encHeight);
  }

  /* crop to max resolution for big picture */
  if (encWidth > maxWidth) {
    GST_ERROR (">>VPU_ENC: Picture width (%d) is too big, set to max (%d)",
        encWidth, maxWidth);
    encWidth = maxWidth;
  }
  if (encHeight > maxHeight) {
    GST_ERROR (">>VPU_ENC: Picture height (%d) is too big, set to max (%d)",
        encHeight, maxHeight);
    encHeight = maxHeight;
  }

  if (encWidth < minWidth) {
    GST_ERROR (">>VPU_ENC: Picture width is too small : %d", encWidth);
    if (vpu_enc->width >= minWidth) {
      GST_DEBUG (">>VPU_ENC: Set picture width to minimum width %d", minWidth);
      encWidth = minWidth;
      if (vpu_enc->crop_left + minWidth > vpu_enc->width)
        vpu_enc->crop_left = vpu_enc->width - minWidth;
    } else {
      return RETCODE_FAILURE;
    }
  }
  if (encHeight < minHeight) {
    GST_ERROR (">>VPU_ENC: Picture height is too small : %d", encHeight);
    if (vpu_enc->height >= minHeight) {
      GST_DEBUG (">>VPU_ENC: Set picture height to minimum height %d",
          minHeight);
      encHeight = minHeight;
      if (vpu_enc->crop_top + minHeight > vpu_enc->height)
        vpu_enc->crop_top = vpu_enc->height - minHeight;
    } else {
      return RETCODE_FAILURE;
    }
  }

  if ((vpu_enc->codec == STD_H263) && (vpu_enc->h263profile0 == TRUE)) {
    if ((encWidth >= 704) && (encHeight >= 576)) {
      encWidth = 704;
      encHeight = 576;
    } else if ((encWidth >= 352) && (encHeight >= 288)) {
      encWidth = 352;
      encHeight = 288;
    } else if ((encWidth >= 176) && (encHeight >= 144)) {
      encWidth = 176;
      encHeight = 144;
    } else if ((encWidth >= 128) && (encHeight >= 96)) {
      encWidth = 128;
      encHeight = 96;
    } else if ((vpu_enc->width >= 128) && (vpu_enc->height >= 96)) {
      if (vpu_enc->crop_left + 128 > vpu_enc->width) {
        vpu_enc->crop_left = vpu_enc->width - 128;
      }

      if (vpu_enc->crop_top + 96 > vpu_enc->height) {
        vpu_enc->crop_top = vpu_enc->height - 96;
      }

      encWidth = 128;
      encHeight = 96;
    } else {
      GST_ERROR
          (">>VPU_ENC: Picture resolution is too small for H.263 P0 : %dx%d",
          vpu_enc->width, vpu_enc->height);
      if (rotate_90_270) {
        SWAP (vpu_enc->width, vpu_enc->height);
        SWAP (vpu_enc->crop_left, vpu_enc->crop_top);
      }
      return RETCODE_FAILURE;
    }
  } else {
    if (vpu_enc->codec != STD_MJPG) {
      encWidth = encWidth & ~1;
      encHeight = encHeight & ~1;
    }
  }

  vpu_enc->encOP->picWidth = encWidth;
  vpu_enc->encOP->picHeight = encHeight;

  if (rotate_90_270) {
    SWAP (vpu_enc->width, vpu_enc->height);
    SWAP (vpu_enc->crop_left, vpu_enc->crop_top);
  }

  GST_INFO
      (">>VPU_ENC: Crop from %dx%d to %dx%d. top offset: %d, left offset: %d",
      vpu_enc->width, vpu_enc->height, encWidth, encHeight, vpu_enc->crop_top,
      vpu_enc->crop_left);

  return RETCODE_SUCCESS;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_framerate

DESCRIPTION:        Configure frame rate related information

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_framerate (MfwGstVPU_Enc * vpu_enc)
{
  gint frameRateInfo = 0;
  RetCode vpu_ret = RETCODE_SUCCESS;

  vpu_enc->num_in_interval = 0;
  vpu_enc->num_enc_in_interval = 0;
  vpu_enc->fDropping_till_IFrame = FALSE;

  if (vpu_enc->codec == STD_H263) {
    if (vpu_enc->h263profile0 == TRUE) {
      /* H.263 profile 0 only supports 29.97fps */
      if ((vpu_enc->framerate_n != 30000) || (vpu_enc->framerate_d != 1001)) {
        GST_ERROR
            (">>VPU_ENC: source frame rate is not 29.97fps for H.263 P0: %.2ffps",
            vpu_enc->src_framerate);
        return RETCODE_FAILURE;
      }
    }
  }

  vpu_enc->tgt_framerate = (gfloat) vpu_enc->framerate_n / vpu_enc->framerate_d;
  frameRateInfo = vpu_enc->framerate_n | ((vpu_enc->framerate_d - 1) << 16);

  vpu_enc->encOP->frameRateInfo = frameRateInfo;

  GST_DEBUG (">>VPU_ENC: Target frame rate = %d/%d",
      vpu_enc->framerate_n, vpu_enc->framerate_d);

  vpu_enc->time_per_frame = gst_util_uint64_scale_int (GST_SECOND,
      vpu_enc->framerate_d, vpu_enc->framerate_n);

  GST_DEBUG (">>VPU_ENC: time per frame = %" GST_TIME_FORMAT,
      GST_TIME_ARGS (vpu_enc->time_per_frame));

  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_set_open_params

DESCRIPTION:        Configure the rotation angle and mirror direction

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       RETCODE_SUCCESS/ VPU failure return code
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
RetCode
mfw_gst_vpuenc_set_open_params (MfwGstVPU_Enc * vpu_enc)
{
  RetCode vpu_ret = RETCODE_SUCCESS;

  vpu_enc->encOP->bitstreamBuffer = vpu_enc->bit_stream_buf.phy_addr;
  vpu_enc->encOP->bitstreamBufferSize = BUFF_FILL_SIZE;

  vpu_enc->encOP->bitstreamFormat = vpu_enc->codec;
  vpu_enc->encOP->bitRate = vpu_enc->bitrate;
  vpu_enc->encOP->gopSize = vpu_enc->gopsize;
  vpu_enc->encOP->rcIntraQp = vpu_enc->intra_qp;

  vpu_ret = mfw_gst_vpuenc_set_format (vpu_enc);

  if (RETCODE_SUCCESS == vpu_ret)
    vpu_ret = mfw_gst_vpuenc_set_framerate (vpu_enc);

  if (RETCODE_SUCCESS == vpu_ret)
    vpu_ret = mfw_gst_vpuenc_set_enc_resolution (vpu_enc);

  if (RETCODE_SUCCESS == vpu_ret)
    vpu_ret = mfw_gst_vpuenc_set_bitrate (vpu_enc);

  if (RETCODE_SUCCESS == vpu_ret)
    vpu_ret = mfw_gst_vpuenc_set_std_param (vpu_enc);

  if (RETCODE_SUCCESS == vpu_ret)
    vpu_ret = mfw_gst_vpuenc_set_qp (vpu_enc);

  vpu_enc->encOP->intraRefresh = vpu_enc->intraRefresh;

  return vpu_ret;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_free_framebuffer

DESCRIPTION:        Free frame buffer allocated memory

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
mfw_gst_vpuenc_free_framebuffer (MfwGstVPU_Enc * vpu_enc)
{
  int i;
  int framenum = vpu_enc->numframebufs;
  vpu_mem_desc *framedesc = vpu_enc->vpuRegisteredFramesDesc;
  FrameBuffer *frame = vpu_enc->vpuRegisteredFrames;

  GST_DEBUG (">>VPU_ENC: mfw_gst_vpuenc_free_framebuffer");
  if (framedesc) {
    for (i = 0; i < framenum; i++) {
      if (framedesc[i].phy_addr) {
        IOFreeVirtMem (&(framedesc[i]));
        IOFreePhyMem (&(framedesc[i]));
      }
    }
    g_free (framedesc);
    vpu_enc->vpuRegisteredFramesDesc = NULL;
  }

  if (frame) {
    if (framenum > 0)
      g_free (frame);
    vpu_enc->vpuRegisteredFrames = NULL;
  }
  if (vpu_enc->hdr_data)
    gst_buffer_unref (vpu_enc->hdr_data);
  vpu_enc->hdr_data = NULL;

  if (vpu_enc->codec_data)
    gst_buffer_unref (vpu_enc->codec_data);
  vpu_enc->codec_data = NULL;

}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_alloc_framebuffer

DESCRIPTION:        Allocate frame buffer memory

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       TRUE (Success)/FALSE (Failure
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
mfw_gst_vpuenc_alloc_framebuffer (MfwGstVPU_Enc * vpu_enc)
{
  int i;
  int framenum = vpu_enc->numframebufs;
  int framebuffersize;
  int picturesize;
  vpu_mem_desc *framedesc;
  FrameBuffer *frame;
  gint picWidth, picHeight;

  framedesc = g_malloc (sizeof (vpu_mem_desc) * framenum);
  if (framedesc == NULL) {
    goto Err;
  }

  frame = g_malloc (sizeof (FrameBuffer) * framenum);
  if (frame == NULL) {
    goto Err;
  }

  memset (framedesc, 0, (sizeof (vpu_mem_desc) * framenum));
  memset (frame, 0, (sizeof (FrameBuffer) * framenum));

  picWidth = (vpu_enc->encOP->picWidth + 15) & ~0xF;
  picHeight = (vpu_enc->encOP->picHeight + 15) & ~0xF;
  picturesize = picWidth * picHeight;
  /* since MJPEG does not need frame buffer and other video encoders only 
   * accept 4:2:0, the frame buffer size is (picturesize * 3 / 2) */
  framebuffersize = picturesize * 3 / 2;        //7/4 - using this crashes with VGA or higher resolutions encoding from file

  for (i = 0; i < framenum; i++) {
    framedesc[i].size = framebuffersize;
    IOGetPhyMem (&(framedesc[i]));
    if (framedesc[i].phy_addr == 0) {
      goto Err;
    }
    framedesc[i].virt_uaddr = IOGetVirtMem (&(framedesc[i]));
    frame[i].strideY = picWidth;
    frame[i].strideC = picWidth >> 1;

    frame[i].bufY = framedesc[i].phy_addr;
    frame[i].bufCb = frame[i].bufY + picturesize;
    frame[i].bufCr = frame[i].bufCb + (picturesize >> 2);
  }

  vpu_enc->vpuRegisteredFrames = frame;
  vpu_enc->vpuRegisteredFramesDesc = framedesc;

  return TRUE;

Err:
  vpu_enc->vpuRegisteredFrames = NULL;
  vpu_enc->vpuRegisteredFramesDesc = NULL;
  mfw_gst_vpuenc_free_framebuffer (vpu_enc);
  return FALSE;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_free_sourcebuffer

DESCRIPTION:        Free frame buffer allocated memory

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
mfw_gst_vpuenc_free_sourcebuffer (MfwGstVPU_Enc * vpu_enc)
{
  vpu_mem_desc *framedesc = &vpu_enc->vpuInFrameDesc;

  GST_DEBUG (">>VPU_ENC: mfw_gst_vpuenc_free_sourcebuffer");
  if (framedesc->phy_addr) {
    IOFreeVirtMem (framedesc);
    IOFreePhyMem (framedesc);
  }
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_alloc_sourcebuffer

DESCRIPTION:        Allocate memory for source frame buffer

ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       TRUE (Success)/FALSE (Failure
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
mfw_gst_vpuenc_alloc_sourcebuffer (MfwGstVPU_Enc * vpu_enc)
{
  int framebuffersize;
  int picturesize;
  int picturesizeC = 0;
  int strideC = 0;
  vpu_mem_desc *framedesc = &vpu_enc->vpuInFrameDesc;
  FrameBuffer *frame = &vpu_enc->vpuInFrame;

  picturesize = vpu_enc->width * vpu_enc->height;
  switch (vpu_enc->yuv_proportion) {
    case MFW_GST_VPUENC_YUV_420:
      framebuffersize = picturesize * 3 / 2;
      strideC = vpu_enc->width >> 1;
      picturesizeC = picturesize >> 2;
      break;
    case MFW_GST_VPUENC_YUV_422H:
      framebuffersize = picturesize * 2;
      strideC = vpu_enc->width >> 1;
      picturesizeC = picturesize >> 1;
      break;
    case MFW_GST_VPUENC_YUV_422V:
      framebuffersize = picturesize * 2;
      strideC = vpu_enc->width;
      picturesizeC = picturesize >> 1;
      break;
    case MFW_GST_VPUENC_YUV_444:
      framebuffersize = picturesize * 3;
      strideC = vpu_enc->width;
      picturesizeC = picturesize;
      break;
    case MFW_GST_VPUENC_YUV_400:
      framebuffersize = picturesize;
      break;
    default:
      goto Err;
  }

  framedesc->size = framebuffersize;
  IOGetPhyMem (framedesc);
  if (framedesc->phy_addr == 0) {
    goto Err;
  }
  framedesc->virt_uaddr = IOGetVirtMem (framedesc);
  frame->strideY = vpu_enc->width;
  frame->strideC = strideC;

  frame->bufY = framedesc->phy_addr;
  frame->bufCb = frame->bufY + picturesize;
  frame->bufCr = frame->bufCb + picturesizeC;

  vpu_enc->curr_addr = (guint8 *) framedesc->virt_uaddr;
  vpu_enc->end_addr = (guint8 *) (framedesc->virt_uaddr + framebuffersize);
  vpu_enc->picSizeY = picturesize;
  vpu_enc->picSizeC = picturesizeC;
  vpu_enc->yuv_frame_size = framebuffersize;

  return TRUE;

Err:
  vpu_enc->curr_addr = NULL;
  vpu_enc->end_addr = NULL;
  mfw_gst_vpuenc_free_sourcebuffer (vpu_enc);
  return FALSE;
}



/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_cleanup

DESCRIPTION:        Closes the Encoder and frees all the memory allocated
             
ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
void
mfw_gst_vpuenc_cleanup (MfwGstVPU_Enc * vpu_enc)
{

  int i = 0;
  RetCode vpu_ret = RETCODE_SUCCESS;
  int ret = 0;
  GST_DEBUG (">>VPU_ENC: mfw_gst_vpuenc_cleanup");

  if (vpu_enc->codec == STD_MJPG) {
    if ((vpu_enc->encOP)
        && (vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_hufTable)) {
      g_free (vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_hufTable);
      vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_hufTable = NULL;
    }
    if ((vpu_enc->encOP)
        && (vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_qMatTable)) {
      g_free (vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_qMatTable);
      vpu_enc->encOP->EncStdParam.mjpgParam.mjpg_qMatTable = NULL;
    }
  }

  mfw_gst_vpuenc_free_sourcebuffer (vpu_enc);
  mfw_gst_vpuenc_free_framebuffer (vpu_enc);
  if (vpu_enc->handle > 0) {
    vpu_ret = vpu_EncClose (vpu_enc->handle);
    if (vpu_ret == RETCODE_FRAME_NOT_COMPLETE) {
      vpu_EncGetOutputInfo (vpu_enc->handle, vpu_enc->outputInfo);
      vpu_ret = vpu_EncClose (vpu_enc->handle);
      if (ret < 0) {
        GST_ERROR (">>VPU_ENC: Error %d closing VPU", vpu_ret);
      }
    }
    vpu_enc->handle = 0;
    vpu_enc->vpu_init = FALSE;
  }

  if (vpu_enc->encOP != NULL) {
    g_free (vpu_enc->encOP);
    vpu_enc->encOP = NULL;
  }
  if (vpu_enc->initialInfo != NULL) {
    g_free (vpu_enc->initialInfo);
    vpu_enc->initialInfo = NULL;
  }
  if (vpu_enc->encParam != NULL) {
    g_free (vpu_enc->encParam);
    vpu_enc->encParam = NULL;
  }
  if (vpu_enc->outputInfo != NULL) {
    g_free (vpu_enc->outputInfo);
    vpu_enc->outputInfo = NULL;
  }

  IOFreeVirtMem (&(vpu_enc->bit_stream_buf));
  IOFreePhyMem (&(vpu_enc->bit_stream_buf));

}

GstBuffer *
mfw_gst_vpuenc_packetize_avchdr (guint8 * sps_data, guint sps_size,
    guint8 * pps_data, guint pps_size)
{
  guint data_size = 6 + (2 + (sps_size - NALU_HEADER_SIZE)) +
      1 + (2 + (pps_size - NALU_HEADER_SIZE));
  GstBuffer *codec_data = gst_buffer_new_and_alloc (data_size);
  guint8 *data = GST_BUFFER_DATA (codec_data);
  gint8 i = 0;
  /* unsigned int(8) configurationVersion = 1; */
  data[i++] = 1;
  /* unsigned int(8) AVCProfileIndication; */
  data[i++] = sps_data[NALU_HEADER_SIZE];
  /* unsigned int(8) profile_compatibility; */
  data[i++] = sps_data[NALU_HEADER_SIZE + 1];
  /* unsigned int(8) AVCLevelIndication; */
  data[i++] = sps_data[NALU_HEADER_SIZE + 2];
  /* bit(6) reserved = '111111'b;
   * unsigned int(2) lengthSizeMinusOne; */
  data[i++] = 0xFF;             /* lengthSizeMinusOne = 3 */
  /* bit(3) reserved = '111'b;
   * unsigned int(5) numOfSequenceParameterSets; */
  data[i++] = 0xE1;             /* numOfSequenceParameterSets = 1 */
  /* unsigned int(16) sequenceParameterSetLength ; */
  data[i++] = 0;
  data[i++] = sps_size - NALU_HEADER_SIZE;
  /* bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit; */
  memcpy (data + i, sps_data + NALU_HEADER_SIZE, sps_size - NALU_HEADER_SIZE);
  i += sps_size - NALU_HEADER_SIZE;
  /* unsigned int(8) numOfPictureParameterSets; */
  data[i++] = 0x01;             /* numOfPictureParameterSets = 1 */
  /* unsigned int(16) pictureParameterSetLength; */
  data[i++] = 0;
  data[i++] = pps_size - NALU_HEADER_SIZE;
  /* bit(8*pictureParameterSetLength) pictureParameterSetNALUnit; */
  memcpy (data + i, pps_data + NALU_HEADER_SIZE, pps_size - NALU_HEADER_SIZE);

  return codec_data;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_fill_headers

DESCRIPTION:        Writes the Headers incase of MPEG4 and H.264 before encoding 
                    the first frame.
             
ARGUMENTS PASSED:   vpu_enc - Plug-in context.   

RETURN VALUE:       0 (SUCCESS)/ -1 (FAILURE)
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
int
mfw_gst_vpuenc_fill_headers (MfwGstVPU_Enc * vpu_enc)
{
  EncHeaderParam enchdr_param = { 0 };
  guint8 *ptr;
  guint8 *header[NUM_INPUT_BUF];        // header memory specific to code
  guint headersize[NUM_INPUT_BUF];      // size for each header element
  gint headercount = 0;         // size of headers
  gint hdrsize = 0;
  RetCode vpu_ret = RETCODE_SUCCESS;


  // Must put encode header before encoding output 
  if ((vpu_enc->codec == STD_MPEG4) /* || (vpu_enc->codec == STD_H263) */ ) {
    headercount = 0;
    enchdr_param.headerType = VOS_HEADER;
    vpu_ret =
        vpu_EncGiveCommand (vpu_enc->handle, ENC_PUT_MP4_HEADER, &enchdr_param);
    if (enchdr_param.size == 0) {
      GST_DEBUG
          (">>VPU_ENC: Error %d in Allocating memory for VOS_HEADER size %d",
          vpu_ret, hdrsize);
    } else {
      headersize[headercount] = enchdr_param.size;
      hdrsize += headersize[headercount];
      header[headercount] = g_malloc (enchdr_param.size);
      ptr =
          vpu_enc->start_addr + enchdr_param.buf -
          vpu_enc->bit_stream_buf.phy_addr;
      memcpy (header[headercount], ptr, enchdr_param.size);
      headercount++;
    }

    enchdr_param.headerType = VIS_HEADER;
    vpu_ret =
        vpu_EncGiveCommand (vpu_enc->handle, ENC_PUT_MP4_HEADER, &enchdr_param);
    if (enchdr_param.size == 0) {
      GST_DEBUG (">>VPU_ENC: Error %d in Allocating memory for VIS_HEADER",
          vpu_ret);
    } else {
      header[headercount] = g_malloc (enchdr_param.size);
      headersize[headercount] = enchdr_param.size;
      hdrsize += headersize[headercount];
      ptr =
          vpu_enc->start_addr + enchdr_param.buf -
          vpu_enc->bit_stream_buf.phy_addr;
      memcpy (header[headercount], ptr, enchdr_param.size);
      headercount++;
    }

    enchdr_param.headerType = VOL_HEADER;
    vpu_ret =
        vpu_EncGiveCommand (vpu_enc->handle, ENC_PUT_MP4_HEADER, &enchdr_param);
    if (enchdr_param.size == 0) {
      GST_DEBUG (">>VPU_ENC: Error %d in Allocating memory for VOL_HEADER",
          vpu_ret);
    } else {
      headersize[headercount] = enchdr_param.size;
      hdrsize += headersize[headercount];
      header[headercount] = g_malloc (enchdr_param.size);
      ptr =
          vpu_enc->start_addr + enchdr_param.buf -
          vpu_enc->bit_stream_buf.phy_addr;
      memcpy (header[headercount], ptr, enchdr_param.size);
      headercount++;
    }

  } else if (vpu_enc->codec == STD_AVC) {

    headercount = 2;
    enchdr_param.headerType = SPS_RBSP;
    vpu_ret =
        vpu_EncGiveCommand (vpu_enc->handle, ENC_PUT_AVC_HEADER, &enchdr_param);
    headersize[SPS_HDR] = enchdr_param.size;
    hdrsize += headersize[SPS_HDR];
    header[SPS_HDR] = g_malloc (enchdr_param.size);
    if (header[SPS_HDR] == NULL) {
      GST_ERROR (">>VPU_ENC: Error %d in Allocating memory for SPS_RBSP Header",
          vpu_ret);
      return -1;
    }

    ptr =
        vpu_enc->start_addr + enchdr_param.buf -
        vpu_enc->bit_stream_buf.phy_addr;
    memcpy (header[SPS_HDR], ptr, enchdr_param.size);
    enchdr_param.headerType = PPS_RBSP;
    vpu_ret =
        vpu_EncGiveCommand (vpu_enc->handle, ENC_PUT_AVC_HEADER, &enchdr_param);
    headersize[PPS_HDR] = enchdr_param.size;
    hdrsize += headersize[PPS_HDR];
    header[1] = g_malloc (enchdr_param.size);
    if (header[PPS_HDR] == NULL) {
      GST_ERROR (">>VPU_ENC: Error %d in Allocating memory for PPS_RBSP Header",
          vpu_ret);
      return -1;
    }

    ptr =
        vpu_enc->start_addr + enchdr_param.buf -
        vpu_enc->bit_stream_buf.phy_addr;
    memcpy (header[PPS_HDR], ptr, enchdr_param.size);

    if (FALSE == vpu_enc->avc_byte_stream) {
      vpu_enc->codec_data =
          mfw_gst_vpuenc_packetize_avchdr (header[SPS_HDR], headersize[SPS_HDR],
          header[PPS_HDR], headersize[PPS_HDR]);
    }
  }
  // prepare the header for both first frame and on some I frames
  if (headercount != 0) {
    guint i = 0, offset = 0;
    //unsigned char *bufhdr;
    vpu_enc->hdr_data = gst_buffer_new_and_alloc (hdrsize);

    for (i = 0; i < headercount; i++) {
      memcpy (GST_BUFFER_DATA (vpu_enc->hdr_data) + offset,
          header[i], headersize[i]);
      offset += headersize[i];
      g_free (header[i]);
    }
    //bufhdr = GST_BUFFER_DATA(vpu_enc->hdr_data);
    //g_print (">>VPU_ENC:hdr_data\n");
    //for (i=0;i<GST_BUFFER_SIZE(vpu_enc->hdr_data);i++)
    //    g_print(" %x",*(bufhdr+i));
    //g_print (" \n");

  }
  return 0;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_send_buffer

DESCRIPTION:        Send the buffer downstream.
             
ARGUMENTS PASSED:   vpu_enc   - Plug-in context.   
                    buffer    - output buffer to send

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GstFlowReturn
mfw_gst_vpuenc_send_buffer (MfwGstVPU_Enc * vpu_enc, GstBuffer * buffer)
{
  GstFlowReturn retval = GST_FLOW_OK;
  GstClockTime ts;

  ts = TSManagerSend (vpu_enc->pTS_Mgr);
  GST_BUFFER_TIMESTAMP (buffer) = ts;

  if (vpu_enc->segment_starttime == 0) {
    vpu_enc->segment_starttime = ts;
    vpu_enc->total_time = ts;
  }
  // adjust the buffer duration for time for encoding
  GST_BUFFER_DURATION (buffer) = vpu_enc->time_per_frame;

  if (vpu_enc->forcefixrate && (vpu_enc->segment_encoded_frame > 30) &&
      GST_BUFFER_TIMESTAMP_IS_VALID (buffer) && GST_ELEMENT (vpu_enc)->clock) {
    GstClockTime curr_ts = 0;
    GstClockTimeDiff diff_ts;
    guint num_frame_delay = 0;
    gboolean video_ahead = FALSE;

    // calculate our current time
    curr_ts = gst_clock_get_time (GST_ELEMENT (vpu_enc)->clock);
    curr_ts -= GST_ELEMENT (vpu_enc)->base_time;

    diff_ts = curr_ts - ts;

    // check if we are ahead or behind by how many frames
    if (curr_ts > ts) {
      while (diff_ts > vpu_enc->time_per_frame) {
        num_frame_delay++;
        diff_ts -= vpu_enc->time_per_frame;
      }
      if (num_frame_delay > 1) {
        //GST_DEBUG(">>VPU_ENC: Video is behind %d frames diff %d", num_frame_delay, (guint) diff_ts);
        GST_BUFFER_TIMESTAMP (buffer) = curr_ts;
      }
    } else {
      diff_ts = ts - curr_ts;
      while (diff_ts > vpu_enc->time_per_frame) {
        num_frame_delay++;
        diff_ts -= vpu_enc->time_per_frame;
      }
      if (num_frame_delay > 1) {
        //GST_DEBUG(">>VPU_ENC: Video is ahead %d frames", num_frame_delay);
      }
      video_ahead = TRUE;
    }

    // Drop frame check if current timestamp is later 1 frame                 
    if (vpu_enc->fDropping_till_IFrame || (!video_ahead && (num_frame_delay > 1))) {    //drop one frame as long as it is a delta frame
      if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
        gst_buffer_unref (buffer);

        // if our qp is really low we should probably try increasing it to minimize 
        // dropping frames in next gop
        if ((vpu_enc->fDropping_till_IFrame == FALSE) &&
            (vpu_enc->qp != VPU_INVALID_QP)) {
          vpu_enc->encParam->quantParam++;
          if (vpu_enc->codec == STD_AVC) {
            if (vpu_enc->encParam->quantParam > VPU_MAX_H264_QP)
              vpu_enc->encParam->quantParam = VPU_MAX_H264_QP;
          } else {
            if (vpu_enc->encParam->quantParam > VPU_MAX_MPEG4_QP)
              vpu_enc->encParam->quantParam = VPU_MAX_MPEG4_QP;
          }
        }
        vpu_enc->fDropping_till_IFrame = TRUE;
        //GST_DEBUG (">>VPU_ENC: Dropping frame %d qp = %d", vpu_enc->num_encoded_frames, vpu_enc->encParam->quantParam);
        return GST_FLOW_OK;
      }
    } else if (video_ahead) {
      GstBuffer *tmp;

      // check if we are more than 1 frame ahead
      // check if current time stamp is faster than current time stamp
      while (num_frame_delay > 1) {
        retval = gst_pad_alloc_buffer_and_set_caps (vpu_enc->srcpad, 0,
            4, GST_PAD_CAPS (vpu_enc->srcpad), &tmp);
        if (retval == GST_FLOW_OK) {
          GST_BUFFER_TIMESTAMP (tmp) = vpu_enc->total_time;
          GST_BUFFER_SIZE (tmp) = 0;
          GST_BUFFER_FLAG_SET (tmp, GST_BUFFER_FLAG_DELTA_UNIT);
          retval = gst_pad_push (vpu_enc->srcpad, tmp);
          GST_DEBUG ("sending dummy frame to keep frame rate %d",
              num_frame_delay);
        }
        vpu_enc->segment_encoded_frame++;
        num_frame_delay--;
        vpu_enc->total_time += vpu_enc->time_per_frame;
      }
    }
  }


  GST_LOG (">>VPU_ENC: Pushing buffer size=%d frame %d",
      GST_BUFFER_SIZE (buffer), vpu_enc->segment_encoded_frame);

  retval = gst_pad_push (vpu_enc->srcpad, buffer);
  if (retval != GST_FLOW_OK) {
    GST_ERROR (">>VPU_ENC: Error %d in Pushing the Output to Source Pad",
        retval);
    return retval;
  }

  vpu_enc->total_time += vpu_enc->time_per_frame;
  vpu_enc->segment_encoded_frame++;

  return retval;
}


/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_vpuinitialize

DESCRIPTION:        The main processing function where the data comes in as buffer. This 
                    data is encoded, and then pushed onto the next element for further
                    processing.

ARGUMENTS PASSED:   vpu_enc   - Plug-in context.   
=======================================================================================*/
GstFlowReturn
mfw_gst_vpuenc_vpuinitialize (MfwGstVPU_Enc * vpu_enc)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  GstFlowReturn retval = GST_FLOW_OK;
  GstCaps *caps = NULL;;
  EncInitialInfo initialInfo = { 0 };
  gchar *mime;

  GST_DEBUG (">>VPU_ENC: mfw_gst_vpuenc_vpuinitialize");

  vpu_enc->bytes_consumed = 0;

  if (mfw_gst_vpuenc_set_open_params (vpu_enc) != RETCODE_SUCCESS) {
    mfw_gst_vpuenc_post_fatal_error_msg (vpu_enc,
        "Invalid VPU open parameters.");
    return GST_FLOW_ERROR;
  }
  // open a VPU's encoder instance 
  vpu_ret = vpu_EncOpen (&vpu_enc->handle, vpu_enc->encOP);
  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR (">>VPU_ENC: vpu_EncOpen failed. Error code is %d", vpu_ret);
    mfw_gst_vpuenc_post_fatal_error_msg (vpu_enc, "vpu_EncOpen failed");
    return GST_FLOW_ERROR;
  }
  // configure VPU RAM memory
  if (mfw_gst_vpuenc_configure (vpu_enc) != RETCODE_SUCCESS) {
    return GST_FLOW_ERROR;
  }
  // configure rotation angle and mirror direction
  if (mfw_gst_vpuenc_set_rotation (vpu_enc) != RETCODE_SUCCESS) {
    return GST_FLOW_ERROR;
  }
  // get the min number of framebuffers to be allocated  
  vpu_ret = vpu_EncGetInitialInfo (vpu_enc->handle, &initialInfo);
  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR (">>VPU_ENC: vpu_EncGetInitialInfo failed. Error code is %d",
        vpu_ret);
    mfw_gst_vpuenc_post_fatal_error_msg (vpu_enc,
        "vpu_EncGetInitialInfo failed");
    return GST_FLOW_ERROR;

  }

  if (!mfw_gst_vpuenc_alloc_sourcebuffer (vpu_enc)) {
    GST_ERROR (">>VPU_ENC: Failure allocating source buffers");
    return GST_FLOW_ERROR;
  }
  // save min number of buffers and allocate 
  vpu_enc->numframebufs = initialInfo.minFrameBufferCount;
  GST_DEBUG (">>VPU_ENC: num frame bufs needed is %d", vpu_enc->numframebufs);
  if (vpu_enc->numframebufs > 0) {
    if (!mfw_gst_vpuenc_alloc_framebuffer (vpu_enc)) {
      GST_ERROR (">>VPU_ENC: Failure allocating frame buffers");
      return GST_FLOW_ERROR;
    }
  } else {
    /* FIXME: to avoid vpu_EncRegisterFrameBuffer() failure, set the 
     * vpu_enc->vpuRegisteredFrames even if there is no frame buffer */
    vpu_enc->vpuRegisteredFrames = &vpu_enc->vpuInFrame;
  }

  // register with vpu the allocated frame buffers
#if (VPU_LIB_VERSION_CODE>=VPU_LIB_VERSION(5, 3, 3))
vpu_ret = vpu_EncRegisterFrameBuffer (vpu_enc->handle,
      vpu_enc->vpuRegisteredFrames,
      vpu_enc->numframebufs,
      ((vpu_enc->encOP->picWidth + 15) & ~0xF), vpu_enc->width, NULL, NULL, NULL);
#elif (VPU_LIB_VERSION_CODE>=VPU_LIB_VERSION(5, 3, 0))
  vpu_ret = vpu_EncRegisterFrameBuffer (vpu_enc->handle,
      vpu_enc->vpuRegisteredFrames,
      vpu_enc->numframebufs,
      ((vpu_enc->encOP->picWidth + 15) & ~0xF), vpu_enc->width, NULL, NULL);

#else
  vpu_ret = vpu_EncRegisterFrameBuffer (vpu_enc->handle,
      vpu_enc->vpuRegisteredFrames,
      vpu_enc->numframebufs,
      ((vpu_enc->encOP->picWidth + 15) & ~0xF), vpu_enc->width);
#endif

  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR (">>VPU_ENC: vpu_EncRegisterFrameBuffer failed.Error code is %d",
        vpu_ret);
    mfw_gst_vpuenc_post_fatal_error_msg (vpu_enc,
        "vpu_EncRegisterFrameBuffer failed ");
    return GST_FLOW_ERROR;
  }
  // Set a default parameters and initialize header
  vpu_enc->encParam->forceIPicture = 0;
  vpu_enc->encParam->skipPicture = 0;
  vpu_enc->encParam->enableAutoSkip = 0;

  // Set the crop information.
  vpu_enc->encParam->encTopOffset = vpu_enc->crop_top;
  vpu_enc->encParam->encLeftOffset = vpu_enc->crop_left;

  if (mfw_gst_vpuenc_fill_headers (vpu_enc) < 0) {
    mfw_gst_vpuenc_post_fatal_error_msg (vpu_enc,
        "Allocation for Headers failed ");
    return GST_FLOW_ERROR;
  }
  // set up source pad capabilities
  if (vpu_enc->codec == STD_MPEG4)
    mime = "video/mpeg";
  else if (vpu_enc->codec == STD_AVC)
    mime = "video/x-h264";
  else if (vpu_enc->codec == STD_H263)
    mime = "video/x-h263";
  else if (vpu_enc->codec == STD_MJPG)
    mime = "image/jpeg";

  caps = gst_caps_new_simple (mime,
      "height", G_TYPE_INT, vpu_enc->encOP->picHeight,
      "width", G_TYPE_INT, vpu_enc->encOP->picWidth,
      "framerate", GST_TYPE_FRACTION,
      vpu_enc->framerate_n, vpu_enc->framerate_d, NULL);

  // some payloaders later do not provide our headers
  // but if we provide it as codec_data it will get through
  if (vpu_enc->codec == STD_MPEG4) {
    gst_caps_set_simple (caps,
        "mpegversion", G_TYPE_INT, 4,
        "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
  } else if (vpu_enc->codec == STD_AVC) {
    if (FALSE == vpu_enc->avc_byte_stream) {
      gst_caps_set_simple (caps,
          "codec_data", GST_TYPE_BUFFER,
          (NULL != vpu_enc->codec_data) ?
          vpu_enc->codec_data : vpu_enc->hdr_data, NULL);
    }
  }

  gst_pad_set_caps (vpu_enc->srcpad, caps);
  GST_INFO ("%" GST_PTR_FORMAT, caps);

  // we are initialized!
  gst_caps_unref (caps);
  vpu_enc->vpu_init = TRUE;

  return retval;
}


/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_copy_sink_start_frame

DESCRIPTION:        This copies the sink buffer into the VPU and starts a frame

ARGUMENTS PASSED:   vpu_enc   - Plug-in context.   
                    buffer - pointer to the input buffer which has the YUV data.

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
GstFlowReturn
mfw_gst_vpuenc_copy_sink_start_frame (MfwGstVPU_Enc * vpu_enc)
{
  RetCode vpu_ret = RETCODE_SUCCESS;
  guint to_copy = GST_BUFFER_SIZE (vpu_enc->gst_buffer) - vpu_enc->gst_copied;

  guint8 *end_addr = vpu_enc->curr_addr + to_copy;
  guint8 *start = (guint8 *) vpu_enc->vpuInFrameDesc.virt_uaddr;


  // We have 3 scenarios
  // 1)  Each input buffer is exactly the YUV size of width*height*3/2 - 
  //        happens with camera as source
  // 2)  Each input buffer is smaller than the YUV size so must take multiple copies
  //       default of filesink is 4096.  For last part of last buffer - using bytes_consumed
  //       you can copy the end to the beginning of buffer if needed
  // 3)  Each input buffer is larger than the YUV size so _chain routine must loop until
  //       all YUV buffers have been consumed before unref of input buffer
  //       can simulate this by setting blocksize=<> on filesrc

  // handle encoding from file in case we have more to copy than space available
  if (end_addr > vpu_enc->end_addr) {
    // can only copy part of the buffer
    //GST_DEBUG (">>VPU_ENC:  getting ready for wrapping input buffer");
    to_copy = vpu_enc->end_addr - vpu_enc->curr_addr;
  }
  // only copy maximum of yuv frame size minus what is already copied - to keep just frame size in buff
  if ((to_copy + vpu_enc->bytes_consumed) > vpu_enc->yuv_frame_size) {
    to_copy = vpu_enc->yuv_frame_size - vpu_enc->bytes_consumed;
  }
  // Setup VPU with the input source buffer
  vpu_enc->encParam->sourceFrame = &(vpu_enc->vpuInFrame);
  if (!vpu_enc->bytes_consumed && IS_DMABLE_BUFFER (vpu_enc->gst_buffer)) {
    GST_DEBUG (">>VPU_ENC:  direct from sink");
    vpu_enc->vpuInFrame.bufY = DMABLE_BUFFER_PHY_ADDR (vpu_enc->gst_buffer);
    if (MFW_GST_VPUENC_PLANAR_Y_V_U == vpu_enc->yuv_planar) {
      vpu_enc->vpuInFrame.bufCr = vpu_enc->vpuInFrame.bufY + vpu_enc->picSizeY;
      vpu_enc->vpuInFrame.bufCb = vpu_enc->vpuInFrame.bufCr + vpu_enc->picSizeC;
    } else {
      vpu_enc->vpuInFrame.bufCb = vpu_enc->vpuInFrame.bufY + vpu_enc->picSizeY;
      vpu_enc->vpuInFrame.bufCr = vpu_enc->vpuInFrame.bufCb + vpu_enc->picSizeC;
    }
    gst_buffer_unref (vpu_enc->gst_buffer);
    vpu_enc->gst_buffer = NULL;
  } else {
    // we must memcpy input
    GST_DEBUG (">>VPU_ENC:  Memcpy %d from input", to_copy);
    vpu_enc->vpuInFrame.bufY =
        (PhysicalAddress) vpu_enc->vpuInFrameDesc.phy_addr;
    if (MFW_GST_VPUENC_PLANAR_Y_V_U == vpu_enc->yuv_planar) {
      vpu_enc->vpuInFrame.bufCr = vpu_enc->vpuInFrame.bufY + vpu_enc->picSizeY;
      vpu_enc->vpuInFrame.bufCb = vpu_enc->vpuInFrame.bufCb + vpu_enc->picSizeC;
    } else {
      vpu_enc->vpuInFrame.bufCb = vpu_enc->vpuInFrame.bufY + vpu_enc->picSizeY;
      vpu_enc->vpuInFrame.bufCr = vpu_enc->vpuInFrame.bufCb + vpu_enc->picSizeC;
    }
    memcpy (vpu_enc->curr_addr,
        GST_BUFFER_DATA (vpu_enc->gst_buffer) + vpu_enc->gst_copied, to_copy);

    vpu_enc->gst_copied += to_copy;

    // do not bother with frame rate adjustment if not from camera source
    vpu_enc->forcefixrate = FALSE;

    // if all of buffer is consumed we can release it 
    if (vpu_enc->gst_copied == GST_BUFFER_SIZE (vpu_enc->gst_buffer)) {
      gst_buffer_unref (vpu_enc->gst_buffer);
      vpu_enc->gst_buffer = NULL;
      vpu_enc->gst_copied = 0;
    }
  }

  vpu_enc->bytes_consumed += to_copy;
  vpu_enc->curr_addr += to_copy;

  //GST_DEBUG (">>VPU_ENC: yuvsize %d bytes_consumed %d curr_addr 0x%x", vpu_enc->yuv_frame_size, vpu_enc->bytes_consumed, vpu_enc->curr_addr);

  // make sure we aren't starting encoding until we have a full frame size to encode.
  // if not return and wait for more data to copy before starting a frame encode
  if (vpu_enc->bytes_consumed < vpu_enc->yuv_frame_size)
    return GST_FLOW_OK;

  // we might have copied more than a frame so after encode is complete we'll move this from end of buffer
  // and put it at the beginning
  vpu_enc->bytes_consumed -= vpu_enc->yuv_frame_size;

  // all of buffer is consumed so next encode will start at beginning of buffer
  if (vpu_enc->bytes_consumed == 0)
    vpu_enc->curr_addr = start;
  else
    vpu_enc->curr_addr -= vpu_enc->bytes_consumed;      // reset to move to top later

  //GST_DEBUG (">>VPU_ENC: Start Decode but %d for next frame", vpu_enc->bytes_consumed);

  // This handles actual skipping - if our target frame rate is lower than our src frame rate
  // we must tell VPU to skip the picture and encode as a small P frame
  // for each of these downsizing frame rates there is an interval of which some will be encoded then
  // the rest skipped.  The num_enc_in_interval is the amount of encodes in interval before skipping 
  // like 20fps encodes 2 then skips 1 with an interval of 3
  // or 15fps encodes 1 then skips 1 with an interval of 2
  // or 10fps encodes 1 then skips 3 with an interval of 4.
  if (vpu_enc->num_enc_in_interval) {
    gboolean skipPicture = FALSE;
    if (vpu_enc->idx_interval
        && (vpu_enc->idx_interval > vpu_enc->num_enc_in_interval))
      skipPicture = TRUE;
    if (vpu_enc->idx_interval == vpu_enc->num_in_interval)
      vpu_enc->idx_interval = 1;
    else
      vpu_enc->idx_interval++;
    if (skipPicture) {
      vpu_enc->num_total_frames++;
      if (vpu_enc->ts_rx == 0)
        vpu_enc->ts_rx = TIMESTAMP_INDEX_MASK - 1;
      else
        vpu_enc->ts_rx--;
      //GST_DEBUG (">>VPU_ENC: Skipping picture %d", vpu_enc->num_total_frames);
      return GST_FLOW_OK;
    }
  }
#ifdef GST_LOGTIME
  if (GST_ELEMENT (vpu_enc)->clock) {
    vpu_enc->enc_start = gst_clock_get_time (GST_ELEMENT (vpu_enc)->clock);
    vpu_enc->enc_start -= GST_ELEMENT (vpu_enc)->base_time;
  }
#endif

  // Start encoding of one frame
  vpu_ret = vpu_EncStartOneFrame (vpu_enc->handle, vpu_enc->encParam);
  if (vpu_ret != RETCODE_SUCCESS) {
    GST_ERROR (">>VPU_ENC: vpu_EncStartOneFrame failed. Error code is %d",
        vpu_ret);
    return GST_FLOW_ERROR;
  }
  vpu_enc->is_frame_started = TRUE;
  return GST_FLOW_OK;
}


static guint8 *
mfw_gst_vpuenc_find_nalu_startcode (guint8 * buffer, guint8 * end)
{
  guint8 *buf_pos = buffer;
  guint8 *buf_end = end;
  guint32 start_code = -1;

  while (buf_pos < buf_end) {
    start_code = ((start_code << 8) + (*buf_pos));
    buf_pos++;
    if (AVC_NALU_START_CODE == start_code) {
      break;
    }
    /* we suppose VPU encoded H.264 stream has NALU start code size of 4 */
    if (AVC_NALU_START_CODE == (start_code & 0x00FFFFFF)) {
      GST_ERROR ("Size of NALU start code == 3 is not support");
      buf_pos = NULL;
      break;
    }
  }

  return buf_pos;
}


gboolean
mfw_gst_vpuenc_nalu_stream (guint8 * dst_buf, guint32 dst_size,
    guint8 * src_buf, guint32 src_size)
{
  guint8 *src = src_buf;
  guint32 size = src_size;
  guint8 *end = src_buf + src_size;
  guint8 *dst = dst_buf;
  guint8 *nalu = NULL;
  guint32 nalu_size = 0;
  guint8 *nalu_size_ptr = (guint8 *) (&nalu_size);
  guint32 start_code_size = 0;
  gboolean ret = TRUE;

  nalu = mfw_gst_vpuenc_find_nalu_startcode (src_buf, end);
  if (src == NULL) {
    return FALSE;
  }

  while (src < end) {
    src = mfw_gst_vpuenc_find_nalu_startcode (src, end);
    if (src == NULL) {
      ret = FALSE;
      break;
    }
    nalu_size = src - nalu;
    if (src < end) {
      nalu_size -= 4;
    }

    *(dst++) = nalu_size_ptr[3];
    *(dst++) = nalu_size_ptr[2];
    *(dst++) = nalu_size_ptr[1];
    *(dst++) = nalu_size_ptr[0];
    memcpy (dst, nalu, nalu_size);
    dst += nalu_size;

    nalu = src;
  }

  return ret;
}



/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_chain

DESCRIPTION:        The main processing function where the data comes in as buffer. This 
                    data is encoded, and then pushed onto the next element for further
                    processing.

ARGUMENTS PASSED:   pad - pointer to the sinkpad of this element
                    buffer - pointer to the input buffer which has the raw data.

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static GstFlowReturn
mfw_gst_vpuenc_chain (GstPad * pad, GstBuffer * buffer)
{
  MfwGstVPU_Enc *vpu_enc = MFW_GST_VPU_ENC (GST_PAD_PARENT (pad));
  RetCode vpu_ret = RETCODE_SUCCESS;
  GstFlowReturn retval = GST_FLOW_OK;
  GstCaps *src_caps = GST_PAD_CAPS (vpu_enc->srcpad);
  GstBuffer *outbuffer = NULL;
  gboolean frame_started = vpu_enc->is_frame_started;
  gint i = 0;

  // Initialize VPU if first time
  if (vpu_enc->vpu_init == FALSE) {
    retval = mfw_gst_vpuenc_vpuinitialize (vpu_enc);
    if (retval != GST_FLOW_OK) {
      GST_ERROR (">>VPU_ENC: mfw_gst_vpuenc_vpuinitialize error is %d", retval);
      return retval;
    }
  }

  vpu_enc->gst_copied = 0;
  vpu_enc->gst_buffer = buffer;

  TSManagerReceive (vpu_enc->pTS_Mgr, GST_BUFFER_TIMESTAMP (buffer));

  if (TRUE == vpu_enc->forcekeyframe) {
    GstStructure *s;
    GstClockTime stream_time, timestamp, running_time;

    timestamp = GST_BUFFER_TIMESTAMP (buffer);
    running_time =
        gst_segment_to_running_time (&vpu_enc->segment, GST_FORMAT_TIME,
        timestamp);
    if ((gint64) running_time < 0) {
      GST_DEBUG_OBJECT (vpu_enc,
          "Dropping buffer, timestamp: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (timestamp));
      gst_buffer_unref (buffer);
      return GST_FLOW_OK;
    }

    stream_time = gst_segment_to_stream_time (&vpu_enc->segment,
        GST_FORMAT_TIME, timestamp);

    s = gst_structure_new ("GstForceKeyUnit",
        "timestamp", G_TYPE_UINT64, timestamp,
        "stream-time", G_TYPE_UINT64, stream_time,
        "running-time", G_TYPE_UINT64, running_time, NULL);

    vpu_enc->encParam->forceIPicture = 1;

    gst_pad_push_event (vpu_enc->srcpad,
        gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s));

    GST_OBJECT_LOCK (vpu_enc);
    vpu_enc->forcekeyframe = FALSE;
    GST_OBJECT_UNLOCK (vpu_enc);
  } else {
    vpu_enc->encParam->forceIPicture = 0;
  }


  // A frame was not started, copy data, and start frame and exit
  if (!frame_started) {
    if (vpu_enc->setbitrate) {
      vpu_ret = vpu_EncGiveCommand (vpu_enc->handle,
          ENC_SET_BITRATE, &vpu_enc->bitrate);
      if (RETCODE_SUCCESS == vpu_ret)
        vpu_enc->setbitrate = FALSE;
    }

    vpu_ret = mfw_gst_vpuenc_copy_sink_start_frame (vpu_enc);
    if (vpu_enc->gst_buffer) {
      if (vpu_ret != GST_FLOW_OK)
        return vpu_ret;
    } else if (!vpu_enc->loopback) {
      if (vpu_enc->num_total_frames == 0)
        return GST_FLOW_OK;
    }
  }
  // if frame started get output - loop until all of input buffer has been consumed
  do {
    gint offset = 0;
    gboolean isIFrame = FALSE;

    // Wait for the VPU to complete the Processing from previous frame
    if (vpu_enc->is_frame_started == TRUE) {
      gfloat avg_bitrate_per_sec = 0.0, avg_bitrate_per_stream = 0.0;

      vpu_ret = RETCODE_FAILURE;
      if (frame_started) {
        // Now get the output
        vpu_ret = vpu_EncGetOutputInfo (vpu_enc->handle, vpu_enc->outputInfo);

      }
      if (vpu_ret != RETCODE_SUCCESS) {
        while (vpu_IsBusy ()) {
          vpu_WaitForInt (100);
        }
        // Now get the output
        vpu_ret = vpu_EncGetOutputInfo (vpu_enc->handle, vpu_enc->outputInfo);
      }

      if (vpu_ret != RETCODE_SUCCESS) {
        GST_ERROR
            (">>VPU_ENC: vpu_EncGetOutputInfo failed in chain. Error code is %d",
            vpu_ret);
        return GST_FLOW_ERROR;
      }
#ifdef GST_LOGTIME
      if (GST_ELEMENT (vpu_enc)->clock) {
        isIFrame = ((vpu_enc->outputInfo->picType & 3) == 0);
        vpu_enc->enc_stop = gst_clock_get_time (GST_ELEMENT (vpu_enc)->clock);
        vpu_enc->enc_stop -= GST_ELEMENT (vpu_enc)->base_time;
        if (isIFrame)
          GST_LOGTIME (">>VPU_ENC: I frame encoding time is %d usec",
              (guint) (vpu_enc->enc_stop - vpu_enc->enc_start) / 1000);
        else
          GST_LOGTIME (">>VPU_ENC: Delta frame encoding time is %d usec",
              (guint) (vpu_enc->enc_stop - vpu_enc->enc_start) / 1000);
      }
#endif

      vpu_enc->is_frame_started = FALSE;

      vpu_enc->num_total_frames++;
      vpu_enc->num_encoded_frames++;

      // Calculate two bitrates - 
      // first is the bitrate per frame rate so kbits per second - add in over num frames or frame rate 
      // second is average over stream.

      vpu_enc->bits_per_second += vpu_enc->outputInfo->bitstreamSize;
      vpu_enc->frame_size[vpu_enc->fs_write] =
          vpu_enc->outputInfo->bitstreamSize;
      vpu_enc->fs_write = (vpu_enc->fs_write + 1) & TIMESTAMP_INDEX_MASK;
      if (vpu_enc->num_encoded_frames > (guint) (vpu_enc->tgt_framerate + 0.5)) {       // subtract oldest frame size beyond scope of last num frames per sec
        vpu_enc->bits_per_second -= vpu_enc->frame_size[vpu_enc->fs_read];
        vpu_enc->fs_read = (vpu_enc->fs_read + 1) & TIMESTAMP_INDEX_MASK;
        //GST_FRAMEDBG(">>VPU_ENC: bits_per sec %d read %d write %d", vpu_enc->bits_per_second, vpu_enc->fs_read, vpu_enc->fs_write);
      }
      avg_bitrate_per_sec = (vpu_enc->bits_per_second << 3) / 1024;     // bits needed so multiply by 8 multiple by 1K
      if (avg_bitrate_per_sec > 1.0)
        GST_FRAMEDBG (">>VPU_ENC: AVG BITRATE PER SEC is %f Target is %d",
            avg_bitrate_per_sec, vpu_enc->bitrate);

      vpu_enc->bits_per_stream += vpu_enc->outputInfo->bitstreamSize;
      avg_bitrate_per_stream =
          ((vpu_enc->bits_per_stream << 3) / vpu_enc->num_encoded_frames) *
          vpu_enc->tgt_framerate;
      avg_bitrate_per_stream = avg_bitrate_per_stream / 1024;

      if (vpu_enc->outputInfo->picType > 2)
        return GST_FLOW_OK;

      // decide when to send headers.   We used to only send them on the first frame
      // but found out that in networking mode they need to be sent on every I frame
      // and in general every 60 frames.
      isIFrame = ((vpu_enc->outputInfo->picType & 3) == 0);
      if (isIFrame) {
        // if last gop had no dropped frames and qp was increased - drop it down
        if (!vpu_enc->fDropping_till_IFrame &&
            (vpu_enc->qp != VPU_INVALID_QP) &&
            (vpu_enc->encParam->quantParam != vpu_enc->qp)) {
          vpu_enc->encParam->quantParam = vpu_enc->qp;
        }
        vpu_enc->fDropping_till_IFrame = FALSE;
        GST_FRAMEDBG
            (">>VPU_ENC IFRAME ==>>frame size %d ==>>stream AVG BIT RATE %f \n QP=%d MAX_QP=%d GAMMA=%d gop_size=%d",
            vpu_enc->outputInfo->bitstreamSize, avg_bitrate_per_stream,
            vpu_enc->qp, vpu_enc->max_qp, vpu_enc->gamma, vpu_enc->gopsize);
      } else {
        vpu_enc->frames_since_hdr_sent++;
        GST_FRAMEDBG
            (">>VPU_ENC Delta frame ==>>frame size %d ==>>stream AVG BIT RATE  %f",
            vpu_enc->outputInfo->bitstreamSize, avg_bitrate_per_stream);
      }

#define MAX_HDR_INTERVAL 60
      //GST_DEBUG (" encoder output info pictype=%x cnt=%d", vpu_enc->outputInfo->picType, vpu_enc->frames_since_hdr_sent);

      if ((vpu_enc->num_encoded_frames == 1) || isIFrame)       // on first frame send headers down
        //(isIFrame && (vpu_enc->frames_since_hdr_sent >= MAX_HDR_INTERVAL)))
      {
        vpu_enc->frames_since_hdr_sent = 1;     // reset frames in gop
        if (vpu_enc->hdr_data)
          offset = GST_BUFFER_SIZE (vpu_enc->hdr_data);
      }

      src_caps = GST_PAD_CAPS (vpu_enc->srcpad);
      // Allocate a buffer - if first frame totalsize includes header size
      retval = gst_pad_alloc_buffer_and_set_caps (vpu_enc->srcpad, 0,
          vpu_enc->outputInfo->bitstreamSize + offset, src_caps, &outbuffer);
      if (retval != GST_FLOW_OK) {
        GST_ERROR (">>VPU_ENC: Error %d in allocating the srcpad buffer",
            retval);
        return retval;
      }
      // Copy VPU encoded frame  - for first frame do it after headers (offset)
      if (vpu_enc->avc_byte_stream) {
        // For first frame copy over header data
        if (offset)             // on first frame send headers down
        {
          memcpy (GST_BUFFER_DATA (outbuffer),
              GST_BUFFER_DATA (vpu_enc->hdr_data), offset);
        } else
          offset = 0;

        memcpy (GST_BUFFER_DATA (outbuffer) + offset, vpu_enc->start_addr,
            vpu_enc->outputInfo->bitstreamSize);
      } else {
        // For first frame copy over header data
        if (offset)             // on first frame send headers down
        {
          retval = mfw_gst_vpuenc_nalu_stream (GST_BUFFER_DATA (outbuffer),
              GST_BUFFER_SIZE (outbuffer),
              GST_BUFFER_DATA (vpu_enc->hdr_data),
              GST_BUFFER_SIZE (vpu_enc->hdr_data));
        }

        retval =
            mfw_gst_vpuenc_nalu_stream (GST_BUFFER_DATA (outbuffer) + offset,
            GST_BUFFER_SIZE (outbuffer) - offset, vpu_enc->start_addr,
            vpu_enc->outputInfo->bitstreamSize);
        if (retval == FALSE) {
          retval = GST_FLOW_ERROR;
          GST_ERROR
              (">>VPU_ENC: Can not convert H.264 byte stream to NAL unit stream");
          return retval;
        }
      }

      // Set the buffer size of the output buffer plus headers if first frame
      GST_BUFFER_SIZE (outbuffer) = vpu_enc->outputInfo->bitstreamSize + offset;

      // if this is not an I frame mark it
      if (vpu_enc->outputInfo->picType) {
        GST_BUFFER_FLAG_SET (outbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
      }
      // send the output buffer downstream
      retval = mfw_gst_vpuenc_send_buffer (vpu_enc, outbuffer);

      vpu_enc->curr_addr = (guint8 *) vpu_enc->vpuInFrameDesc.virt_uaddr;
    }
    // In the case of a frame started we must copy and start a frame before leaving
    // or if we have a buffer not completely consumed - must keep looping 
    if (vpu_enc->gst_buffer && (retval == GST_FLOW_OK)) {
      retval = mfw_gst_vpuenc_copy_sink_start_frame (vpu_enc);
    }
  } while (vpu_enc->gst_buffer && (retval == GST_FLOW_OK));

done:
  // if error happened be sure to release the input buffer
  if (vpu_enc->gst_buffer) {
    gst_buffer_unref (vpu_enc->gst_buffer);
  }
  return retval;
}

/*=============================================================================
FUNCTION:           CalcMJPEGQuantTables

DESCRIPTION:        recalculate the mjpeg quant table for different bitrate encoding.

ARGUMENTS PASSED:   pointers to huffman and quantization tables

RETURN VALUE:       0 (SUCCESS)/ -1 (FAILURE)
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/

static void
mfw_gst_CalcMJPEGQuantTables (MfwGstVPU_Enc * vpu_enc)
{

  gint i;
  guint temp, new_quality;
  guint m_iFrameHeight, m_iFrameWidth, m_iBitRate, quality, compress_ratio;
  gfloat m_iFrameRate;


  quality = 93;
  compress_ratio = 3;
  new_quality = 0;
  m_iFrameHeight = vpu_enc->height;
  m_iFrameWidth = vpu_enc->width;
  m_iBitRate = vpu_enc->bitrate;
  m_iFrameRate = vpu_enc->tgt_framerate;
  // found all vailable information
  if ((INVALID_BITRATE == m_iBitRate) ||
      (INVALID_FRAME_RATE == m_iFrameRate) ||
      (INVALID_WIDTH == m_iFrameWidth) || (INVALID_HEIGHT == m_iFrameHeight)) {
    // use the defaul table, does not do anything.
    return;
  }
  // compression ratio formula 
  //          W * H * fps * 12
  // ratio = -----------------------
  //              bps
  compress_ratio =
      ((long) m_iFrameHeight * m_iFrameWidth * m_iFrameRate * 12) /
      ((long) m_iBitRate * 1024);
  GST_DEBUG ("width = %d, height = %d", m_iFrameWidth, m_iFrameHeight);
  GST_DEBUG ("encode frame rate m_iFrameRate = %f %d", m_iFrameRate,
      m_iBitRate);
  GST_DEBUG ("compress ratio compress_ratio = %d %d", compress_ratio,
      m_iBitRate);

  // look up curve -- compress_ratio VS Quality to get a reasonable value of Quality

  if (CompressRatioTable[0] <= compress_ratio) {
    quality = QualityLookupTable[0];
  } else if (CompressRatioTable[LOOKUP_TAB_MAX - 1] >= compress_ratio) {
    quality = QualityLookupTable[LOOKUP_TAB_MAX - 1];
  } else {
    gint k;
    for (k = 0; k < LOOKUP_TAB_MAX - 2; k++) {
      if ((CompressRatioTable[k] > compress_ratio)
          && (CompressRatioTable[k + 1] <= compress_ratio)) {
        // compress_ratio is in this area
        temp = (QualityLookupTable[k + 1] - QualityLookupTable[k]) *
            (compress_ratio - CompressRatioTable[k + 1]) /
            (CompressRatioTable[k] - CompressRatioTable[k + 1]);
        quality = QualityLookupTable[k + 1] - temp;
      }
    }
  }

  // re-calculate the Q-matrix

  if (50 > quality)
    new_quality = 5000 / quality;
  else
    new_quality = 200 - 2 * quality;

  GST_DEBUG ("quality = %d %d", quality, new_quality);

  // recalculate  luma Quantification table
  for (i = 0; i < 64; i++) {
    temp = ((unsigned int) lumaQ2[i] * new_quality + 50) / 100;
    if (temp <= 0)
      temp = 1;
    if (temp > 255)
      temp = 255;

    lumaQ2[i] = (unsigned char) temp;
  }
  GST_DEBUG ("Luma Quant Table is ");
  for (i = 0; i < 64; i = i + 8) {
    GST_DEBUG ("0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, ",
        lumaQ2[i], lumaQ2[i + 1], lumaQ2[i + 2], lumaQ2[i + 3],
        lumaQ2[i + 4], lumaQ2[i + 5], lumaQ2[i + 6], lumaQ2[i + 7]);
  }

  // chromaB Quantification Table
  for (i = 0; i < 64; i++) {
    temp = ((unsigned int) chromaBQ2[i] * new_quality + 50) / 100;
    if (temp <= 0)
      temp = 1;
    if (temp > 255)
      temp = 255;

    chromaBQ2[i] = (unsigned char) temp;
  }
  GST_DEBUG ("chromaB Quantification Table is ");
  for (i = 0; i < 64; i = i + 8) {
    GST_DEBUG ("0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, ",
        chromaBQ2[i], chromaBQ2[i + 1], chromaBQ2[i + 2], chromaBQ2[i + 3],
        chromaBQ2[i + 4], chromaBQ2[i + 5], chromaBQ2[i + 6], chromaBQ2[i + 7]);
  }

  // chromaR Quantification Table
  for (i = 0; i < 64; i++) {
    temp = ((unsigned int) chromaRQ2[i] * new_quality + 50) / 100;
    if (temp <= 0)
      temp = 1;
    if (temp > 255)
      temp = 255;

    chromaRQ2[i] = (unsigned char) temp;
  }
  GST_DEBUG ("chromaR Quantification Table is ");
  for (i = 0; i < 64; i = i + 8) {
    GST_DEBUG ("0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, ",
        chromaRQ2[i], chromaRQ2[i + 1], chromaRQ2[i + 2], chromaRQ2[i + 3],
        chromaRQ2[i + 4], chromaRQ2[i + 5], chromaRQ2[i + 6], chromaRQ2[i + 7]);
  }

  return;
}


/*=============================================================================
FUNCTION:           mfw_gst_vpudec_generate_jpeg_tables

DESCRIPTION:        Generate JPEG tables for MJPEG support

ARGUMENTS PASSED:   pointers to huffman and quantization tables

RETURN VALUE:       0 (SUCCESS)/ -1 (FAILURE)
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean
mfw_gst_vpudec_generate_jpeg_tables (MfwGstVPU_Enc * vpu_enc,
    Uint8 * *pphuftable, Uint8 * *ppqmattable)
{
  Uint8 *qMatTable;
  Uint8 *huffTable;
  int i;

  huffTable = g_malloc (VPU_HUFTABLE_SIZE);
  qMatTable = g_malloc (VPU_QMATTABLE_SIZE);

  if ((!huffTable) || (!qMatTable))
    goto Err;

  memset (huffTable, 0, VPU_HUFTABLE_SIZE);
  memset (qMatTable, 0, VPU_QMATTABLE_SIZE);

  for (i = 0; i < 16; i += 4) {
    huffTable[i] = lumaDcBits[i + 3];
    huffTable[i + 1] = lumaDcBits[i + 2];
    huffTable[i + 2] = lumaDcBits[i + 1];
    huffTable[i + 3] = lumaDcBits[i];
  }
  for (i = 16; i < 32; i += 4) {
    huffTable[i] = lumaDcValue[i + 3 - 16];
    huffTable[i + 1] = lumaDcValue[i + 2 - 16];
    huffTable[i + 2] = lumaDcValue[i + 1 - 16];
    huffTable[i + 3] = lumaDcValue[i - 16];
  }
  for (i = 32; i < 48; i += 4) {
    huffTable[i] = lumaAcBits[i + 3 - 32];
    huffTable[i + 1] = lumaAcBits[i + 2 - 32];
    huffTable[i + 2] = lumaAcBits[i + 1 - 32];
    huffTable[i + 3] = lumaAcBits[i - 32];
  }
  for (i = 48; i < 216; i += 4) {
    huffTable[i] = lumaAcValue[i + 3 - 48];
    huffTable[i + 1] = lumaAcValue[i + 2 - 48];
    huffTable[i + 2] = lumaAcValue[i + 1 - 48];
    huffTable[i + 3] = lumaAcValue[i - 48];
  }
  for (i = 216; i < 232; i += 4) {
    huffTable[i] = chromaDcBits[i + 3 - 216];
    huffTable[i + 1] = chromaDcBits[i + 2 - 216];
    huffTable[i + 2] = chromaDcBits[i + 1 - 216];
    huffTable[i + 3] = chromaDcBits[i - 216];
  }
  for (i = 232; i < 248; i += 4) {
    huffTable[i] = chromaDcValue[i + 3 - 232];
    huffTable[i + 1] = chromaDcValue[i + 2 - 232];
    huffTable[i + 2] = chromaDcValue[i + 1 - 232];
    huffTable[i + 3] = chromaDcValue[i - 232];
  }
  for (i = 248; i < 264; i += 4) {
    huffTable[i] = chromaAcBits[i + 3 - 248];
    huffTable[i + 1] = chromaAcBits[i + 2 - 248];
    huffTable[i + 2] = chromaAcBits[i + 1 - 248];
    huffTable[i + 3] = chromaAcBits[i - 248];
  }
  for (i = 264; i < 432; i += 4) {
    huffTable[i] = chromaAcValue[i + 3 - 264];
    huffTable[i + 1] = chromaAcValue[i + 2 - 264];
    huffTable[i + 2] = chromaAcValue[i + 1 - 264];
    huffTable[i + 3] = chromaAcValue[i - 264];
  }


  /* accoring to the bitrate, recalculate the quant table */
  mfw_gst_CalcMJPEGQuantTables (vpu_enc);


  /* Rearrange and insert pre-defined Q-matrix to deticated variable. */
  for (i = 0; i < 64; i += 4) {
    qMatTable[i] = lumaQ2[i + 3];
    qMatTable[i + 1] = lumaQ2[i + 2];
    qMatTable[i + 2] = lumaQ2[i + 1];
    qMatTable[i + 3] = lumaQ2[i];
  }
  for (i = 64; i < 128; i += 4) {
    qMatTable[i] = chromaBQ2[i + 3 - 64];
    qMatTable[i + 1] = chromaBQ2[i + 2 - 64];
    qMatTable[i + 2] = chromaBQ2[i + 1 - 64];
    qMatTable[i + 3] = chromaBQ2[i - 64];
  }
  for (i = 128; i < 192; i += 4) {
    qMatTable[i] = chromaRQ2[i + 3 - 128];
    qMatTable[i + 1] = chromaRQ2[i + 2 - 128];
    qMatTable[i + 2] = chromaRQ2[i + 1 - 128];
    qMatTable[i + 3] = chromaRQ2[i - 128];
  }

  *pphuftable = huffTable;
  *ppqmattable = qMatTable;
  return TRUE;

Err:
  if (huffTable)
    g_free (huffTable);
  if (qMatTable)
    g_free (qMatTable);
  return FALSE;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_change_state

DESCRIPTION:        This function keeps track of different states of pipeline.

ARGUMENTS PASSED:
                element     -   pointer to element
                transition  -   state of the pipeline

RETURN VALUE:
                GST_STATE_CHANGE_FAILURE    - the state change failed
                GST_STATE_CHANGE_SUCCESS    - the state change succeeded
                GST_STATE_CHANGE_ASYNC      - the state change will happen
                                                asynchronously
                GST_STATE_CHANGE_NO_PREROLL - the state change cannot be prerolled

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static GstStateChangeReturn
mfw_gst_vpuenc_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  MfwGstVPU_Enc *vpu_enc = MFW_GST_VPU_ENC (element);
  gint vpu_ret = 0;
  gfloat time_val = 0.0;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    {
      vpu_versioninfo ver;

      GST_DEBUG (">>VPU_ENC: VPU State: Null to Ready");
      vpu_ret = vpu_Init (NULL);
      if (vpu_ret < 0) {
        GST_DEBUG (">>VPU_ENC: Error %d in initializing the VPU", vpu_ret);
        return GST_STATE_CHANGE_FAILURE;
      }

      vpu_ret = vpu_GetVersionInfo (&ver);
      if (vpu_ret) {
        GST_DEBUG (">>VPU_ENC: Error %d in geting the VPU version", vpu_ret);
        vpu_UnInit ();
        return GST_STATE_CHANGE_FAILURE;
      }


      g_print (YELLOW_STR
          ("VPU Version: firmware %d.%d.%d; libvpu: %d.%d.%d \n", ver.fw_major,
              ver.fw_minor, ver.fw_release, ver.lib_major, ver.lib_minor,
              ver.lib_release));

      if (VPU_LIB_VERSION (ver.lib_major, ver.lib_minor,
              ver.lib_release) != VPU_LIB_VERSION_CODE) {
        g_print (RED_STR
            ("Vpu library version mismatch, please recompile the plugin with running correct head file!!\n"));
      }
#define MFW_GST_VPU_ENCODER_PLUGIN VERSION
      PRINT_PLUGIN_VERSION (MFW_GST_VPU_ENCODER_PLUGIN);

      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      guint8 *virt_bit_stream_buf = NULL;
      GstFlowReturn retval = GST_FLOW_OK;

      GST_DEBUG (">>VPU_ENC: VPU State: Ready to Paused vpu_enc=0x%x", vpu_enc);

      vpu_enc->encOP = g_malloc (sizeof (EncOpenParam));
      if (vpu_enc->encOP == NULL) {
        GST_DEBUG (">>VPU_ENC: Error in allocating encoder"
            "open parameter structure ");
        mfw_gst_vpuenc_cleanup (vpu_enc);
        return GST_STATE_CHANGE_FAILURE;
      }

      vpu_enc->initialInfo = g_malloc (sizeof (EncInitialInfo));
      if (vpu_enc->initialInfo == NULL) {
        GST_DEBUG (">>VPU_ENC: Error in allocating encoder"
            "initial info structure ");
        mfw_gst_vpuenc_cleanup (vpu_enc);
        return GST_STATE_CHANGE_FAILURE;
      }

      vpu_enc->encParam = g_malloc (sizeof (EncParam));
      if (vpu_enc->encParam == NULL) {
        GST_DEBUG (">>VPU_ENC: Error in allocating encoder"
            "parameter structure ");
        mfw_gst_vpuenc_cleanup (vpu_enc);
        return GST_STATE_CHANGE_FAILURE;
      }

      vpu_enc->outputInfo = g_malloc (sizeof (EncOutputInfo));
      if (vpu_enc->outputInfo == NULL) {
        GST_DEBUG (">>VPU_ENC: Error in allocating encoder"
            "output structure ");
        mfw_gst_vpuenc_cleanup (vpu_enc);
        return GST_STATE_CHANGE_FAILURE;
      }

      memset (vpu_enc->initialInfo, 0, sizeof (EncInitialInfo));
      memset (vpu_enc->encParam, 0, sizeof (EncParam));
      memset (vpu_enc->encOP, 0, sizeof (EncOpenParam));
      memset (vpu_enc->outputInfo, 0, sizeof (EncOutputInfo));
      memset (&vpu_enc->bit_stream_buf, 0, sizeof (vpu_mem_desc));

      vpu_enc->pTS_Mgr = createTSManager (0);
      resyncTSManager (vpu_enc->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);

      vpu_enc->bit_stream_buf.size = BUFF_FILL_SIZE;
      IOGetPhyMem (&vpu_enc->bit_stream_buf);
      if (vpu_enc->bit_stream_buf.phy_addr == 0) {
        GST_DEBUG (">>VPU_ENC: Error in allocating encoder"
            "bitstream buffer ");
        mfw_gst_vpuenc_cleanup (vpu_enc);
        return GST_STATE_CHANGE_FAILURE;
      }
      virt_bit_stream_buf = (guint8 *) IOGetVirtMem (&vpu_enc->bit_stream_buf);
      vpu_enc->start_addr = virt_bit_stream_buf;
      vpu_enc->curr_addr = NULL;

      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
      GST_DEBUG (">>VPU_ENC: VPU State: Paused to Playing");
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  GST_DEBUG (">>VPU_ENC: State Change for VPU returned %d", ret);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
      GST_DEBUG (">>VPU_ENC: VPU State: Playing to Paused");
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      GST_DEBUG (">>VPU_ENC: VPU State: Paused to Ready");
      mfw_gst_vpuenc_cleanup (vpu_enc);
      if (vpu_enc->pTS_Mgr) {
        destroyTSManager (vpu_enc->pTS_Mgr);
        (vpu_enc->pTS_Mgr) = NULL;
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
      GST_DEBUG (">>VPU_ENC: VPU State: Ready to Null");
      vpu_UnInit ();
      GST_DEBUG (">>VPU_ENC: after vpu_uninit");
      break;
    }
    default:
      break;
  }

  return ret;

}


/*==================================================================================================
FUNCTION:           mfw_gst_vpuenc_sink_event

DESCRIPTION:        Send an event to sink  pad of downstream element

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event

RETURN VALUE:
        TRUE       -    event is handled properly
        FALSE      -	event is not handled properly

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
==================================================================================================*/
static gboolean
mfw_gst_vpuenc_sink_event (GstPad * pad, GstEvent * event)
{
  MfwGstVPU_Enc *vpu_enc = NULL;
  gboolean ret = TRUE;
  vpu_enc = MFW_GST_VPU_ENC (GST_PAD_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gboolean update;
      gint64 start, stop, position;
      gdouble rate, applied_rate;

      gst_event_parse_new_segment_full (event, &update, &rate,
          &applied_rate, &format, &start, &stop, &position);

      gst_segment_set_newsegment_full (&vpu_enc->segment, update, rate,
          applied_rate, format, start, stop, position);

      if (format == GST_FORMAT_BYTES) {
        ret = gst_pad_push_event (vpu_enc->srcpad,
            gst_event_new_new_segment (FALSE,
                1.0, GST_FORMAT_TIME, 0, GST_CLOCK_TIME_NONE, 0));
      }
      break;
    }

    case GST_EVENT_EOS:
    {
      GST_DEBUG (">>VPU_ENC: EOS Total Number of frames = %d encoded frames=%d",
          vpu_enc->num_total_frames, vpu_enc->num_encoded_frames);
      ret = gst_pad_push_event (vpu_enc->srcpad, event);

      if (TRUE != ret) {
        GST_ERROR (">>VPU_ENC:  Error in pushing the event,result	is %d",
            ret);
        gst_event_unref (event);
      }
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      resyncTSManager (vpu_enc->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
      gst_segment_init (&vpu_enc->segment, GST_FORMAT_UNDEFINED);
      ret = gst_pad_push_event (vpu_enc->srcpad, event);
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      const GstStructure *s;

      s = gst_event_get_structure (event);

      if (gst_structure_has_name (s, "GstForceKeyUnit"))
        GST_OBJECT_LOCK (vpu_enc);
      vpu_enc->forcekeyframe = TRUE;
      GST_OBJECT_UNLOCK (vpu_enc);
      ret = gst_pad_push_event (vpu_enc->srcpad, event);
      break;
    }
    default:
    {
      ret = gst_pad_event_default (pad, event);
      break;
    }
  }
  return ret;
}


/*==================================================================================================
FUNCTION:           mfw_gst_vpuenc_src_event

DESCRIPTION:        Send an event to src  pad of downstream element

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event

RETURN VALUE:
        TRUE       -    event is handled properly
        FALSE      -	event is not handled properly

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
==================================================================================================*/
static gboolean
mfw_gst_vpuenc_src_event (GstPad * pad, GstEvent * event)
{
  MfwGstVPU_Enc *vpu_enc = NULL;
  gboolean ret = TRUE;
  vpu_enc = MFW_GST_VPU_ENC (GST_PAD_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      const GstStructure *s;

      s = gst_event_get_structure (event);

      if (gst_structure_has_name (s, "GstForceKeyUnit")) {
        GST_OBJECT_LOCK (vpu_enc);
        vpu_enc->forcekeyframe = TRUE;
        GST_OBJECT_UNLOCK (vpu_enc);
        /* consume the event, and a new event will be sent in chain */
        ret = TRUE;
        gst_event_unref (event);
      } else {
        ret = gst_pad_push_event (vpu_enc->sinkpad, event);
      }
      break;
    }
    default:
    {
      ret = gst_pad_event_default (pad, event);
      break;
    }
  }
  return ret;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_setcaps

DESCRIPTION:        This function negoatiates the caps set on the sink pad

ARGUMENTS PASSED:
                pad   -   pointer to the sinkpad of this element
                caps  -   pointer to the caps set

RETURN VALUE:
               TRUE   negotiation success full
               FALSE  negotiation Failed

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static gboolean
mfw_gst_vpuenc_setcaps (GstPad * pad, GstCaps * caps)
{
  MfwGstVPU_Enc *vpu_enc = NULL;
  gint width = 0;
  gint height = 0;

  vpu_enc = MFW_GST_VPU_ENC (gst_pad_get_parent (pad));
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  gst_video_format_parse_caps (caps, &vpu_enc->format, &width, &height);
  if ((width < MIN_WIDTH) || (height < MIN_HEIGHT)) {
    GST_ERROR (">>VPU_ENC: Source resolution is too small : %dx%d",
        width, height);
    gst_object_unref (vpu_enc);
    return FALSE;
  }
  vpu_enc->width = width;
  vpu_enc->height = height;

  if (gst_structure_has_field (structure, CAPS_FIELD_CROP_LEFT)) {
    gst_structure_get_int (structure, CAPS_FIELD_CROP_LEFT,
        &vpu_enc->crop_left);
  } else {
    vpu_enc->crop_left = 0;
  }

  if (gst_structure_has_field (structure, CAPS_FIELD_CROP_RIGHT)) {
    gst_structure_get_int (structure, CAPS_FIELD_CROP_RIGHT,
        &vpu_enc->crop_right);
  } else {
    vpu_enc->crop_right = 0;
  }

  if (gst_structure_has_field (structure, CAPS_FIELD_CROP_TOP)) {
    gst_structure_get_int (structure, CAPS_FIELD_CROP_TOP, &vpu_enc->crop_top);
  } else {
    vpu_enc->crop_top = 0;
  }

  if (gst_structure_has_field (structure, CAPS_FIELD_CROP_BOTTOM)) {
    gst_structure_get_int (structure, CAPS_FIELD_CROP_BOTTOM,
        &vpu_enc->crop_bottom);
  } else {
    vpu_enc->crop_bottom = 0;
  }
  gst_structure_get_int (structure, "crop-left-by-pixel", &vpu_enc->crop_left);
  gst_structure_get_int (structure, "crop-right-by-pixel",
      &vpu_enc->crop_right);
  gst_structure_get_int (structure, "crop-top-by-pixel", &vpu_enc->crop_top);
  gst_structure_get_int (structure, "crop-bottom-by-pixel",
      &vpu_enc->crop_bottom);

  vpu_enc->width += (vpu_enc->crop_left + vpu_enc->crop_right);
  vpu_enc->height += (vpu_enc->crop_top + vpu_enc->crop_bottom);

  GST_DEBUG (">>VPU_ENC: Input Height is %d", vpu_enc->height);
  gst_structure_get_fraction (structure, "framerate", &vpu_enc->framerate_n,
      &vpu_enc->framerate_d);

  if ((vpu_enc->framerate_n != 0) && (vpu_enc->framerate_d != 0)) {
    vpu_enc->src_framerate =
        (gfloat) vpu_enc->framerate_n / vpu_enc->framerate_d;
  } else {
    GST_ERROR
        (">>VPU_ENC: Invalid source frame rate (%d/%d), set to default (%d/%d)",
        vpu_enc->framerate_n, vpu_enc->framerate_d, DEFAULT_FRAME_RATE, 1);
    vpu_enc->framerate_n = DEFAULT_FRAME_RATE;
    vpu_enc->framerate_d = 1;
  }
  GST_DEBUG (">>VPU_ENC: src framerate=%f", vpu_enc->src_framerate);
  setTSManagerFrameRate (vpu_enc->pTS_Mgr, vpu_enc->framerate_n,
      vpu_enc->framerate_d);
  gst_object_unref (vpu_enc);
  return gst_pad_set_caps (pad, caps);
}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_gen_src_pad_caps

DESCRIPTION:        Generates src pad caps

ARGUMENTS PASSED:   None

RETURN VALUE:       src pad caps

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GstCaps *
mfw_gst_vpuenc_gen_src_pad_caps (CHIP_CODE chip_code)
{
  GstCaps *caps = NULL;
  guint max_width = 0, max_height = 0;
  gint i = 0;
  gchar *caps_list[6];
  gchar *resolution_str = NULL;
  gchar *caps_str = NULL;

  /* set the caps list for different chips */
  caps_list[i++] = "video/x-h264, %s;";

  caps_list[i++] = "video/x-h263, %s;";

  caps_list[i++] = "video/mpeg, %s, "
      "mpegversion = (int)4, " "systemstream = (boolean)false;";

  /* set the max resolution for different chips */
  if (CC_MX53 == chip_code) {
    max_width = 1280;
    max_height = 720;
  } else if ((CC_MX51 == chip_code) || (CC_MX27 == chip_code)) {
    max_width = 720;
    max_height = 576;
  }

  /* loop the list and generate caps */
  caps = gst_caps_new_empty ();
  if (caps) {
    resolution_str = g_strdup_printf ("width = (int)[48, %d], "
        "height = (int)[32, %d]", max_width, max_height);
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
  if (HAS_MJPEG_ENCODER (chip_code)) {
    GstCaps *newcaps = gst_caps_from_string ("image/jpeg, "
        "width = (int)[48, 8192], " "height = (int)[32, 8192];");
    if (newcaps)
      gst_caps_append (caps, newcaps);
  }

  return caps;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_src_pad_template

DESCRIPTION:        Gets src pad template

ARGUMENTS PASSED:   None

RETURN VALUE:       src pad template

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GstPadTemplate *
mfw_gst_vpuenc_src_pad_template (CHIP_CODE chip_code)
{
  FILE *fp = NULL;
  GstCaps *caps = NULL;
  static GstPadTemplate *templ = NULL;

  caps = mfw_gst_vpuenc_gen_src_pad_caps (chip_code);

  if (NULL == caps) {
    templ = gst_pad_template_new ("src", GST_PAD_SRC,
        GST_PAD_ALWAYS, GST_CAPS_NONE);
  } else {
    templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  }

  return templ;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_gen_sink_pad_caps

DESCRIPTION:        Generates sink pad caps

ARGUMENTS PASSED:   None

RETURN VALUE:       sink pad caps

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GstCaps *
mfw_gst_vpuenc_gen_sink_pad_caps (CHIP_CODE chip_code)
{
  GstCaps *caps = NULL;
  gint i = 0;
  GstVideoFormat fmts[] = { GST_VIDEO_FORMAT_I420,
    GST_VIDEO_FORMAT_YV12,
    GST_VIDEO_FORMAT_NV12,
    GST_VIDEO_FORMAT_Y42B,
    //GST_VIDEO_FORMAT_YV16, 
    GST_VIDEO_FORMAT_Y444,
#if GST_CHECK_VERSION(0, 10, 29)
    GST_VIDEO_FORMAT_GRAY8,
#if GST_CHECK_VERSION(0, 10, 30)
    GST_VIDEO_FORMAT_Y800,
#endif
#endif
    GST_VIDEO_FORMAT_UNKNOWN
  };

  caps = gst_caps_new_simple ("video/x-raw-yuv",
      "format", GST_TYPE_FOURCC,
      gst_video_format_to_fourcc (fmts[i++]),
      "width", GST_TYPE_INT_RANGE, 48, 8192,
      "height", GST_TYPE_INT_RANGE, 32, 8192, NULL);

  if ((CC_MX51 == chip_code) || (CC_MX53 == chip_code)) {
    while (GST_VIDEO_FORMAT_UNKNOWN != fmts[i]) {
      GstCaps *newcaps = gst_caps_new_simple ("video/x-raw-yuv",
          "format", GST_TYPE_FOURCC,
          gst_video_format_to_fourcc (fmts[i]),
          "width", GST_TYPE_INT_RANGE, 48, 8192,
          "height", GST_TYPE_INT_RANGE, 32, 8192,
          NULL);
      gst_caps_append (caps, newcaps);
      i++;
    }
  } else if (CC_MX27 == chip_code) {
    caps = NULL;
  }

  return caps;
}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_sink_pad_template

DESCRIPTION:        Gets sink pad template

ARGUMENTS PASSED:   None

RETURN VALUE:       sink pad template

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GstPadTemplate *
mfw_gst_vpuenc_sink_pad_template (CHIP_CODE chip_code)
{
  FILE *fp = NULL;
  GstCaps *caps = NULL;
  static GstPadTemplate *templ = NULL;

  caps = mfw_gst_vpuenc_gen_sink_pad_caps (chip_code);

  if (NULL == caps) {
    templ = gst_pad_template_new ("sink", GST_PAD_SINK,
        GST_PAD_ALWAYS, GST_CAPS_NONE);
  } else {
    templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
  }

  return templ;
}

/*=======================================================================================
FUNCTION:           mfw_gst_vpuenc_base_init

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
mfw_gst_vpuenc_base_init (MfwGstVPU_EncClass * klass)
{

  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  CHIP_CODE chip_code = getChipCode ();
  const gchar *description = "";

  gst_element_class_add_pad_template (element_class,
      mfw_gst_vpuenc_src_pad_template (chip_code));

  gst_element_class_add_pad_template (element_class,
      mfw_gst_vpuenc_sink_pad_template (chip_code));

  switch (chip_code) {
    case CC_MX53:
    case CC_MX51:
      description = "Encodes raw YUV 4:2:0 data to MPEG4 SP, H.264 BP or "
          "H.263 (Annex J, K (RS=0 and ASO=0) and T) elementary data;"
          "Encodes raw YUV 4:2:0, 4:2:2 horizontal, 4:2:2 vertical "
          "or 4:0:0 data into MJPEG elementary data;";
      break;

    case CC_MX27:
      description = "Encodes raw YUV 4:2:0 data to MPEG4 SP, H.264 BP or "
          "H.263 (Annex J, K (RS=0 and ASO=0) and T) elementary data;";
      break;
  }

  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "VPU-based video encoder",
      "Codec/Encoder/Video", "Encoder video raw to compressed data by using VPU");

}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_codec_get_type

DESCRIPTION:        Gets an enumeration for the different 
                    codec standards supported by the encoder

ARGUMENTS PASSED:   None

RETURN VALUE:       Enumerated type of the codec standards supported by the encoder

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GType
mfw_gst_vpuenc_codec_get_type (void)
{
  CHIP_CODE chip_code = getChipCode ();
  const gchar *name = "MfwGstVpuEncCodecs";

  if ((CC_MX51 == chip_code) || (CC_MX53 == chip_code)) {
    static GEnumValue vpudec_codecs[] = {
      {STD_MPEG4, STR (STD_MPEG4), "std_mpeg4"},
      {STD_H263, STR (STD_H263), "std_h263"},
      {STD_AVC, STR (STD_AVC), "std_avc"},
      {STD_MJPG, STR (STD_MJPG), "std_mjpg"},
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
FUNCTION:           mfw_gst_vpuenc_mirrordir_get_type

DESCRIPTION:        Gets an enumeration for the mirror directions

ARGUMENTS PASSED:   None

RETURN VALUE:       Enumerated type of the codec standards supported by the encoder

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static GType
mfw_gst_vpuenc_mirrordir_get_type (void)
{
  static GType vpuenc_mirror_dir = 0;
  static GEnumValue vpuenc_mirdirs[] = {
    {MIRDIR_NONE, STR (MIRDIR_NONE), "none                "},
    {MIRDIR_VER, STR (MIRDIR_VER), "vertical mirroring  "},
    {MIRDIR_HOR, STR (MIRDIR_HOR), "horizontal mirroring"},
    {MIRDIR_HOR_VER, STR (MIRDIR_HOR_VER), "both directions     "},
    {0, NULL, NULL},
  };
  if (!vpuenc_mirror_dir) {
    vpuenc_mirror_dir =
        g_enum_register_static ("MfwGstVpuEncMirDir", vpuenc_mirdirs);
  }

  return vpuenc_mirror_dir;
}

static void
mfw_gst_vpuenc_vpu_class_finalize (GObject * object)
{
  mfw_gst_vpuenc_vpu_finalize();
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_class_init

DESCRIPTION:        Initialise the class.(specifying what signals,
                    arguments and virtual functions the class has and setting up
                    global states)

ARGUMENTS PASSED:   klass - pointer to H.264Encoder element class

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static void
mfw_gst_vpuenc_class_init (MfwGstVPU_EncClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  guint max_width, max_height;
  CHIP_CODE chip_code = getChipCode ();

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstelement_class->change_state = mfw_gst_vpuenc_change_state;
  gobject_class->set_property = mfw_gst_vpuenc_set_property;
  gobject_class->get_property = mfw_gst_vpuenc_get_property;
  gobject_class->finalize = mfw_gst_vpuenc_vpu_class_finalize;
  //gobject_class->dispose = mfw_gst_vpuenc_vpu_finalize;

  parent_class = g_type_class_peek_parent (klass);

  /* set the max resolution for different chips */
  if (CC_MX53 == chip_code) {
    max_width = 1280;
    max_height = 720;
  } else if ((CC_MX51 == chip_code) || (CC_MX27 == chip_code)) {
    max_width = 720;
    max_height = 576;
  }

  g_object_class_install_property (gobject_class, MFW_GST_VPU_PROF_ENABLE,
      g_param_spec_boolean ("profile", "Profile",
          "enable time profile of the vpu encoder plug-in",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPU_CODEC_TYPE,
      g_param_spec_enum ("codec-type", "codec_type",
          "selects the codec type for encoding",
          mfw_gst_vpuenc_codec_get_type (), STD_AVC, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_WIDTH,
      g_param_spec_uint ("width", "Width",
          "width of the frame to be encoded",
          MIN_WIDTH, MAX_MJPEG_WIDTH, MIN_WIDTH, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_HEIGHT,
      g_param_spec_uint ("height", "Height",
          "height of the frame to be encoded",
          MIN_HEIGHT, MAX_MJPEG_HEIGHT, MIN_HEIGHT, G_PARAM_READWRITE));


  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "target bitrate (in kbps) at which stream is to be encoded - 0 for VBR and others for CBR",
          0, MAX_BITRATE, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_QP,
      g_param_spec_uint ("qp", "QP",
          "gets the quantization parameter - range is 0-51 - will be ignored for CBR (bitrate!=0)",
          0, 51, VPU_DEFAULT_MPEG4_QP, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_MAX_QP,
      g_param_spec_uint ("max_qp", "MAX_QP",
          "Maximum quantization parameter for CBR - range is 0-51 for H264 and 1-31 for MPEG4 - lower value brings better video quality but higher frame sizes",
          0, 51, 51, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_MIN_QP,
      g_param_spec_uint ("min_qp", "MIN_QP",
          "Minimum quantization parameter for CBR - range is 0-51 for H264 and 1-31 for MPEG4 - lower value brings better video quality but higher frame sizes",
          0, 51, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_GAMMA,
      g_param_spec_uint ("gamma", "GAMMA",
          "gamma value for CBR - tells VPU the speed on changing qp - lower will cause better video quality",
          0, MAX_GAMMA, DEFAULT_GAMMA, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_GOP,
      g_param_spec_uint ("gopsize", "Gopsize",
          "gets the GOP size at which stream is to be encoded",
          0, MAX_GOP_SIZE, DEFAULT_GOP_SIZE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_INTRAREFRESH, g_param_spec_uint ("intrarefresh", "intraRefresh", "0 - Intra MB refresh is not used. Otherwise - At least N MB's in every P-frame will be encoded as intra MB's.", 0, ((max_width * max_height) >> 8), 0, G_PARAM_READWRITE));  // max is D1 / 16 /16 to get max MBs - should be lower if resolution is lower

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_H263PROFILE0,
      g_param_spec_boolean ("h263profile0", "H263 Profile0",
          "enable encoding of H.263 profile 0 when codec-type is set to std_h263",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_LOOPBACK,
      g_param_spec_boolean ("loopback", "Loopback",
          "disables parallelization for performance - turn off if pipeline with decoder",
          TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_INTRA_QP,
      g_param_spec_uint ("intra_qp", "INTRA_QP",
          "Quantization parameter for I frame. When this value is -1, the quantization "
          "parameter for I frames is automatically determined by the VPU. In MPEG4/H.263 "
          "mode, the range is 1¨C31; in H.264 mode, the range is from 0¨C51. This is ignored "
          "for STD_MJPG",
          H264_QP_MIN, H264_QP_MAX, VPU_DEFAULT_MPEG4_QP, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_CROP_LEFT,
      g_param_spec_uint ("crop-left", "crop-left",
          "The left crop value of input frame to be encoded",
          0, G_MAXUINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_CROP_TOP,
      g_param_spec_uint ("crop-top", "crop-top",
          "The top crop value of input frame to be encoded",
          0, G_MAXUINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_CROP_RIGHT,
      g_param_spec_uint ("crop-right", "crop-right",
          "The right crop value of input frame to be encoded",
          0, G_MAXUINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_CROP_BOTTOM,
      g_param_spec_uint ("crop-bottom", "crop-bottom",
          "The bottom crop value of input frame to be encoded",
          0, G_MAXUINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MFW_GST_VPUENC_ROTATION_ANGLE,
      g_param_spec_uint ("rotation-angle", "rotation-angle",
          "Pre-rotation angle - should be 0, 90, 180 or 270 ",
          0, 270, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      MFW_GST_VPUENC_MIRROR_DIRECTION, g_param_spec_enum ("mirror-direction",
          "mirror-direction",
          "mirror direction from source image to encoded image",
          mfw_gst_vpuenc_mirrordir_get_type (), MIRDIR_NONE,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      MFW_GST_VPUENC_AVC_BYTE_STREAM, g_param_spec_boolean ("h264-byte-stream",
          "h264-byte-stream",
          "Generate H.264 byte stream format of NALU. Take effect for H.264 stream only.",
          TRUE, G_PARAM_READWRITE));
}

void
vpuenc_free_gst_buf_meta (GstBuffer * buf)
{
  gint index = G_N_ELEMENTS (buf->_gst_reserved) - 1;

  GstBufferMeta *meta = (GstBufferMeta *) (buf->_gst_reserved[index]);

#ifdef USE_HW_ALLOCATOR
  mfw_free_hw_buffer (meta->priv);
#else
  IOFreeVirtMem (meta->priv);
  IOFreePhyMem (meta->priv);
  g_free (meta->priv);
#endif
  gst_buffer_meta_free (meta);
  buf = NULL;
  GST_DEBUG ("vpu free buffer");
}

/*=============================================================================
FUNCTION:           mfw_gst_vpuenc_buffer_alloc   
        
DESCRIPTION:        This function initailise the sl driver
                    and gets the new buffer for display             

ARGUMENTS PASSED:  
          bsink :   pointer to GstBaseSink
		  buf   :   pointer to new GstBuffer
		  size  :   size of the new buffer
          offset:   buffer offset
		  caps  :   pad capability
        
RETURN VALUE:       GST_FLOW_OK/GST_FLOW_ERROR
      
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstFlowReturn
mfw_gst_vpuenc_buffer_alloc (GstPad * pad, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  GstBuffer *ibuf = NULL;
  GST_DEBUG ("called vpuenc buffer alloc");
  gint index;
  GstBufferMeta *bufmeta;
  unsigned int *vaddr = NULL, *paddr = NULL;
#ifdef USE_HW_ALLOCATOR
  void *handle;
  if ((handle = mfw_new_hw_buffer (size, &paddr, &vaddr, 0)) == 0) {
    GST_ERROR (">>VPU_ENC: Could not allocate hardware buffer");
    *buf = NULL;
    return GST_FLOW_ERROR;
  }
#else
  vpu_mem_desc *handle;
  if (handle = g_malloc (sizeof (vpu_mem_desc))) {
    memset (handle, sizeof (vpu_mem_desc), 0);
    handle->size = size;
    IOGetPhyMem (handle);
    if (handle->phy_addr) {
      paddr = handle->phy_addr;
      IOGetVirtMem (handle);
      if (handle->virt_uaddr) {
        vaddr = handle->virt_uaddr;
      } else {
        goto fail;
      }
    } else {
      goto fail;
    }
  } else {
    goto fail;
  }

#endif
  ibuf = gst_buffer_new ();
  GST_BUFFER_SIZE (ibuf) = size;
  GST_BUFFER_DATA (ibuf) = vaddr;
  bufmeta = gst_buffer_meta_new ();
  GST_BUFFER_MALLOCDATA (ibuf) = ibuf;
  GST_BUFFER_FREE_FUNC (ibuf) = vpuenc_free_gst_buf_meta;

  index = G_N_ELEMENTS (ibuf->_gst_reserved) - 1;
  bufmeta->physical_data = (gpointer) paddr;
  bufmeta->priv = handle;
  ibuf->_gst_reserved[index] = bufmeta;




  if (ibuf != NULL) {
    *buf = GST_BUFFER_CAST (ibuf);
    GST_BUFFER_SIZE (*buf) = size;
    gst_buffer_set_caps (*buf, caps);

    return GST_FLOW_OK;
  } else {
    g_print (" VPU_ENC: Could not allocate buffer");
    *buf = NULL;
    return GST_FLOW_ERROR;

  }
fail:
#ifdef USE_HW_ALLOCATOR
#else
  if (handle) {
    if (vaddr) {
      IOFreeVirtMem (handle);
    }
    if (paddr) {
      IOFreePhyMem (handle);
    }
    g_free (handle);
  }

  *buf = NULL;
  return GST_FLOW_ERROR;
#endif
}

/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_init

DESCRIPTION:        Create the pad template that has been registered with the
                    element class in the _base_init

ARGUMENTS PASSED:   vpu_enc - pointer to vpu_encoder element structure

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static void
mfw_gst_vpuenc_init (MfwGstVPU_Enc * vpu_enc, MfwGstVPU_EncClass * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (vpu_enc);

  vpuenc_global_ptr = vpu_enc;

  /* create the sink and src pads */
  vpu_enc->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (klass, "sink"), "sink");
  vpu_enc->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (klass, "src"), "src");
  gst_element_add_pad (GST_ELEMENT (vpu_enc), vpu_enc->sinkpad);
  gst_element_add_pad (GST_ELEMENT (vpu_enc), vpu_enc->srcpad);
  gst_pad_set_chain_function (vpu_enc->sinkpad, mfw_gst_vpuenc_chain);
  gst_pad_set_event_function (vpu_enc->sinkpad,
      GST_DEBUG_FUNCPTR (mfw_gst_vpuenc_sink_event));
  gst_pad_set_bufferalloc_function (vpu_enc->sinkpad,
      GST_DEBUG_FUNCPTR (mfw_gst_vpuenc_buffer_alloc));
  gst_pad_set_setcaps_function (vpu_enc->sinkpad, mfw_gst_vpuenc_setcaps);

  vpu_enc->chip_code = getChipCode ();

  vpu_enc->handle = 0;

  // State members
  vpu_enc->vpu_init = FALSE;
  vpu_enc->is_frame_started = FALSE;

  // Header members
  vpu_enc->num_total_frames = 0;
  vpu_enc->num_encoded_frames = 0;
  vpu_enc->frames_since_hdr_sent = 1;
  vpu_enc->codec_data = NULL;

  // Properties set by input
  vpu_enc->codec = STD_AVC;
  vpu_enc->width = INVALID_WIDTH;
  vpu_enc->height = INVALID_HEIGHT;

  vpu_enc->vpuRegisteredFramesDesc = NULL;
  vpu_enc->vpuRegisteredFrames = NULL;
  vpu_enc->numframebufs = 0;
  vpu_enc->encWidth = INVALID_WIDTH;
  vpu_enc->encHeight = INVALID_HEIGHT;

  // Frame rate members
  vpu_enc->src_framerate = DEFAULT_FRAME_RATE;
  vpu_enc->tgt_framerate = INVALID_FRAME_RATE;

  // members for bitrate
  vpu_enc->bitrate = MAX_BITRATE + 1;   //INVALID_BITRATE;
  vpu_enc->setbitrate = FALSE;
  vpu_enc->qp = VPU_INVALID_QP;
  vpu_enc->max_qp_en = FALSE;
  vpu_enc->min_qp_en = FALSE;
  vpu_enc->max_qp = VPU_INVALID_QP;
  vpu_enc->min_qp = VPU_INVALID_QP;
  vpu_enc->intra_qp = VPU_INVALID_QP;
  vpu_enc->gamma = DEFAULT_GAMMA;
  vpu_enc->fs_read = 0;
  vpu_enc->fs_write = 0;
  vpu_enc->bits_per_second = 0;
  vpu_enc->bits_per_stream = 0;
  vpu_enc->gopsize = DEFAULT_GOP_SIZE;
  vpu_enc->ts_rx = vpu_enc->ts_rx = 0;

  // members for fixed bit rate
  vpu_enc->forcefixrate = FALSE;
  vpu_enc->segment_starttime = 0;
  vpu_enc->segment_encoded_frame = 0;
  vpu_enc->total_time = 0;
  vpu_enc->framerate_d = vpu_enc->framerate_n = 0;
  vpu_enc->intraRefresh = 0;
  vpu_enc->h263profile0 = FALSE;
  vpu_enc->loopback = TRUE;
  vpu_enc->forceIFrameInterval = DEFAULT_I_FRAME_INTERVAL;
  vpu_enc->format = GST_VIDEO_FORMAT_I420;
  vpu_enc->avc_byte_stream = TRUE;

  vpu_enc->crop_top = 0;
  vpu_enc->crop_left = 0;
  vpu_enc->crop_right = 0;
  vpu_enc->crop_bottom = 0;

  vpu_enc->rotation = 0;
  vpu_enc->mirror = MIRDIR_NONE;

  vpu_enc->forcekeyframe = FALSE;

  if (CC_MX53 == vpu_enc->chip_code) {
    vpu_enc->max_width = 1280;
    vpu_enc->max_height = 720;
  } else {                      /* (CC_MX51 == vpu_enc->chip_code) || (CC_MX27 == vpu_enc->chip_code) */

    vpu_enc->max_width = 720;
    vpu_enc->max_height = 576;
  }

  gst_segment_init (&vpu_enc->segment, GST_FORMAT_UNDEFINED);

  memset (&(vpu_enc->vpuInFrame), 0, sizeof (FrameBuffer));
  memset (&(vpu_enc->vpuInFrameDesc), 0, sizeof (vpu_mem_desc));
  memset (&(vpu_enc->bit_stream_buf), 0, sizeof (vpu_mem_desc));

}

/*======================================================================================
FUNCTION:           plugin_init

DESCRIPTION:        Special function , which is called as soon as the plugin or
                    element is loaded and information returned by this function
                    will be cached in central registry

ARGUMENTS PASSED:   plugin - pointer to container that contains features loaded
                             from shared object module

RETURN VALUE:       return TRUE or FALSE depending on whether it loaded initialized any
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
    ret = gst_element_register (plugin, "mfw_vpuencoder",
        GST_RANK_PRIMARY, MFW_GST_TYPE_VPU_ENC);
    fclose (fp);
  }

  return ret;
}


/*======================================================================================
FUNCTION:           mfw_gst_type_vpu_enc_get_type

DESCRIPTION:        Interfaces are initiated in this function.you can register one
                    or more interfaces after having registered the type itself.

ARGUMENTS PASSED:   None

RETURN VALUE:       Numerical value ,which represents the unique identifier of 
                    this element.

PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
     static GType mfw_gst_type_vpu_enc_get_type (void)
{
  static GType vpu_enc_type = 0;


  if (!vpu_enc_type) {
    static const GTypeInfo vpu_enc_info = {
      sizeof (MfwGstVPU_EncClass),
      (GBaseInitFunc) mfw_gst_vpuenc_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_vpuenc_class_init,
      NULL,
      NULL,
      sizeof (MfwGstVPU_Enc),
      0,
      (GInstanceInitFunc) mfw_gst_vpuenc_init,
    };
    vpu_enc_type = g_type_register_static (GST_TYPE_ELEMENT,
        "MfwGstVPU_Enc", &vpu_enc_info, 0);
  }

  GST_DEBUG_CATEGORY_INIT (mfw_gst_vpuenc_debug,
      "mfw_vpuencoder", 0, "FreeScale's VPU  Encoder's Log");

  return vpu_enc_type;
}



/*======================================================================================
FUNCTION:           mfw_gst_vpuenc_vpu_finalize

DESCRIPTION:        Handles cleanup of any unreleased memory if player is closed

ARGUMENTS PASSED:   None
RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
void __attribute__ ((destructor)) mfw_gst_vpuenc_vpu_finalize (void);

void
mfw_gst_vpuenc_vpu_finalize (void)
{
  MfwGstVPU_Enc *vpu_enc = vpuenc_global_ptr;
  if (vpu_enc) {
    GST_DEBUG (">>VPU_ENC: Destructor - final cleanup");
    mfw_gst_vpuenc_cleanup (vpu_enc);
    vpu_UnInit ();
    vpuenc_global_ptr = NULL;
  }
}

FSL_GST_PLUGIN_DEFINE("mfwvpuenc", "VPU-based video encoder", plugin_init);


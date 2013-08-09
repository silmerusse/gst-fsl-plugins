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
 * Module Name:    mfw_gst_mpeg2dec.c
 *
 * Description:    GStreamer Plug-in for MPEG2-Decoder.
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
#include <memory.h>
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include "mpeg2_dec_api.h"
#include "mfw_gst_mpeg2dec.h"

#include "mfw_gst_utils.h"

#ifdef MPEG2_MEMORY_DEBUG
#include "mfw_gst_debug.h"

Mem_Mgr mpeg2_tm;

#define MPEG2_MALLOC(size)      \
    dbg_malloc(&mpeg2_tm, (size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )

#define MPEG2_FREE(ptr)         \
    dbg_free(&mpeg2_tm, (ptr), "line " STR(__LINE__) " of " STR(__FUNCTION__) )

#else

#define MPEG2_MALLOC(size) g_malloc(size)
#define MPEG2_FREE(ptr)    g_free(ptr)

#endif
/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None	*/

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/



/*=============================================================================
                STATIC VARIABLES (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

static GstStaticPadTemplate mfw_gst_mpeg2dec_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/mpeg, systemstream=(boolean)false, mpegversion=(int){1,2}")
    );


/* table with framerates expressed as fractions */
static const gint fpss[][2] = { {24000, 1001},
{24, 1}, {25, 1}, {30000, 1001},
{30, 1}, {50, 1}, {60000, 1001},
{60, 1}, {0, 1}
};

static GstElementClass *parent_class = NULL;

/*=============================================================================
                                LOCAL MACROS
=============================================================================*/

/* used for debugging */
#define GST_CAT_DEFAULT mfw_gst_mpeg2dec_debug


/*=============================================================================
                               LOCAL VARIABLES
=============================================================================*/

/* None */


/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/

/* None. */

/*=============================================================================
                            LOCAL FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC (mfw_gst_mpeg2dec_debug);

static gint32 mfw_gst_mpeg2dec_allocmem_dec (sMpeg2DecMemAllocInfo *);
static void mfw_gst_mpeg2dec_initobject (sMpeg2DecObject *);
static void mfw_gst_mpeg2dec_free (sMpeg2DecObject *);
static gint32 mfw_gst_mpeg2dec_callback (int *, unsigned char **, int, void *);
static GstFlowReturn mfw_gst_mpeg2_decode (MFW_GST_MPEG2DEC_INFO_T *);
static GstFlowReturn mfw_gst_mpeg2dec_chain (GstPad *, GstBuffer *);
static GstStateChangeReturn
mfw_gst_mpeg2dec_change_state (GstElement *, GstStateChange);
static gboolean mfw_gst_mpeg2dec_sink_event (GstPad *, GstEvent *);
static gboolean mfw_gst_mpeg2dec_set_caps (GstPad *, GstCaps *);
static GstPadTemplate *mfw_gst_mpeg2dec_src_templ (void);

static void mfw_gst_mpeg2dec_set_index (GstElement *, GstIndex *);

static GstIndex *mfw_gst_mpeg2dec_get_index (GstElement *);

static void mfw_gst_mpeg2dec_get_property (GObject *, guint,
    GValue *, GParamSpec *);
static void mfw_gst_mpeg2dec_set_property (GObject *, guint,
    GValue *, GParamSpec *);
static void mfw_gst_mpeg2dec_init (MFW_GST_MPEG2DEC_INFO_T *);
static void mfw_gst_mpeg2dec_class_init (MFW_GST_MPEG2DEC_INFO_CLASS_T *);
static void mfw_gst_mpeg2dec_base_init (gpointer);
static gboolean mfw_gst_mpeg2dec_src_event (GstPad *, GstEvent *);

/* Call back function used for direct render v2 */
static void *mfw_gst_MPEG2_getbuffer (void *pvAppContext);
static void mfw_gst_MPEG2_rejectbuffer (void *pbuffer, void *pvAppContext);
static void mfw_gst_MPEG2_releasebuffer (void *pbuffer, void *pvAppContext);

/* timestamp functions */
#if 0
static void mfw_gst_init_timestamp (sMpeg2TimestampObject * timestampObject);
static void mfw_gst_receive_timestamp (sMpeg2TimestampObject * timestampObject,
    GstClockTime timestamp);
static gboolean mfw_gst_get_timestamp (sMpeg2TimestampObject * timestampObject,
    GstClockTime * ptimestamp);
#endif

/*=============================================================================
                        LOCAL FUNCTION
=============================================================================*/
/*============================================================================
FUNCTION:               mfw_gst_mpeg2dec_allocmem_dec             

DESCRIPTION:            allocates memory required by the Decoder                        

ARGUMENTS PASSED:       
                        memalloc_info   -   pointer to a structure which holds 
                                            allocated memory

RETURN VALUE:   
                        0   -   success
                        -1  -   failure    

PRE-CONDITIONS:
                        None.

POST-CONDITIONS:        None.

IMPORTANT NOTES:        None
=============================================================================*/
static gint32
mfw_gst_mpeg2dec_allocmem_dec (sMpeg2DecMemAllocInfo * memalloc_info)
{

  gint32 block_count = 0, cnt;
  sMpeg2DecMemBlock *mem_info;

  g_print ("Total malloc count: %d(%d,%d).\n",
      memalloc_info->s32NumReqs + memalloc_info->s32BlkNum,
      memalloc_info->s32NumReqs, memalloc_info->s32BlkNum);

  for (block_count = memalloc_info->s32BlkNum; block_count <
      (memalloc_info->s32NumReqs + memalloc_info->s32BlkNum); block_count++) {

    mem_info = &memalloc_info->asMemBlks[block_count];
    mem_info->pvBuffer = MPEG2_MALLOC (mem_info->s32Size);

    if (NULL == mem_info->pvBuffer) {
      if (block_count != 0) {
        for (cnt = 0; cnt < (block_count - 1); cnt++) {
          mem_info = &memalloc_info->asMemBlks[block_count];
          MPEG2_FREE (mem_info->pvBuffer);
          mem_info->pvBuffer = NULL;
        }
      }
      return -1;

    }
  }
  return 0;
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_initobject             

DESCRIPTION:            initalize the decoder configuration structure             

ARGUMENTS PASSED:
                        dec_object  -   pointer to Mpeg2 Decoder configuration
                                        structure.
                 
RETURN VALUE:
                        None.   

PRE-CONDITIONS:
                        None.

POST-CONDITIONS:        None.

IMPORTANT NOTES:        None
=============================================================================*/
static void
mfw_gst_mpeg2dec_initobject (sMpeg2DecObject * decoder_obj)
{

  gint32 count = 0;

  // Memory info initialization
  decoder_obj->sMemInfo.s32NumReqs = 0;
  for (count = 0; count < MAX_NUM_MEM_REQS; count++) {
    decoder_obj->sMemInfo.asMemBlks[count].s32Size = 0;
    decoder_obj->sMemInfo.asMemBlks[count].pvBuffer = NULL;
  }

  decoder_obj->sDecParam.sOutputBuffer.pu8YBuf = NULL;
  decoder_obj->sDecParam.sOutputBuffer.s32YBufLen = 0;

  decoder_obj->sDecParam.u16FrameWidth = 0;
  decoder_obj->sDecParam.u16FrameHeight = 0;
  decoder_obj->pvMpeg2Obj = NULL;
  decoder_obj->pvAppContext = NULL;
  decoder_obj->eState = E_MPEG2D_INVALID;
  decoder_obj->ptr_cbkMPEG2DBufRead = NULL;


}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_free             

DESCRIPTION:            It deallocates all the memory which was allocated            

ARGUMENTS PASSED:
                        dec_object  -   pointer to Mpeg2 Decoder configuration
                                        structure.                        

RETURN VALUE:
                        None.                        

PRE-CONDITIONS:
                        None.

POST-CONDITIONS:        None.

IMPORTANT NOTES:        None
=============================================================================*/
static void
mfw_gst_mpeg2dec_free (sMpeg2DecObject * dec_object)
{
  sMpeg2DecMemAllocInfo *psMemAllocInfo;
  gint s32MemBlkCnt = 0;

  if (NULL != dec_object) {
    psMemAllocInfo = &(dec_object->sMemInfo);

    if (psMemAllocInfo != NULL) {
      for (s32MemBlkCnt = 0; s32MemBlkCnt < MAX_NUM_MEM_REQS; s32MemBlkCnt++) {

        if (psMemAllocInfo->asMemBlks[s32MemBlkCnt].pvBuffer != NULL) {
          MPEG2_FREE (psMemAllocInfo->asMemBlks[s32MemBlkCnt].pvBuffer);
          psMemAllocInfo->asMemBlks[s32MemBlkCnt].pvBuffer = NULL;
        }
      }
    }

  }

}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_callback                            

DESCRIPTION:            used by the decoder to get a new input buffer for decoding.
                        It copies the "buff_len" of encoded data into 
                        "buff_ptr". This function is called by the decoder
                        in eMPEG2D_Re_Querymem() and eMPEG2DDecode() functions,
                        when it runs out of current bit stream input buffer.    

ARGUMENTS PASSED:
                        buff_len    -   pointer to size of buffer   
                        buff_ptr    -   pointer to buffer        
                        offset      -   offset value
                        app_context -   pointer to application context

RETURN VALUE:
                        retval      -   number of bytes return to decoder lib
                        -1          -   Error

PRE-CONDITIONS:
                        None.

POST-CONDITIONS:        None.

IMPORTANT NOTES:        None
=============================================================================*/
static gint32
mfw_gst_mpeg2dec_callback (int *buff_len, unsigned char **buff_ptr,
    int offset, void *app_context)
{

  gint32 retval;

  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec;
  mpeg2dec = (MFW_GST_MPEG2DEC_INFO_T *) app_context;

  if (NULL != mpeg2dec->input_buffer) {

    *buff_ptr = GST_BUFFER_DATA (mpeg2dec->input_buffer);
    *buff_len = GST_BUFFER_SIZE (mpeg2dec->input_buffer);
    retval = *buff_len;
  } else {
    *buff_ptr = NULL;
    *buff_len = 0;
    retval = -1;

  }

  return retval;
}

/*=============================================================================
FUNCTION:               mfw_gst_MPEG2_getbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder need a new frame buffer.

ARGUMENTS PASSED:       pvAppContext -> Pointer to the context variable.

RETURN VALUE:           Pointer to a frame buffer.  -> On success.
                        Null.                       -> On fail.

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void *
mfw_gst_MPEG2_getbuffer (void *pvAppContext)
{
  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec = (MFW_GST_MPEG2DEC_INFO_T *) pvAppContext;
  void *pbuffer;
  GstCaps *caps = NULL;
  guint outsize;

  if (mpeg2dec->caps_set == FALSE) {
    guint fourcc = GST_STR_FOURCC ("I420");
    gint crop_height;
    gint crop_width;

    crop_height =
        (16 - mpeg2dec->dec_object->sDecParam.u16FrameHeight % 16) % 16;
    crop_width = (16 - mpeg2dec->dec_object->sDecParam.u16FrameWidth % 16) % 16;

    mpeg2dec->padded_height =
        mpeg2dec->dec_object->sDecParam.u16FrameHeight + crop_height;
    mpeg2dec->padded_width =
        mpeg2dec->dec_object->sDecParam.u16FrameWidth + crop_width;


    GST_DEBUG (" Caps being set for decoder source pad\n");
    mpeg2dec->caps_set = TRUE;
    caps = gst_caps_new_simple ("video/x-raw-yuv",
        "format", GST_TYPE_FOURCC, fourcc,
        "width", G_TYPE_INT, mpeg2dec->padded_width,
        "height", G_TYPE_INT, mpeg2dec->padded_height,
        "pixel-aspect-ratio", GST_TYPE_FRACTION,
        /* Revise the aspect value */
        1, 1,
        CAPS_FIELD_CROP_RIGHT, G_TYPE_INT, crop_width,
        CAPS_FIELD_CROP_BOTTOM, G_TYPE_INT, crop_height,
        CAPS_FIELD_REQUIRED_BUFFER_NUMBER, G_TYPE_INT, BM_GET_BUFFERNUM, NULL);

    if (!(gst_pad_set_caps (mpeg2dec->srcpad, caps))) {
      GST_ERROR (" Could not set the caps for the decoder src pad \n");
    }

    gst_caps_unref (caps);
  }
  outsize = mpeg2dec->padded_width *
      mpeg2dec->padded_height * sizeof (char) * 3 / 2;

  BM_GET_BUFFER (mpeg2dec->srcpad, outsize, pbuffer);
  return pbuffer;
}

/*=============================================================================
FUNCTION:               mfw_gst_MPEG2_rejectbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder want to indicate a frame buffer would not be 
                        used as a output.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for reject
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void
mfw_gst_MPEG2_rejectbuffer (void *pbuffer, void *pvAppContext)
{
  BM_REJECT_BUFFER (pbuffer);
}


/*=============================================================================
FUNCTION:               mfw_gst_MPEG2_releasebuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder want to indicate a frame buffer would never used
                        as a reference.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for release
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void
mfw_gst_MPEG2_releasebuffer (void *pbuffer, void *pvAppContext)
{
  BM_RELEASE_BUFFER (pbuffer);
}

#if 0
static void
mfw_gst_init_timestamp (sMpeg2TimestampObject * timestampObject)
{
  timestampObject->timestamp_tx = timestampObject->timestamp_rx =
      timestampObject->no_timestamp_count = timestampObject->last_send = 0;
}

static void
mfw_gst_receive_timestamp (sMpeg2TimestampObject * timestampObject,
    GstClockTime timestamp)
{
  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    timestampObject->timestamp[timestampObject->timestamp_rx] = timestamp;
    timestampObject->timestamp_rx =
        ((timestampObject->timestamp_rx + 1) & TIMESTAMP_INDEX_MASK);
  } else {
    timestampObject->no_timestamp_count++;
  }
}

static gboolean
mfw_gst_get_timestamp (sMpeg2TimestampObject * timestampObject,
    GstClockTime * ptimestamp)
{
  gboolean found = FALSE;
  int i = timestampObject->timestamp_tx;
  int index;
  GstClockTime timestamp = 0;
  while (i != timestampObject->timestamp_rx) {
    if (found) {
      if (timestampObject->timestamp[i] < timestamp) {
        timestamp = timestampObject->timestamp[i];
        index = i;
      }
    } else {
      timestamp = timestampObject->timestamp[i];
      index = i;
      found = TRUE;
    }
    i = ((i + 1) & TIMESTAMP_INDEX_MASK);
  }
  if (found) {
    if ((timestampObject->no_timestamp_count)
        && (timestampObject->half_interval)) {
      if (timestampObject->timestamp[index] >=
          ((*ptimestamp) + timestampObject->half_interval)) {
        timestampObject->no_timestamp_count--;
        return FALSE;
      }
    }
    if (index != timestampObject->timestamp_tx) {
      timestampObject->timestamp[index] =
          timestampObject->timestamp[timestampObject->timestamp_tx];
    }
    timestampObject->timestamp_tx =
        ((timestampObject->timestamp_tx + 1) & TIMESTAMP_INDEX_MASK);
    //g_print("output %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(timestamp));
    *ptimestamp = timestamp;
    return TRUE;
  } else {
    return FALSE;
  }
}

static void
mfw_gst_sync_all (sMpeg2TimestampObject * timestampObject,
    GstClockTime timestamp)
{
  int i = timestampObject->timestamp_tx;

  while (i != timestampObject->timestamp_rx) {
    timestampObject->timestamp[i] = timestamp;
    i = ((i + 1) & TIMESTAMP_INDEX_MASK);
  }
}
#endif

#if 0
static GstClockTime oldts = 0;
#endif

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2_decode             

DESCRIPTION:            this functon calls the library decode call and populates 
                        the output GstBuffer .

ARGUMENTS PASSED:       
                        mpeg2dec    -   pointer to mpeg2 decoder plugin strcuture.

RETURN VALUE:
                        poinetr to GstBuffer
                        

PRE-CONDITIONS:
                        None.

POST-CONDITIONS:        None.

IMPORTANT NOTES:        None
=============================================================================*/
static GstFlowReturn
mfw_gst_mpeg2_decode (MFW_GST_MPEG2DEC_INFO_T * mpeg2dec)
{
  eMpeg2DecRetType eDecRetVal = E_MPEG2D_FAILURE;
  guint32 decode_bytes = 0;
  gint32 output_size = 0;
  gint32 block_count = 0;
  gfloat time_val = 0;
  guint8 *outdata = NULL;
  GstBuffer *outbuffer = NULL;
  GstCaps *src_caps = NULL;
  GstFlowReturn result = GST_FLOW_OK;
  if (mpeg2dec->demo_mode == 2)
    return GST_FLOW_ERROR;

  output_size = mpeg2dec->padded_width *
      mpeg2dec->padded_height * sizeof (char) * 3 / 2;

  GST_DEBUG (" output_size = %d \n", output_size);

  /* calling the library decode function */
  eDecRetVal = eMPEG2Decode (mpeg2dec->dec_object, &decode_bytes,
      mpeg2dec->dec_object->pvAppContext);

  GST_DEBUG (" Return value of decode library  = % d \n", eDecRetVal);

  if ((E_MPEG2D_FRAME_READY == eDecRetVal)
      || (E_MPEG2D_ENDOF_BITSTREAM == eDecRetVal)
      || (E_MPEG2D_DEMO_PROTECT == eDecRetVal)
      || (E_MPEG2D_ERROR_STREAM == eDecRetVal)
      || (E_MPEG2D_FAILURE == eDecRetVal)) {
    GstClockTime ts = 0;
    GstFlowReturn result;

    mpeg2dec->bit_rate = mpeg2dec->dec_object->sDecParam.bitrate;
#if 0
    mpeg2dec->decoded_frames += 1;
    if (mpeg2dec->frame_rate != 0) {
      time_val = (gfloat) ((gfloat) mpeg2dec->decoded_frames /
          (gfloat) mpeg2dec->frame_rate);
      ts = time_val * 1000 * 1000 * 1000 + mpeg2dec->start_time;
    }

    if ((mfw_gst_get_timestamp (&mpeg2dec->timestamp_object, &ts))) {
      mpeg2dec->decoded_frames = 0;
      mpeg2dec->start_time = ts;
    }
#endif
    ts = TSManagerSend (mpeg2dec->pTS_Mgr);

#if 0
    if (ts < oldts) {
      g_print (RED_STR ("Error stamp!\n", 0));
    }
    oldts = ts;
#endif

    DEMO_LIVE_CHECK (mpeg2dec->demo_mode, ts, mpeg2dec->srcpad);

    BM_RENDER_BUFFER (mpeg2dec->dec_object->sDecParam.sOutputBuffer.pu8YBuf,
        mpeg2dec->srcpad, result, ts, 0);
    return result;
  } else {
    GST_WARNING (" Can not decode with return value = %d \n", eDecRetVal);
    return GST_FLOW_OK;
  }

}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_chain

DESCRIPTION:            This is the main control function for the plugin.
                        During the first call to this function, we initialize
                        the decoder library and subsequently we decode frames.
                        This funtion will be called once per frame.

ARGUMENTS PASSED:
                        pad     -   The sink pad on which chain is registered.
                        buffer  -   The buffer which contains raw video data.

RETURN VALUE:
                        GST_FLOW_OK     -   on success
                        GST_FLOW_ERROR  -   on error.

PRE-CONDITIONS:
                        None.

POST-CONDITIONS:        None.

IMPORTANT NOTES:        None
=============================================================================*/
static GstFlowReturn
mfw_gst_mpeg2dec_chain (GstPad * pad, GstBuffer * buffer)
{

  GstFlowReturn retval = GST_FLOW_OK;
  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec = MFW_GST_MPEG2DEC (GST_PAD_PARENT (pad));
  eMpeg2DecRetType eDecRetVal = E_MPEG2D_FAILURE;
  GstBuffer *outbuffer = NULL;
  GstCaps *caps = NULL;

#if 0
  mfw_gst_receive_timestamp (&mpeg2dec->timestamp_object,
      (GST_BUFFER_TIMESTAMP (buffer)));
#endif
  TSManagerReceive (mpeg2dec->pTS_Mgr, (GST_BUFFER_TIMESTAMP (buffer)));

  if (mpeg2dec->is_init_done == FALSE) {

    GST_DEBUG ("\nComing for the first time to do the decoder  \
                        initialization\n");

    /* concatenation of the codec_data and input buffer */
    buffer = gst_buffer_join (mpeg2dec->codec_data, buffer);


    mpeg2dec->input_buffer = buffer;

    /*      
       When we are coming for the first time, we need to initialize the 
       decoder.
       We first call the QueryMem to get memory requirements.
       We then fill in the structure and then call ReQueryMem. 
       This will raise a callback to get some input data.
     */

    mpeg2dec->dec_object =
        (sMpeg2DecObject *) MPEG2_MALLOC (sizeof (sMpeg2DecObject));

    if (NULL == mpeg2dec->dec_object) {
      GST_ERROR ("Unable to allocate memory for Mpeg2 Dec structure\n");
      return GST_FLOW_ERROR;
    } else {

      mfw_gst_mpeg2dec_initobject (mpeg2dec->dec_object);

    }

    mpeg2dec->dec_object->pvAppContext = mpeg2dec;

    eDecRetVal = eMPEG2DQuerymem (mpeg2dec->dec_object);

    GST_DEBUG (" return val of eMPEG2DQuerymem = %d\n", eDecRetVal);

    if (E_MPEG2D_SUCCESS != eDecRetVal) {
      GST_ERROR ("Function eMPEG2DQuerymem() resulted in failure\n");
      GST_ERROR ("MPEG2D Error Type : %d\n", eDecRetVal);
      /*! Freeing Memory */
      MPEG2_FREE (mpeg2dec->dec_object);
      mpeg2dec->dec_object = NULL;
      return GST_FLOW_ERROR;
    }
    // Allocating Memory for MPEG2 Decoder
    mpeg2dec->memalloc_info = &(mpeg2dec->dec_object->sMemInfo);

    if (-1 == mfw_gst_mpeg2dec_allocmem_dec (mpeg2dec->memalloc_info)) {
      /*! Freeing Memory allocated by the Application */
      MPEG2_FREE (mpeg2dec->dec_object);
      return GST_FLOW_ERROR;
    }

    eDecRetVal = eMPEG2D_Init (mpeg2dec->dec_object);

    GST_DEBUG (" return val of eMPEG2D_Init = %d\n", eDecRetVal);

    Mpeg2_register_func (mpeg2dec->dec_object, mfw_gst_mpeg2dec_callback);


    eDecRetVal = eMPEG2D_Re_Querymem (mpeg2dec->dec_object);


    GST_DEBUG (" return val of eMPEG2D_Re_Querymem = %d\n", eDecRetVal);

    /* 
     * If the return value is E_MPEG2D_NOT_ENOUGH_BITS,
     * waiting for the next data.
     */
    if (E_MPEG2D_NOT_ENOUGH_BITS == eDecRetVal) {
      return GST_FLOW_OK;
    }

    if (E_MPEG2D_SUCCESS != eDecRetVal) {
      GST_ERROR ("Function eMPEG2D_Re_Querymem() resulted in failure\n");
      GST_ERROR ("MPEG2D Error Type : %d\n", eDecRetVal);
      /*! Freeing Memory  */
      mfw_gst_mpeg2dec_free (mpeg2dec->dec_object);
      /* Free the sMpeg2DecObject structure */
      MPEG2_FREE (mpeg2dec->dec_object);
      mpeg2dec->dec_object = NULL;
      return GST_FLOW_ERROR;
    }

    GST_DEBUG (" mpeg2dec->dec_object.sDecParam.u16FrameWidth = %d \n",
        mpeg2dec->dec_object->sDecParam.u16FrameWidth);
    GST_DEBUG (" mpeg2dec->dec_object.sDecParam.u16FrameHeight= %d \n",
        mpeg2dec->dec_object->sDecParam.u16FrameHeight);

    // Allocating Memory for MPEG2 Decoder output       
    mpeg2dec->memalloc_info = &(mpeg2dec->dec_object->sMemInfo);

    if (-1 == mfw_gst_mpeg2dec_allocmem_dec (mpeg2dec->memalloc_info)) {
      /*! Freeing Memory  */
      mfw_gst_mpeg2dec_free (mpeg2dec->dec_object);
      return GST_FLOW_ERROR;
    }
    // Calling MPEG2 Decoder Init Function
    eDecRetVal = eMPEG2D_ReInit (mpeg2dec->dec_object);

    /* Init buffer manager for correct working mode. */
    BM_INIT ((mpeg2dec->bmmode) ? BMINDIRECT : BMDIRECT,
        mpeg2dec->dec_object->sMemInfo.s32MinFrameBufferNum,
        RENDER_BUFFER_MAX_NUM);

    MPEG2D_FrameManager mpeg2dfrm;
    mpeg2dfrm.BfGetter = mfw_gst_MPEG2_getbuffer;
    mpeg2dfrm.BfRejector = mfw_gst_MPEG2_rejectbuffer;
    MPEG2DSetBufferManager (mpeg2dec->dec_object, &mpeg2dfrm);
    MPEG2DSetAdditionalCallbackFunction (mpeg2dec->dec_object, E_RELEASE_FRAME,
        mfw_gst_MPEG2_releasebuffer);


    GST_DEBUG (" return val of eMPEG2D_ReInit = %d\n", eDecRetVal);

    if ((E_MPEG2D_SUCCESS != eDecRetVal)
        && (E_MPEG2D_ENDOF_BITSTREAM != eDecRetVal)) {
      /*!  Freeing Memory  */
      mfw_gst_mpeg2dec_free (mpeg2dec->dec_object);
      return GST_FLOW_ERROR;
    }

    mpeg2dec->is_init_done = TRUE;

  }

  /*  
     This will assign the input buffer to the context and call decode.
   */

  mpeg2dec->input_buffer = buffer;

  retval = mfw_gst_mpeg2_decode (mpeg2dec);
  gst_buffer_unref (buffer);
  return retval;
}


/*=============================================================================
FUNCTION:   mfw_gst_mpeg2dec_src_event   

DESCRIPTION:    send an event to src  pad of mpeg2 decoder element    

ARGUMENTS PASSED:
        Pad     -   pointer to GstPad
        event   -   pointer to GstEvent  
            
RETURN VALUE:
        TRUE    -   sucess
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_mpeg2dec_src_event (GstPad * pad, GstEvent * event)
{
  gboolean result;

  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec =
      MFW_GST_MPEG2DEC (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      /* pushing seek event to upstream element */
      result = gst_pad_push_event (mpeg2dec->sinkpad, event);

      if (TRUE != result) {
        GST_DEBUG
            ("\n Error	in pushing the event,result	is %d\n", result);
        gst_object_unref (mpeg2dec);
        gst_event_unref (event);
        return result;
      }

      break;
    }
    default:
      result = FALSE;
      gst_event_unref (event);
      break;
  }

  gst_object_unref (mpeg2dec);
  return result;

}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_change_state

DESCRIPTION:            This function keeps track of different states of
                        pipeline.

ARGUMENTS PASSED:
                        element     -   pointer to element
                        transition  -   state of the pipeline

RETURN VALUE:
                        GST_STATE_CHANGE_FAILURE    -   the state change failed
                        GST_STATE_CHANGE_SUCCESS    -   the state change 
                                                        succeeded
                        GST_STATE_CHANGE_ASYNC      -   the state change will
                                                        happen asynchronously
                        GST_STATE_CHANGE_NO_PREROLL -   the state change cannot
                                                        be prerolled

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static GstStateChangeReturn
mfw_gst_mpeg2dec_change_state (GstElement * element, GstStateChange transition)
{
  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec = NULL;
  GstStateChangeReturn ret = 0;
  mpeg2dec = MFW_GST_MPEG2DEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      /*
         Reset all the state variables here. The same operations 
         needs to be done even when the plug-in kills after executing.
       */
      mpeg2dec->is_init_done = FALSE;
      mpeg2dec->caps_set = FALSE;
      mpeg2dec->dec_object = NULL;
      mpeg2dec->memalloc_info = NULL;
      mpeg2dec->input_buffer = NULL;
      mpeg2dec->decoded_frames = 0;
      mpeg2dec->start_time = 0;
      // mpeg2dec->outbuffer = NULL;
      mpeg2dec->bit_rate = 0;
#if 0
      mfw_gst_init_timestamp (&mpeg2dec->timestamp_object);
#endif
      resyncTSManager (mpeg2dec->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);

      break;
    }

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = parent_class->change_state (element, transition);

  if (GST_STATE_CHANGE_FAILURE == ret) {
    return ret;
  }

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      /* freeing memory */
      mfw_gst_mpeg2dec_free (mpeg2dec->dec_object);
      MPEG2_FREE (mpeg2dec->dec_object);
      mpeg2dec->dec_object = NULL;

      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      mpeg2dec->is_init_done = FALSE;
      mpeg2dec->caps_set = FALSE;
      mpeg2dec->dec_object = NULL;
      mpeg2dec->memalloc_info = NULL;
      mpeg2dec->input_buffer = NULL;
      mpeg2dec->decoded_frames = 0;
      mpeg2dec->start_time = 0;
      //mpeg2dec->outbuffer = NULL;
      mpeg2dec->bit_rate = 0;
      BM_CLEAN_LIST;
      break;
    default:
      break;
  }
  return ret;
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_sink_event

DESCRIPTION:            Handles an event on the sink pad

ARGUMENTS PASSED:
                        pad     -   the pad on which event needs to be handled.
                        event   -   the event to be handled.

RETURN VALUE:
                        TRUE
                        FALSE

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static gboolean
mfw_gst_mpeg2dec_sink_event (GstPad * pad, GstEvent * event)
{

  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec = MFW_GST_MPEG2DEC (GST_PAD_PARENT (pad));
  gboolean result = TRUE;
  GstFormat format = GST_FORMAT_UNDEFINED;

  GST_DEBUG ("handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gint64 start, stop, position;
      gdouble rate;

      gst_event_parse_new_segment (event, NULL, &rate, &format,
          &start, &stop, &position);

      GST_DEBUG (" receiving new seg \n");
      GST_DEBUG (" start = %" GST_TIME_FORMAT, GST_TIME_ARGS (start));
      GST_DEBUG (" stop = %" GST_TIME_FORMAT, GST_TIME_ARGS (stop));

      GST_DEBUG (" position in mpeg2dec   =%" GST_TIME_FORMAT,
          GST_TIME_ARGS (position));

      if (GST_FORMAT_TIME == format) {
        mpeg2dec->start_time = start;
        mpeg2dec->decoded_frames = 0;
#if 0
        mfw_gst_sync_all (&mpeg2dec->timestamp_object, start);
#endif
        result = gst_pad_push_event (mpeg2dec->srcpad, event);

      } else {
        GST_DEBUG ("dropping newsegment	event in format	%s",
            gst_format_get_name (format));
        gst_event_unref (event);
        result = TRUE;
      }
      resyncTSManager (mpeg2dec->pTS_Mgr, start, MODE_AI);

      break;
    }
    case GST_EVENT_EOS:
    {
      GST_DEBUG ("\n Got the EOS from sinkpad, sending to src pad\n");
      result = gst_pad_push_event (mpeg2dec->srcpad, event);
      if (TRUE != result) {
        GST_ERROR ("\n Error in pushing the event,result is %d\n", result);
      } else {
        GST_DEBUG ("\n EOS event sent to the peer element\n");
      }
      break;
    }
    case GST_EVENT_FLUSH_START:
    {
      result = gst_pad_push_event (mpeg2dec->srcpad, event);

      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      result = gst_pad_push_event (mpeg2dec->srcpad, event);
      if (TRUE != result) {

        GST_ERROR ("\n Error in pushing the event,result	is %d\n",
            result);
        gst_event_unref (event);
      }
      break;
    }
    default:
    {
      result = gst_pad_event_default (pad, event);
      break;
    }
  }

  return result;
}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_set_caps

DESCRIPTION:            this function handles the link with other plug-ins and 
                        used for capability negotiation  between pads

ARGUMENTS PASSED:
                        pad     -   pointer to GstPad
                        caps    -   pointer to GstCaps

RETURN VALUE:
                        TRUE    -   if capabilities are set properly
                        FALSE   -   if capabilities are not set properly
PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static gboolean
mfw_gst_mpeg2dec_set_caps (GstPad * pad, GstCaps * caps)
{


  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec = NULL;
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  gint32 frame_rate_de = 0;
  gint32 frame_rate_nu = 0;
  const gchar *mime;
  GValue *codec_data_buf = NULL;

  GST_DEBUG (" in mfw_gst_mpeg2dec_set_caps function \n");

  mpeg2dec = MFW_GST_MPEG2DEC (gst_pad_get_parent (pad));

  mime = gst_structure_get_name (structure);

  gst_structure_get_fraction (structure, "framerate", &frame_rate_nu,
      &frame_rate_de);
  mpeg2dec->frame_rate = (gfloat) (frame_rate_nu) / frame_rate_de;


  GST_DEBUG (" Frame Rate = %f \n", mpeg2dec->frame_rate);

#if 0
  if (mpeg2dec->frame_rate) {
    mpeg2dec->timestamp_object.half_interval =
        (gint64) 500000000 / mpeg2dec->frame_rate;
  } else {
    mpeg2dec->timestamp_object.half_interval = 0;
  }
#endif
  setTSManagerFrameRate (mpeg2dec->pTS_Mgr, frame_rate_nu, frame_rate_de);

  /* Handle the codec_data information */
  codec_data_buf = (GValue *) gst_structure_get_value (structure, "codec_data");
  if (NULL != codec_data_buf) {
    gint i;
    guint8 *hdrextdata;
    mpeg2dec->codec_data = gst_value_get_buffer (codec_data_buf);;
    mpeg2dec->codec_data_len = GST_BUFFER_SIZE (mpeg2dec->codec_data);
    GST_DEBUG ("\n>>mpeg2 decoder: codec data length is %d\n",
        mpeg2dec->codec_data_len);
    g_print (RED_STR ("\n>>mpeg2 decoder: codec data length is %d\n",
            mpeg2dec->codec_data_len));
    GST_DEBUG ("mpeg2 codec data is\n");
    hdrextdata = GST_BUFFER_DATA (mpeg2dec->codec_data);
    for (i = 0; i < mpeg2dec->codec_data_len; i++) {
      GST_DEBUG ("%x ", hdrextdata[i]);
    }
    GST_DEBUG ("\n");
  }


  if (!gst_pad_set_caps (mpeg2dec->sinkpad, caps)) {
    gst_object_unref (mpeg2dec);
    return FALSE;
  }
  gst_object_unref (mpeg2dec);

  return TRUE;
}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_src_templ

DESCRIPTION:            Template to create a srcpad for the video decoder.

ARGUMENTS PASSED:       
                        None.


RETURN VALUE:           
                        A source pad template for the video pad.


PRE-CONDITIONS:  	    
                        None

POST-CONDITIONS:   	    
                        None

IMPORTANT NOTES:   	    
                        None
=============================================================================*/
static GstPadTemplate *
mfw_gst_mpeg2dec_src_templ (void)
{

  static GstPadTemplate *templ = NULL;

  if (!templ) {
    GstCaps *caps;
    GstStructure *structure;
    GValue list = { 0 }
    , fps = {
    0}
    , fmt = {
    0};
    char *fmts[] = { "I420", NULL };
    guint n;

    caps = gst_caps_new_simple ("video/x-raw-yuv",
        "format", GST_TYPE_FOURCC,
        GST_MAKE_FOURCC ('I', '4', '2', '0'),
        "width", GST_TYPE_INT_RANGE, 16, 4096,
        "height", GST_TYPE_INT_RANGE, 16, 4096, NULL);

    structure = gst_caps_get_structure (caps, 0);

#if 0                           /* Disable the framerate limitation */
    g_value_init (&list, GST_TYPE_LIST);
    g_value_init (&fps, GST_TYPE_FRACTION);

    for (n = 0; fpss[n][0] != 0; n++) {
      gst_value_set_fraction (&fps, fpss[n][0], fpss[n][1]);
      gst_value_list_append_value (&list, &fps);
    }

    gst_structure_set_value (structure, "framerate", &list);
    g_value_unset (&list);
    g_value_unset (&fps);
#endif

    g_value_init (&list, GST_TYPE_LIST);
    g_value_init (&fmt, GST_TYPE_FOURCC);

    for (n = 0; fmts[n] != NULL; n++) {
      gst_value_set_fourcc (&fmt, GST_STR_FOURCC (fmts[n]));
      gst_value_list_append_value (&list, &fmt);
    }

    gst_structure_set_value (structure, "format", &list);
    g_value_unset (&list);
    g_value_unset (&fmt);

    templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  }

  return templ;
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_get_index

DESCRIPTION:            

ARGUMENTS PASSED:
                        element -   pointer to plugin element.   
                        
RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static GstIndex *
mfw_gst_mpeg2dec_get_index (GstElement * element)
{

  /* Presently not implemented. */
  return 0;
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_set_index

DESCRIPTION:            

ARGUMENTS PASSED:
                        element -   pointer to plugin element.
                        index   -   pointer to GstIndex.

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void
mfw_gst_mpeg2dec_set_index (GstElement * element, GstIndex * index)
{

  /* Presently not implemented. */
}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_get_property

DESCRIPTION:            Gets the Property of the element.

ARGUMENTS PASSED:
                        object  -   pointer on which property is to be obtained.
                        prop_id -   ID.
                        value   -   Value to be get.
                        pspec   -   Parameters to be get.

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void
mfw_gst_mpeg2dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_set_property

DESCRIPTION:            Sets the Property of the element.

ARGUMENTS PASSED:
                        object  -   pointer on which property is to be obtained.
                        prop_id -   ID.
                        value   -   Value to be get.
                        pspec   -   Parameters to be get.

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void
mfw_gst_mpeg2dec_set_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec = (MFW_GST_MPEG2DEC_INFO_T *) (object);
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_init

DESCRIPTION:            Create src and sink pads for the plug-in. Add these pads
                        onto the element and register the various functions.

ARGUMENTS PASSED:
                        mpeg2dec    -   pointer to mpeg2decoder element structure

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/


static void
mfw_gst_mpeg2dec_init (MFW_GST_MPEG2DEC_INFO_T * mpeg2dec)
{



  GstElementClass *klass = GST_ELEMENT_GET_CLASS (mpeg2dec);

  /* create the sink and src pads */
  mpeg2dec->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (klass, "sink"), "sink");

  mpeg2dec->srcpad =
      gst_pad_new_from_template (mfw_gst_mpeg2dec_src_templ (), "src");

  gst_element_add_pad (GST_ELEMENT (mpeg2dec), mpeg2dec->sinkpad);
  gst_element_add_pad (GST_ELEMENT (mpeg2dec), mpeg2dec->srcpad);

  gst_pad_set_setcaps_function (mpeg2dec->sinkpad,
      GST_DEBUG_FUNCPTR (mfw_gst_mpeg2dec_set_caps));

  gst_pad_set_event_function (mpeg2dec->sinkpad,
      GST_DEBUG_FUNCPTR (mfw_gst_mpeg2dec_sink_event));

  gst_pad_set_chain_function (mpeg2dec->sinkpad,
      GST_DEBUG_FUNCPTR (mfw_gst_mpeg2dec_chain));

  gst_pad_set_event_function (mpeg2dec->srcpad,
      GST_DEBUG_FUNCPTR (mfw_gst_mpeg2dec_src_event));

  mpeg2dec->pTS_Mgr = createTSManager (0);


#define MFW_GST_MPEG2_DECODER_PLUGIN VERSION
  PRINT_CORE_VERSION (MPEG2DCodecVersionInfo ());
  PRINT_PLUGIN_VERSION (MFW_GST_MPEG2_DECODER_PLUGIN);

  INIT_DEMO_MODE (MPEG2DCodecVersionInfo (), mpeg2dec->demo_mode);
#ifdef MPEG2_MEMORY_DEBUG
  init_memmanager (&mpeg2_tm);
#endif
}

/*======================================================================================
FUNCTION:           mfw_gst_mpeg2dec_finalize

DESCRIPTION:        Class finalized

ARGUMENTS PASSED:   object     - pointer to the elements object

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static void
mfw_gst_mpeg2dec_finalize (GObject * object)
{
  MFW_GST_MPEG2DEC_INFO_T *mpeg2dec = (MFW_GST_MPEG2DEC_INFO_T *) (object);
  destroyTSManager (mpeg2dec->pTS_Mgr);
  GST_DEBUG (">>MPEG2 DEC: class finalized.\n");
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_class_init

DESCRIPTION:            Initialise the class only once (specifying what signals,
                        arguments and virtual functions the class has and 
                        setting up global state)

ARGUMENTS PASSED:
        klass           pointer to mpeg2decoder element class

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void
mfw_gst_mpeg2dec_class_init (MFW_GST_MPEG2DEC_INFO_CLASS_T * klass)
{

  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  gobject_class->set_property = mfw_gst_mpeg2dec_set_property;
  gobject_class->get_property = mfw_gst_mpeg2dec_get_property;
  gstelement_class->change_state = mfw_gst_mpeg2dec_change_state;
  gstelement_class->set_index = mfw_gst_mpeg2dec_set_index;
  gstelement_class->get_index = mfw_gst_mpeg2dec_get_index;
  gobject_class->finalize = mfw_gst_mpeg2dec_finalize;


}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg2dec_base_init

DESCRIPTION:            Element details are registered with the plugin during
                        base_init ,This function will initialise the class and 
                        child class properties during each new child class 
                        creation.

ARGUMENTS PASSED:
        Klass           void pointer

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void
mfw_gst_mpeg2dec_base_init (gpointer klass)
{

  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&mfw_gst_mpeg2dec_sink_template_factory));

  gst_element_class_add_pad_template (element_class,
      mfw_gst_mpeg2dec_src_templ ());

  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE (element_class, "mpeg2 video decoder",
      "Codec/Decoder/Video", "Decode compressed mpeg2 video to raw data");

}

/*=============================================================================

FUNCTION:               mfw_gst_mpeg2dec_get_type

DESCRIPTION:            intefaces are initiated in this function.you can 
                        register one or more interfaces after having 
                        registered the type itself.

ARGUMENTS PASSED:
                        None

RETURN VALUE:
                        A numerical value ,which represents the unique 
                        identifier of this elment(mpeg2decoder).

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
GType
mfw_gst_mpeg2dec_get_type (void)
{
  static GType mpeg2dec_type = 0;

  if (!mpeg2dec_type) {
    static const GTypeInfo gstmpeg2dec_info = {
      sizeof (MFW_GST_MPEG2DEC_INFO_CLASS_T),
      mfw_gst_mpeg2dec_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_mpeg2dec_class_init,
      NULL,
      NULL,
      sizeof (MFW_GST_MPEG2DEC_INFO_T),
      0,
      (GInstanceInitFunc) mfw_gst_mpeg2dec_init,
    };
    mpeg2dec_type = g_type_register_static (GST_TYPE_ELEMENT,
        "MFW_GST_MPEG2DEC_INFO_T", &gstmpeg2dec_info, 0);
  }

  GST_DEBUG_CATEGORY_INIT (mfw_gst_mpeg2dec_debug, "mfw_mpeg2decoder", 0,
      "FreeScale's MPEG2 Video Decoder's Log");
  return mpeg2dec_type;
}

/*=============================================================================
FUNCTION:               plugin_init

DESCRIPTION:            special function , which is called as soon as the 
                        plugin or element is loaded and information returned 
                        by this function will be cached in central registry

ARGUMENTS PASSED:
                        plugin  -   pointer to container that contains features loaded
                        from shared object module

RETURN VALUE:
                        return TRUE or FALSE depending on whether it loaded 
                        initialized any dependency correctly.
PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "mfw_mpeg2decoder",
      GST_RANK_PRIMARY, MFW_GST_TYPE_MPEG2DEC);
}

FSL_GST_PLUGIN_DEFINE ("mpeg2dec", "mpeg2 video decoder", plugin_init);

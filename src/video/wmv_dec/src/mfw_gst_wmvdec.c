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
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc. All rights reserved. 
 *
 */


 
/*
 * Module Name:    mfw_gst_wmvdecode.c
 *
 * Description:    GStreamer Plug-in for WMV-Decoder.
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
#include <gst/base/gstadapter.h>
#include "wmv789_dec_api.h"
#include "mfw_gst_wmvdec.h"

#include "mfw_gst_utils.h"


/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/


/*=============================================================================
                STATIC VARIABLES (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

static GstStaticPadTemplate mfw_gst_wmvdec_sink_template_factory =
GST_STATIC_PAD_TEMPLATE("sink",
            GST_PAD_SINK,
            GST_PAD_ALWAYS,
            GST_STATIC_CAPS("video/x-wmv, "
                    "wmvversion = (int) [1,2], "
                    "systemstream = (boolean) false; "
                                        "video/x-msmpeg, "
                    "msmpegversion = (int) [41, 43], "
                    "systemstream = (boolean) false")

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
#define GST_CAT_DEFAULT         mfw_gst_wmvdec_debug


/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/

/* None. */

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_wmvdec_debug);

static void mfw_gst_wmvdec_class_init(MFW_GST_WMVDEC_INFO_CLASS_T * klass);
static void mfw_gst_wmvdec_base_init(gpointer klass);

static void mfw_gst_wmvdec_init(MFW_GST_WMVDEC_INFO_T * filter);

/* Set and Get Property are not used in this version. */

static void mfw_gst_wmvdec_set_property(GObject * object, guint prop_id,
                    const GValue * value,
                    GParamSpec * pspec);

static void mfw_gst_wmvdec_get_property(GObject * object, guint prop_id,
                    GValue * value,
                    GParamSpec * pspec);

static gboolean mfw_gst_wmvdec_set_caps(GstPad * pad, GstCaps * caps);
static GstFlowReturn mfw_gst_wmvdec_chain(GstPad * pad, GstBuffer * buf);
static GstStateChangeReturn mfw_gst_wmvdec_change_state(GstElement *
                            element,
                            GstStateChange
                            transition);
static eWmv9DecRetType mfw_gst_wmv_decode(MFW_GST_WMVDEC_INFO_T * wmv_dec);
static gboolean mfw_gst_wmvdec_sink_event(GstPad * pad, GstEvent * event);
WMV9D_Void AppFreeMemory(sWmv9DecMemAllocInfoType * psMemInfo);
/* call back function used to get the part of bitstream */
WMV9D_S32 AppGetBitstream(WMV9D_S32 s32BufLen,
              WMV9D_U8 * pu8Buf,
              WMV9D_S32 * bEndOfFrame,
              WMV9D_Void * pvAppContext);
static gboolean
mfw_gst_wmvdec_src_event(GstPad * src_pad, GstEvent * event);
/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/

/* None. */

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/
/*=============================================================================
FUNCTION:               AppAllocateSlowMemory

DESCRIPTION:            Allocates Slow Memory to the decoder.

ARGUMENTS PASSED:       size    -> bytes to be allocated.


RETURN VALUE:           void*   -> pointer to the allocated memory.


PRE-CONDITIONS:         None.


POST-CONDITIONS:          None.

IMPORTANT NOTES:           None.
=============================================================================*/

WMV9D_Void *AppAllocateSlowMemory(WMV9D_S32 s32Size)
{
    return g_malloc(s32Size);
}

/*=============================================================================
FUNCTION:               AppAllocateFastMemory

DESCRIPTION:            Allocates Fast Memory to the decoder.

ARGUMENTS PASSED:       size    -> bytes to be allocated.


RETURN VALUE:           void*   -> pointer to the allocated memory.


PRE-CONDITIONS:         None.


POST-CONDITIONS:          None.

IMPORTANT NOTES:           Right now, there is no difference between fast memory and
                        slow memory. Going ahead, when we can get memory from cache
                        able and non-cacheable region, we can use these 2 calls
                        to get fast memory for Decoder internals and normal memory 
                        for our usage.
=============================================================================*/

WMV9D_Void *AppAllocateFastMemory(WMV9D_S32 s32Size)
{
    return g_malloc(s32Size);
}

/*=============================================================================
FUNCTION:               AppAllocateMemory

DESCRIPTION:            Allocates memory to the decoder structures.

ARGUMENTS PASSED:       psMemInfo -> Pointer to the memory structure .


RETURN VALUE:           0 -> Success.


PRE-CONDITIONS:         None.


POST-CONDITIONS:          None.

IMPORTANT NOTES:           None.
=============================================================================*/

int AppAllocateMemory(sWmv9DecMemAllocInfoType * psMemInfo)
{
    WMV9D_S32 s32Count = 0;
    WMV9D_Void *pvUnalignedBuf = NULL;
    WMV9D_Void *pvOldBuf = NULL;
    WMV9D_S32 s32ExtraSize = 0;
    WMV9D_S32 s32Mask = 0;

    for (s32Count = 0; s32Count < psMemInfo->s32NumReqs; s32Count++) {
    sWmv9DecMemBlockType *psMemBlk = psMemInfo->asMemBlks + s32Count;

    /* Get the extra amount to be allocated for the alignment */
    switch (psMemBlk->eMemAlign) {
    case E_WMV9D_ALIGN_NONE:
        s32ExtraSize = 0;
        s32Mask = 0xffffffff;
        break;

    case E_WMV9D_ALIGN_HALF_WORD:
        s32ExtraSize = 1;
        s32Mask = 0xfffffffe;
        break;

    case E_WMV9D_ALIGN_WORD:
        s32ExtraSize = 3;
        s32Mask = 0xfffffffc;
        break;

    case E_WMV9D_ALIGN_DWORD:
        s32ExtraSize = 7;
        s32Mask = 0xfffffff8;
        break;

    case E_WMV9D_ALIGN_QWORD:
        s32ExtraSize = 15;
        s32Mask = 0xfffffff0;
        break;

    case E_WMV9D_ALIGN_OCTAWORD:
        s32ExtraSize = 31;
        s32Mask = 0xffffffe0;
        break;

    default:
        s32ExtraSize = -1;    /* error condition */
        s32Mask = 0x00000000;
        break;
    }

    /* Save the old pointer, in case memory is being reallocated */
    pvOldBuf = psMemBlk->pvBuffer;

    /* Allocate new memory, if required */
    if (psMemBlk->s32Size > 0) {
        if (WMV9D_IS_SLOW_MEMORY(psMemBlk->s32MemType))
        pvUnalignedBuf = AppAllocateSlowMemory(psMemBlk->s32Size +
                               s32ExtraSize);
        else
        pvUnalignedBuf = AppAllocateFastMemory(psMemBlk->s32Size +
                               s32ExtraSize);

        psMemBlk->pvBuffer = (WMV9D_Void *)
        (((WMV9D_S32) pvUnalignedBuf + s32ExtraSize) & s32Mask);

    } else {
        pvUnalignedBuf = NULL;
        psMemBlk->pvBuffer = NULL;
    }

    /* Check if the memory is being reallocated */
    if (WMV9D_IS_SIZE_CHANGED(psMemBlk->s32MemType)) {
        if (psMemBlk->s32OldSize > 0) {
        WMV9D_S32 s32CopySize = psMemBlk->s32OldSize;

        if (psMemBlk->s32Size < s32CopySize)
            s32CopySize = psMemBlk->s32Size;

        if (WMV9D_NEEDS_COPY_AT_RESIZE(psMemBlk->s32MemType))
            memcpy(psMemBlk->pvBuffer, pvOldBuf, s32CopySize);

        free(psMemBlk->pvUnalignedBuffer);
        }
    }

    /* Now save the new unaligned buffer pointer */
    psMemBlk->pvUnalignedBuffer = pvUnalignedBuf;
    }

    return 0;
}

/*=============================================================================
FUNCTION:               AppFreeMemory

DESCRIPTION:            Frees the memory allocated to the decoder structure.

ARGUMENTS PASSED:       psMemInfo -> Pointer to the memory structure .


RETURN VALUE:           None.


PRE-CONDITIONS:         None.


POST-CONDITIONS:          None.

IMPORTANT NOTES:           None.
=============================================================================*/

WMV9D_Void AppFreeMemory(sWmv9DecMemAllocInfoType * psMemInfo)
{
    WMV9D_S32 s32Count;

    for (s32Count = 0; s32Count < psMemInfo->s32NumReqs; s32Count++) {
    if (psMemInfo->asMemBlks[s32Count].s32Size > 0) {

        if (psMemInfo->asMemBlks[s32Count].pvUnalignedBuffer != NULL) {
        g_free(psMemInfo->asMemBlks[s32Count].pvUnalignedBuffer);
        psMemInfo->asMemBlks[s32Count].pvUnalignedBuffer = NULL;
        }

    }
    }
}

/*=============================================================================
FUNCTION:               AppGetBitstream

DESCRIPTION:            Input buffer callback function should be a synchronous function
                        used by the decoder to read the portion of bit stream from the
                        application. This function is called by the decoder in eWMV9DInit
                        and eWMV9DDecode functions, when it needs the bit stream data.
                        The decoder assumes that the application knows the end of the
                        bit stream data for the current frame, and data returned is for
                        current frame only. While initializing, only sequence header data
                        should be returned..

ARGUMENTS PASSED:       s32BufLen    -> Length of the buffer provided.
                        pu8Buf         -> Pointer to the buffer.
                        bEndOfFrame     -> Set to 1, if no more data for the current
                                        frame (or sequence header) is available.
                         pvAppContext ->    Caller context, as set while initializing
                                        the decoder. It can be used to distinguish
                                        between calls from different decoding threads
                                        in multi-threaded environment.



RETURN VALUE:           s32NumRead   -> Number of bytes provided to the decoder.


PRE-CONDITIONS:         None.


POST-CONDITIONS:          None.

IMPORTANT NOTES:           When the call back is called for the first time, we need
                        to provide the SeqData to it. This data is required for
                        initialization of the library. Subsequent calls from the library
                        will get raw compressed data.
=============================================================================*/

WMV9D_S32 AppGetBitstream(WMV9D_S32 s32BufLen, WMV9D_U8 * pu8Buf,
              WMV9D_S32 * bEndOfFrame,
              WMV9D_Void * pvAppContext)
{

    sInputHandlerType *psInHandler = NULL;
    WMV9D_S32 s32NumRead = -1;
    MFW_GST_WMVDEC_INFO_T *wmvdec;
    wmvdec = (MFW_GST_WMVDEC_INFO_T*) pvAppContext;
    psInHandler =
    (sInputHandlerType *) &wmvdec->wmvdec_handle_info;
    GstAdapter *adapter = NULL;
    guint bytesavailable = 0;
    guint8 *temp_buffer;

    
    if(!wmvdec->is_init_done)
    {
        /*
        The first call is for the sequence data, return that from the handler. This is
        called from the init routine.
        */
        if (psInHandler->s32SeqDataLen > 0) 
        {
            memcpy(pu8Buf, psInHandler->SeqData, psInHandler->s32SeqDataLen);
        }
        s32NumRead = psInHandler->s32SeqDataLen;        
        *bEndOfFrame = 1;
    }

    else 
    {

        adapter = wmvdec->adapter;
        bytesavailable = gst_adapter_available(adapter);
        if (bytesavailable <= s32BufLen) 
        {

            temp_buffer =
            (guint8 *) gst_adapter_peek(adapter, bytesavailable);
            memcpy(pu8Buf, temp_buffer, bytesavailable);
            *bEndOfFrame = 1;
            s32NumRead = bytesavailable;
            gst_adapter_flush(adapter, bytesavailable);
        } 
        else 
        {

            temp_buffer = (guint8 *) gst_adapter_peek(adapter, s32BufLen);
            memcpy(pu8Buf, temp_buffer, s32BufLen);
            *bEndOfFrame = 0;
            s32NumRead = s32BufLen;
            gst_adapter_flush(adapter, s32BufLen);
        }

    }
    return s32NumRead;
}


static eWmv9DecRetType mfw_gst_wmv_init_decoder(MFW_GST_WMVDEC_INFO_T * wmvdec)
{
    int status = E_WMV9D_SUCCESS;
    sInputHandlerType *psInHandler = (sInputHandlerType *) & wmvdec->wmvdec_handle_info;
    sWmv9DecObjectType *sDecObj = NULL;

    GST_DEBUG("Create & init wmv decoder library...\n");
    /*
       When we are coming for the first time, we need to initialize the decoder.
       We first call the QueryMem to get memory requirements and then call the init.
     */
   

    sDecObj = (sWmv9DecObjectType *) & psInHandler->sDecObj;
    status = eWMV9DQuerymem(sDecObj, psInHandler->s32Height,
                psInHandler->s32Width);

    /* Allocate memory for the decoder */
    if (AppAllocateMemory(&sDecObj->sMemInfo)) {
        GST_ERROR("\nFailed to allocate memory for the decoder.\n");

        GError *error = NULL;
        GQuark domain;
        domain = g_quark_from_string("mfw_wmvdecoder");
        error = g_error_new(domain, 10, "fatal error");
        gst_element_post_message(GST_ELEMENT(wmvdec),
            gst_message_new_error(GST_OBJECT
            (wmvdec),
            error,
            "non recoverable error while allocating memory for the"
            " the WMV Decoder"));
        
        return status;

    }

    sDecObj->sDecParam.sOutputBuffer.tOutputFormat  = IYUV_WMV_PADDED;


    /*
       Save the dec object pointer in the handler, so we that we can access
       it in the callback, if required.  (FIXME: Can be optimized)
     */
    psInHandler->pWmv9Object = sDecObj;

    /* Do the initialization of the structure, and call init */
    sDecObj->pfCbkBuffRead = AppGetBitstream;
    sDecObj->pvAppContext = (void*)wmvdec;
    sDecObj->sDecParam.s32FrameRate = psInHandler->s32FrameRate;
    sDecObj->sDecParam.s32BitRate = psInHandler->s32BitRate;
    sDecObj->sDecParam.u16FrameWidth =
        (WMV9D_U16) psInHandler->s32Width;
    sDecObj->sDecParam.u16FrameHeight =
        (WMV9D_U16) psInHandler->s32Height;
    sDecObj->sDecParam.eCompressionFormat =
        eWMV9DCompFormat(psInHandler->pu8CompFmtString);

    GST_DEBUG("\n%d\n", psInHandler->pu8CompFmtString[3]);
    GST_DEBUG("\nCompression format is %d\n",
          sDecObj->sDecParam.eCompressionFormat);
    GST_DEBUG("\nDecoder Init being called\n");
    status = eWMV9DInit(sDecObj);

    if (status != E_WMV9D_SUCCESS) {
        GST_ERROR
        ("\nCould not initialize the wmv decoder, error is %d\n",
         status);
        GError *error = NULL;
        GQuark domain;
        domain = g_quark_from_string("mfw_wmvdecoder");
        error = g_error_new(domain, 10, "fatal error");
        gst_element_post_message(GST_ELEMENT(wmvdec),
            gst_message_new_error(GST_OBJECT
            (wmvdec),
            error,
            "non recoverable error while initializing"
            " the WMV Decoder"));

        return status;
    }
    
    GST_DEBUG("\nDecoder library is inited successfully\n");
    wmvdec->is_init_done = TRUE;
    wmvdec->fatal_err_found = TRUE; /* recovreed from previous error */
    
    return status;
}

/*=============================================================================
FUNCTION:               mfw_gst_wmv_decode

DESCRIPTION:            Main Decoding call for the library. This call will be called
                        once per frame. Once the data is decoded successfully,
                        it is pushed onto its neighbouring element.

ARGUMENTS PASSED:       wmv_dec -> Pointer to the context variable.


RETURN VALUE:           E_WMV9D_SUCCESS  -> On success.


PRE-CONDITIONS:         None.


POST-CONDITIONS:          None.
=============================================================================*/

static eWmv9DecRetType mfw_gst_wmv_decode(MFW_GST_WMVDEC_INFO_T * wmvdec)
{
    eWmv9DecRetType status = E_WMV9D_SUCCESS;
    GstCaps *src_caps = NULL;
    GstCaps *caps = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    sWmv9DecYCbCrBufferType *psFrameBuffer = NULL;
    WMV9D_S32 s32BytesRead = 0;
    WMV9D_S32 s32Buffer = gst_adapter_available(wmvdec->adapter);
    sInputHandlerType *psInHandler = NULL;
    int output_size = 0;
    GstBuffer *outbuffer = NULL;
    char *outdata = NULL;
    int copy_size = 0;
    int offset = 0;


    if (wmvdec->demo_mode == 2)
        return GST_FLOW_ERROR;

    psFrameBuffer =
    &(wmvdec->wmvdec_handle_info.sDecObj.sDecParam.sOutputBuffer);
    psInHandler = (sInputHandlerType *) & wmvdec->wmvdec_handle_info;


    DEMO_LIVE_CHECK(wmvdec->demo_mode, 
        psInHandler->timestamp, 
        wmvdec->srcpad);
    

    status =
    eWMV9DDecode(&(wmvdec->wmvdec_handle_info.sDecObj), s32Buffer);



    switch (status) {
    case E_WMV9D_SUCCESS:
    case E_WMV9D_NOT_ENOUGH_BITS:
    case E_WMV9D_BAD_MEMORY:
    case E_WMV9D_WRONG_ALIGNMENT:
    case E_WMV9D_SIZE_CHANGED:
    case E_WMV9D_NO_OUTPUT:
    case E_WMV9D_BROKEN_FRAME:
    case E_WMV9D_DEMO_PROTECT:    
    break;
    default:
    {
        GST_ERROR("wmv decode:Frame failed for decoding, status %d\n",
              status);
        break;
    }
    }
    if ((status != E_WMV9D_SUCCESS) && (status != E_WMV9D_DEMO_PROTECT)) {
        
        
       

    /*  
       There is no output and hence we are not displaying anything.
       We are going out to get a new frame of data. 
     */

    GST_ERROR("Error in decoding status = %d\n", status);
    return status;
    } else {




    /*
       When the control come here, it means that the frame was successfully decoded
       and we can push the data onto next element. Since WMV Decoder does not explicitly
       allocate memory for the output buffer, we need to copy the data from the v4l 
       buffer before passing it onto next element. This will affect the performance 
       and is a temporary solution.
     */
    if (wmvdec->caps_set == FALSE) {
        gint64 start = 0;    /*  proper timestamp has to set here */
        guint fourcc = GST_STR_FOURCC("I420");
        gint crop_height;
        gint crop_width; 
        gint EXPANDY = 64;
                
        crop_height = (16 - psInHandler->s32Height&0xf)&0xf;    
        crop_width = (16 - psInHandler->s32Width&0xf)&0xf;

        wmvdec->padded_height = psInHandler->s32Height + crop_height;
        wmvdec->padded_width = psInHandler->s32Width + crop_width;
        wmvdec->padded_height += EXPANDY;
        wmvdec->padded_width  += EXPANDY;
            
        wmvdec->caps_set = TRUE;


        /*  FIXME: The fourcc code is temporarily set to 0 
           and the frame rate is set to 15 */

        caps = gst_caps_new_simple("video/x-raw-yuv",
                       "format", GST_TYPE_FOURCC, fourcc,
                       "width", G_TYPE_INT,
                       wmvdec->padded_width, "height",
                       G_TYPE_INT,  wmvdec->padded_height,
                       "pixel-aspect-ratio",
                       GST_TYPE_FRACTION,
                       1,1, "framerate",GST_TYPE_FRACTION, 
                       psInHandler->u32FrameRateNu,psInHandler->u32FrameRateDe,
                       CAPS_FIELD_CROP_BOTTOM,G_TYPE_INT,(crop_height+EXPANDY/2),
                       CAPS_FIELD_CROP_RIGHT,G_TYPE_INT,(crop_width+EXPANDY/2), 
                       CAPS_FIELD_CROP_TOP,G_TYPE_INT,EXPANDY/2,
                       CAPS_FIELD_CROP_LEFT,G_TYPE_INT,EXPANDY/2,
                       NULL);

        if (!(gst_pad_set_caps(wmvdec->srcpad, caps))) {
        GST_ERROR
            ("\nCould not set the caps for the wmvdecoder src pad\n");
        return E_WMV9D_FAILED;
        }
        gst_caps_unref(caps);
        caps = NULL;

    }

    /*
       An output buffer of size equal to the size of a YUV420 frame is being
       created. The output buffer will have the capabilities as set above and will
       be created on the srcpad of the plug-in. Going ahead, this call will give us
       a de-queued buffer from v4l plug-in.
     */

    output_size =
         wmvdec->padded_height *  wmvdec->padded_width * 3 / 2;
    copy_size =  wmvdec->padded_height *  wmvdec->padded_width;



    src_caps = GST_PAD_CAPS(wmvdec->srcpad);
    result =
        gst_pad_alloc_buffer_and_set_caps(wmvdec->srcpad, 0,
                          output_size, src_caps,
                          &outbuffer);

    if (result != GST_FLOW_OK) {
        GST_ERROR
        ("Can not create output buffer for wmv decoder source pad\n");
        return E_WMV9D_FAILED;
    }

    outdata = (char *) GST_BUFFER_DATA(outbuffer);
    
    status = eWMV9DecGetOutputFrame(&(wmvdec->wmvdec_handle_info.sDecObj));
    if(status != E_WMV9D_SUCCESS)
    {
        GST_ERROR("error in eWMV9DecGetOutputFrame");
        return status;
    }
    
    memcpy(outdata,psFrameBuffer->pu8YBuf,copy_size);
    outdata += copy_size;
    memcpy(outdata, psFrameBuffer->pu8CbBuf,copy_size/4);
    outdata += copy_size/4;
    memcpy(outdata, psFrameBuffer->pu8CrBuf,copy_size/4);



    GST_BUFFER_SIZE(outbuffer) = output_size;
    GST_BUFFER_TIMESTAMP(outbuffer) = psInHandler->timestamp;
    GST_BUFFER_DURATION(outbuffer) = psInHandler->u32TimePerFrame;
    //wmvdec->last_ts = GST_BUFFER_TIMESTAMP(outbuffer);
    if (wmvdec->state == 1) {
        wmvdec->state = 0;
        if (outbuffer) {
        gst_buffer_unref(outbuffer);
        outbuffer = NULL;
        }
        return E_WMV9D_SUCCESS;
    }

    result = gst_pad_push(wmvdec->srcpad, outbuffer);
    if (result == GST_FLOW_OK) {

        return E_WMV9D_SUCCESS;
    } else {
        GST_ERROR
        ("Can not push the output data to sink element, reason is %d\n",
         result);
        return E_WMV9D_FAILED;

    }
    }

    return status;
}

/*=============================================================================
FUNCTION:               src_templ

DESCRIPTION:            Template to create a srcpad for the decoder.

ARGUMENTS PASSED:       None.


RETURN VALUE:           a GstPadTemplate


PRE-CONDITIONS:          None

POST-CONDITIONS:           None

IMPORTANT NOTES:           None
=============================================================================*/
static GstPadTemplate *src_templ(void)
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
    char *fmts[] = { "YV12", "I420", "Y42B", NULL };
    guint n;

    caps = gst_caps_new_simple("video/x-raw-yuv",
                   "format", GST_TYPE_FOURCC,
                   GST_MAKE_FOURCC('I', '4', '2', '0'),
                   "width", GST_TYPE_INT_RANGE, 16, 4096,
                   "height", GST_TYPE_INT_RANGE, 16, 4096,
                   NULL);

    structure = gst_caps_get_structure(caps, 0);

#if 0
    g_value_init(&list, GST_TYPE_LIST);
    g_value_init(&fps, GST_TYPE_FRACTION);
    for (n = 0; fpss[n][0] != 0; n++) {
        gst_value_set_fraction(&fps, fpss[n][0], fpss[n][1]);
        gst_value_list_append_value(&list, &fps);
    }
    gst_structure_set_value(structure, "framerate", &list);
    g_value_unset(&list);
    g_value_unset(&fps);

#endif

    g_value_init(&list, GST_TYPE_LIST);
    g_value_init(&fmt, GST_TYPE_FOURCC);
    for (n = 0; fmts[n] != NULL; n++) {
        gst_value_set_fourcc(&fmt, GST_STR_FOURCC(fmts[n]));
        gst_value_list_append_value(&list, &fmt);
    }
    gst_structure_set_value(structure, "format", &list);
    g_value_unset(&list);
    g_value_unset(&fmt);

    templ =
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
    }
    return templ;
}



/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_set_caps

DESCRIPTION:            this function handles the link with other plug-ins and used for
                        capability negotiation  between pads

ARGUMENTS PASSED:
        pad             pointer to GstPad
        caps            pointer to GstCaps

RETURN VALUE:
        TRUE            if capabilities are set properly
        FALSE           if capabilities are not set properly
PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean mfw_gst_wmvdec_set_caps(GstPad * pad, GstCaps * caps)
{



    MFW_GST_WMVDEC_INFO_T *wmvdec = NULL;
    sInputHandlerType *psInHandler = NULL;

    int wmvversion = 0;
    GValue *codec_data = NULL;
    GstBuffer *gstbuf = NULL;
    unsigned char *dataptr = NULL;

    GstStructure *structure = gst_caps_get_structure(caps, 0);

    wmvdec = MFW_GST_WMVDEC(gst_pad_get_parent(pad));
    psInHandler = (sInputHandlerType *) & wmvdec->wmvdec_handle_info;
    psInHandler->s32SeqDataLen = 0;

    wmvdec->savecaps = gst_caps_copy((const GstCaps *) caps);

    psInHandler->pu8CompFmtString = (WMV9D_U8 *) g_malloc(5);


    gst_structure_get_int(structure, "wmvversion", &wmvversion);
    GST_DEBUG("\nVersion of WMV decoder is %d\n", wmvversion);
    if (wmvversion > 3) {
    GST_ERROR
        ("\nVersion of the incoming file is wrong.... exitting\n");
    return FALSE;
    }
    switch (wmvversion) {
    case 1:
    psInHandler->pu8CompFmtString[0] = 'W';
    psInHandler->pu8CompFmtString[1] = 'M';
    psInHandler->pu8CompFmtString[2] = 'V';
    psInHandler->pu8CompFmtString[3] = '1';
    break;
    case 2:
    psInHandler->pu8CompFmtString[0] = 'W';
    psInHandler->pu8CompFmtString[1] = 'M';
    psInHandler->pu8CompFmtString[2] = 'V';
    psInHandler->pu8CompFmtString[3] = '2';
    break;
    case 3:
    psInHandler->pu8CompFmtString[0] = 'W';
    psInHandler->pu8CompFmtString[1] = 'M';
    psInHandler->pu8CompFmtString[2] = 'V';
    psInHandler->pu8CompFmtString[3] = '3';
    break;
    }
    psInHandler->pu8CompFmtString[4] = '\0';

    gst_structure_get_int(structure, "width", &psInHandler->s32Width);
    GST_DEBUG("\nInput Width is %d\n", psInHandler->s32Width);

    gst_structure_get_int(structure, "height", &psInHandler->s32Height);
    GST_DEBUG("\nInput Height is %d\n", psInHandler->s32Height);

    codec_data =
    (GValue *) gst_structure_get_value(structure, "codec_data");

    if (codec_data) {
        gstbuf = gst_value_get_buffer(codec_data);
        dataptr = GST_BUFFER_DATA(gstbuf);
        psInHandler->s32SeqDataLen = GST_BUFFER_SIZE(gstbuf);
        psInHandler->SeqData = g_malloc(psInHandler->s32SeqDataLen);
        memcpy(psInHandler->SeqData, GST_BUFFER_DATA(gstbuf),
            psInHandler->s32SeqDataLen);
    }

    GST_DEBUG("\nCodec specific data length is %d\n",
          psInHandler->s32SeqDataLen);


    GST_DEBUG("\nCodec specific data is %x\n", psInHandler->SeqData);



    gst_structure_get_fraction (structure, "framerate",
                                &psInHandler->u32FrameRateNu,
                                &psInHandler->u32FrameRateDe);
    GST_DEBUG("\nFrame Rate is %d/%d\n", psInHandler->u32FrameRateNu, 
                                         psInHandler->u32FrameRateDe);
    
    if (psInHandler->u32FrameRateDe > 0)
        psInHandler->u32TimePerFrame = 
            gst_util_uint64_scale_int (GST_SECOND,
                                       psInHandler->u32FrameRateDe,
                                       psInHandler->u32FrameRateNu);
    else
        psInHandler->u32TimePerFrame = 
            gst_util_uint64_scale_int (GST_SECOND, 1,
                                       DEFAULT_FRAME_RATE_NUMERATOR);

    psInHandler->s32BitRate = 0;
    psInHandler->s32FrameRate = psInHandler->u32FrameRateNu / 
                                psInHandler->u32FrameRateDe;

    if (!gst_pad_set_caps(wmvdec->sinkpad, caps)) {
    GST_ERROR("set caps error\n");
        gst_object_unref(wmvdec);
    return FALSE;
    }

    gst_object_unref(wmvdec);

    return TRUE;
}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_sink_event

DESCRIPTION:            Handles an event on the sink pad

ARGUMENTS PASSED:
                        pad     -> the pad on which event needs to be handled.
                        event   -> the event to be handled.

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
static gboolean mfw_gst_wmvdec_sink_event(GstPad * pad, GstEvent * event)
{


    MFW_GST_WMVDEC_INFO_T *wmvdec = MFW_GST_WMVDEC(GST_PAD_PARENT(pad));
    gboolean result = TRUE;
    GstFormat format = GST_FORMAT_UNDEFINED;



    switch (GST_EVENT_TYPE(event)) {

    case GST_EVENT_NEWSEGMENT:
    {

        GstFormat format;
        gint64 start, stop, position;
        gst_event_parse_new_segment(event, NULL, NULL, &format, &start,
                    &stop, &position);
        wmvdec->flush = FALSE;
        wmvdec->error_count = 0;
        GST_DEBUG(" start = %" GST_TIME_FORMAT, GST_TIME_ARGS(start));
        GST_DEBUG(" stop = %" GST_TIME_FORMAT, GST_TIME_ARGS(stop));
        GST_DEBUG(" \n position =%" GST_TIME_FORMAT,
              GST_TIME_ARGS(position));
        wmvdec->seeked_time = position;

        if (format == GST_FORMAT_TIME) {
        result =  gst_pad_event_default (pad, event);
        if (TRUE != result) {
            GST_ERROR("Error in pushing the event,result is %d", result);
            result = FALSE;
        }
        } else {
        gst_event_unref(event);
        result = TRUE;
        }
        break;
    }

    case GST_EVENT_EOS:
    {
        GST_DEBUG
        ("\n Got the EOS from Neighbouring element in mfw_gst_wmvdec_sink_event\n");

        result = gst_pad_push_event(wmvdec->srcpad, event);
        if (result != TRUE) {
        GST_ERROR("\n Error in pushing the event,result is %d\n",
              result);
        }
        
        break;
    }

    case GST_EVENT_FLUSH_START:
    {
        wmvdec->flush = 1;
        result = gst_pad_push_event(wmvdec->srcpad, event);

        break;
    }

    case GST_EVENT_FLUSH_STOP:
    {
       /*
        This section is commented for smooth playback while seeking.
        The new library (from mmcodecs) is not behaving as expected 
        when this section is uncommented.
       */

        result = gst_pad_push_event(wmvdec->srcpad, event);
        break;
    }
    default:
    {
        result = gst_pad_event_default(pad, event);
        break;
    }
    }
    return result;
}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_change_state

DESCRIPTION:            This function keeps track of different states of pipeline.

ARGUMENTS PASSED:
                element     -> pointer to element
                transition  -> state of the pipeline

RETURN VALUE:
        GST_STATE_CHANGE_FAILURE     the state change failed
        GST_STATE_CHANGE_SUCCESS     the state change succeeded
        GST_STATE_CHANGE_ASYNC       the state change will happen
                                        asynchronously
        GST_STATE_CHANGE_NO_PREROLL  the state change cannot be prerolled

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static GstStateChangeReturn
mfw_gst_wmvdec_change_state(GstElement * element,
                GstStateChange transition)
{
    MFW_GST_WMVDEC_INFO_T *wmvdec = NULL;
    GstStateChangeReturn ret;
    wmvdec = MFW_GST_WMVDEC(element);

    sInputHandlerType *psInHandler = NULL;
    psInHandler = (sInputHandlerType *) & wmvdec->wmvdec_handle_info;


    wmvdec->transistion = transition;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    psInHandler->pu8CompFmtString = NULL;
    wmvdec->seek_flag = FALSE;
    wmvdec->seeked_time = 0;

    psInHandler->timestamp = 0;

    break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        /*
           Reset all the state variables here. The same operations needs to be done
           even when the plug-in kills after executing.
         */
        wmvdec->flush = FALSE;
        wmvdec->error_count = 0;
        wmvdec->is_init_done = FALSE;
        wmvdec->fatal_err_found = FALSE;
        wmvdec->caps_set = FALSE;
        wmvdec->last_ts = 0;
        memset(&(wmvdec->wmvdec_handle_info), 0,
           sizeof(sInputHandlerType));
        wmvdec->state = 0;
        wmvdec->savecaps = NULL;

        wmvdec->adapter = gst_adapter_new();

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {

        wmvdec->element = *element;
        break;
    }
    default:
    break;
    }

    ret = parent_class->change_state(element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE) {
    return ret;
    }

    switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {

        wmvdec->state = 1;
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
        wmvdec->flush = FALSE;
        wmvdec->error_count = 0;
        wmvdec->is_init_done = FALSE;
        wmvdec->fatal_err_found = FALSE;
        wmvdec->caps_set = FALSE;
        wmvdec->last_ts = 0;
        wmvdec->state = 0;
        psInHandler->timestamp = 0;

        gst_caps_unref(wmvdec->savecaps);
        wmvdec->savecaps = NULL;
        gst_adapter_clear(wmvdec->adapter);
        g_object_unref(wmvdec->adapter);
        wmvdec->adapter = NULL;
        if (psInHandler->SeqData) {
            g_free(psInHandler->SeqData);
            psInHandler->SeqData = NULL;
        }
        if (psInHandler->pu8CompFmtString) {
        g_free(psInHandler->pu8CompFmtString);
        psInHandler->pu8CompFmtString = NULL;
        }
        if(&psInHandler->pWmv9Object->sMemInfo != NULL) {
            AppFreeMemory(&psInHandler->pWmv9Object->sMemInfo);
        }
        memset(&wmvdec->wmvdec_handle_info, 0,
           sizeof(sInputHandlerType));
    }
    break;
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
        GST_DEBUG("\nwmv: decode done\n");
        break;
    }
    default:
    break;
    }
    return ret;
}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_chain

DESCRIPTION:            This is the main control function for the plugin.
                        During the first call to this function, we initialize
                        the decoder library and subsequently we decode frames.
                        This funtion will be called once per frame.

ARGUMENTS PASSED:
                        pad     -> The sink pad on which chain is registered.
                        buffer  -> The buffer which contains raw video data.

RETURN VALUE:
                        GST_FLOW_OK on success
                        GST_FLOW_ERROR on error.

PRE-CONDITIONS:
                        None.

POST-CONDITIONS:        None.

IMPORTANT NOTES:        None
=============================================================================*/

static GstFlowReturn mfw_gst_wmvdec_chain(GstPad * pad, GstBuffer * buffer)
{

    GstFlowReturn retval = GST_FLOW_OK;
    int status = E_WMV9D_SUCCESS;
    sInputHandlerType *psInHandler = NULL;
    MFW_GST_WMVDEC_INFO_T *wmvdec = NULL;    
    GstFlowReturn res = GST_FLOW_OK;

    wmvdec = MFW_GST_WMVDEC(GST_PAD_PARENT(pad));
    
    if (wmvdec->flush) {
    return GST_FLOW_OK;
    }

    psInHandler = (sInputHandlerType *) & wmvdec->wmvdec_handle_info;
    
    
    if (wmvdec->is_init_done == FALSE) 
    {
         /* If no fatal error is found, assume 1st frame is always a key frame and index is not necessary.
         But if fatal error happens, wait for next key frame & re-init decoder, 
         and so index table must be present to indicate key frames. */

        if(wmvdec->fatal_err_found)
        {  
            if (buffer && GST_BUFFER_FLAG_IS_SET(buffer,GST_BUFFER_FLAG_DELTA_UNIT))
            //if (buffer && !GST_BUFFER_FLAG_IS_SET(buffer,GST_BUFFER_FLAG_IS_SYNC))
            {
                GST_DEBUG("Drop non-key frame\n");
                gst_buffer_unref(buffer);
                return GST_FLOW_OK;
            }

            GST_DEBUG("key frame found after the fatal error\n");
            GST_DEBUG("clear adapter ...\n");
            gst_adapter_clear(wmvdec->adapter);            
            
        } 

        status = mfw_gst_wmv_init_decoder(wmvdec);
        if(E_WMV9D_SUCCESS != status)
        {
            GST_ERROR("\nFailed to create & init decoder.\n");
            return GST_FLOW_ERROR;
        }
        GST_DEBUG("Init decoder done.\n");
    }

    gst_adapter_push(wmvdec->adapter, buffer);
    psInHandler->timestamp = GST_BUFFER_TIMESTAMP(buffer);
    
    status = mfw_gst_wmv_decode(wmvdec);
    switch (status) {
    case E_WMV9D_SUCCESS:
    return GST_FLOW_OK;
    break;

    case E_WMV9D_NOT_ENOUGH_BITS:
    case E_WMV9D_BAD_MEMORY:
    case E_WMV9D_WRONG_ALIGNMENT:
    case E_WMV9D_SIZE_CHANGED:
    case E_WMV9D_NO_OUTPUT:
    case E_WMV9D_BROKEN_FRAME:
    case E_WMV9D_DEMO_PROTECT:
    {
        GST_ERROR("wmv acceptible error %d\n",status);
        return GST_FLOW_OK;
        break;
    }


    default:
    {
        GST_ERROR("wmv fatal error %d (error counter %d)\n",status, wmvdec->error_count);
        if (wmvdec->error_count > 5) {
            /* not use this branch, error_count never increase. */
            GST_ERROR
                ("wmv chain Frame failed for decoding, status %d\n",
                 status);
            GError *error = NULL;
            GQuark domain;
            domain = g_quark_from_string("mfw_wmvdecoder");
            error = g_error_new(domain, 10, "fatal error");
            gst_element_post_message(GST_ELEMENT(wmvdec),
                         gst_message_new_error(GST_OBJECT
                                       (wmvdec),
                                       error,
                                       "non recoverable error while decoding"));

            GST_DEBUG("status=%d\n", status);
            return GST_FLOW_ERROR;
        } 
        else 
        {             
            GST_DEBUG("free decoder ...\n");
            eWMV9DFree(psInHandler->pWmv9Object);
            

            GST_DEBUG("app free memory ...\n");
            if(&psInHandler->pWmv9Object->sMemInfo)
                AppFreeMemory(&psInHandler->pWmv9Object->sMemInfo);
            
            wmvdec->is_init_done = FALSE;
            wmvdec->fatal_err_found = TRUE; /* will re-init decoder */
           
            return GST_FLOW_OK;
        }

        //return GST_FLOW_OK;
        break;
    }
    }
}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_set_property

DESCRIPTION:            Sets the Property for the element.

ARGUMENTS PASSED:
                        object  -> pointer on which property is set
                        prop_id -> ID.
                        value   -> Value to be set.
                        pspec   -> Parameters to be set
RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void mfw_gst_wmvdec_set_property(GObject * object, guint prop_id,
                    const GValue * value,
                    GParamSpec * pspec)
{

}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_get_property

DESCRIPTION:            Gets the Property of the element.

ARGUMENTS PASSED:
                        object  -> pointer on which property is to be obtained.
                        prop_id -> ID.
                        value   -> Value to be get.
                        pspec   -> Parameters to be get.

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void mfw_gst_wmvdec_get_property(GObject * object, guint prop_id,
                    GValue * value, GParamSpec * pspec)
{

}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_init

DESCRIPTION:            Create src and sink pads for the plug-in. Add these pads
                        onto the element and register the various functions.

ARGUMENTS PASSED:
                        wmvdec -> pointer to wmvdecoder element structure

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/

static void mfw_gst_wmvdec_init(MFW_GST_WMVDEC_INFO_T * wmvdec)
{
    GstElementClass *klass = GST_ELEMENT_GET_CLASS(wmvdec);

    /* create the sink and src pads */
    wmvdec->sinkpad =
    gst_pad_new_from_template(gst_element_class_get_pad_template
                  (klass, "sink"), "sink");

    wmvdec->srcpad = gst_pad_new_from_template(src_templ(), "src");

    gst_element_add_pad(GST_ELEMENT(wmvdec), wmvdec->sinkpad);
    gst_element_add_pad(GST_ELEMENT(wmvdec), wmvdec->srcpad);

    gst_pad_set_setcaps_function(wmvdec->sinkpad, mfw_gst_wmvdec_set_caps);

    gst_pad_set_event_function(wmvdec->sinkpad,
                   GST_DEBUG_FUNCPTR
                   (mfw_gst_wmvdec_sink_event));

    gst_pad_set_chain_function(wmvdec->sinkpad,
                   GST_DEBUG_FUNCPTR(mfw_gst_wmvdec_chain));

    /* Get the decoder version */
{

    INIT_DEMO_MODE(WMV9DecCodecVersionInfo(), wmvdec->demo_mode);
#define MVW_GST_WMV789_PLUGIN VERSION

    PRINT_CORE_VERSION(WMV9DecCodecVersionInfo());
    PRINT_PLUGIN_VERSION(MVW_GST_WMV789_PLUGIN);
}
    
}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_class_init

DESCRIPTION:            Initialise the class only once (specifying what signals,
                        arguments and virtual functions the class has and setting up
                        global stata)

ARGUMENTS PASSED:
        klass           pointer to wmvdecoder element class

RETURN VALUE:
                        None

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static void mfw_gst_wmvdec_class_init(MFW_GST_WMVDEC_INFO_CLASS_T * klass)
{


    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
    gstelement_class->change_state = mfw_gst_wmvdec_change_state;


}

/*=============================================================================
FUNCTION:               mfw_gst_wmvdec_base_init

DESCRIPTION:            Element details are registered with the plugin during
                        base_init ,This function will initialise the class and child
                        class properties during each new child class creation

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
static void mfw_gst_wmvdec_base_init(gpointer klass)
{


    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(element_class,
                       gst_static_pad_template_get
                       (&mfw_gst_wmvdec_sink_template_factory));

    gst_element_class_add_pad_template(element_class, src_templ());

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "wmv video decoder",
        "Codec/Decoder/Video", "Decode compressed wmv video to raw data");

}

/*=============================================================================

FUNCTION:               mfw_gst_wmvdec_get_type

DESCRIPTION:            intefaces are initiated in this function.you can register one
                        or more interfaces after having registered the type itself.

ARGUMENTS PASSED:
                        None

RETURN VALUE:
                        A numerical value ,which represents the unique identifier of this
                        elment(wmvdecoder)

PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
GType mfw_gst_wmvdec_get_type(void)
{
    static GType wmvdec_type = 0;

    if (!wmvdec_type) {
    static const GTypeInfo gstwmvdec_info = {
        sizeof(MFW_GST_WMVDEC_INFO_CLASS_T),
        mfw_gst_wmvdec_base_init,
        NULL,
        (GClassInitFunc) mfw_gst_wmvdec_class_init,
        NULL,
        NULL,
        sizeof(MFW_GST_WMVDEC_INFO_T),
        0,
        (GInstanceInitFunc) mfw_gst_wmvdec_init,
    };
    wmvdec_type = g_type_register_static(GST_TYPE_ELEMENT,
                         "MFW_GST_WMVDEC_INFO_T",
                         &gstwmvdec_info, 0);
    }

    GST_DEBUG_CATEGORY_INIT(mfw_gst_wmvdec_debug, "mfw_wmvdecoder", 0,
                "FreeScale's WMV Decoder's Log");

    return wmvdec_type;
}

/*=============================================================================
FUNCTION:               plugin_init

DESCRIPTION:            special function , which is called as soon as the plugin or
                        element is loaded and information returned by this function
                        will be cached in central registry

ARGUMENTS PASSED:
        plugin:         pointer to container that contains features loaded
                        from shared object module

RETURN VALUE:
                        return TRUE or FALSE depending on whether it loaded initialized
                        any dependency correctly
PRE-CONDITIONS:
                        None

POST-CONDITIONS:
                        None

IMPORTANT NOTES:
                        None
=============================================================================*/
static gboolean plugin_init(GstPlugin * plugin)
{
    return gst_element_register(plugin, "mfw_wmvdecoder", GST_RANK_PRIMARY,
                MFW_GST_TYPE_WMVDEC);
}

/*===========================================================================*/

FSL_GST_PLUGIN_DEFINE("wmvdec", "wmv video decoder", plugin_init);


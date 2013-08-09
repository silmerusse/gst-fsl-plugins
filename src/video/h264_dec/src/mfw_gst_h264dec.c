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
 * Module Name:    mfw_gst_h264dec.c
 *
 * Description:    Implementation of h264 decode plugin in push based
 *                 model.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 */


/*=============================================================================
					INCLUDE	FILES
=============================================================================*/


#include<gst/gst.h>
#include<memory.h>

#include "avcd_dec_api.h"

#include "mfw_gst_utils.h"

#include "mfw_gst_h264dec.h"


/*=============================================================================
					LOCAL CONSTANTS
=============================================================================*/
#define NAL_HEADER_SIZE 4
#define MAX_FRAME_SIZE 4096
#define MIN_FRAME_SIZE 16

#define CROP_LEFT_LENGTH  16
#define CROP_TOP_LENGTH 16

/*=============================================================================
			LOCAL TYPEDEFS (STRUCTURES,	UNIONS,	ENUMS)
=============================================================================*/
enum {
    ID_0,
    ID_PROFILE_ENABLE,
    ID_BADDATA_DISPLAY,
	ID_BMMODE,
	ID_SFD,
};
/*=============================================================================
					LOCAL MACROS
=============================================================================*/
/* used	for debugging */
#define	GST_CAT_DEFAULT	mfw_gst_h264dec_debug
#define PROCESSOR_CLOCK    532

/*=============================================================================
				LOCAL VARIABLES
=============================================================================*/
static GstStaticPadTemplate mfw_gst_h264dec_sink_template_factory =
GST_STATIC_PAD_TEMPLATE("sink",
			GST_PAD_SINK,
			GST_PAD_ALWAYS,
			GST_STATIC_CAPS("video/x-h264"));



/* table with framerates expressed as fractions */
static const gint fpss[][2] = { {24000, 1001},
{24, 1}, {25, 1}, {30000, 1001},
{30, 1}, {50, 1}, {60000, 1001},
{60, 1}, {0, 1}
};


/* used	in change state	function */
static GstElementClass *parent_class = NULL;

/*============================================================================
					LOCAL FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_h264dec_debug);
static GType mfw_gst_h264dec_get_type(void);
static void mfw_gst_h264dec_base_init(gpointer);
static void mfw_gst_h264dec_class_init(MFW_GST_H264DEC_INFO_CLASS_T *);
static void mfw_gst_h264dec_init(MFW_GST_H264DEC_INFO_T *);
static gboolean mfw_gst_h264dec_set_caps(GstPad *, GstCaps *);
static gboolean mfw_gst_h264dec_sink_event(GstPad *, GstEvent *);
static gboolean mfw_gst_h264dec_src_event(GstPad *, GstEvent *);
static GstFlowReturn mfw_gst_h264dec_chain(GstPad *, GstBuffer *);
static GstFlowReturn mfw_gst_h264dec_dframe(MFW_GST_H264DEC_INFO_T *,
					    GstBuffer *);
static GstStateChangeReturn
mfw_gst_h264dec_change_state(GstElement *, GstStateChange);
static void mfw_gst_h264dec_initappmemory(sAVCDMemAllocInfo *);
static gboolean mfw_gst_h264dec_allocdecmem(sAVCDMemAllocInfo *);
static gboolean mfw_gst_h264dec_allocoutmem(sAVCDMemAllocInfo *);
static gboolean mfw_gst_h264dec_allocframemem(sAVCDYCbCrStruct *,
					  sAVCDConfigInfo *);
static void mfw_gst_h264dec_freedecmem(sAVCDMemAllocInfo *);
static void mfw_gst_calculate_nal_units(MFW_GST_H264DEC_INFO_T *);

/* Call back function used for direct render v2 */
static void* mfw_gst_h264_getbuffer(void* pvAppContext);
static void mfw_gst_h264_rejectbuffer(void* pbuffer, void* pvAppContext);
static void mfw_gst_h264_releasebuffer(void* pbuffer, void* pvAppContext);

/*=============================================================================
					GLOBAL VARIABLES
=============================================================================*/
/* None	*/

/*=============================================================================
					LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================

FUNCTION:    mfw_gst_calculate_nal_units

DESCRIPTION:    -   This function calculate the total number of NAL units in a
                    frame chunk and number of bytes in a NAL unit.


ARGUMENTS PASSED:
         h264dec_struct -  pointer	to h264deocder element structure

RETURN VALUE:
         None

PRE-CONDITIONS:
         None

POST-CONDITIONS:
         None

IMPORTANT NOTES:
         None
=============================================================================*/
static void mfw_gst_calculate_nal_units(MFW_GST_H264DEC_INFO_T *
					h264dec_struct)
{
    gint32 i = 0;
    guint8 *temp = NULL;
    temp = GST_BUFFER_DATA(h264dec_struct->input_buffer);

    /* parses the NAL Header and find out how many NAL units are there in a
       chunk and numbers of bytes in each NAL units */
    while (i < GST_BUFFER_SIZE(h264dec_struct->input_buffer)) {
	if (temp[i] == 0x00 && temp[i + 1] == 0x00 &&
	    temp[i + 2] == 0x00 && temp[i + 3] == 0x01) {
	    GST_DEBUG("\nFound a NAL Unit in the given buffer\n");
	    h264dec_struct->number_of_nal_units += 1;
	    h264dec_struct->nal_size[h264dec_struct->number_of_nal_units] =
		0;
	    i += NAL_HEADER_SIZE;
	} else {
	    i++;
	    h264dec_struct->nal_size[h264dec_struct->
				     number_of_nal_units] += 1;
	}

    }
}

static void mfw_gst_AVC_Create_NALheader(GstBuffer **buffer)
{
    guint32 jump, startcode;
    guint16 len;
    unsigned char * buf = GST_BUFFER_DATA(*buffer);
    guint32 end = GST_BUFFER_DATA(*buffer)+GST_BUFFER_SIZE(*buffer)-4;
    unsigned char *bufout = NULL;
    GstBuffer *hdrBuf;
    GstBuffer *preBuf,*nxtBuf;
    guint32 offset = 0;

    /*
     * The qtdemux will use the 16bits length instead of startcode(00 00 00 01)
     * If the first two bytes of data is not zero, we assume it is the NAL length.
     */
    while(((guint32)buf)<end){

        jump = startcode = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
        GST_INFO("jump %x %x %x %x = %x\n", buf[0],  buf[1],  buf[2],  buf[3], jump);
        if (startcode==0x00000001) {
            GST_INFO("nal found\n");
            break;
        }
        len = (buf[0]<<8) | (buf[1]);

        GST_INFO("len = %04X\n", len);
        if (len != 0) {
            hdrBuf = gst_buffer_new_and_alloc(2);
            bufout = GST_BUFFER_DATA(hdrBuf);
            bufout[0] = bufout[1] = 0x00;
            GST_BUFFER_SIZE(hdrBuf) = 2;
            buf[0] = 0;
            buf[1] = 0x01;
            if (offset) {
                preBuf = gst_buffer_create_sub(*buffer,0,offset);
                nxtBuf = gst_buffer_create_sub(*buffer,offset,GST_BUFFER_SIZE(*buffer)-offset);
                preBuf = gst_buffer_join(preBuf, hdrBuf);
            }
            else {
                preBuf = hdrBuf;
                nxtBuf = *buffer;
            }
            *buffer = gst_buffer_join(preBuf,nxtBuf);

            offset += len+4;

            buf = GST_BUFFER_DATA(*buffer);
            end = GST_BUFFER_DATA(*buffer)+GST_BUFFER_SIZE(*buffer)-4;

            buf+=offset;

        }
        else {
            buf[2] = buf[1] = buf[0] = 0;
            buf[3] = 0x01;
            buf+=jump+4;
        }


    }

}


/*======================================================================================
FUNCTION:           mfw_gst_h264_AVC_Fix_NALheader

DESCRIPTION:        Modify the buffer so that the length 4 bytes is changed to start code
                    open source demuxers like qt and ffmpeg_mp4 generate length+payload
                    instead of startcode plus payload which VPU can't support

ARGUMENTS PASSED:   h264dec_struct  - H264 decoder plugins context
                    buffer - pointer to the input buffer which has the video data.

RETURN VALUE:       GstFlowReturn - Success of Failure.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
static GstFlowReturn mfw_gst_h264_AVC_Fix_NALheader(MFW_GST_H264DEC_INFO_T *
					h264dec_struct, GstBuffer **buffer)
{
    guint32 jump;
    unsigned char * buf;
    guint32 end;
    GstBuffer *hdrBuf=NULL;
    if (h264dec_struct->codec_data_len)
    {
        // must be qtdemux input which must be parsed more carefully
        // if any codec data from qtdemux then add startcode and parse our paylose
        // usually there are 3 sections.  First section has 8 bytes with last 2 bytes
        // being length so put a startcode plus copy the length from codec data
        // next section is 3 bytes with 2nd two bytes being length (usually 4 bytes)
        // so put another start code and copy 4 bytes from codec data
        // last step is make sure input buffer is fixed with start codes instead of
        // length
        unsigned char *buf_hdr = GST_BUFFER_DATA(h264dec_struct->codec_data);
        guint32 startcode =  (buf_hdr[0]<<24)|(buf_hdr[1]<<16)|(buf_hdr[2]<<8)|(buf_hdr[3]);
        unsigned char *bufout = NULL;

        if (startcode == 0x00000001) { /* FSL parser already encapsulate the codec data */
            GST_WARNING("FSL parser, no necessary to convert data\n");
            hdrBuf = gst_buffer_copy(h264dec_struct->codec_data);
        }
        else {
            hdrBuf = gst_buffer_new_and_alloc(h264dec_struct->codec_data_len);
            bufout = GST_BUFFER_DATA(hdrBuf);

            {
                gint length = (buf_hdr[6] << 8) | buf_hdr[7];
                gint length2 = 0;
                gint total_length = length+4;
                g_print (" AVC SPS header length=%d\n", length);
                bufout[2] = bufout[1] = bufout[0] = 0;
                bufout[3] = 0x01;
                length2 = h264dec_struct->codec_data_len - length - 8;
                if(length2<=0)
                    return GST_FLOW_OK;

                memcpy(bufout+4,buf_hdr + 8,length);

                if (length2 > 0)
                {
                    gint offset = length+4;  // start code plus length copied
                    gint offset_src = length+8+3;
                    length2 = (buf_hdr[length+8+1]<<8) | buf_hdr[length+8+2];
                    g_print (" AVC PPS Header length=%d\n", length2);
                    bufout[offset] = bufout[offset+1] = bufout[offset+2] = 0;
                    bufout[offset+3] = 0x01;
                    offset += 4;
                    memcpy(bufout + offset, buf_hdr + offset_src, length2);
                    total_length += length2+4;
                }
                GST_BUFFER_SIZE(hdrBuf) = total_length;
            }

        }
        mfw_gst_AVC_Create_NALheader(buffer);

        *buffer = gst_buffer_join(hdrBuf,*buffer);

    }


    return GST_FLOW_OK;
}


/*=============================================================================

FUNCTION:   mfw_gst_h264dec_initappmemory

DESCRIPTION:    -   Initialize the memory.


ARGUMENTS PASSED:
         psMemPtr    -  pointer to the structure which holds the information
                        required for the memory management of the decoder.
RETURN VALUE:
         None

PRE-CONDITIONS:
         None

POST-CONDITIONS:
         None

IMPORTANT NOTES:
         None
=============================================================================*/
static void mfw_gst_h264dec_initappmemory(sAVCDMemAllocInfo * psMemPtr)
{
    gint32 s32Count = 0, maxNumReqs = 0;
    maxNumReqs = psMemPtr->s32NumReqs;
    for (s32Count = 0; s32Count < maxNumReqs; s32Count++) {
	psMemPtr->asMemBlks[s32Count].pvBuffer = NULL;
    }
}


/*==========================================================================
FUNCTION:   mfw_gst_h264dec_allocdecmem

DESCRIPTION:    -   Allocates the memory.

ARGUMENTS PASSED:
        psMemPtr    -   pointer to the structure which holds the information
                        required for the memory management of the decoder.
RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean mfw_gst_h264dec_allocdecmem(sAVCDMemAllocInfo * psMemPtr)
{
    gint32 s32Count, maxNumReqs;
    GST_DEBUG("\n inside mfw_gst_h264dec_allocdecmem  function \n");
    maxNumReqs = psMemPtr->s32NumReqs;
    for (s32Count = 0; s32Count < maxNumReqs; s32Count++) {
	if (psMemPtr->asMemBlks[s32Count].s32Allocate == 1) {
	    psMemPtr->asMemBlks[s32Count].pvBuffer =
		g_malloc(psMemPtr->asMemBlks[s32Count].s32Size);
            memset(psMemPtr->asMemBlks[s32Count].pvBuffer,0,psMemPtr->asMemBlks[s32Count].s32Size);
	    GST_DEBUG("Allocation for %d element : %d\n", s32Count,
		      psMemPtr->asMemBlks[s32Count].s32Size);
		if (psMemPtr->asMemBlks[s32Count].pvBuffer == NULL)
		{
		  GST_ERROR("\nFailed in malloc decmem.\n");
		  return FALSE;
		 }
	}
    }

    GST_DEBUG("\n out of mfw_gst_h264dec_allocdecmem  function \n");
    return TRUE;
}

/*==========================================================================
FUNCTION:   mfw_gst_h264dec_allocoutmem

DESCRIPTION:    -    Allocates the memory.

ARGUMENTS PASSED:
        psMemPtr    -   pointer to the structure which holds the information
                        required for the memory management of the decoder.

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean mfw_gst_h264dec_allocoutmem(sAVCDMemAllocInfo * psMemPtr)
{
    gint32 s32Count = 0, maxNumReqs = 0;

    GST_DEBUG("\n inside mfw_gst_h264dec_allocoutmem  function \n");

    maxNumReqs = psMemPtr->s32NumReqs;
    for (s32Count = 0; s32Count < maxNumReqs; s32Count++) {
	if (psMemPtr->asMemBlks[s32Count].s32SizeDependant == 1 &&
	    psMemPtr->asMemBlks[s32Count].s32Allocate == 1) {
	    if ((psMemPtr->asMemBlks[s32Count].pvBuffer != NULL) &&
		(psMemPtr->asMemBlks[s32Count].s32Copy == 1)) {
		psMemPtr->asMemBlks[s32Count].pvBuffer =
		    g_realloc(psMemPtr->asMemBlks[s32Count].pvBuffer,
			      psMemPtr->asMemBlks[s32Count].s32Size);
		if (psMemPtr->asMemBlks[s32Count].pvBuffer == NULL)
		{
		  GST_ERROR("\nFailed in malloc out mem.\n");
		  return FALSE;
		 }
	         memset(psMemPtr->asMemBlks[s32Count].pvBuffer,0,psMemPtr->asMemBlks[s32Count].s32Size);
                GST_DEBUG("ReAllocation for %d element : %d\n", s32Count,
			  psMemPtr->asMemBlks[s32Count].s32Size);
	    } else {
           	if(psMemPtr->asMemBlks[s32Count].pvBuffer !=NULL)
		{
             	    g_free(psMemPtr->asMemBlks[s32Count].pvBuffer);
		}
		psMemPtr->asMemBlks[s32Count].pvBuffer =
		    g_malloc(psMemPtr->asMemBlks[s32Count].s32Size);
		if (psMemPtr->asMemBlks[s32Count].pvBuffer == NULL)
		{
		  GST_ERROR("\nFailed in malloc out mem.\n");
		  return FALSE;
		 }
                memset(psMemPtr->asMemBlks[s32Count].pvBuffer,0,psMemPtr->asMemBlks[s32Count].s32Size);
		GST_DEBUG("Allocation for %d element : %d\n", s32Count,
			  psMemPtr->asMemBlks[s32Count].s32Size);
	    }
	}
    }
    GST_DEBUG("\n out of  mfw_gst_h264dec_allocoutmem  function \n");
    return TRUE;
}


/*==========================================================================
FUNCTION:   mfw_gst_h264dec_allocframemem

DESCRIPTION:    -   Allocates memory for output frames.

ARGUMENTS PASSED:
        psFrame -   pointer to  structure encapsulates the decoded YCbCr
                    buffer.
        pConfig -   pointer to structure which holds configuration
                    parameters.

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_h264dec_allocframemem(sAVCDYCbCrStruct * psFrame,
			      sAVCDConfigInfo * pConfig)
{

    GST_DEBUG("\n Inside mfw_gst_h264dec_allocframemem function \n");

    int s32Xsize = 0, s32Ysize = 0, s32Cysize = 0, s32Cxsize = 0;

    switch (psFrame->eOutputFormat) {

	/* in this format each component is stored as a separate array ,
	   the final image being a fusing of the three separate planes */
    case E_AVCD_420_PLANAR:
	psFrame->s16Xsize = (pConfig->s16FrameWidth);
	s32Xsize = psFrame->s16Xsize;
	s32Ysize = (pConfig->s16FrameHeight);
	psFrame->s16CxSize = psFrame->s16Xsize >> 1;
	s32Cysize = s32Ysize >> 1;
	s32Cxsize = psFrame->s16CxSize;
	break;

	/* in this format each component is stored as a separate array and padded ,
	   the final image being a fusing of the three separate planes */
    case E_AVCD_420_PLANAR_PADDED:
	psFrame->s16Xsize = (pConfig->s16FrameWidth);
	s32Xsize = psFrame->s16Xsize;
	s32Ysize = (pConfig->s16FrameHeight);
	psFrame->s16CxSize = psFrame->s16Xsize >> 1;
	s32Cysize = s32Ysize >> 1;
	s32Cxsize = psFrame->s16CxSize;
	psFrame->pu8y = NULL;
	psFrame->pu8cb = NULL;
	psFrame->pu8cr = NULL;
	break;

	/* in this format Y sample at every pixel, U and V sampled at
	   every second pixel horizontally on each line */
    case E_AVCD_422_UYVY:
	psFrame->s16Xsize = (pConfig->s16FrameWidth) * 2;
	s32Xsize = psFrame->s16Xsize;
	s32Ysize = (pConfig->s16FrameHeight);
	psFrame->pu8y =
	    (unsigned char *) g_malloc(s32Xsize * s32Ysize * sizeof(char));
	if (psFrame->pu8y == NULL) {
	    GST_ERROR
		("\nFailed to allocate memory for the output pointer Y\n");
	    return FALSE;
	}
	psFrame->pu8cb = NULL;
	psFrame->pu8cr = NULL;
	break;
    }
    GST_DEBUG("\n out of mfw_gst_h264dec_allocframemem function \n");
    return TRUE;
}


/*==========================================================================
FUNCTION:   -   mfw_gst_h264dec_freedecmem

DESCRIPTION:    -     Free all the memory occupied by decoder

ARGUMENTS PASSED:
        psMemPtr    -   pointer to the structure which holds the
                                information required for the memory management
                                of the decoder.

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static void mfw_gst_h264dec_freedecmem(sAVCDMemAllocInfo * psMemPtr)
{
    gint32 s32Count = 0, maxNumReqs = 0;

    maxNumReqs = psMemPtr->s32NumReqs;

    GST_DEBUG("\n inside  mfw_gst_h264dec_freedecmem function \n");

    for (s32Count = 0; s32Count < maxNumReqs; s32Count++) {
	if (psMemPtr->asMemBlks[s32Count].pvBuffer != NULL) {
	    g_free(psMemPtr->asMemBlks[s32Count].pvBuffer);
	    GST_DEBUG("DeAllocation for %d element \n", s32Count);
	    psMemPtr->asMemBlks[s32Count].pvBuffer = NULL;
	}
    }
    GST_DEBUG("\n out of mfw_gst_h264dec_freedecmem function \n");
}

/*=============================================================================
FUNCTION:               mfw_gst_h264_getbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when
                        decoder need a new frame buffer.

ARGUMENTS PASSED:       pvAppContext -> Pointer to the context variable.

RETURN VALUE:           Pointer to a frame buffer.  -> On success.
                        Null.                       -> On fail.

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void* mfw_gst_h264_getbuffer(void* pvAppContext)
{
    MFW_GST_H264DEC_INFO_T *h264dec_struct = (MFW_GST_H264DEC_INFO_T *)pvAppContext;
	void * pbuffer;
	GstCaps *caps = NULL;
	guint outsize ;

    if (h264dec_struct->caps_set == FALSE) {
        guint fourcc = GST_STR_FOURCC("I420");
        guint crop_right_len = 0, crop_bottom_len = 0;

        GST_DEBUG(" Caps being set for decoder source pad\n");
        h264dec_struct->caps_set = TRUE;
        if (h264dec_struct->dec_config.sFrameData.eOutputFormat ==
                E_AVCD_420_PLANAR_PADDED) {
            crop_right_len =
            h264dec_struct->frame_width_padded -
            h264dec_struct->frame_width - CROP_LEFT_LENGTH;
            crop_bottom_len =
            h264dec_struct->frame_height_padded -
            h264dec_struct->frame_height - CROP_TOP_LENGTH;

            GST_DEBUG("right crop=%d,bottom crop=%d\n", crop_right_len,
            crop_bottom_len);
            caps = gst_caps_new_simple("video/x-raw-yuv", "format",
                    GST_TYPE_FOURCC, fourcc, "width",
                    G_TYPE_INT,
                    h264dec_struct->frame_width_padded,
                    "height", G_TYPE_INT,
                    h264dec_struct->frame_height_padded,
                    "pixel-aspect-ratio",
                    GST_TYPE_FRACTION, 1, 1, "framerate",
                    GST_TYPE_FRACTION,
                    h264dec_struct->framerate_n,
                    h264dec_struct->framerate_d,
                    "crop-left-by-pixel", G_TYPE_INT, CROP_LEFT_LENGTH,
                    "crop-top-by-pixel", G_TYPE_INT, CROP_TOP_LENGTH,
                    "crop-right-by-pixel", G_TYPE_INT,
                    (crop_right_len + 7) / 8 * 8,
                    "crop-bottom-by-pixel", G_TYPE_INT,
                    (crop_bottom_len + 7) / 8 * 8,
                    "num-buffers-required", G_TYPE_INT,
                    BM_GET_BUFFERNUM
                    ,NULL);
            GST_ERROR("need allocate %d\n", BM_GET_BUFFERNUM);

        }else{
            caps = gst_caps_new_simple("video/x-raw-yuv",
                    "format", GST_TYPE_FOURCC, fourcc,
                    "width", G_TYPE_INT,
                    h264dec_struct->frame_width,
                    "height", G_TYPE_INT,
                    h264dec_struct->frame_height,
                    "pixel-aspect-ratio",
                    GST_TYPE_FRACTION,
                    1,1,
                    "framerate", GST_TYPE_FRACTION,
                    h264dec_struct->framerate_n,
                    h264dec_struct->framerate_d,
                    NULL);
        }

        if (!(gst_pad_set_caps(h264dec_struct->srcpad, caps))) {
            GST_ERROR(" Could not set the caps for the decoder src pad\n");
            return NULL;
        }
        gst_caps_unref(caps);
        caps = NULL;
    }

    outsize = (h264dec_struct->frame_width_padded *
		               h264dec_struct->frame_height_padded)*3/2;

	BM_GET_BUFFER(h264dec_struct->srcpad, outsize, pbuffer);
	return pbuffer;
}

/*=============================================================================
FUNCTION:               mfw_gst_h264_rejectbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when
                        decoder want to indicate a frame buffer would not be
                        used as a output.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for reject
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void mfw_gst_h264_rejectbuffer(void* pbuffer, void* pvAppContext)
{
    BM_REJECT_BUFFER(pbuffer);
}

/*=============================================================================
FUNCTION:               mfw_gst_h264_releasebuffer

DESCRIPTION:            Callback function for decoder. The call is issued when
                        decoder want to indicate a frame buffer would never used
                        as a reference.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for release
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void mfw_gst_h264_releasebuffer(void* pbuffer, void* pvAppContext)
{
    BM_RELEASE_BUFFER(pbuffer);
}

static int mfw_gst_h264_querybufferphyaddress(void* pbuffer, void* pvAppContext)
{
    int hwbuffer = 0;
    BM_QUERY_HWADDR(pbuffer, hwbuffer);
    return hwbuffer;
}

/*=============================================================================
FUNCTION:	mfw_gst_h264dec_dframe

DESCRIPTION:	decode the  data by	calling	library function ,this library
                function is receiving the data from the call back	function .
                After decoding the  decoded data is pushed to the sink element.

ARGUMENTS PASSED:
		h264dec_struct  -	pointer	to h264deocder element structure

RETURN VALUE:
        GST_FLOW_OK     -   success.
        other           -   failure.

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static GstFlowReturn
mfw_gst_h264dec_dframe(MFW_GST_H264DEC_INFO_T * h264dec_struct,
		       GstBuffer * inbuffer)
{
    GstFlowReturn result = GST_FLOW_OK;
    GstCaps *src_caps = NULL;
    gint32 output_size = 0;
    GstClockTime ts;
    GstCaps *caps = NULL;
    guint32 i_loop = 0;
    GstBuffer *outbuffer = NULL;
    struct timeval tv_prof, tv_prof1;
    long time_before = 0, time_after = 0;
    guint8 *temp = NULL;
    unsigned int addr = 0;
    gboolean first_display = TRUE;

{
    if (h264dec_struct->demo_mode == 2)
        return GST_FLOW_ERROR;

    DEMO_LIVE_CHECK(h264dec_struct->demo_mode,
        getTSManagerPosition(h264dec_struct->pTS_Mgr),
        h264dec_struct->srcpad);

}
    /* temp is pointing to the data part of the buffer which we are receiving in chain */
    temp = GST_BUFFER_DATA(h264dec_struct->input_buffer);

    h264dec_struct->status = E_AVCD_NOERROR;

    /*calculate the number of NAL units in a chunk */
    mfw_gst_calculate_nal_units(h264dec_struct);

    GST_DEBUG("Total number of NAL units in buffer = %d\n",
	      h264dec_struct->number_of_nal_units);

    while (i_loop < h264dec_struct->number_of_nal_units) {
	/* setting capabilities for sink element */

    h264dec_struct->dec_config.s32InBufferLength =
	    GST_BUFFER_SIZE(h264dec_struct->input_buffer);

	/* Number of bytes in a NAL unit */
	h264dec_struct->dec_config.s32NumBytes =
	    h264dec_struct->nal_size[i_loop + 1];

	h264dec_struct->number_of_bytes =
	    h264dec_struct->dec_config.s32NumBytes;

	GST_DEBUG(" Total  bytes in NAL unit  =%d, for loop %d\n",
		  h264dec_struct->dec_config.s32NumBytes, i_loop + 1);

	/* skip the NAL header from the input data  */
	temp = temp + NAL_HEADER_SIZE;

	/* callback is not required to provide input data in decode library call  ,
	   just pass a complete NAL unit of data at the time of decode library call */
	h264dec_struct->dec_config.pvInBuffer = temp;

    if (h264dec_struct->profile) {
	    gettimeofday(&tv_prof, 0);
	}

	/* decode library call */
	h264dec_struct->status =
	    eAVCDecodeNALUnit(&h264dec_struct->dec_config,
			      h264dec_struct->ff_flag);

	GST_DEBUG(" eStatus decoder  = %d\n", h264dec_struct->status);
	/* next time pass the input data pointer that points next NAL unit,
	   so increament it by the number of bytes already passed to decode library call */
	temp = temp + h264dec_struct->dec_config.s32NumBytes;

	if (h264dec_struct->profile) {
	    gettimeofday(&tv_prof1, 0);
	    time_before = (tv_prof.tv_sec * 1000000) + tv_prof.tv_usec;
	    time_after = (tv_prof1.tv_sec * 1000000) + tv_prof1.tv_usec;
	    h264dec_struct->Time += time_after - time_before;

	    if ((h264dec_struct->status == E_AVCD_NOERROR) ||
		(h264dec_struct->status == E_AVCD_FLUSH_STATE)) {

		h264dec_struct->no_of_frames++;

	    } else if ((h264dec_struct->status ==
			E_AVCD_OUTPUT_FORMAT_NOT_SUPPORTED)
		       || (h264dec_struct->status == E_AVCD_BAD_DATA)
		       ||
		       (h264dec_struct->status ==
			E_AVCD_INVALID_PARAMETER_SET)) {

		h264dec_struct->no_of_frames_dropped++;

	    }

	}
	if (h264dec_struct->status == E_AVCD_CODEC_TYPE_NOT_SUPPORTED) {

	GST_ERROR
		("\nCould not decode the stream: Not supported, error number is %d\n",
		 h264dec_struct->status);
        GError *error = NULL;
        GQuark domain;
        domain = g_quark_from_string("mfw_h264decoder");
        error = g_error_new(domain, 10, "fatal error");
        gst_element_post_message(GST_ELEMENT(h264dec_struct),
            gst_message_new_error(GST_OBJECT
            (h264dec_struct),
            error,
            "non recoverable error while decoding"
            " the H.264 Data"));
        return GST_FLOW_ERROR;
	}


	if (h264dec_struct->status == E_AVCD_NULL_POINTER) {

   	    GST_ERROR
		("Decoder returned %d, check if memory is allocated\n",
		 h264dec_struct->status);
	    return GST_FLOW_ERROR;
	}

	if (h264dec_struct->status == E_AVCD_SEQ_CHANGE) {
	    GST_DEBUG("ReQuery Memory for Allocation \n\n");

	    h264dec_struct->dec_config.s32NumBytes = h264dec_struct->
		nal_size[i_loop + 1];

	    h264dec_struct->status =
		eAVCDReQueryMem(&h264dec_struct->dec_config);



        /* Init buffer manager for correct working mode.*/
        BM_INIT((h264dec_struct->bmmode) ? BMINDIRECT : BMDIRECT, h264dec_struct->dec_config.sMemInfo.s32MinFrameBufferNum, RENDER_BUFFER_MAX_NUM);

        if (h264dec_struct->caps_set == FALSE) {
        guint fourcc = GST_STR_FOURCC("I420");
        guint crop_right_len = 0, crop_bottom_len = 0;

	    GST_DEBUG(" Caps being set for decoder source pad\n");
	    h264dec_struct->caps_set = TRUE;
	    if (h264dec_struct->dec_config.sFrameData.eOutputFormat ==
		    E_AVCD_420_PLANAR_PADDED) {
    		crop_right_len =
    		    h264dec_struct->frame_width_padded -
    		    h264dec_struct->frame_width - CROP_LEFT_LENGTH;
    		crop_bottom_len =
    		    h264dec_struct->frame_height_padded -
    		    h264dec_struct->frame_height - CROP_TOP_LENGTH;

    		GST_DEBUG("right crop=%d,bottom crop=%d\n", crop_right_len,
    			  crop_bottom_len);
            caps = gst_caps_new_simple("video/x-raw-yuv", "format",
                    GST_TYPE_FOURCC, fourcc, "width",
                    G_TYPE_INT,
                    h264dec_struct->frame_width_padded,
                    "height", G_TYPE_INT,
                    h264dec_struct->frame_height_padded,
                    "pixel-aspect-ratio",
                    GST_TYPE_FRACTION, 1, 1,
                    "framerate",
                    GST_TYPE_FRACTION,
                    h264dec_struct->framerate_n,
                    h264dec_struct->framerate_d,
                    CAPS_FIELD_CROP_LEFT, G_TYPE_INT, CROP_LEFT_LENGTH,
                    CAPS_FIELD_CROP_TOP, G_TYPE_INT, CROP_TOP_LENGTH,
                    CAPS_FIELD_CROP_RIGHT, G_TYPE_INT, crop_right_len,
                    CAPS_FIELD_CROP_BOTTOM, G_TYPE_INT, crop_bottom_len,
                    CAPS_FIELD_REQUIRED_BUFFER_NUMBER, G_TYPE_INT, BM_GET_BUFFERNUM
                    ,NULL);
            GST_WARNING("need allocate %d\n", BM_GET_BUFFERNUM);
        }else{
	        caps = gst_caps_new_simple("video/x-raw-yuv",
                "format", GST_TYPE_FOURCC, fourcc,
                "width", G_TYPE_INT,
                h264dec_struct->frame_width,
                "height", G_TYPE_INT,
                h264dec_struct->frame_height,
                "pixel-aspect-ratio",
                GST_TYPE_FRACTION,
                1,1,
                "framerate", GST_TYPE_FRACTION,
                h264dec_struct->framerate_n,
                h264dec_struct->framerate_d,
                NULL);
        }

        if (h264dec_struct->is_sfd) {
            GST_ADD_SFD_FIELD(caps);
        }

	    if (!(gst_pad_set_caps(h264dec_struct->srcpad, caps))) {
		GST_ERROR
		    (" Could not set the caps for the decoder src pad\n");
    		return GST_FLOW_ERROR;
	    }
	    gst_caps_unref(caps);
	    caps = NULL;
	}






        {
            /*
                FIX me, following code should try to allocate a gstbuffer from downstream element, try to
                find out all buffers type(hw or sw), make a desition to use hw or sw deblock.
                In future, the codec should dynamic use hw or sw deblock based on each buffer type.
            */
            GstFlowReturn result;
            GstBuffer * buffer;

            result = gst_pad_alloc_buffer_and_set_caps(h264dec_struct->srcpad, 0,1, GST_PAD_CAPS(h264dec_struct->srcpad),&buffer);
            if (result==GST_FLOW_OK){
                GstStructure * s = gst_caps_get_structure(GST_PAD_CAPS(h264dec_struct->srcpad), 0);
                gint buffernum = 0;
                gst_structure_get_int(s, "num-buffers-required",
			      &buffernum);
                if (buffernum<BM_GET_BUFFERNUM){
                    g_print("Can't allocate enough hwbuffer, use sw deblock.\n");
                    AVCDSetDeblockOption(&h264dec_struct->dec_config, E_AVCD_SW_DEBLOCK);
                }else{
//#ifndef _MX233
#if ((!defined (_MX233)) && (!defined (_MX28)) && (!defined (_MX50)))
                    GST_DEBUG("Use hw deblock.\n");
                    AVCDSetDeblockOption(&h264dec_struct->dec_config, E_AVCD_HW_DEBLOCK);
#else
                    GST_DEBUG("Use sw deblock.\n");
                    AVCDSetDeblockOption(&h264dec_struct->dec_config, E_AVCD_SW_DEBLOCK);
#endif
				}
                gst_buffer_unref(buffer);
            }
            AVCD_FrameManager frameMgr;
            frameMgr.BfGetter = mfw_gst_h264_getbuffer;
            frameMgr.BfRejector = mfw_gst_h264_rejectbuffer;

            AVCDSetBufferManager(&h264dec_struct->dec_config, &frameMgr);
            H264SetAdditionalCallbackFunction(&h264dec_struct->dec_config, E_RELEASE_FRAME, mfw_gst_h264_releasebuffer);
            H264SetAdditionalCallbackFunction(&h264dec_struct->dec_config, E_QUERY_PHY_ADDR, mfw_gst_h264_querybufferphyaddress);
        }
	    GST_DEBUG(" Re-Query mem status in seq change = %d \n",
		      h264dec_struct->status);

	   if (! mfw_gst_h264dec_allocoutmem(&h264dec_struct->dec_config.sMemInfo))
	       return GST_FLOW_ERROR;

	    GST_DEBUG("Frame Width %d Frame Height %d \n",
		      h264dec_struct->dec_config.sConfig.s16FrameWidth,
		      h264dec_struct->dec_config.sConfig.s16FrameHeight);

	    if ((h264dec_struct->dec_config.sConfig.s16FrameWidth != 0) ||
		(h264dec_struct->dec_config.sConfig.s16FrameHeight != 0)) {

           if (!mfw_gst_h264dec_allocframemem(&h264dec_struct->dec_config.
					      sFrameData,
					      &h264dec_struct->dec_config.
					      sConfig))
			 return GST_FLOW_ERROR;
	    }
	    /* In this case  , next time we need to provide same data which we provided last time ,
	       so change the pointer to old location */
	    temp =
		temp - h264dec_struct->dec_config.s32NumBytes -
		NAL_HEADER_SIZE;

	    i_loop--;

	}
      if ((h264dec_struct->status != E_AVCD_NOERROR) &&
          (h264dec_struct->status != E_AVCD_CHANGE_SERVICED) &&
          (h264dec_struct->status != E_AVCD_NO_OUTPUT) &&
          (h264dec_struct->status != E_AVCD_DEMO_PROTECT) &&
          (h264dec_struct->status != E_AVCD_FLUSH_STATE) &&
          (h264dec_struct->status != E_NO_PICTURE_PAR_SET_NAL) &&
          (h264dec_struct->status != E_NO_SEQUENCE_PAR_SET_NAL) &&
          (h264dec_struct->status != E_AVCD_BAD_DATA))
      {
               GST_ERROR ("Decoder returned %d, decode failed\n",h264dec_struct->status);
           return GST_FLOW_ERROR;
      }

      /* engr43949 av not sync after seek */
      if ((h264dec_struct->status == E_NO_PICTURE_PAR_SET_NAL) ||
         (!h264dec_struct->display_baddata && (h264dec_struct->status == E_AVCD_BAD_DATA)))
      {
	    /* increament the pushed timestamp index by one although not display this frame */

           TSManagerSend(h264dec_struct->pTS_Mgr);
      }
      else
       if ((h264dec_struct->status == E_AVCD_NOERROR)
           || (h264dec_struct->status == E_AVCD_DEMO_PROTECT)
	         ||  (h264dec_struct->status == E_AVCD_FLUSH_STATE) ||
        (h264dec_struct->display_baddata && (h264dec_struct->status == E_AVCD_BAD_DATA) )) {

            /*One frame isn't matched with one NAL in special stream, then timestamp isn't correct */
            if (first_display)
            {
                first_display = FALSE;
                h264dec_struct->dec_config.sFrameData.s32FrameNumber *= 2;

                if (h264dec_struct->dec_config.sFrameData.eOutputFormat == E_AVCD_420_PLANAR_PADDED){
                    addr = (unsigned int)h264dec_struct->dec_config.sFrameData.pu8y;
                    /*we need to move decoded data address to padded start address */
                    addr = addr - h264dec_struct->frame_width_padded * CROP_TOP_LENGTH - CROP_LEFT_LENGTH;
                } else {
                    addr = h264dec_struct->dec_config.sFrameData.pu8y ;
                    eAVCDGetFrame(&h264dec_struct->dec_config);
                }

                /* increament the puhsed timestamp index by one */
				ts = TSManagerSend(h264dec_struct->pTS_Mgr);


                BM_RENDER_BUFFER(addr, h264dec_struct->srcpad, result, ts, 0);
            }
       }

       i_loop++;

	if (h264dec_struct->status == E_AVCD_FLUSH_STATE) {
	    /* In this case  , next time we need to provide same data which we provided last time ,
	       so change the pointer to old location */
	    temp =
		temp - h264dec_struct->dec_config.s32NumBytes -
		NAL_HEADER_SIZE;
	    i_loop--;
    }
    }
    return result;
}

/*=============================================================================
FUNCTION:	mfw_gst_h264dec_chain

DESCRIPTION:	this function called to	get	data from sink pad ,it also
				initialize the h264decoder first	time and starts	the	decoding
				process.

ARGUMENTS PASSED:
		pad		-	pointer	to the pad
		buffer	-	pointer	to GstBuffer

RETURN VALUE:
		GST_FLOW_OK -	success.
        other       -   failure.

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/

static GstFlowReturn mfw_gst_h264dec_chain(GstPad * pad,
					   GstBuffer * buffer)
{


    eAVCDRetType eStatus = E_AVCD_NOERROR;
    GstFlowReturn result = GST_FLOW_OK;
    struct timeval tv_prof2, tv_prof3;
    long time_before = 0;
    long time_after = 0;

    MFW_GST_H264DEC_INFO_T *h264dec_struct =
	MFW_GST_H264DEC(GST_PAD_PARENT(pad));

    if (h264dec_struct->profile) {
	gettimeofday(&tv_prof2, 0);
    }

    /* Receive the buffer's timestamps and store them in a timestamp array */
    //h264dec_struct->TimeStamp_Object.timestamp_buffer[h264dec_struct->TimeStamp_Object.ts_tx] =
	//GST_BUFFER_TIMESTAMP(buffer);


        GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);
        TSManagerReceive(h264dec_struct->pTS_Mgr, timestamp);

    if (!h264dec_struct->is_decode_init) {
    	h264dec_struct->dec_config.s32FrameNumber = 0;
    	h264dec_struct->dec_config.sFrameData.pu8y = NULL;
    	h264dec_struct->dec_config.sFrameData.pu8cb = NULL;
    	h264dec_struct->dec_config.sFrameData.pu8cr = NULL;
    	h264dec_struct->dec_config.sConfig.s16FrameWidth = 0;
    	h264dec_struct->dec_config.sConfig.s16FrameHeight = 0;
    	h264dec_struct->dec_config.pAppContext = (void *) h264dec_struct;


    if ( mfw_gst_h264_AVC_Fix_NALheader(h264dec_struct, &buffer) != GST_FLOW_OK)
        return GST_FLOW_OK;

    	// Allocate Memory for input bit buffer
    	h264dec_struct->dec_config.s32InBufferLength =
    	    GST_BUFFER_SIZE(buffer);

    	GST_DEBUG("size of buffer =%d \n",
    		  h264dec_struct->dec_config.s32InBufferLength);

    	h264dec_struct->input_buffer = buffer;

    	h264dec_struct->dec_config.pvInBuffer = GST_BUFFER_DATA(buffer);

    	eStatus = eAVCDInitQueryMem(&h264dec_struct->dec_config.sMemInfo);

    	GST_DEBUG(" init memory status =%d \n", eStatus);

    	if (E_AVCD_QUERY != eStatus) {
    	    GST_ERROR(" query for initialization of memory failed  \n");
    	    GError *error = NULL;
    	    GQuark domain;
    	    domain = g_quark_from_string("mfw_h264decoder");
    	    error = g_error_new(domain, 10, "fatal error");
    	    gst_element_post_message(GST_ELEMENT(h264dec_struct),
    				     gst_message_new_error(GST_OBJECT
    							   (h264dec_struct),
    							   error,
    							   "query for initialization of memory failed  "
    							   "  H.264 decode plugin "));
    	}

    	mfw_gst_h264dec_initappmemory(&h264dec_struct->dec_config.
    				      sMemInfo);

    	if (!mfw_gst_h264dec_allocdecmem(&h264dec_struct->dec_config.sMemInfo))
    	   return GST_FLOW_ERROR;

    	h264dec_struct->dec_config.sFrameData.eOutputFormat =
    	    E_AVCD_420_PLANAR_PADDED;

    	GST_DEBUG("  Initialize the decoder \n");
    	/* Initializing the decoder */
    	eStatus = eAVCDInitVideoDecoder(&h264dec_struct->dec_config);

    	GST_DEBUG(" init decoder status =%d \n", eStatus);
    	if (eStatus != E_AVCD_INIT) {
    	    GST_ERROR("Error in initializing AVC/H.264 decoder \n Error No %d: \
                            see eAVCDVideoDecoder.h\n",
    		      eStatus);
    	    GError *error = NULL;
    	    GQuark domain;
    	    domain = g_quark_from_string("mfw_h264decoder");
    	    error = g_error_new(domain, 10, "fatal error");
    	    gst_element_post_message(GST_ELEMENT(h264dec_struct),
    				     gst_message_new_error(GST_OBJECT
    							   (h264dec_struct),
    							   error,
    							   "Error in initializing AVC/H.264  "
    							   "  H.264 decoder plugin "));
    	    return GST_FLOW_ERROR;
    	}
    	h264dec_struct->is_decode_init = TRUE;
    }

    else {

        mfw_gst_AVC_Create_NALheader(&buffer);

    	h264dec_struct->dec_config.s32InBufferLength =
    	    GST_BUFFER_SIZE(buffer);
    	h264dec_struct->input_buffer = buffer;
    	h264dec_struct->dec_config.pvInBuffer =
    	    GST_BUFFER_DATA(h264dec_struct->input_buffer);
    	GST_DEBUG(" size  of buffer =%d\n",
    		  h264dec_struct->dec_config.s32InBufferLength);
    }



    if (h264dec_struct->is_sfd) { //
        static int is_key_frame = 0;
        GstFlowReturn ret;
        struct sfd_frames_info *pSfd_info = &h264dec_struct->sfd_info;
        is_key_frame = (!GST_BUFFER_FLAG_IS_SET(buffer,GST_BUFFER_FLAG_DELTA_UNIT));
        if (is_key_frame) {
         GST_INFO("IDR count = %d.\n",pSfd_info->total_key_frames);
        }
        ret = Strategy_FD(is_key_frame,pSfd_info);
        if (ret == GST_FLOW_ERROR)
        {
           TSManagerSend(h264dec_struct->pTS_Mgr);
            gst_buffer_unref(buffer);
            return GST_FLOW_OK;
        }
    }

    result =
	mfw_gst_h264dec_dframe(h264dec_struct,
			       h264dec_struct->input_buffer);

    if (result != GST_FLOW_OK) {

	GST_ERROR(" could not  decode frame %d \n", result);
    }

    h264dec_struct->number_of_nal_units = 0;
    memset(h264dec_struct->nal_size, 0, sizeof(h264dec_struct->nal_size));
    gst_buffer_unref(buffer);
    buffer = NULL;
    h264dec_struct->input_buffer = NULL;
    if (h264dec_struct->profile) {
	gettimeofday(&tv_prof3, 0);
	time_before = (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
	time_after = (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
	h264dec_struct->chain_Time += time_after - time_before;
    }

    return result;
}

/*=============================================================================
FUNCTION:	mfw_gst_h264dec_src_event

DESCRIPTION:	send an	event to source	 pad of	h264decoder element

ARGUMENTS PASSED:
		pad		   -	pointer	to pad
		event	   -	pointer	to event
RETURN VALUE:
		TRUE	   -	event is handled properly
		FALSE	   -	event is not handled properly

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static gboolean
mfw_gst_h264dec_src_event(GstPad * src_pad, GstEvent * event)
{

    gboolean res = TRUE;

    MFW_GST_H264DEC_INFO_T *h264dec_struct =
	MFW_GST_H264DEC(gst_pad_get_parent(src_pad));

   switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEEK:
	{
	    res = gst_pad_push_event(h264dec_struct->sinkpad, event);

	    if (TRUE != res) {
		GST_DEBUG
		    ("\n Error	in pushing the event,result	is %d\n",
		     res);
                gst_object_unref(h264dec_struct);
		return res;
            }
	    break;
	}
	/* Qos Event handle, trigger the strategy of frame dropping. */
    case GST_EVENT_QOS:
    {
    if (h264dec_struct->is_sfd) { //
        struct sfd_frames_info *pSfd_info = &h264dec_struct->sfd_info;
		gdouble proportion;
		GstClockTimeDiff diff;
		GstClockTime timestamp;

		gst_event_parse_qos(event, &proportion, &diff, &timestamp);

		if (diff >= 0) {
	        GST_QOS_EVENT_HANDLE(pSfd_info,diff,h264dec_struct->frame_rate);
		} else {
		    GST_DEBUG
			("the time of decoding is before the system, it is OK\n");
		}
		res = gst_pad_push_event(h264dec_struct->sinkpad, event);
    }
	break;
    }

	default:
	res = FALSE;
	gst_event_unref(event);
	break;
    }
    gst_object_unref(h264dec_struct);
    return res;
}

/*=============================================================================
FUNCTION:	mfw_gst_h264_flushframe

DESCRIPTION:	flush remaining frame in h264 decoder

ARGUMENTS PASSED:
                        h264dec_struct handler
RETURN VALUE:
                        GstFlowReturn
PRE-CONDITIONS:
=============================================================================*/

static GstFlowReturn mfw_gst_h264_flushframe( MFW_GST_H264DEC_INFO_T *h264dec_struct)
{
        GstClockTime ts;
        GstFlowReturn result = GST_FLOW_OK;
        eAVCDRetType eStatus = E_AVCD_NOERROR;
        sAVCDecoderConfig * pdec_config = &h264dec_struct->dec_config;
         unsigned int addr;

    	while( eStatus == E_AVCD_NOERROR)
    	{
    		 eStatus=eAVCDecoderFlushAll(pdec_config);
            if(eStatus == E_AVCD_NOERROR) {
                if (h264dec_struct->dec_config.sFrameData.eOutputFormat == E_AVCD_420_PLANAR_PADDED){
                    addr = (unsigned int)h264dec_struct->dec_config.sFrameData.pu8y;
                    /*we need to move decoded data address to padded start address */
                    addr = addr - h264dec_struct->frame_width_padded * CROP_TOP_LENGTH - CROP_LEFT_LENGTH;
                } else {
                    addr = h264dec_struct->dec_config.sFrameData.pu8y ;
                }
                ts = TSManagerSend(h264dec_struct->pTS_Mgr);
                BM_RENDER_BUFFER(addr, h264dec_struct->srcpad, result, ts, 0);
            }
    	}
        return result;
}

/*=============================================================================
FUNCTION:	mfw_gst_h264dec_sink_event

DESCRIPTION:	send an	event to source	 pad of	h264decoder element

ARGUMENTS PASSED:
		pad		   -	pointer	to pad
		event	   -	pointer	to event
RETURN VALUE:
		TRUE	   -	event is handled properly
		FALSE	   -	event is not handled properly

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static gboolean mfw_gst_h264dec_sink_event(GstPad * pad, GstEvent * event)
{
    GST_DEBUG("	in mfw_gst_h264dec_sink_event function \n");

    MFW_GST_H264DEC_INFO_T *h264dec_struct =
	MFW_GST_H264DEC(GST_PAD_PARENT(pad));
    gboolean result = TRUE;
    eAVCDRetType eStatus = E_AVCD_NOERROR;
    GstFormat format;
    GST_DEBUG("handling %s	event\n ", GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
	{

	    GstFormat format;
	    gint64 start, stop, position;
	    gdouble rate;

	    gst_event_parse_new_segment(event, NULL, &rate, &format,
					&start, &stop, &position);

	    GST_DEBUG(" receiving new seg \n");
	    GST_DEBUG(" start = %" GST_TIME_FORMAT, GST_TIME_ARGS(start));
	    GST_DEBUG(" stop = %" GST_TIME_FORMAT, GST_TIME_ARGS(stop));

	    GST_DEBUG(" position in h264  =%" GST_TIME_FORMAT,
		      GST_TIME_ARGS(position));

	    if (GST_FORMAT_TIME == format) {

		result = gst_pad_push_event(h264dec_struct->srcpad, event);

	    } else {
		GST_DEBUG("dropping newsegment	event in format	%s",
			  gst_format_get_name(format));
                gst_event_unref(event);
		result = TRUE;
	    }

		resyncTSManager(h264dec_struct->pTS_Mgr, start, MODE_AI);
	    break;
	}
    case GST_EVENT_EOS:
	{
	    GST_DEBUG("\n Got the EOS from sink\n");
        GST_WARNING("total frames :%d, dropped frames: %d.\n",h264dec_struct->sfd_info.total_frames,
            h264dec_struct->sfd_info.dropped_frames);

            mfw_gst_h264_flushframe(h264dec_struct);

	    result = gst_pad_push_event(h264dec_struct->srcpad, event);
	    if (TRUE != result) {
		GST_ERROR("\n Error in pushing the event,result	is %d\n",
			  result);
	    }

	    break;
	}

    case GST_EVENT_FLUSH_STOP:
	{

	    if (h264dec_struct->input_buffer != NULL) {

		gst_buffer_unref(h264dec_struct->input_buffer);
		h264dec_struct->input_buffer = NULL;
	    }
	    /* when flush stop event occurs clear all the decoder and plugin
	       memory and initialize the decoder again */

	    h264dec_struct->is_decode_init = FALSE;
	    eAVCDFreeVideoDecoder(&h264dec_struct->dec_config);
	    mfw_gst_h264dec_freedecmem(&h264dec_struct->dec_config.
				       sMemInfo);
        resyncTSManager(h264dec_struct->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
	    result = gst_pad_push_event(h264dec_struct->srcpad, event);

	    if (TRUE != result) {
		GST_ERROR("\n Error in pushing the event,result	is %d\n",
			  result);
	    }
	    break;
	}

    case GST_EVENT_FLUSH_START:
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    if (TRUE != result) {
		GST_ERROR("\n Error in pushing the event,result	is %d\n",
			  result);
	    }

	    break;
	}
    }

    GST_DEBUG("	going out of  mfw_gst_h264dec_sink_event function \n");
    return result;
}


/*=============================================================================
FUNCTION:   src_templ

DESCRIPTION:    Template to create a srcpad for the decoder.

ARGUMENTS PASSED:
        None.

RETURN VALUE:
        a GstPadTemplate

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
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
				   "width", GST_TYPE_INT_RANGE,
				   MIN_FRAME_SIZE, MAX_FRAME_SIZE,
				   "height", GST_TYPE_INT_RANGE,
				   MIN_FRAME_SIZE, MAX_FRAME_SIZE, NULL);
#if 0


	structure = gst_caps_get_structure(caps, 0);

	g_value_init(&list, GST_TYPE_LIST);
	g_value_init(&fps, GST_TYPE_FRACTION);
	for (n = 0; fpss[n][0] != 0; n++) {
	    gst_value_set_fraction(&fps, fpss[n][0], fpss[n][1]);
	    gst_value_list_append_value(&list, &fps);
	}
	gst_structure_set_value(structure, "framerate", &list);
	g_value_unset(&list);
	g_value_unset(&fps);
	g_value_init(&list, GST_TYPE_LIST);
	g_value_init(&fmt, GST_TYPE_FOURCC);
	for (n = 0; fmts[n] != NULL; n++) {
	    gst_value_set_fourcc(&fmt, GST_STR_FOURCC(fmts[n]));
	    gst_value_list_append_value(&list, &fmt);
	}
	gst_structure_set_value(structure, "format", &list);
	g_value_unset(&list);
	g_value_unset(&fmt);

#endif

	templ =
	    gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
    }
    return templ;
}


/*=============================================================================
FUNCTION:	mfw_gst_h264dec_set_caps

DESCRIPTION:	this function handles the link with	other plug-ins and used	for
				capability negotiation	between	pads

ARGUMENTS PASSED:
		pad		   -	pointer	to GstPad
		caps	   -	pointer	to GstCaps

RETURN VALUE:
		TRUE	   -	if capabilities	are	set	properly
		FALSE	   -	if capabilities	are	not	set	properly
PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static gboolean mfw_gst_h264dec_set_caps(GstPad * pad, GstCaps * caps)
{


    MFW_GST_H264DEC_INFO_T *h264dec_struct;

    GstStructure *structure = gst_caps_get_structure(caps, 0);
    h264dec_struct = MFW_GST_H264DEC(GST_PAD_PARENT(pad));
    const gchar *mime;
    gint32 frame_rate_de = 0;
    gint32 frame_rate_nu = 0;

    GValue *codec_data_buf = NULL;

    mime = gst_structure_get_name(structure);

    if (strcmp(mime, "video/x-h264") != 0) {
	GST_WARNING("Wrong mimetype %s	provided, we only support %s",
		    mime, "video/x-h264");
	return FALSE;
    }

    gst_structure_get_fraction(structure, "framerate", &h264dec_struct->framerate_n,
			       &h264dec_struct->framerate_d);

    if (h264dec_struct->framerate_d){
        h264dec_struct->frame_rate = (gfloat)h264dec_struct->framerate_n/(gfloat)h264dec_struct->framerate_d;
       setTSManagerFrameRate(h264dec_struct->pTS_Mgr, h264dec_struct->framerate_n, h264dec_struct->framerate_d);
    }

    gst_structure_get_int(structure, "width",
			  &h264dec_struct->frame_width);
    gst_structure_get_int(structure, "height",
			  &h264dec_struct->frame_height);

    GST_DEBUG(" Frame Width  = %d\n", h264dec_struct->frame_width);

    GST_DEBUG(" Frame Height = %d\n", h264dec_struct->frame_height);

    h264dec_struct->frame_width_padded = (h264dec_struct->frame_width+15)/16*16+32;
    h264dec_struct->frame_height_padded = (h264dec_struct->frame_height+15)/16*16+32;

    /* Handle the codec_data information */
    codec_data_buf = (GValue *) gst_structure_get_value(structure, "codec_data");
    if (NULL != codec_data_buf) {
        guint8 *hdrextdata;
        gint i;
        h264dec_struct->codec_data = gst_value_get_buffer(codec_data_buf);
        GST_DEBUG ("H.264 SET CAPS check for codec data \n");
        /*
         * Enable the SFD strategy only
         *  if the demuxer is compatible to pass the key frames
         */
       // h264dec_struct->is_sfd = FALSE;

        h264dec_struct->codec_data_len = GST_BUFFER_SIZE(h264dec_struct->codec_data);
        GST_DEBUG("\n>>H264 decoder: AVC Codec specific data length is %d\n",h264dec_struct->codec_data_len);
        GST_DEBUG("AVC codec data is \n");
        hdrextdata = GST_BUFFER_DATA(h264dec_struct->codec_data);
        for(i=0;i<h264dec_struct->codec_data_len;i++)
            GST_DEBUG("%x ",hdrextdata[i]);
        GST_DEBUG("\n");

    }

    return TRUE;

}

/*=============================================================================
FUNCTION:	mfw_gst_h264dec_change_state

DESCRIPTION: this function keeps track of different	states of pipeline.

ARGUMENTS PASSED:
		element		-	pointer	to element
		transition	-	state of the pipeline

RETURN VALUE:
		GST_STATE_CHANGE_FAILURE	- the state	change failed
		GST_STATE_CHANGE_SUCCESS	- the state	change succeeded
		GST_STATE_CHANGE_ASYNC		- the state	change will	happen
										asynchronously
		GST_STATE_CHANGE_NO_PREROLL	- the state	change cannot be prerolled

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static GstStateChangeReturn
mfw_gst_h264dec_change_state(GstElement * element,
			     GstStateChange transition)
{

    MFW_GST_H264DEC_INFO_T *h264dec_struct;
    GstStateChangeReturn ret;
    h264dec_struct = MFW_GST_H264DEC(element);
    switch (transition) {

    case GST_STATE_CHANGE_NULL_TO_READY:
	break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
	h264dec_struct->caps_set = FALSE;
	h264dec_struct->is_decode_init = FALSE;
	h264dec_struct->input_buffer = NULL;
	h264dec_struct->ff_flag = 0;
	h264dec_struct->Time = 0;
	h264dec_struct->chain_Time = 0;
	h264dec_struct->no_of_frames_dropped = 0;
	h264dec_struct->no_of_frames = 0;
	//h264dec_struct->TimeStamp_Object.ts_rx = 0;
	//h264dec_struct->TimeStamp_Object.ts_tx = 0;
	//memset(&h264dec_struct->TimeStamp_Object.timestamp_buffer[0], 0, MAX_STREAM_BUF);
    //   mfw_gst_init_ts(&h264dec_struct->TimeStamp_Object);
    resyncTSManager(h264dec_struct->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);
    bm_get_buf_init = FALSE;
	break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	break;

    default:
	break;
    }
    ret = parent_class->change_state(element, transition);
    switch (transition) {

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
	{
	    gfloat avg_mcps = 0, avg_plugin_time = 0, avg_dec_time = 0;
	    gfloat temp;

	    if (h264dec_struct->profile) {
		g_print
		    ("\nTotal decode time for h264 is                   %ldus",
		     h264dec_struct->Time);
		g_print
		    ("\nTotal plugin time for h264 is                   %ldus",
		     h264dec_struct->chain_Time);
		g_print
		    ("\nTotal number of frames decoded  for h264 is      %d",
		     h264dec_struct->no_of_frames);

		temp = ((float) 1000000 *
			(h264dec_struct->no_of_frames -
			 h264dec_struct->no_of_frames_dropped));

		avg_mcps =
		    ((float) h264dec_struct->Time * PROCESSOR_CLOCK /
		     temp * h264dec_struct->frame_rate);
		g_print
		    ("\nAverage decode MCPS for h264 is               %f",
		     avg_mcps);

		avg_mcps =
		    ((float) h264dec_struct->chain_Time * PROCESSOR_CLOCK /
		     temp * h264dec_struct->frame_rate);
		g_print
		    ("\nAverage plug-in MCPS for h264 is               %f",
		     avg_mcps);
		avg_dec_time =
		    ((float) h264dec_struct->Time) /
		    h264dec_struct->no_of_frames;
		g_print
		    ("\nAverage decoding time for h264 is               %fus",
		     avg_dec_time);
		avg_plugin_time =
		    ((float) h264dec_struct->chain_Time) /
		    h264dec_struct->no_of_frames;
		g_print
		    ("\nAverage plugin time  for h264 is                 %fus\n",
		     avg_plugin_time);
	    }
		h264dec_struct->Time = 0;
		h264dec_struct->chain_Time = 0;
		h264dec_struct->no_of_frames = 0;
	    h264dec_struct->no_of_frames_dropped = 0;


        resyncTSManager(h264dec_struct->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);

	    /* freeing the allocated memory */
            eAVCDFreeVideoDecoder(&h264dec_struct->dec_config);
	    mfw_gst_h264dec_freedecmem(&h264dec_struct->dec_config.sMemInfo);

	}
	break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        BM_CLEAN_LIST;
	break;

    default:
	break;
    }
    return ret;
}


/*=============================================================================
FUNCTION: mfw_gst_h264dec_set_property

DESCRIPTION: sets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property set by the application
        pspec      - pointer to the attributes of the property

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static void
mfw_gst_h264dec_set_property(GObject * object, guint prop_id,
			     const GValue * value, GParamSpec * pspec)
{

    MFW_GST_H264DEC_INFO_T *h264dec_struct = MFW_GST_H264DEC(object);
    switch (prop_id) {
    case ID_PROFILE_ENABLE:
	h264dec_struct->profile = g_value_get_boolean(value);
	GST_DEBUG("profile=%d\n", h264dec_struct->profile);
	break;
    case ID_BADDATA_DISPLAY:
	h264dec_struct->display_baddata = g_value_get_boolean(value);
	GST_DEBUG("display_badata=%d\n", h264dec_struct->display_baddata);
	break;
    case ID_BMMODE:
	h264dec_struct->bmmode= g_value_get_int(value);
	GST_DEBUG("buffermanager mode=%d\n", h264dec_struct->bmmode);
	break;
	case ID_SFD:
	h264dec_struct->is_sfd = g_value_get_boolean(value);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
}

/*=============================================================================
FUNCTION: mfw_gst_h264dec_get_property

DESCRIPTION: gets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property to be set for the next element
        pspec      - pointer to the attributes of the property

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_h264dec_get_property(GObject * object, guint prop_id,
			     GValue * value, GParamSpec * pspec)
{

    MFW_GST_H264DEC_INFO_T *h264dec_struct = MFW_GST_H264DEC(object);
    switch (prop_id) {
    case ID_PROFILE_ENABLE:
	GST_DEBUG("profile=%d\n", h264dec_struct->profile);
	g_value_set_boolean(value, h264dec_struct->profile);
	break;
    case ID_BADDATA_DISPLAY:
	GST_DEBUG("display_data=%d\n", h264dec_struct->display_baddata);
	g_value_set_boolean(value, h264dec_struct->display_baddata);
	break;
    case ID_BMMODE:
    g_value_set_int(value, BM_GET_MODE);
    break;
	case ID_SFD:
	g_value_set_boolean(value, h264dec_struct->is_sfd);

	break;

    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
}


/*=============================================================================
FUNCTION:	mfw_gst_h264dec_init

DESCRIPTION:	create the pad template	that has been registered with the
				element	class in the _base_init

ARGUMENTS PASSED:
		h264dec_struct-	  pointer to h264decoder	element	structure

RETURN VALUE:
		None

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static void mfw_gst_h264dec_init(MFW_GST_H264DEC_INFO_T * h264dec_struct)
{
    GST_DEBUG("\n in mfw_gst_h264dec_init  routine\n");

    GstElementClass *klass = GST_ELEMENT_GET_CLASS(h264dec_struct);


    h264dec_struct->sinkpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "sink"), "sink");

    gst_element_add_pad(GST_ELEMENT(h264dec_struct),
			h264dec_struct->sinkpad);

    gst_pad_set_setcaps_function(h264dec_struct->sinkpad,
				 mfw_gst_h264dec_set_caps);

    h264dec_struct->srcpad = gst_pad_new_from_template(src_templ(), "src");

    gst_pad_set_chain_function(h264dec_struct->sinkpad,
			       GST_DEBUG_FUNCPTR(mfw_gst_h264dec_chain));

    gst_pad_set_event_function(h264dec_struct->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_h264dec_sink_event));

    gst_pad_set_event_function(h264dec_struct->srcpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_h264dec_src_event));

    gst_element_add_pad(GST_ELEMENT(h264dec_struct),
			h264dec_struct->srcpad);
    h264dec_struct->profile = FALSE;
    h264dec_struct->display_baddata = TRUE;

    h264dec_struct->framerate_n = 25;
    h264dec_struct->framerate_d = 1;
    h264dec_struct->frame_rate = 25;

    h264dec_struct->pTS_Mgr = createTSManager(0);

    GST_DEBUG("	out	of mfw_gst_h264dec_init routine\n");
    INIT_SFD_INFO(&h264dec_struct->sfd_info);
    h264dec_struct->is_sfd = TRUE;

    bm_get_buf_init = FALSE;

#define MFW_GST_H264_DECODER_PLUGIN VERSION
    PRINT_CORE_VERSION(H264DCodecVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_H264_DECODER_PLUGIN);

    INIT_DEMO_MODE(H264DCodecVersionInfo(),h264dec_struct->demo_mode);
}
/*======================================================================================
FUNCTION:           mfw_gst_h264dec_finalize

DESCRIPTION:        Class finalized

ARGUMENTS PASSED:   object     - pointer to the elements object

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static void
mfw_gst_h264dec_finalize (GObject * object)
{
    MFW_GST_H264DEC_INFO_T *h264dec_struct = MFW_GST_H264DEC(object);
	destroyTSManager(h264dec_struct->pTS_Mgr);
    GST_DEBUG (">>H246 DEC: class finalized.\n");
}



/*=============================================================================
FUNCTION:	mfw_gst_h264dec_class_init

DESCRIPTION:	Initialise the class only once (specifying what	signals,
				arguments and virtual functions	the	class has and setting up
				global state)

ARGUMENTS PASSED:
		klass	   -	pointer	to h264decoder element class

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
mfw_gst_h264dec_class_init(MFW_GST_H264DEC_INFO_CLASS_T * klass)
{
    GST_DEBUG("	in mfw_gst_h264dec_class_init function \n");

    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
    gobject_class->set_property = mfw_gst_h264dec_set_property;
    gobject_class->get_property = mfw_gst_h264dec_get_property;
    gstelement_class->change_state = mfw_gst_h264dec_change_state;
    gobject_class->finalize = mfw_gst_h264dec_finalize;

    g_object_class_install_property(gobject_class, ID_PROFILE_ENABLE,
				    g_param_spec_boolean("profiling",
							 "Profiling",
							 "Enable time profiling of decoder and plugin",
							 FALSE,
							 G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, ID_BADDATA_DISPLAY,
				    g_param_spec_boolean("display_baddata",
							 "Display_baddata",
							 "Enable to display bad data of decoding",
							 TRUE,
							 G_PARAM_READWRITE));
    /* install property for buffer manager mode control. */
    g_object_class_install_property(gobject_class, ID_BMMODE,
				    g_param_spec_int("bmmode",
						       "BMMode",
						       "set the buffer manager mode direct/indirect",
						       0,
						       1, 0,
						       G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_SFD,
				    g_param_spec_boolean("sfd",
						       "Strategy of Frame Dropping",
						       "Strategy of Frame Dropping, 0: Disable, 1: Enable",
						       TRUE,
						       G_PARAM_READWRITE));

    GST_DEBUG("	out	of mfw_gst_h264dec_class_init function \n");
}


/*=============================================================================
FUNCTION:	mfw_gst_h264dec_base_init

DESCRIPTION:	Element	details	are	registered with	the	plugin during
				 _base_init	,This function will	initialise the class and child
				class properties during each new child	class creation

ARGUMENTS PASSED:
		Klass	   -	void pointer

RETURN VALUE:
		None

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static void mfw_gst_h264dec_base_init(gpointer klass)
{
    GST_DEBUG("	in mfw_gst_h264dec_base_init	function \n");

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&mfw_gst_h264dec_sink_template_factory));

    gst_element_class_add_pad_template(element_class, src_templ());

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "h264 video decoder",
        "Codec/Decoder/Video", "Decode compressed h264 video to raw data");

    GST_DEBUG("	out	of mfw_gst_h264dec_base_init	function \n");

}


/*=============================================================================

FUNCTION:	mfw_gst_type_h264dec_get_type

DESCRIPTION:	intefaces are initiated	in this	function.you can register one
				or more	interfaces after having	registered the type	itself.

ARGUMENTS PASSED:
		None

RETURN VALUE:
		A numerical	value ,which represents	the	unique identifier of this
		elment(h264decoder)

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
GType mfw_gst_h264dec_get_type(void)
{
    static GType h264dec_type = 0;

    if (!h264dec_type) {
	static const GTypeInfo gsth264dec_info = {
	    sizeof(MFW_GST_H264DEC_INFO_CLASS_T),
	    mfw_gst_h264dec_base_init,
	    NULL,
	    (GClassInitFunc) mfw_gst_h264dec_class_init,
	    NULL,
	    NULL,
	    sizeof(MFW_GST_H264DEC_INFO_T),
	    0,
	    (GInstanceInitFunc) mfw_gst_h264dec_init,
	};
	h264dec_type = g_type_register_static(GST_TYPE_ELEMENT,
					      "MFW_GST_H264DEC_INFO_T",
					      &gsth264dec_info, 0);
    }

    GST_DEBUG_CATEGORY_INIT(mfw_gst_h264dec_debug, "mfw_h264decoder", 0,
			    "FreeScale's H264  Decoder's Log");

    return h264dec_type;

}

/*=============================================================================
FUNCTION:	plugin_init

DESCRIPTION:	special	function , which is	called as soon as the plugin or
				element	is loaded and information returned by this function
				will be	cached in central registry

ARGUMENTS PASSED:
		plugin	   -	pointer	to container that contains features	loaded
						from shared	object module

RETURN VALUE:
		return TRUE	or FALSE depending on whether it loaded	initialized	any
		dependency correctly

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static gboolean plugin_init(GstPlugin * plugin)
{
    return gst_element_register(plugin, "mfw_h264decoder",
				GST_RANK_PRIMARY, MFW_GST_TYPE_H264DEC);
}

FSL_GST_PLUGIN_DEFINE("h264", "h264 video decoder", plugin_init);

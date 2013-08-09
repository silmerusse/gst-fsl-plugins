/*
 * Copyright (c) 2009-2012, Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_mpeg4asp_dec.c
 *
 * Description:    GStreamer Plug-in for MPEG4-Decoder
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * June 25 2009 Dexter Ji <b01140@freescale.com>
 * - Initial version
 *
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/

#include <fcntl.h>
#include <string.h>
#include <gst/gst.h>
#include <time.h>
#include <sys/time.h>
#include "mpeg4_asp_api.h"


#include "mfw_gst_utils.h"

#include "mfw_gst_mpeg4asp_dec.h"

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/
#define CALL_BUFF_LEN	   512
#define PROCESSOR_CLOCK    532
#define CROP_LEFT_LENGTH  16
#define CROP_TOP_LENGTH    16

#define MFW_GST_MPEG4ASP_VIDEO_CAPS \
    "video/mpeg, "                  \
    "mpegversion = (int) 4, "       \
    "width = (int) [0, 1280], "     \
    "height = (int) [0, 720]; "     \
                                    \
    "video/x-h263, "                \
    "width = (int) [0, 1280], "     \
    "height = (int)[0, 720]; "      \
                                    \
    "video/x-xvid, "                \
    "width = (int) [16, 1280], "    \
    "height = (int)[16, 720] "





/* used	for	debugging */
#define	GST_CAT_DEFAULT    mfw_gst_mpeg4asp_decoder_debug
/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/


/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
enum {
    PROF_ENABLE = 1,
    MFW_MPEG4ASPDEC_FRAMERATE,
	ID_BMMODE,
	ID_SFD,  /* Strategy of Frame dropping */
    
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
								   GST_PAD_SINK,
								   GST_PAD_ALWAYS,
								   GST_STATIC_CAPS
								   (MFW_GST_MPEG4ASP_VIDEO_CAPS)
    );
/*=============================================================================
                                LOCAL MACROS
=============================================================================*/

/* None. */

/*=============================================================================
                               STATIC VARIABLES
=============================================================================*/
static GstElementClass *parent_class = NULL;

/* table with framerates expressed as fractions */
static const gint fpss[][2] = { {24000, 1001},
{24, 1}, {25, 1}, {30000, 1001},
{30, 1}, {50, 1}, {60000, 1001},
{60, 1}, {0, 1}
};

/*=============================================================================
                        STATIC FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_mpeg4asp_decoder_debug);

static void mfw_gst_mpeg4asp_decoder_class_init(MFW_GST_MPEG4ASP_DECODER_CLASS_T
					     * klass);
static void mfw_gst_mpeg4asp_decoder_base_init(MFW_GST_MPEG4ASP_DECODER_CLASS_T *
					    klass);
static void mfw_gst_mpeg4asp_decoder_init(MFW_GST_MPEG4ASP_DECODER_INFO_T *
				       filter);

static void mfw_gst_mpeg4asp_decoder_set_property(GObject * object,
					       guint prop_id,
					       const GValue * value,
					       GParamSpec * pspec);

static void mfw_gst_mpeg4asp_decoder_get_property(GObject * object,
					       guint prop_id,
					       GValue * value,
					       GParamSpec * pspec);

static gboolean mfw_gst_mpeg4asp_decoder_sink_event(GstPad *, GstEvent *);
static gboolean mfw_gst_mpeg4asp_decoder_src_event(GstPad *, GstEvent *);

static gboolean mfw_gst_mpeg4asp_decoder_set_caps(GstPad * pad,
					     GstCaps * caps);
static GstFlowReturn mfw_gst_mpeg4asp_decoder_chain(GstPad * pad,
						 GstBuffer * buf);

static GstStateChangeReturn mfw_gst_mpeg4asp_decoder_change_state(GstElement *, 
                         GstStateChange);

/* Call back function used for direct render v2 */
static void* mfw_gst_mpeg4asp_getbuffer(void* pvAppContext);
static void  mfw_gst_mpeg4asp_rejectbuffer(void* pbuffer, void* pvAppContext);
static void  mfw_gst_mpeg4asp_releasebuffer(void* pbuffer, void* pvAppContext);


    
/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
FUNCTION:               mfw_gst_mpeg4asp_getbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder need a new frame buffer.

ARGUMENTS PASSED:       pvAppContext -> Pointer to the context variable.

RETURN VALUE:           Pointer to a frame buffer.  -> On success.
                        Null.                       -> On fail.

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void* mfw_gst_mpeg4asp_getbuffer(void* pvAppContext)
{

	MFW_GST_MPEG4ASP_DECODER_INFO_T * mpeg4asp_dec = 
        (MFW_GST_MPEG4ASP_DECODER_INFO_T *)pvAppContext;
	void * pbuffer;
	GstCaps *caps = NULL;
	int output_size ;

	sMpeg4DecInitInfo *Mpeg4DecInitInfo = mpeg4asp_dec->Mpeg4DecInitInfo;


	if (mpeg4asp_dec->caps_set == FALSE) {
	gint64 start = 0;	/*  proper timestamp has to set here */
	GstCaps *caps;
	gint fourcc = GST_STR_FOURCC("I420");
	guint framerate_n, framerate_d;
    guint crop_right_len = 0, crop_bottom_len = 0;

 	crop_right_len = (mpeg4asp_dec->frame_width_padded - 
                Mpeg4DecInitInfo->sStreamInfo.u16ActFrameWidth) - CROP_LEFT_LENGTH;
	     
	crop_bottom_len = (mpeg4asp_dec->frame_height_padded -
	    Mpeg4DecInitInfo->sStreamInfo.u16ActFrameHeight ) - CROP_TOP_LENGTH; 


	caps =
	    gst_caps_new_simple("video/x-raw-yuv", "format",
				GST_TYPE_FOURCC, fourcc, "width",
				G_TYPE_INT,
				mpeg4asp_dec->frame_width_padded,
				"height", G_TYPE_INT,
				mpeg4asp_dec->frame_height_padded,
				"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
				CAPS_FIELD_CROP_LEFT, G_TYPE_INT, CROP_LEFT_LENGTH,
				CAPS_FIELD_CROP_TOP, G_TYPE_INT, CROP_TOP_LENGTH,
				CAPS_FIELD_CROP_RIGHT, G_TYPE_INT, crop_right_len,
				CAPS_FIELD_CROP_BOTTOM, G_TYPE_INT, crop_bottom_len, 
				CAPS_FIELD_REQUIRED_BUFFER_NUMBER, G_TYPE_INT,	BM_GET_BUFFERNUM,
				   NULL); 

    GST_DEBUG("set caps:%s.\n",gst_caps_to_string(caps));
 
    if ( (mpeg4asp_dec->is_sfd) ) {
        GST_ADD_SFD_FIELD(caps);
    }
    
	if (mpeg4asp_dec->framerate_d) {
	    gst_caps_set_simple(caps, "framerate", GST_TYPE_FRACTION,
				mpeg4asp_dec->framerate_n, mpeg4asp_dec->framerate_d, NULL);
	}
    
	if (!(gst_pad_set_caps(mpeg4asp_dec->srcpad, caps))) {
	    GST_ERROR
		("\nCould not set the caps for the mpeg4aspdecoder src pad\n");
	}
	mpeg4asp_dec->caps_set = TRUE;
	gst_caps_unref(caps);
    }

	/* Get the GST buffer pointer */
	BM_GET_BUFFER(mpeg4asp_dec->srcpad, mpeg4asp_dec->outsize, pbuffer);

    /* FIXME: Set it to Output buffer sMpeg4DecYCbCrBuffer structure */
    
	return pbuffer;
}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg4asp_rejectbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder want to indicate a frame buffer would not be 
                        used as a output.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for reject
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void mfw_gst_mpeg4asp_rejectbuffer(void* pbuffer, void* pvAppContext)
{
    BM_REJECT_BUFFER(pbuffer);
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg4asp_releasebuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder want to indicate a frame buffer would never used
                        as a reference.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for release
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void mfw_gst_mpeg4asp_releasebuffer(void* pbuffer, void* pvAppContext)
{
    BM_RELEASE_BUFFER(pbuffer);
}
/*=============================================================================
FUNCTION: mfw_gst_mpeg4asp_decoder_set_property

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
mfw_gst_mpeg4asp_decoder_set_property(GObject * object, guint prop_id,
				   const GValue * value,
				   GParamSpec * pspec)
{

    MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec =
	                                    MFW_GST_MPEG4ASP_DECODER(object);
    switch (prop_id) {
        case PROF_ENABLE:
    	mpeg4asp_dec->profiling = g_value_get_boolean(value);
    	GST_DEBUG("profiling=%d\n", mpeg4asp_dec->profiling);
    	break;

    	case ID_BMMODE:
    	mpeg4asp_dec->bmmode= g_value_get_int(value);
    	GST_DEBUG("buffermanager mode=%d\n", mpeg4asp_dec->bmmode);
    	break;

    	case ID_SFD:
    	mpeg4asp_dec->is_sfd = g_value_get_boolean(value);

        default:
    	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    	break;
    }

}

/*=============================================================================
FUNCTION: mfw_gst_mpeg4asp_decoder_get_property

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
mfw_gst_mpeg4asp_decoder_get_property(GObject * object, guint prop_id,
				   GValue * value, GParamSpec * pspec)
{

    MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec =
	MFW_GST_MPEG4ASP_DECODER(object);
    switch (prop_id) {
        case PROF_ENABLE:
    	g_value_set_boolean(value, mpeg4asp_dec->profiling);
    	break;

    	case ID_BMMODE:
    	g_value_set_int(value, BM_GET_MODE);
    	break;

    	case ID_SFD:
    	g_value_set_boolean(value, mpeg4asp_dec->is_sfd);

        default:
    	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    	break;
    }

}

/*=============================================================================
FUNCTION: pvAllocateFastMem

DESCRIPTION: allocates memory of required size from a fast memory

ARGUMENTS PASSED:
        size       - size of memory required to allocate
        align      - alignment required

RETURN VALUE:
        void *      - base address of the memory allocated

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        We have not taken any extra step to align the memory to 
        the required type. Assumption is that the allocated memory 
        is always word aligned. 
=============================================================================*/

void *pvAllocateFastMem(gint size, gint align)
{
    return MM_MALLOC(size);

}

/*=============================================================================
FUNCTION: pvAllocateSlowMem

DESCRIPTION: allocates memory of required size from a slow memory

ARGUMENTS PASSED:
        size       - size of memory required to allocate
        align      - alignment required

RETURN VALUE:
        void *      - base address of the memory allocated

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        We have not taken any extra step to align the memory to 
        the required type. Assumption is that the allocated memory 
        is always word aligned. 
=============================================================================*/

void *pvAllocateSlowMem(gint size, gint align)
{
    return g_malloc(size);

}

/*=============================================================================
FUNCTION: pvFreeMem

DESCRIPTION: allocates memory of required size from a fast memory

ARGUMENTS PASSED:
        void *      - base address of the memory allocated

RETURN VALUE:

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
=============================================================================*/

void Mpeg4AspFreeMem(void *ptr)
{
    return MM_FREE(ptr);

}


/*=============================================================================
FUNCTION:		mfw_gst_mpeg4asp_decoder_allocatememory

DESCRIPTION:	This function allocates memory required by the decoder

ARGUMENTS PASSED:
        psMemAllocInfo     -  is a pointer to a structure which holds allocated
							  memory. This allocated memory is required by the
							  decoder.
RETURN VALUE:
				returns the status of Memory Allocation, -1/ 0

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

gint mfw_gst_mpeg4asp_decoder_allocatememory(sMpeg4DecInitInfo * psMpeg4DecInitInfo)
{

    gint pMemSize;
    sMpeg4DecMemAllocInfo *psMemInfo = &psMpeg4DecInitInfo->sMemInfo;


    pMemSize = psMemInfo->sFastMemBlk.s32Size;
    
	psMemInfo->sFastMemBlk.pvBuffer = g_malloc(pMemSize);
	GST_DEBUG("allocate mem=0x%x, size=%d \n", psMemInfo->sFastMemBlk.pvBuffer,
		  pMemSize);

	if (psMemInfo->sFastMemBlk.pvBuffer == NULL) {
	    return -1;
	}
    return 0;

}

/*=============================================================================
FUNCTION:		mfw_gst_mpeg4asp_decoder_freememory

DESCRIPTION:	It deallocates all the memory which was allocated for the decoder

ARGUMENTS PASSED:
		psMemAllocInfo	-  is a pointer to a structure which holds allocated
							  memory. This allocated memory is required by the
							  decoder.
RETURN VALUE:
		None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
void mfw_gst_mpeg4asp_decoder_freememory(sMpeg4DecInitInfo * psMpeg4DecInitInfo)
{

    sMpeg4DecMemAllocInfo *psMemInfo = &psMpeg4DecInitInfo->sMemInfo;
    if (psMemInfo == NULL)
        return;
    if (psMemInfo->sFastMemBlk.pvBuffer)
	    Mpeg4AspFreeMem(psMemInfo->sFastMemBlk.pvBuffer);


}

/*=============================================================================
FUNCTION:		mfw_gst_mpeg4asp_decoder_cleanup

DESCRIPTION:	It deallocates all the memory which was allocated by Application

ARGUMENTS PASSED:
        psMpeg4DecObject    -   is a pointer to Mpeg4 Decoder handle

RETURN VALUE:
		None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

void mfw_gst_mpeg4asp_decoder_cleanup(MFW_GST_MPEG4ASP_DECODER_INFO_T * mpeg4asp_dec)
{


    GST_DEBUG("In function mfw_gst_mpeg4asp_decoder_cleanup.\n");
    if ( mpeg4asp_dec->Mpeg4DecInitInfo != NULL) {
        Mpeg4AspFreeMem(mpeg4asp_dec->Mpeg4DecInitInfo);
        mpeg4asp_dec->Mpeg4DecInitInfo = NULL;
    }
    GST_DEBUG("out of function mfw_gst_mpeg4asp_decoder_cleanup.\n");

}



/*=============================================================================

FUNCTION:   Mpeg4AspFrameBufInit

DESCRIPTION: This function initialzes all members of the sMpeg4DecFrameManager.

ARGUMENTS PASSED:
        psMpeg4DecObject     -   pointer to decoder handle


RETURN VALUE:
		None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static void Mpeg4AspFrameBufInit(MFW_GST_MPEG4ASP_DECODER_INFO_T * mpeg4asp_dec)
{

    sMpeg4DecFrameManager *psMpeg4FM = &mpeg4asp_dec->Mpeg4FM;
    sMpeg4DecAppCap *psMpeg4Caps = &mpeg4asp_dec->Mpeg4Caps;
    
    psMpeg4FM->GetterBuffer   = mfw_gst_mpeg4asp_getbuffer ;
    psMpeg4FM->RejectorBuffer	= mfw_gst_mpeg4asp_rejectbuffer;
    psMpeg4FM->ReleaseBuffer  = mfw_gst_mpeg4asp_releasebuffer;
    psMpeg4FM->pvAppContext   = (void *)mpeg4asp_dec;

    psMpeg4Caps->s32MaxFastMem = 1024*1024*32; /* 32M limitation */
    psMpeg4Caps->s32MaxSlowMem = 1024*1024*32; /* 32M limitation */
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4asp_decframe

DESCRIPTION: This function decodes and outputs one frame per call to the next 
             element

ARGUMENTS PASSED:
        mpeg4asp_dec            -   mpeg4asp decoder plugin handle
        psMpeg4DecObject     -   pointer to decoder handle


RETURN VALUE:
            GST_FLOW_OK if decode is successfull
            GST_FLOW_ERROR if error in decoding
    

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static GstFlowReturn mfw_gst_mpeg4asp_decframe(MFW_GST_MPEG4ASP_DECODER_INFO_T *
					    mpeg4asp_dec,	MPEG4DHandle Mpeg4Handle)
{
    eMpeg4DecRetType eDecRetVal = E_MPEG4D_FAILURE;
    GstBuffer *outbuffer = NULL;
    guint8 *outdata = NULL;
    GstCaps *src_caps = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    gchar *hw_qp;
    gint8 *quant;
    sMpeg4DecYCbCrBuffer *psOutBuffer = &mpeg4asp_dec->sOutBuffer;

    unsigned short row, col;
    /*gint cur_buf = 0; */
    struct timeval tv_prof, tv_prof1;
    long time_before = 0, time_after = 0;
    unsigned int addr = 0;

    src_caps = GST_PAD_CAPS(mpeg4asp_dec->srcpad);
    gint Length;
    guint8 *pvBuf = NULL;
    guint32 copy_size=0;
	GstClockTime ts;


    if (mpeg4asp_dec->demo_mode == 2)
        return GST_FLOW_ERROR;
       
    if (mpeg4asp_dec->profiling) {
	    gettimeofday(&tv_prof, 0);
    }

    /* Prepare the bitstream for decoding */
    Length = mpeg4asp_dec->sizebuffer;
	pvBuf = (guint8 *) GST_BUFFER_DATA(mpeg4asp_dec->input_buffer);

    /*The main decoder function is eMPEG4DDecodeFrame. This function decodes 
       the MPEG4 bit stream in the input buffers to generate one frame of decoder 
       output in every call. */
    if(mpeg4asp_dec->control_flag)
	    eDecRetVal = eMPEG4DDecodeFrame_oldDX(Mpeg4Handle, (void *)pvBuf, &Length, mpeg4asp_dec->width, mpeg4asp_dec->height);
    else
		eDecRetVal = eMPEG4DDecodeFrame(Mpeg4Handle, (void *)pvBuf, &Length);
    // GST_BUFFER_OFFSET(mpeg4asp_dec->input_buffer) += Length;
	
    if (mpeg4asp_dec->profiling) {
    	gettimeofday(&tv_prof1, 0);
    	time_before = (tv_prof.tv_sec * 1000000) + tv_prof.tv_usec;
    	time_after = (tv_prof1.tv_sec * 1000000) + tv_prof1.tv_usec;
    	mpeg4asp_dec->Time += time_after - time_before;
    	if (eDecRetVal == E_MPEG4D_SUCCESS) {
    	    mpeg4asp_dec->no_of_frames++;
    	} else {
    	    mpeg4asp_dec->no_of_frames_dropped++;
    	}
    }
	if(eDecRetVal != E_MPEG4D_SUCCESS)
	{           
	           /* while decode frame error occurs, we always need to move timestamp
	            * index to the next, other wise, the array will be refilled without use
	            **/
                mpeg4asp_dec->sfd_info.total_frames++;
                mpeg4asp_dec->sfd_info.dropped_frames++;    			   
                TSManagerSend(mpeg4asp_dec->pTS_Mgr);
		return GST_FLOW_OK;
	}
#ifdef OUTPUT_BUFFER_CHANGES       
    else if (eDecRetVal == E_MPEG4D_SUCCESS) 
	    eDecRetVal = eMPEG4DGetOutputFrame(Mpeg4Handle, &psOutBuffer);
#endif
	if(eDecRetVal != E_MPEG4D_SUCCESS)
	{           
	           /* while decode frame error occurs, we always need to move timestamp
	            * index to the next, other wise, the array will be refilled without use
	            **/
                mpeg4asp_dec->sfd_info.total_frames++;
                mpeg4asp_dec->sfd_info.dropped_frames++;	            
                TSManagerSend(mpeg4asp_dec->pTS_Mgr);
		return GST_FLOW_OK;
	}	


    if  (eDecRetVal == E_MPEG4D_SUCCESS) 
    {

        ts = TSManagerSend(mpeg4asp_dec->pTS_Mgr);
            
        DEMO_LIVE_CHECK(mpeg4asp_dec->demo_mode, 
                        ts, 
                        mpeg4asp_dec->srcpad);

	    /* the data is pushed onto the next element */

	    if (mpeg4asp_dec->send_newseg) {
	    gboolean ret = FALSE;
	    ret =
		gst_pad_push_event(mpeg4asp_dec->srcpad,
				   gst_event_new_new_segment(FALSE, 1.0,
							     GST_FORMAT_TIME,
							     ts,
							     GST_CLOCK_TIME_NONE,
							     ts));
	    mpeg4asp_dec->send_newseg = FALSE;
	    }
        
	    BM_RENDER_BUFFER(mpeg4asp_dec->sOutBuffer.pu8YBuf, mpeg4asp_dec->srcpad, result, ts, 0);
    }
	
    mpeg4asp_dec->decoded_frames++;
    return result;
}


/*=============================================================================
FUNCTION: mfw_gst_mpeg4asp_decoder_chain

DESCRIPTION: Initializing the decoder and calling the actual decoding function

ARGUMENTS PASSED:
        pad     - pointer to pad
        buffer  - pointer to received buffer

RETURN VALUE:
        GST_FLOW_OK		- Frame decoded successfully
		GST_FLOW_ERROR	- Failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static GstFlowReturn
mfw_gst_mpeg4asp_decoder_chain(GstPad * pad, GstBuffer * buffer)
{

    MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec;
    sMpeg4DecInitInfo *psMpeg4DecInitInfo = NULL;
    eMpeg4DecRetType eDecRetVal = E_MPEG4D_FAILURE;
    gint ret = GST_FLOW_ERROR;
    guint64 outsize = 0;
    GstBuffer *outbuffer = NULL;
    guint8 *outdata = NULL;
    GstCaps *src_caps = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    sMpeg4DecMemAllocInfo *psMemAllocInfo = NULL;
    guint8 *frame_buffer = NULL;
    guint temp_length = 0;
    gchar *hw_qp;
    gint8 *quant;
    unsigned short row, col;
    gint cur_buf = 0;
    struct timeval tv_prof2, tv_prof3;
    long time_before = 0, time_after = 0;
    mpeg4asp_dec = MFW_GST_MPEG4ASP_DECODER(GST_PAD_PARENT(pad));
    if (mpeg4asp_dec->profiling) {
	gettimeofday(&tv_prof2, 0);
    }

	{
        GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);
        TSManagerReceive(mpeg4asp_dec->pTS_Mgr, timestamp);
    }

    mpeg4asp_dec->input_buffer = buffer;
    mpeg4asp_dec->sizebuffer = GST_BUFFER_SIZE(buffer);

    if (!mpeg4asp_dec->init_done) {
        
    	if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buffer))) {
    	    mpeg4asp_dec->next_ts = GST_BUFFER_TIMESTAMP(buffer);
    	}

        /* For open source demux (qtdemux), it needs the initial codec data */
        if (mpeg4asp_dec->codec_data_len) 
        {
            gst_buffer_ref(mpeg4asp_dec->codec_data);
            mpeg4asp_dec->input_buffer = gst_buffer_join(mpeg4asp_dec->codec_data,mpeg4asp_dec->input_buffer);
       
        } 

        /* allocate memory for the decoder */
    	psMpeg4DecInitInfo =
    	    (sMpeg4DecInitInfo *) g_malloc(sizeof(sMpeg4DecInitInfo));

    	if (psMpeg4DecInitInfo == NULL) {
    	    GST_ERROR
    		("\nUnable to allocate memory for Mpeg4 Decoder structure\n");
    	    return GST_FLOW_ERROR;
    	} else {
    	    // InitailizeMpeg4DecObject(psMpeg4DecObject);
	        memset(psMpeg4DecInitInfo,0,sizeof(sMpeg4DecInitInfo));
            Mpeg4AspFrameBufInit(mpeg4asp_dec);

    	}

    	frame_buffer = (guint8 *) GST_BUFFER_DATA(mpeg4asp_dec->input_buffer);

    	/* This function returns the memory requirement for the decoder. 
    	   The decoder will parse the sent bit stream to determine the type of 
    	   video content, based on which sMpeg4DecObject.sMemInfo structure 
    	   will be populated. The plugin will use this structure to pre-allocate 
    	   the requested memory block (chunks) by setting the pointers of asMemBlks 
    	   in sMpeg4DecObject.sMemInfo structure to the required size, type & 
    	   aligned memory. */
        if(!(mpeg4asp_dec->control_flag))	
    	    eDecRetVal = eMPEG4DQueryInitInfo(psMpeg4DecInitInfo,
    				     frame_buffer, mpeg4asp_dec->sizebuffer, &mpeg4asp_dec->Mpeg4Caps);
        else
    	    eDecRetVal = eMPEG4DQueryInitInfo_oldDX(psMpeg4DecInitInfo,
    				     mpeg4asp_dec->width, mpeg4asp_dec->height, &mpeg4asp_dec->Mpeg4Caps);

        GST_DEBUG("Video info:Level:%d,\t Profile:%d\tWidth:%d, \tHeight:%d",
            psMpeg4DecInitInfo->sStreamInfo.s32Level,psMpeg4DecInitInfo->sStreamInfo.s32Profile,
            psMpeg4DecInitInfo->sStreamInfo.u16ActFrameWidth,psMpeg4DecInitInfo->sStreamInfo.u16ActFrameHeight);

        GST_DEBUG("Min frame buffer count:%d.\n",psMpeg4DecInitInfo->s32MinFrameBufferCount);

    	if (eDecRetVal != E_MPEG4D_SUCCESS) {
    	    GST_ERROR("Function eMPEG4DQueryInitInfo() resulted in failure\n");
    	    GST_ERROR("MPEG4D Error Type : %d\n", eDecRetVal);

    	    /*! Freeing Memory allocated by the Application */
    	    mfw_gst_mpeg4asp_decoder_cleanup(mpeg4asp_dec);
    	    return GST_FLOW_ERROR;
    	}

    	/*!
    	 *   Allocating Memory for MPEG4 Decoder
    	 */
    	if (mfw_gst_mpeg4asp_decoder_allocatememory(psMpeg4DecInitInfo) == -1) {
    	    GST_ERROR("\nUnable to allocate memory for Mpeg4 Decoder\n");

    	    /*! Freeing Memory allocated by the Application */
    	    mfw_gst_mpeg4asp_decoder_cleanup(mpeg4asp_dec);
    	    return GST_FLOW_ERROR;
    	}


    	/* In current version, it should return "E_MPEG4D_SUCCESS", 
    	   and the application don't need to check any other */
    	eDecRetVal = eMPEG4DCreate(psMpeg4DecInitInfo,&(mpeg4asp_dec->Mpeg4FM), &(mpeg4asp_dec->Mpeg4Handle));

    	if (eDecRetVal != E_MPEG4D_SUCCESS) {
    	    /*!  Freeing Memory allocated by the Application */
            GST_ERROR("\neMPEG4DCreate Error\n");
            mfw_gst_mpeg4asp_decoder_freememory(mpeg4asp_dec->Mpeg4DecInitInfo);
     	    mfw_gst_mpeg4asp_decoder_cleanup(mpeg4asp_dec);
    	    return GST_FLOW_ERROR;
    	}

    	/* Allocate memory to hold the quant values, make sure that we round it
    	 * up in the higher side, as non-multiple of 16 will be extended to
    	 * next 16 bits value
    	 */


        /* Init buffer manager for correct working mode.*/
    	BM_INIT((mpeg4asp_dec->bmmode)? BMINDIRECT : BMDIRECT, psMpeg4DecInitInfo->s32MinFrameBufferCount, RENDER_BUFFER_MAX_NUM);

        mpeg4asp_dec->frame_width_padded = psMpeg4DecInitInfo->sStreamInfo.u16PaddedFrameWidth; 
        mpeg4asp_dec->frame_height_padded = psMpeg4DecInitInfo->sStreamInfo.u16PaddedFrameHeight;

    	/* output buffer size */
    	mpeg4asp_dec->outsize = (mpeg4asp_dec->frame_width_padded * mpeg4asp_dec->frame_height_padded * 3) / 2;

    	ret = GST_FLOW_OK;
    	mpeg4asp_dec->Mpeg4DecInitInfo = psMpeg4DecInitInfo;
    	mpeg4asp_dec->init_done = TRUE;

    }


    if (mpeg4asp_dec->is_sfd) { //
        static int is_key_frame = 0;
        GstFlowReturn ret;
        struct sfd_frames_info *pSfd_info = &mpeg4asp_dec->sfd_info;
        is_key_frame = (!GST_BUFFER_FLAG_IS_SET(buffer,GST_BUFFER_FLAG_DELTA_UNIT));
        if (is_key_frame) {
         GST_DEBUG("Sync count = %d.\n",pSfd_info->total_key_frames);
        }
        ret = Strategy_FD(is_key_frame,pSfd_info);

        if (ret == GST_FLOW_ERROR)
        {
            TSManagerSend(mpeg4asp_dec->pTS_Mgr);

            gst_buffer_unref(mpeg4asp_dec->input_buffer);
            return GST_FLOW_OK;
        }
    }

    /* no need while to decode since demuxer need to make sure only one frame data pushed to here */
    result = mfw_gst_mpeg4asp_decframe(mpeg4asp_dec, mpeg4asp_dec->Mpeg4Handle);
    
    if (mpeg4asp_dec->profiling) {
	gettimeofday(&tv_prof3, 0);
	time_before = (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
	time_after = (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
	mpeg4asp_dec->chain_Time += time_after - time_before;
    }
    
    gst_buffer_unref(mpeg4asp_dec->input_buffer);
    GST_DEBUG("Out of function mfw_gst_mpeg4asp_decoder_chain.\n");
    return result;
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4asp_decoder_change_state

DESCRIPTION: this function keeps track of different states of pipeline.

ARGUMENTS PASSED:
        element     -   pointer to element
        transition  -   state of the pipeline

RETURN VALUE:
        GST_STATE_CHANGE_FAILURE    - the state change failed
        GST_STATE_CHANGE_SUCCESS    - the state change succeeded
        GST_STATE_CHANGE_ASYNC      - the state change will happen
                                        asynchronously
        GST_STATE_CHANGE_NO_PREROLL - the state change cannot be prerolled

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static GstStateChangeReturn
mfw_gst_mpeg4asp_decoder_change_state(GstElement * element,
				   GstStateChange transition)
{

    GstStateChangeReturn ret = 0;
    MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec;
    mpeg4asp_dec = MFW_GST_MPEG4ASP_DECODER(element);
    sMpeg4DecInitInfo *Mpeg4DecInitInfo = NULL;
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
	{

	    mpeg4asp_dec->input_buffer = NULL;
	    mpeg4asp_dec->sizebuffer = 0;
	    mpeg4asp_dec->eos = 0;
	    mpeg4asp_dec->caps_set = FALSE;
	    //mpeg4asp_dec->pf_handle.pf_initdone = FALSE;
	    mpeg4asp_dec->Time = 0;
	    mpeg4asp_dec->chain_Time = 0;
	    mpeg4asp_dec->no_of_frames = 0;
	    mpeg4asp_dec->avg_fps_decoding = 0.0;
	    mpeg4asp_dec->no_of_frames_dropped = 0;
        bm_get_buf_init = FALSE;
	}
	break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
	{
	    mpeg4asp_dec->decoded_frames = 0;
	    mpeg4asp_dec->pffirst = 1;
	    mpeg4asp_dec->cur_buf = 0;
	    mpeg4asp_dec->send_newseg = FALSE;
	    mpeg4asp_dec->next_ts = 0;
	}
	break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	break;
    default:
	break;
    }

    ret = parent_class->change_state(element, transition);

    switch (transition) {
	float avg_mcps = 0, avg_plugin_time = 0, avg_dec_time = 0;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:

	mpeg4asp_dec->send_newseg = FALSE;
	mpeg4asp_dec->cur_buf = 0;
	mpeg4asp_dec->pffirst = 1;
	Mpeg4DecInitInfo = mpeg4asp_dec->Mpeg4DecInitInfo;


    /*! Freeing Memory allocated by the Application */

    mfw_gst_mpeg4asp_decoder_freememory(mpeg4asp_dec->Mpeg4DecInitInfo);
	mfw_gst_mpeg4asp_decoder_cleanup(mpeg4asp_dec);

    if (mpeg4asp_dec->profiling) {

	    g_print("PROFILE FIGURES OF MPEG4 DECODER PLUGIN");
	    g_print("\nTotal decode time is                   %ldus",
		    mpeg4asp_dec->Time);
	    g_print("\nTotal plugin time is                   %ldus",
		    mpeg4asp_dec->chain_Time);
	    g_print("\nTotal number of frames decoded is      %d",
		    mpeg4asp_dec->no_of_frames);
	    g_print("\nTotal number of frames dropped is      %d\n",
		    mpeg4asp_dec->no_of_frames_dropped);

	    if (mpeg4asp_dec->frame_rate != 0) {
		avg_mcps = ((float) mpeg4asp_dec->Time * PROCESSOR_CLOCK /
			    (1000000 *
			     (mpeg4asp_dec->no_of_frames -
			      mpeg4asp_dec->no_of_frames_dropped)))
		    * mpeg4asp_dec->frame_rate;
		g_print("\nAverage decode MCPS is               %f",
			avg_mcps);

		avg_mcps =
		    ((float) mpeg4asp_dec->chain_Time * PROCESSOR_CLOCK /
		     (1000000 *
		      (mpeg4asp_dec->no_of_frames -
		       mpeg4asp_dec->no_of_frames_dropped)))
		    * mpeg4asp_dec->frame_rate;
		g_print("\nAverage plug-in MCPS is               %f",
			avg_mcps);
	    } else {
		g_print
		    ("enable the Frame Rate property of the decoder to get the MCPS \
               ..... \n ! mfw_mpeg4aspdecoder framerate=value ! .... \
               \n Note: value denotes the framerate to be set");
	    }



	    avg_dec_time =
		((float) mpeg4asp_dec->Time) / mpeg4asp_dec->no_of_frames;
	    g_print("\nAverage decoding time is               %fus",
		    avg_dec_time);
	    avg_plugin_time =
		((float) mpeg4asp_dec->chain_Time) / mpeg4asp_dec->no_of_frames;
	    g_print("\nAverage plugin time is                 %fus\n",
		    avg_plugin_time);

	    mpeg4asp_dec->Time = 0;
	    mpeg4asp_dec->chain_Time = 0;
	    mpeg4asp_dec->no_of_frames = 0;
	    mpeg4asp_dec->avg_fps_decoding = 0.0;
	    mpeg4asp_dec->no_of_frames_dropped = 0;
	}


	mpeg4asp_dec->input_buffer = NULL;
	mpeg4asp_dec->sizebuffer = 0;
	mpeg4asp_dec->eos = 0;
	mpeg4asp_dec->caps_set = FALSE;
	//mpeg4asp_dec->pf_handle.pf_initdone = FALSE;
	mpeg4asp_dec->caps_set = FALSE;
	mpeg4asp_dec->init_done = 0;
    resyncTSManager(mpeg4asp_dec->pTS_Mgr, TSM_TIMESTAMP_NONE, MODE_AI);

    /* Cleanup the codec_data buffer */
    if (mpeg4asp_dec->codec_data){
      gst_buffer_unref(mpeg4asp_dec->codec_data);
      mpeg4asp_dec->codec_data = NULL;
      mpeg4asp_dec->codec_data_len = 0;
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
FUNCTION:		mfw_gst_mpeg4asp_decoder_sink_event

DESCRIPTION:	This functions handles the events that triggers the
				sink pad of the mpeg4asp decoder element.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -	if event is sent to sink properly
	    FALSE	   -	if event is not sent to sink properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
  static gboolean
mfw_gst_mpeg4asp_decoder_sink_event(GstPad * pad, GstEvent * event)
{
  GstFlowReturn result = GST_FLOW_OK;
  gboolean ret = TRUE;
  MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec;
  GstBuffer *outbuffer;
  guint8 *outdata;
  eMpeg4DecRetType eDecRetVal = E_MPEG4D_FAILURE;
  guint size;
  GstCaps *src_caps = NULL;
  gchar *hw_qp;
  gint8 *quant;
  unsigned short row, col;
  gint cur_buf = 0;
  GstFormat format;

  mpeg4asp_dec = MFW_GST_MPEG4ASP_DECODER(GST_PAD_PARENT(pad));

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
      {
        GstFormat format;
        gint64 start, stop, position;
        gdouble rate;

        gst_event_parse_new_segment(event, NULL, &rate, &format,
            &start, &stop, &position);

        GST_DEBUG(" start = %" GST_TIME_FORMAT, GST_TIME_ARGS(start));
        GST_DEBUG(" stop = %" GST_TIME_FORMAT, GST_TIME_ARGS(stop));

        GST_DEBUG(" position in mpeg4asp  =%" GST_TIME_FORMAT,
            GST_TIME_ARGS(position));

        if (GST_FORMAT_TIME == format) {
          mpeg4asp_dec->decoded_frames = (gint32) (start * (gfloat) (mpeg4asp_dec->frame_rate) / GST_SECOND);	
        } else {
          mpeg4asp_dec->send_newseg = TRUE;
        }
        resyncTSManager(mpeg4asp_dec->pTS_Mgr, start, MODE_AI);
        break;
      }

    case GST_EVENT_EOS:
      {
        sMpeg4DecYCbCrBuffer *psOutBuffer = &mpeg4asp_dec->sOutBuffer;
        eMpeg4DecRetType eDecRetVal = E_MPEG4D_FAILURE;

        GST_INFO ("got EOS");
        mpeg4asp_dec->eos = 1;

        /* Flush the frames reserved by decoder */
        eMPEG4DFlushFrame(mpeg4asp_dec->Mpeg4Handle);
        eDecRetVal = eMPEG4DGetOutputFrame(mpeg4asp_dec->Mpeg4Handle, &psOutBuffer); 
        if (eDecRetVal == E_MPEG4D_SUCCESS) {
          GST_DEBUG("Got reserved buffer.\n");
          BM_RENDER_BUFFER(mpeg4asp_dec->sOutBuffer.pu8YBuf, mpeg4asp_dec->srcpad, result, mpeg4asp_dec->next_ts, 0);
        }
        GST_WARNING("total frames :%d, dropped frames: %d.\n",mpeg4asp_dec->sfd_info.total_frames,
            mpeg4asp_dec->sfd_info.dropped_frames);

       break;
      }
   default:
      {
        break;
      }

  }

  ret = gst_pad_event_default(pad, event);
  return ret;
}


/*=============================================================================
FUNCTION:   mfw_gst_mpeg4asp_decoder_src_event

DESCRIPTION: This functions handles the events that triggers the
			 source pad of the mpeg4asp decoder element.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
	    FALSE	   -	if event is not sent to src properly
        TRUE       -	if event is sent to src properly
PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static gboolean
mfw_gst_mpeg4asp_decoder_src_event(GstPad * pad, GstEvent * event)
{
    gboolean res;

    MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec =
	        MFW_GST_MPEG4ASP_DECODER(gst_pad_get_parent(pad));

    if (mpeg4asp_dec == NULL) {
	GST_DEBUG_OBJECT(mpeg4asp_dec, "no decoder, cannot handle event");
	gst_event_unref(event);
	return FALSE;
    }

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEEK:
	res = gst_pad_push_event(mpeg4asp_dec->sinkpad, event);
	break;
	/* judge the timestamp from system time */
    case GST_EVENT_QOS:
    {
        struct sfd_frames_info *pSfd_info = &mpeg4asp_dec->sfd_info;
		gdouble proportion;
		GstClockTimeDiff diff;
		GstClockTime timestamp;

		gst_event_parse_qos(event, &proportion, &diff, &timestamp);

		if (diff >= 0) {
            if (mpeg4asp_dec->is_sfd) {
                GST_QOS_EVENT_HANDLE(pSfd_info,diff,mpeg4asp_dec->frame_rate);
            }
            else {
			#if 0
                eMPEG4DParameter eParaName = E_MPEG4_PARA_SKIP_BNP_FRAME;
            	int skip;
                eMPEG4DGetParameter(mpeg4asp_dec->Mpeg4Handle, eParaName, &skip);
                /* Drop B&P frames if later more than 30 millisecond. */
                if (/*(!skip)  && */ ( diff / GST_MSECOND) > 30) { 
                    GST_WARNING("Disable the B&P frames.\n");
                    skip = 1;
                    eMPEG4DSetParameter(mpeg4asp_dec->Mpeg4Handle, eParaName, &skip);
                }
			#endif
                
            }

		} else {
    		if (mpeg4asp_dec->is_sfd) {
    		    GST_DEBUG
    			("the time of decoding is before the system, it is OK\n");
            }
            else
            {
			#if 0
                eMPEG4DParameter eParaName = E_MPEG4_PARA_SKIP_BNP_FRAME;
            	int skip;
                eMPEG4DGetParameter(mpeg4asp_dec->Mpeg4Handle, eParaName, &skip);
                {
                    GST_DEBUG("Enable the B&P frames.\n");
                    skip = 0;
                    eMPEG4DSetParameter(mpeg4asp_dec->Mpeg4Handle, eParaName, &skip);

                }
			#endif
            }
		}
		res = gst_pad_push_event(mpeg4asp_dec->sinkpad, event);
	    break;
    }
    case GST_EVENT_NAVIGATION:
	/* Forward a navigation event unchanged */
    default:
	res = gst_pad_push_event(mpeg4asp_dec->sinkpad, event);
	break;
    }

    gst_object_unref(mpeg4asp_dec);
    return res;

}

/*=============================================================================
FUNCTION:               src_templ

DESCRIPTION:            Template to create a srcpad for the decoder.

ARGUMENTS PASSED:       None.


RETURN VALUE:           a GstPadTemplate


PRE-CONDITIONS:  	    None

POST-CONDITIONS:   	    None

IMPORTANT NOTES:   	    None
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
	gchar *fmts[] = { "YV12", "I420", "Y42B", NULL };
	guint n;

	caps = gst_caps_new_simple("video/x-raw-yuv",
				   "format", GST_TYPE_FOURCC,
				   GST_MAKE_FOURCC('I', '4', '2', '0'),
				   "width", GST_TYPE_INT_RANGE, 16, 4096,
				   "height", GST_TYPE_INT_RANGE, 16, 4096,
				   NULL);
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
FUNCTION:   mfw_gst_mpeg4asp_decoder_set_caps

DESCRIPTION:    this function handles the link with other plug-ins and used for
                capability negotiation  between pads

ARGUMENTS PASSED:
        pad        -    pointer to GstPad
        caps       -    pointer to GstCaps

RETURN VALUE:
        TRUE       -    if capabilities are set properly
        FALSE      -    if capabilities are not set properly
PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_mpeg4asp_decoder_set_caps(GstPad * pad, GstCaps * caps)
{
    MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    mpeg4asp_dec = MFW_GST_MPEG4ASP_DECODER(GST_OBJECT_PARENT(pad));

    const gchar *mime, *video_type="video/x-divx";
    gint32 frame_rate_de = 0;
    gint32 frame_rate_nu = 0;

    GValue *codec_data_buf = NULL;

    mime = gst_structure_get_name(structure);

    gst_structure_get_fraction(structure, "framerate", &mpeg4asp_dec->framerate_n,
			       &mpeg4asp_dec->framerate_d);

    if (mpeg4asp_dec->framerate_d) {
    	mpeg4asp_dec->frame_rate = (gfloat)mpeg4asp_dec->framerate_n/(gfloat)mpeg4asp_dec->framerate_d;
    }
    
    GST_DEBUG(" Frame Rate = %f \n", mpeg4asp_dec->frame_rate);
#ifdef PADDED_OUTPUT
    gst_structure_get_int(structure, "width",
			  &mpeg4asp_dec->width);
    gst_structure_get_int(structure, "height",
			  &mpeg4asp_dec->height);

    GST_DEBUG(" Frame Width  = %d\n", mpeg4asp_dec->width);

    GST_DEBUG(" Frame Height = %d\n", mpeg4asp_dec->height);

    mpeg4asp_dec->frame_width_padded = (mpeg4asp_dec->width+15)/16*16+32; 
    mpeg4asp_dec->frame_height_padded = (mpeg4asp_dec->height+15)/16*16+32;
#endif /* OUTPUT_BUFFER_CHANGES */
    if(!strcmp(mime, video_type))
    {
		guint32 version = 0;
		/* divx3 version need license, so we had to set a flag to decode 
		 * divx3 mpeg4 bitstream */
		gst_structure_get_int(structure, "divxversion", &version);
		if(version == 3)
           mpeg4asp_dec->control_flag = 1;
    }

    /* Handle the codec_data information */
    codec_data_buf = (GValue *) gst_structure_get_value(structure, "codec_data");
    if ( (NULL != codec_data_buf) && (mpeg4asp_dec->codec_data == NULL)) {
        guint8 *hdrextdata;
        gint i;
        mpeg4asp_dec->codec_data = gst_buffer_ref(gst_value_get_buffer(codec_data_buf));
        GST_DEBUG ("MPEG4 check for codec data \n");
        mpeg4asp_dec->codec_data_len = GST_BUFFER_SIZE(mpeg4asp_dec->codec_data);
       /* 
        * Enable the SFD strategy only 
        *  if the demuxer is compatible to pass the key frames 
        */
       // mpeg4asp_dec->is_sfd = FALSE;
        GST_DEBUG("\n>>MPEG4 decoder: Codec specific data length is %d\n",mpeg4asp_dec->codec_data_len);
        GST_DEBUG("MPEG4 codec data is \n");
        hdrextdata = GST_BUFFER_DATA(mpeg4asp_dec->codec_data);
        for(i=0;i<mpeg4asp_dec->codec_data_len;i++)
            GST_DEBUG("%x ",hdrextdata[i]);
        GST_DEBUG("\n");

    }

#if 0    
    if (!gst_pad_set_caps(mpeg4asp_dec->srcpad, caps)) {
	return FALSE;
    }
#endif    
    return TRUE;
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4asp_decoder_init

DESCRIPTION:This function creates the pads on the elements and register the
			function pointers which operate on these pads.

ARGUMENTS PASSED:
        pointer the mpeg4asp_decoder element handle.

RETURN VALUE:
        None

PRE-CONDITIONS:
        _base_init and _class_init are called

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_mpeg4asp_decoder_init(MFW_GST_MPEG4ASP_DECODER_INFO_T * mpeg4asp_dec)
{
    GstElementClass *klass = GST_ELEMENT_GET_CLASS(mpeg4asp_dec);
    mpeg4asp_dec->init_done = 0;
	mpeg4asp_dec->control_flag = 0;

    mpeg4asp_dec->sinkpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "sink"), "sink");

    gst_pad_set_setcaps_function(mpeg4asp_dec->sinkpad,
				 mfw_gst_mpeg4asp_decoder_set_caps);
    gst_pad_set_chain_function(mpeg4asp_dec->sinkpad,
			       mfw_gst_mpeg4asp_decoder_chain);
    gst_pad_set_event_function(mpeg4asp_dec->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_mpeg4asp_decoder_sink_event));

    gst_element_add_pad(GST_ELEMENT(mpeg4asp_dec), mpeg4asp_dec->sinkpad);

    mpeg4asp_dec->srcpad = gst_pad_new_from_template(src_templ(), "src");
    gst_pad_set_event_function(mpeg4asp_dec->srcpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_mpeg4asp_decoder_src_event));

    gst_element_add_pad(GST_ELEMENT(mpeg4asp_dec), mpeg4asp_dec->srcpad);
    //mpeg4asp_dec->pf_handle.deblock = DBL_DISABLE;
    mpeg4asp_dec->profiling = FALSE;

    mpeg4asp_dec->framerate_n = 25;
    mpeg4asp_dec->framerate_d = 1;
    mpeg4asp_dec->frame_rate = 25;
    INIT_SFD_INFO(&mpeg4asp_dec->sfd_info);
    mpeg4asp_dec->is_sfd = TRUE;

    mpeg4asp_dec->pTS_Mgr = createTSManager(0);

#define MFW_GST_MPEG4ASP_DECODER_PLUGIN VERSION
    PRINT_CORE_VERSION(eMPEG4DCodecVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_MPEG4ASP_DECODER_PLUGIN);

    bm_get_buf_init = FALSE;

    INIT_DEMO_MODE(eMPEG4DCodecVersionInfo(), mpeg4asp_dec->demo_mode);
}

/*======================================================================================
FUNCTION:           mfw_gst_mpeg4aspdec_finalize

DESCRIPTION:        Class finalized

ARGUMENTS PASSED:   object     - pointer to the elements object

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
========================================================================================*/
static void
mfw_gst_mpeg4aspdec_finalize (GObject * object)
{
	MFW_GST_MPEG4ASP_DECODER_INFO_T *mpeg4asp_dec =
	MFW_GST_MPEG4ASP_DECODER(object);
    destroyTSManager(mpeg4asp_dec->pTS_Mgr);
    GST_DEBUG (">>MPEG4ASP DEC: class finalized.\n");
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4asp_decoder_class_init

DESCRIPTION:Initialise the class only once (specifying what signals,
            arguments and virtual functions the class has and setting up
            global state)
ARGUMENTS PASSED:
       	klass   - pointer to mpeg4asp element class

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
mfw_gst_mpeg4asp_decoder_class_init(MFW_GST_MPEG4ASP_DECODER_CLASS_T * klass)
{
    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
    gobject_class->set_property = mfw_gst_mpeg4asp_decoder_set_property;
    gobject_class->get_property = mfw_gst_mpeg4asp_decoder_get_property;
    gstelement_class->change_state = mfw_gst_mpeg4asp_decoder_change_state;
    gobject_class->finalize = mfw_gst_mpeg4aspdec_finalize;

    g_object_class_install_property(gobject_class, PROF_ENABLE,
				    g_param_spec_boolean("profiling", "Profiling", "enable time profiling of the plug-in \
        and the decoder", FALSE, G_PARAM_READWRITE));

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
	
}

/*=============================================================================
FUNCTION:  mfw_gst_mpeg4asp_decoder_base_init

DESCRIPTION:
            mpeg4aspdecoder element details are registered with the plugin during
            _base_init ,This function will initialise the class and child
            class properties during each new child class creation


ARGUMENTS PASSED:
        Klass   -   pointer to mpeg4asp decoder plug-in class
        g_param_spec_float("framerate", "FrameRate", 
        "gets the framerate at which the input stream is to be displayed",
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_mpeg4asp_decoder_base_init(MFW_GST_MPEG4ASP_DECODER_CLASS_T * klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, src_templ());
    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&sink_factory));

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "mpeg4 video decoder",
        "Codec/Decoder/Video", "Decode compressed mpeg4 video to raw data");
}

/*=============================================================================
FUNCTION: mfw_gst_mpeg4asp_decoder_get_type

DESCRIPTION:    intefaces are initiated in this function.you can register one
                or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:
            None

RETURN VALUE:
                 A numerical value ,which represents the unique identifier of this
            element(mpeg4aspdecoder)

PRE-CONDITIONS:
            None

POST-CONDITIONS:
            None

IMPORTANT NOTES:
            None
=============================================================================*/

GType mfw_gst_mpeg4asp_decoder_get_type(void)
{
    static GType mpeg4asp_decoder_type = 0;

    if (!mpeg4asp_decoder_type) {
	static const GTypeInfo mpeg4asp_decoder_info = {
	    sizeof(MFW_GST_MPEG4ASP_DECODER_CLASS_T),
	    (GBaseInitFunc) mfw_gst_mpeg4asp_decoder_base_init,
	    NULL,
	    (GClassInitFunc) mfw_gst_mpeg4asp_decoder_class_init,
	    NULL,
	    NULL,
	    sizeof(MFW_GST_MPEG4ASP_DECODER_INFO_T),
	    0,
	    (GInstanceInitFunc) mfw_gst_mpeg4asp_decoder_init,
	};
	mpeg4asp_decoder_type = g_type_register_static(GST_TYPE_ELEMENT,
						    "MFW_GST_MPEG4ASP_DECODER_INFO_T",
						    &mpeg4asp_decoder_info,
						    0);
    }
    GST_DEBUG_CATEGORY_INIT(mfw_gst_mpeg4asp_decoder_debug,
			    "mfw_mpeg4aspdecoder", 0,
			    "FreeScale's MPEG4ASP Decoder's Log");
    return mpeg4asp_decoder_type;
}

/*=============================================================================
FUNCTION:   plugin_init

DESCRIPTION:    special function , which is called as soon as the plugin or
                element is loaded and information returned by this function
                will be cached in central registry

ARGUMENTS PASSED:
        plugin     -    pointer to container that contains features loaded
                        from shared object module

RETURN VALUE:
        return TRUE or FALSE depending on whether it loaded initialized any
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
    return gst_element_register(plugin, "mfw_mpeg4aspdecoder",
				GST_RANK_PRIMARY,
				MFW_GST_TYPE_MPEG4ASP_DECODER);
}

FSL_GST_PLUGIN_DEFINE("mpeg4dec", "mpeg4 video decoder", plugin_init);

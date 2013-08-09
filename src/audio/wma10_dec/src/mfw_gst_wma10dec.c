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
 * Module Name:    mfw_gst_wma10dec.c
 *
 * Description:    Implementation of wma10 decode plugin in.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */


/*=============================================================================
					INCLUDE	FILES
=============================================================================*/
#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <stdlib.h>
#include <memory.h>
#include "wma10-dec/wma10_dec_interface.h"
#include "mfw_gst_wma10dec.h"

#include "mfw_gst_utils.h"

/*=============================================================================
					LOCAL CONSTANTS
=============================================================================*/
#ifndef IN_BUFFER_SIZE_
#define	IN_BUFFER_SIZE	        4096
#endif

#ifndef _WMA10D_FRAME_SIZE_
#define WMA10D_FRAME_SIZE       4096
#endif

#define DECOPT_CHANNEL_DOWNMIXING      0x00000001

#define	GST_TAG_MFW_WMA_CHANNELS		"channels"
#define GST_TAG_MFW_WMA_SAMPLING_RATE	"sampling_frequency"


/*=============================================================================
					LOCAL TYPEDEFS (STRUCTURES,	UNIONS,	ENUMS)
=============================================================================*/
enum {
    ID_0,
    ID_PROFILE_ENABLE
};

/*=============================================================================
					LOCAL MACROS
=============================================================================*/
/* used	for	debugging */
#define	GST_CAT_DEFAULT	mfw_gst_wma10dec_debug

/* the clock in MHz for iMX31, to be changed for other platforms */
#define PROCESSOR_CLOCK 532


/*=============================================================================
				LOCAL VARIABLES
=============================================================================*/

/* source pad properties of wma10decoder element */

static GstStaticPadTemplate mfw_gst_wma10dec_src_template_factory =
    GST_STATIC_PAD_TEMPLATE("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
        "audio/x-raw-int, "
		"endianness	= (int)	" G_STRINGIFY(G_BYTE_ORDER) ","
        "signed	= (boolean)	true, "
        "width = (int) {8, 16, 24, 32}, "
        "depth = (int) {8, 16, 24, 32}, "
        "rate =	(int)  {8000, 11025, 12000, 16000, 22050, 24000, 32000,	44100, 48000, 64000, 88200, 96000},"
		"channels =	(int) [	1, 8 ]"));

/* sink pad properties	of wma10decoder element */

static GstStaticPadTemplate mfw_gst_wma10dec_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE("sink",
    GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS("audio/x-wma, "
	"wmaversion	= (int)	[ 1, 4 ], "
	"bitrate = [4000 ,	10000000],"
	"rate =	(int)  {8000, 11025, 12000, 16000, 22050, 24000, 32000,	44100, 48000, 64000, 88200,96000},"
    "channels =	(int) [	1, 8 ]"));


/* used	in change state	function */
static GstElementClass *parent_class = NULL;

/*=============================================================================
					LOCAL FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC                       (mfw_gst_wma10dec_debug);
static GType mfw_gst_wma10dec_get_type          (void);
static void mfw_gst_wma10dec_base_init          (gpointer);
static void mfw_gst_wma10dec_class_init         (MFW_GST_WMA10DEC_INFO_CLASS_T *);
static void mfw_gst_wma10dec_init               (MFW_GST_WMA10DEC_INFO_T *);
static gboolean mfw_gst_wma10dec_set_caps       (GstPad *, GstCaps *);
static gboolean mfw_gst_wma10dec_sink_event     (GstPad *, GstEvent *);
static GstFlowReturn mfw_gst_wma10dec_chain     (GstPad *, GstBuffer *);
static GstFlowReturn mfw_gst_wma10dec_dframe    (MFW_GST_WMA10DEC_INFO_T *);
static void *get_buffer                         (gint);
static gboolean mfw_gst_wma10dec_src_event      (GstPad *, GstEvent *);

static WMAD_UINT32 mfw_gst_wma10dec_callback    (void *, WMAD_UINT64,
					                                WMAD_UINT32 *, unsigned char **,
					                                void *, WMAD_UINT32 *);
static GstStateChangeReturn mfw_gst_wma10dec_change_state
                                                (GstElement *, GstStateChange);

/*=============================================================================
					GLOBAL VARIABLES
=============================================================================*/
/* None	*/

/*=============================================================================
					LOCAL FUNCTIONS
=============================================================================*/
/*=============================================================================
FUNCTION:	        get_buffer

DESCRIPTION:        allocate memory	with alignment to word boundary

ARGUMENTS PASSED:
		int	size -	size of	memory in bytes

RETURN VALUE:
		pointer	to allocated memory

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static void *get_buffer(gint size)
{
    gpointer ptr_buf = NULL;
    ptr_buf = MM_MALLOC(size);

    if (NULL == ptr_buf) {
    	GST_ERROR("\n can not allocate memory \n");
    }
    return ptr_buf;
}

/*==========================================================================
FUNCTION:	    mfw_gst_wma10dec_callback

DESCRIPTION:	This function is called	by the decoder whenever	it runs	out	of
		        the	current	bit	stream buffer.

ARGUMENTS PASSED:
		state:              present state of the decoder.
        offset:             offset from where the input buffer is required.
        num_bytes:          number of bytes requested by decoder.
        ppData:             Pointer to the input buffer given by application.
        pAppcontext:        Application specific context.
        compress_payload:   NA

RETURN VALUE:
		cWMA_NoErr      -   success
        cWMA_Failed     -   failure

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
WMAD_UINT32 mfw_gst_wma10dec_callback (void *state, WMAD_UINT64 offset,
                                        WMAD_UINT32 *num_bytes,
                                        WMAD_UINT8  **ppData, void *pAppContext,
                                        WMAD_UINT32 *compress_payload)
{
    GstBuffer *buf                  = NULL;
    MFW_GST_WMA10DEC_INFO_T *pApp   = (MFW_GST_WMA10DEC_INFO_T *)pAppContext;
    gint inbuffsize = 0;
    gint BlockAlign = pApp->block_align;

    if (pApp->fillbuf){
            gst_buffer_unref(pApp->fillbuf);
            pApp->fillbuf = NULL;
        }

    inbuffsize = gst_adapter_available(pApp->pAdapter);

    if (1 == pApp->end_of_stream && inbuffsize == 0) {
    	*ppData = NULL;
    	*num_bytes = 0;

	    return cWMA_NoMoreFrames;
    }else{
         if (inbuffsize && inbuffsize>=BlockAlign){
                if (inbuffsize%BlockAlign !=0)
                    *num_bytes = inbuffsize - inbuffsize%BlockAlign;
                else
                    *num_bytes = inbuffsize;
        
            pApp->fillbuf = gst_adapter_take_buffer(pApp->pAdapter,*num_bytes);
            *ppData = GST_BUFFER_DATA(pApp->fillbuf);

        *compress_payload = 0;

         GST_DEBUG("callback %p size %d\n", *ppData, *num_bytes);
        return cWMA_NoErr;
        }
        else{
            *ppData = NULL;
	        *num_bytes = 0;
        //   gst_adapter_clear(pApp->pAdapter); //add this for bugfix of conti, 
                                                //but may exist potential problem if clear remain data.
	    return cWMA_NoMoreDataThisTime;
        }
}
}

/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_dframe

DESCRIPTION:	decode the	data by	calling	 eWMADecodeFrame function
				i.e. library function ,	this library function is receiving the
				data from the call back	function . After decoding the  decoded
				data is	pushed to the sink element.

ARGUMENTS PASSED:
		wmadec_struct  -	pointer	to wmadeocder element structure

RETURN VALUE:
		GST_FLOW_OK             -   Data passing was ok.
        Other error based on return values of functions.

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static GstFlowReturn mfw_gst_wma10dec_dframe(MFW_GST_WMA10DEC_INFO_T *wmadec_struct)
{
    WMAD_INT32 sample_rate  = 0;
    WMAD_INT32 num_channels = 0;
    WMAD_UINT32 num_samples = 0;
    GstBuffer *outbuffer    = NULL;
    WMAD_UINT32 outBufSize  = 0;
    gint16 *outdata         = NULL;
    gint64 time_duration    = 0;
    gint total_samples      = 0;
    GstCaps *src_caps       = NULL;
    GstCaps *caps           = NULL;
    GstFlowReturn result    = GST_FLOW_OK;
    GstEvent *event         = NULL;
    tWMAFileStatus retval   = cWMA_NoErr;

    struct timeval tv_prof, tv_prof1;
    long time_before = 0, time_after = 0;

{
    if (wmadec_struct->demo_mode == 2)
    {
        return GST_FLOW_ERROR;
    }

    DEMO_LIVE_CHECK(wmadec_struct->demo_mode,
        wmadec_struct->time_offset,wmadec_struct->srcpad);

}


    num_channels = wmadec_struct->dec_param->us16Channels;
    if(num_channels > 2) //downmix all multi-channel to 2.
        num_channels = 2;

    while (1) {

        /*The decoding is not continued further, if the FLUSH event is not yet
            handled or the decoder is not initialized back after flush during seek */
        if((wmadec_struct->flush_complete == FALSE) ||
                                            (wmadec_struct->is_decode_init == 0))
            return GST_FLOW_OK;

        /* allocating memory for output data */
        outBufSize = wmadec_struct->dec_param->us32OutputBufSize;
	    wmadec_struct->output_buff = get_buffer(outBufSize);

        if (wmadec_struct->profile) {
    	    gettimeofday(&tv_prof, 0);
	    }

        /* Codec call for decoding of a frame.*/

	    retval = eWMADecodeFrame(wmadec_struct->dec_config, wmadec_struct->dec_param,
				                    wmadec_struct->output_buff, outBufSize);
            GST_DEBUG("Decoder result, ret=%d.\n",retval);
	    if (wmadec_struct->profile) {
    	    gettimeofday(&tv_prof1, 0);
	        time_before = (tv_prof.tv_sec * 1000000) + tv_prof.tv_usec;
	        time_after = (tv_prof1.tv_sec * 1000000) + tv_prof1.tv_usec;
	        wmadec_struct->Time += time_after - time_before;

    	    if (retval == cWMA_NoErr) {
		        wmadec_struct->total_frames++;
	        }
            else if (retval != cWMA_NoMoreFrames) {
		        wmadec_struct->no_of_frames_dropped++;
	        }
	    }

            if (retval ==  cWMA_NoMoreDataThisTime){
                if(wmadec_struct->output_buff != NULL) {
                    MM_FREE(wmadec_struct->output_buff);
                    wmadec_struct->output_buff = NULL;
                }                
                 return GST_FLOW_OK;
            }
        /* Once the decoding is done, set the caps on the output pad and prepare
            to send the data to play on audio driver.
        */
		if (!wmadec_struct->caps_set) {

            GstTagList  *list = gst_tag_list_new();
            gchar  *codec_name;


            if ((wmadec_struct->format_tag == 0x0160) || (wmadec_struct->format_tag == 0x0161))
                codec_name = "Standard";
            else if ((wmadec_struct->format_tag == 0x0167) || (wmadec_struct->format_tag == 0x0163))
                codec_name = "Lossless";
            else
                codec_name = "Professional";

            codec_name = g_strdup_printf("WMA version%d, %s, FormatTag:0x%x",wmadec_struct->version, codec_name, wmadec_struct->format_tag);

            gst_tag_list_add(list,GST_TAG_MERGE_APPEND,GST_TAG_AUDIO_CODEC,
                    codec_name,NULL);
            gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
                    (guint)(wmadec_struct->avg_bytes_per_second<<3),NULL);
            gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_WMA_SAMPLING_RATE,
                    (guint)wmadec_struct->samples_per_second,NULL);
            gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_WMA_CHANNELS,
                    (guint)wmadec_struct->channels,NULL);

            gst_element_found_tags(GST_ELEMENT(wmadec_struct),list);

	        caps = gst_caps_new_simple(
                    "audio/x-raw-int", "endianness", G_TYPE_INT,
				    G_BYTE_ORDER, "signed",
				    G_TYPE_BOOLEAN, TRUE,
                    "width", G_TYPE_INT, wmadec_struct->dec_param->us32ValidBitsPerSample,
                    "depth", G_TYPE_INT, wmadec_struct->dec_param->us32ValidBitsPerSample,
                    "rate", G_TYPE_INT,  wmadec_struct->dec_param->us32SamplesPerSec,
                    "channels", G_TYPE_INT, num_channels, NULL);

            g_free(codec_name);


	        gst_pad_set_caps(wmadec_struct->srcpad, caps);
	        gst_caps_unref(caps);
            wmadec_struct->caps_set = TRUE;
    	}
            if (retval == cWMA_NoMoreFrames) {
              
              if(wmadec_struct->output_buff != NULL) {
                      MM_FREE(wmadec_struct->output_buff);
                      wmadec_struct->output_buff = NULL;
                  }
                return GST_FLOW_OK;
            }
	    if (retval == cWMA_NoErr) {
	        src_caps = GST_PAD_CAPS(wmadec_struct->srcpad);

            /* If the decoding was successful, then create an output buffer with the caps
                        as created above and copy the PCM data into this buffer.*/
	        result = gst_pad_alloc_buffer(wmadec_struct->srcpad,
                                0, outBufSize, src_caps, &outbuffer);

	        if (result != GST_FLOW_OK) {
		        GST_ERROR("\n Could	not	allocate output	buffer\n");
                if(wmadec_struct->output_buff != NULL) {
                    MM_FREE(wmadec_struct->output_buff);
                    wmadec_struct->output_buff = NULL;
                }                
		        return result;
	        }

            /* By default, we are selecting 16 bit PCM samples. This needs to be changed for
                       24 BIT PCM samples. */

	        outdata         = (WMAD_INT16 *) GST_BUFFER_DATA(outbuffer);
	        sample_rate     = wmadec_struct->dec_param->us32SamplesPerSec;
	        //num_channels    = wmadec_struct->dec_param->us16Channels;
	        num_samples     = wmadec_struct->dec_param->us16NumSamples;

	        /* Before sending the first decoded buffer, we need to send the new_segment, so that
                        the basesrc adjusts its timestamp with the new time stamp.*/

	        if (FALSE == wmadec_struct->new_seg_flag) {
		        gint64 start = 0;
                gboolean res = TRUE;
		        event = gst_event_new_new_segment(FALSE, 1.0, GST_FORMAT_TIME,
					                        start, GST_CLOCK_TIME_NONE, start);

		        res = gst_pad_push_event(wmadec_struct->srcpad, event);
		        if (TRUE != res) {
		            GST_ERROR("\n Error	in pushing the event,result	is %d\n", res);
		            if(wmadec_struct->output_buff != NULL) {
                        MM_FREE(wmadec_struct->output_buff);
                        wmadec_struct->output_buff = NULL;
                    }
                    gst_buffer_unref(outbuffer);
                    return GST_FLOW_ERROR;
        	    }
                wmadec_struct->new_seg_flag = TRUE;
	        }

    		/* for mono  sound */
    		if (1 == num_channels) {

        			GST_BUFFER_SIZE(outbuffer) = num_samples * 2;
    		}

    		 /* for stereo sound     */
    		else if ( num_channels >=2 ) {
    			  /* size of output buffer */
    		        GST_BUFFER_SIZE(outbuffer) = num_channels *
                                             num_samples *
                                             wmadec_struct->dec_param->us32ValidBitsPerSample/8;
    		}

    		memcpy(outdata, wmadec_struct->output_buff,  GST_BUFFER_SIZE(outbuffer));

            //time_duration               = (num_samples / sample_rate)*GST_SECOND;
            time_duration		= gst_util_uint64_scale_int(GST_SECOND,num_samples, sample_rate);
    	    /* The timestamp in     nanoseconds     of the data     in the buffer. */
            GST_BUFFER_TIMESTAMP(outbuffer) = wmadec_struct->time_offset;
                wmadec_struct->time_offset += time_duration;



            /* The duration in nanoseconds of the data in the buffer */
            GST_BUFFER_DURATION(outbuffer) = time_duration;

            /* The offset in the source     file of the     beginning of this buffer */
    	    GST_BUFFER_OFFSET(outbuffer) = 0;


            /* pushing the output data to the sink element                  */
    	    result = gst_pad_push(wmadec_struct->srcpad, outbuffer);
            if(wmadec_struct->output_buff != NULL) {
                MM_FREE(wmadec_struct->output_buff);
                wmadec_struct->output_buff = NULL;
            }
    	    if (result != GST_FLOW_OK) {
                GST_ERROR("\n Error	in pad_push, result is %d", result);
                return result;
            }

            continue;
        }
        else {
	        GST_ERROR("\n Error during decoding of a frame\n");
            if(wmadec_struct->output_buff != NULL) {
                MM_FREE(wmadec_struct->output_buff);
                wmadec_struct->output_buff = NULL;
            }                
        
	        return GST_FLOW_ERROR;
	    }
    }
    return GST_FLOW_OK;
}


/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_chain

DESCRIPTION:	this is the main plugin function. It gets the input data on its
                sink pad, transforms the data (in this case decodes the data) and
                pushes the data onto src pad for further processing.
				process.

ARGUMENTS PASSED:
		pad		-	pointer	to the pad
		buffer	-	pointer	to GstBuffer

RETURN VALUE:
		GST_FLOW_OK	-	success

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static GstFlowReturn mfw_gst_wma10dec_chain(GstPad *pad, GstBuffer *buffer)
{

    GstFlowReturn retval                    = GST_FLOW_OK;
    gint i_loop                             = 0;
    gint nr                                 = 0;
    GstBuffer *tempBuf                      = NULL;
    long time_before                        = 0;
    long time_after                         = 0;
    tWMAFileStatus status                   = cWMA_NoErr;
    MFW_GST_WMA10DEC_INFO_T *wmadec_info    = NULL;
    wmadec_info                             = MFW_GST_WMA10DEC(GST_PAD_PARENT(pad));
    gint inbuffsize;

    struct timeval tv_prof2, tv_prof3;

    /* Start the _chain Profiling. This information gives the total time consumed
        by the plugin.
    */
    if (wmadec_info->profile) {
	    gettimeofday(&tv_prof2, 0);
    }

    /* first step is initializing the decoder. */
    /*
     * ENGR63488: Move the init part code here in case when seek to the last sevel
     * packets, which the sum size of these pakcets is less than required input data
     * size.
     */
    if (wmadec_info->is_decode_init == 0) {

	wmadec_info->time_offset =  GST_BUFFER_TIMESTAMP(buffer);

    	wmadec_info->dec_config =
                    (WMADDecoderConfig *)get_buffer(sizeof(WMADDecoderConfig));
	    wmadec_info->dec_param  =
                    (WMADDecoderParams *)get_buffer(sizeof(WMADDecoderParams));

        memset(wmadec_info->dec_param, 0, sizeof(WMADDecoderParams));

        if (NULL == wmadec_info->dec_config) {
	        GST_ERROR("could not allocate memory config structure\n");
	        return GST_FLOW_ERROR;
        }

    	/* initializing call back function and other decoder variables */
	    wmadec_info->dec_config->app_swap_buf       = (void *) mfw_gst_wma10dec_callback;
        wmadec_info->dec_param->bIsRawDecoder       = 1;
        wmadec_info->dec_param->us16Version         = wmadec_info->version;
        wmadec_info->dec_param->us16wFormatTag      = wmadec_info->format_tag;
        wmadec_info->dec_param->us16Channels        = wmadec_info->channels;
        wmadec_info->dec_param->us32SamplesPerSec   = wmadec_info->samples_per_second;
        wmadec_info->dec_param->us32AvgBytesPerSec  = wmadec_info->avg_bytes_per_second;

        wmadec_info->dec_param->bDropPacket         = 0;
        wmadec_info->dec_param->nDRCSetting         = 0;
        wmadec_info->dec_param->nDecoderFlags       = 0;
        wmadec_info->dec_param->nDstChannelMask     = 0;
        wmadec_info->dec_param->nInterpResampRate   = 0;
        wmadec_info->dec_param->nMBRTargetStream    = 1;



        if (wmadec_info->channels>2)
        {
        	//add for downmix, currently only downix multi-channel to 2 channel
        	wmadec_info->dec_param->nDecoderFlags       |= DECOPT_CHANNEL_DOWNMIXING;
        	wmadec_info->dec_param->nDstChannelMask     = 0x03;
        }
        wmadec_info->dec_param->us32nBlockAlign     = wmadec_info->block_align;
        wmadec_info->dec_param->us32ChannelMask     = wmadec_info->channel_mask;
        wmadec_info->dec_param->us16EncodeOpt       = wmadec_info->encode_opt;
        wmadec_info->dec_config->pContext           = wmadec_info;
        wmadec_info->dec_param->us16AdvancedEncodeOpt   = wmadec_info->advanced_encode_opt;
        wmadec_info->dec_param->us32AdvancedEncodeOpt2  = wmadec_info->advanced_encode_opt2;
        wmadec_info->dec_param->us32ValidBitsPerSample  = wmadec_info->bits_per_sample;
        wmadec_info->dec_config->sDecodeParams          = wmadec_info->dec_param;

	    /* query for memory     */
        status = eWMADQueryMem(wmadec_info->dec_config);
        if (status != cWMA_NoErr) {
	        GST_ERROR("\n Could	not	Query the Memory, retval=%d\n",status);
	        return GST_FLOW_ERROR;
        }

	    nr = wmadec_info->dec_config->sWMADMemInfo.s32NumReqs;
	    for (i_loop = 0; i_loop < nr; i_loop++) {
	        wmadec_info->mem = &(wmadec_info->dec_config->sWMADMemInfo.sMemInfoSub[i_loop]);
	        if (wmadec_info->mem->s32WMADType == WMAD_FAST_MEMORY) {

	    	    wmadec_info->mem->app_base_ptr = get_buffer(wmadec_info->mem->s32WMADSize);

		        if (NULL == wmadec_info->mem->app_base_ptr) {
		            GST_ERROR("\n Could	not	allocate fast memory for the Decoder\n");
		            return GST_FLOW_ERROR;
                }
            }
            else {
		        wmadec_info->mem->app_base_ptr = get_buffer(wmadec_info->mem->s32WMADSize);

                if (NULL == wmadec_info->mem->app_base_ptr) {
		            GST_ERROR("\n Could	not	allocate  memory for the Decoder\n");
        	        return GST_FLOW_ERROR;
		        }
	        }
	    }

	    status = eInitWMADecoder(wmadec_info->dec_config, wmadec_info->dec_param, NULL, 0);

        if (status != cWMA_NoErr) {
	        GST_ERROR("	decoder	initialization failed with return value	%d\n",status);
	        return GST_FLOW_ERROR;
        }
	    wmadec_info->is_decode_init = 1;
    }

    /* If we are coming to the function for the first time, we need to buffer
        around 16k worth of data. This is because of the requirement of the
        codec to raise that amount of data during the callback.
    */

   #if 0
    if (NULL == wmadec_info->input_buffer) {
	    wmadec_info->input_buffer = buffer;
	    return GST_FLOW_OK;
    }
    else {
	    tempBuf = gst_buffer_join(wmadec_info->input_buffer, buffer);
	    wmadec_info->input_buffer = tempBuf;
    }

    if ((4 * IN_BUFFER_SIZE) > GST_BUFFER_SIZE(wmadec_info->input_buffer)) {
        return GST_FLOW_OK;
    }
   #endif

    gst_adapter_push(wmadec_info->pAdapter, buffer);

    if ((inbuffsize = gst_adapter_available(wmadec_info->pAdapter))< (4 * IN_BUFFER_SIZE))
      return GST_FLOW_OK;

    wmadec_info->inbuffsize = inbuffsize;
    /* If we have 4 frames worth of data, then start doing the decoding process. */
    /* Decode all the frames that are present in the present set of buffers. */
    retval = mfw_gst_wma10dec_dframe(wmadec_info);
    if (retval != GST_FLOW_OK) {
    	GST_ERROR("\nError in decoding of frames\n");
	    return retval;
    }

    /* Stop the profiler. */
    if (wmadec_info->profile) {
	    gettimeofday(&tv_prof3, 0);
	    time_before = (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
	    time_after = (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
	    wmadec_info->chain_Time += time_after - time_before;
    }
    return GST_FLOW_OK;
}

/*=============================================================================
FUNCTION:   	mfw_gst_wma10dec_src_event

DESCRIPTION:	Handles an event on the source pad.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -	if event is sent to source properly
	    FALSE	   -	if event is not sent to source properly

PRE-CONDITIONS:    None

POST-CONDITIONS:   None

IMPORTANT NOTES:   None
=============================================================================*/
static gboolean mfw_gst_wma10dec_src_event(GstPad *src_pad, GstEvent *event)
{
    gboolean res = TRUE;
    MFW_GST_WMA10DEC_INFO_T *wmadec_struct = MFW_GST_WMA10DEC(gst_pad_get_parent(src_pad));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEEK:
	{
	    res = gst_pad_push_event(wmadec_struct->sinkpad, event);
	    if (TRUE != res) {
		    GST_ERROR ("\n Error in pushing the event,result is %d\n", res);
            gst_object_unref(wmadec_struct);
		    return res;
	    }
	    break;
	}
    default:
        res = gst_pad_event_default(src_pad, event);
        break;
    }
    gst_object_unref(wmadec_struct);
    return res;
}

/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_sink_event

DESCRIPTION:	send an	event to source	 pad of	wmadecoder element

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
static gboolean mfw_gst_wma10dec_sink_event(GstPad *pad, GstEvent *event)
{

    MFW_GST_WMA10DEC_INFO_T *wmadec_struct  = MFW_GST_WMA10DEC(GST_PAD_PARENT(pad));
    gboolean result                         = TRUE;
    GstFlowReturn retval                    = GST_FLOW_OK;

    GstFormat format;
    struct timeval tv_prof2, tv_prof3;
    long time_before = 0, time_after = 0;

    GST_DEBUG("handling %s	event", GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
	{
	    GstFormat format;
	    gint64 start, stop, position;

	    gst_event_parse_new_segment(event, NULL, NULL, &format, &start,
					&stop, &position);
	    wmadec_struct->time_offset = position;

        if (format == GST_FORMAT_TIME) {
		    result = gst_pad_push_event(wmadec_struct->srcpad, event);
		    if (TRUE != result) {
		        GST_ERROR("\n Error	in pushing the event,result	is %d\n", result);
		    }
	    }
        else {
		    result = gst_pad_event_default(pad, event);
	    }
	    break;
	}
    case GST_EVENT_FLUSH_STOP:
	{
	    /* first free the decoder buffer */

        wmadec_struct->flush_complete = FALSE;
        gst_adapter_clear(wmadec_struct->pAdapter);
        //gst_adapter_flush(wmadec_struct->pAdapter, wmadec_struct->inbuffsize_used);
        wmadec_struct->inbuffsize = 0;


	    if (NULL != wmadec_struct->output_buff) {
	    	MM_FREE(wmadec_struct->output_buff);
		    wmadec_struct->output_buff = NULL;
	    }

	    if (NULL != wmadec_struct->dec_config) {
		    gint i_loop;
    		for (i_loop = 0; i_loop < wmadec_struct->dec_config->sWMADMemInfo.s32NumReqs;
		                    i_loop++) {
		        wmadec_struct->mem =
                            &(wmadec_struct->dec_config->sWMADMemInfo.sMemInfoSub[i_loop]);

                if (NULL != wmadec_struct->mem->app_base_ptr) {
			        MM_FREE(wmadec_struct->mem->app_base_ptr);
			        wmadec_struct->mem->app_base_ptr = NULL;
		        }
		    }
            if(wmadec_struct->dec_config != NULL) {
		        MM_FREE(wmadec_struct->dec_config);
		        wmadec_struct->dec_config = NULL;
	        }
	    }
	    /* reset the decoder */
	    wmadec_struct->caps_set         = FALSE;
	    wmadec_struct->end_of_stream    = 0;
	    wmadec_struct->is_decode_init   = 0;
        wmadec_struct->flush_complete   = TRUE;

	    /* forward the event downstream */
	    result = gst_pad_push_event(wmadec_struct->srcpad, event);
	    if (TRUE != result) {
		    GST_ERROR("\n Error	in pushing the event,result	is %d\n", result);
	    }
	    break;
	}
    case GST_EVENT_EOS:
	{
	    if ((wmadec_struct->is_decode_init) &&(wmadec_struct->flush_complete==TRUE)) {
		    GstFlowReturn ret_val = GST_FLOW_OK;
		    GST_DEBUG("\n Got the EOS from sink\n");
		    wmadec_struct->end_of_stream = 1;

		    if (wmadec_struct->profile) {
		        gettimeofday(&tv_prof2, 0);
		    }

    		/* decode call for last remaining buffer */
	    	ret_val = mfw_gst_wma10dec_dframe(wmadec_struct);
    		if (ret_val != GST_FLOW_OK) {
	    	    GST_ERROR("\nError while decoding in EOS, reason is %d\n", ret_val);
                return FALSE;

		    }
		    if (wmadec_struct->profile) {
		        gettimeofday(&tv_prof3, 0);
		        time_before = (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
		        time_after =  (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
		        wmadec_struct->chain_Time += time_after - time_before;
		    }
		    result = gst_pad_push_event(wmadec_struct->srcpad, event);
		    if (TRUE != result) {
		        GST_ERROR("\n Error	in pushing the event,result	is %d\n", result);
                return FALSE;
		    }
        }
        else {
		    result = gst_pad_push_event(wmadec_struct->srcpad, event);
		    if (TRUE != result) {
		        GST_ERROR("\n Error	in pushing the event,result	is %d\n", result);
                return FALSE;
		    }
	    }
	    break;
	}
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    break;
	}
    }
    return TRUE;
}


/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_set_caps

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
static gboolean mfw_gst_wma10dec_set_caps(GstPad *pad, GstCaps *caps)
{
    MFW_GST_WMA10DEC_INFO_T *wmadec_struct    = NULL;
    GstStructure *structure                 = gst_caps_get_structure(caps, 0);
    wmadec_struct                           = MFW_GST_WMA10DEC(GST_OBJECT_PARENT(pad));
    const gchar *mime                       = NULL;
    const GValue *codec_data                = NULL;
    GstBuffer *gstbuf                       = NULL;
    unsigned char *dataptr                  = NULL;

    mime = gst_structure_get_name(structure);

    /* check for MIME type in the decoder. */
    if (strcmp(mime, "audio/x-wma") != 0) {
	GST_WARNING("Wrong	mimetype %s	provided, supported is %s", mime, "audio/x-wma");
	    return FALSE;
    }

    gst_structure_get_int(structure, "wmaversion", &wmadec_struct->version);
    gst_structure_get_int(structure, "channels", &wmadec_struct->channels);
    gst_structure_get_int(structure, "rate", &wmadec_struct->samples_per_second);
    gst_structure_get_int(structure, "bitrate", &wmadec_struct->avg_bytes_per_second);
    gst_structure_get_int(structure, "block_align", &wmadec_struct->block_align);
    gst_structure_get_int(structure, "depth", &wmadec_struct->bits_per_sample);

    wmadec_struct->format_tag = wmadec_struct->version + 0x160 - 0x1;
    wmadec_struct->avg_bytes_per_second /= 8;
    /* wma version is 3,not 4 for wma v3 loseless.
       Had better pass format_tag from asf to wma,not pass wma_version.
       Temporarily change version here after format_tag is gotten correctly */
    if (wmadec_struct->version == 4)
       wmadec_struct->version = 3;

    GST_DEBUG("\n Version = %d \n", wmadec_struct->version);
    GST_DEBUG("\n Format TAg = %d \n", wmadec_struct->format_tag);
    GST_DEBUG("\n Channels = %d \n", wmadec_struct->channels);
    GST_DEBUG("\n Samples Per Second = %d \n", wmadec_struct->samples_per_second);
    GST_DEBUG("\n Bytes Per Second = %d \n", wmadec_struct->avg_bytes_per_second / 8);
    GST_DEBUG("\n Block Align = %d \n", wmadec_struct->block_align);
    GST_DEBUG("\n Bits Per Sample = %d \n", wmadec_struct->bits_per_sample);

    codec_data = gst_structure_get_value(structure, "codec_data");
    if (codec_data) {
	    gstbuf = gst_value_get_buffer(codec_data);
	    dataptr = GST_BUFFER_DATA(gstbuf);
    }

    if (wmadec_struct->format_tag == 0x0160)	// Windows Media Audio V1
    	wmadec_struct->encode_opt = *(unsigned int*)(dataptr + 2);
    else if (wmadec_struct->format_tag == 0x0161)	// Windows Media Audio V2
	    wmadec_struct->encode_opt = *(unsigned int*)(dataptr + 4);
    else if ((wmadec_struct->format_tag == 0x0162) || (wmadec_struct->format_tag == 0x0163))
	// Windows Media Audio V3
    {
        wmadec_struct->channel_mask         = *(unsigned int    *)(dataptr + 2);
	    wmadec_struct->encode_opt           = *(unsigned short *)(dataptr + 14);
        wmadec_struct->advanced_encode_opt  = *(unsigned short *)(dataptr + 16);
        wmadec_struct->advanced_encode_opt2 = *(unsigned int   *)(dataptr + 10);

        GST_DEBUG("Channel Mask = %u\n", wmadec_struct->channel_mask);

        GST_DEBUG("Encode Opt = %u\n", wmadec_struct->advanced_encode_opt);
        GST_DEBUG("Encode Opt2 = %u\n", wmadec_struct->advanced_encode_opt2);
    }
    GST_DEBUG("Encode Opt = %u\n", wmadec_struct->encode_opt);

    if (!gst_pad_set_caps(wmadec_struct->srcpad, caps)) {
	    return FALSE;
    }
    return TRUE;
}

/*=============================================================================
FUNCTION: mfw_gst_wma10dec_set_property

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

static void mfw_gst_wma10dec_set_property(GObject *object, guint prop_id,
			                            const GValue *value, GParamSpec *pspec)
{
    MFW_GST_WMA10DEC_INFO_T *wmadec_info = MFW_GST_WMA10DEC(object);
    switch (prop_id) {
    case ID_PROFILE_ENABLE:
	    wmadec_info->profile = g_value_get_boolean(value);
	    GST_DEBUG("profile=%d\n", wmadec_info->profile);
	    break;
    default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
}

/*=============================================================================
FUNCTION: mfw_gst_wma10dec_get_property

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
mfw_gst_wma10dec_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    MFW_GST_WMA10DEC_INFO_T *wmadec_info = MFW_GST_WMA10DEC(object);
    switch (prop_id) {
    case ID_PROFILE_ENABLE:
	    GST_DEBUG("profile=%d\n", wmadec_info->profile);
	    g_value_set_boolean(value, wmadec_info->profile);
	    break;
    default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
}


/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_init

DESCRIPTION:	create the pad template	that has been registered with the
				element	class in the _base_init

ARGUMENTS PASSED:
		wmadec_struct-	  pointer to wmadecoder	element	structure

RETURN VALUE:
		None

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static void mfw_gst_wma10dec_init(MFW_GST_WMA10DEC_INFO_T *wmadec_struct)
{
    GstElementClass *klass = GST_ELEMENT_GET_CLASS(wmadec_struct);
    GstPadTemplate *template;


    wmadec_struct->sinkpad = gst_pad_new_from_static_template (&mfw_gst_wma10dec_sink_template_factory, "sink");
    wmadec_struct->srcpad = gst_pad_new_from_static_template (&mfw_gst_wma10dec_src_template_factory, "src");


    gst_element_add_pad(GST_ELEMENT(wmadec_struct), wmadec_struct->sinkpad);
    gst_pad_set_setcaps_function(wmadec_struct->sinkpad, mfw_gst_wma10dec_set_caps);


    gst_pad_set_chain_function(wmadec_struct->sinkpad,
                                    GST_DEBUG_FUNCPTR(mfw_gst_wma10dec_chain));
    gst_pad_set_event_function(wmadec_struct->sinkpad,
			                        GST_DEBUG_FUNCPTR(mfw_gst_wma10dec_sink_event));

    gst_element_add_pad(GST_ELEMENT(wmadec_struct),
                                    wmadec_struct->srcpad);

    gst_pad_set_event_function(wmadec_struct->srcpad,
			                        GST_DEBUG_FUNCPTR(mfw_gst_wma10dec_src_event));
    wmadec_struct->profile = FALSE;
    wmadec_struct->fillbuf = NULL;

    /* Get the decoder version */
    char *strVer;
    strVer = WMA10DoderVersionInfo();

    INIT_DEMO_MODE(strVer, wmadec_struct->demo_mode);
#define MFW_GST_WMA10DEC_PLUGIN VERSION
    PRINT_CORE_VERSION(WMA10DoderVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_WMA10DEC_PLUGIN);

}

static void
mfw_gst_wma10dec_finalize (GObject * object)
{
    PRINT_FINALIZE("wma10_dec");
    G_OBJECT_CLASS (parent_class)->finalize (object);

}

/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_class_init

DESCRIPTION:	Initialise the class only once (specifying what	signals,
				arguments and virtual functions	the	class has and setting up
				global stata)

ARGUMENTS PASSED:
		klass	   -	pointer	to wmadecoder element class

RETURN VALUE:
		None

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
static void mfw_gst_wma10dec_class_init(MFW_GST_WMA10DEC_INFO_CLASS_T *klass)
{
    GObjectClass *gobject_class         = NULL;
    GstElementClass *gstelement_class   = NULL;
    gobject_class                       = (GObjectClass *) klass;
    gstelement_class                    = (GstElementClass *) klass;
    
    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property         = mfw_gst_wma10dec_set_property;
    gobject_class->get_property         = mfw_gst_wma10dec_get_property;
    gobject_class->finalize = mfw_gst_wma10dec_finalize;
    
    gstelement_class->change_state      = mfw_gst_wma10dec_change_state;

    g_object_class_install_property(gobject_class, ID_PROFILE_ENABLE,
				    g_param_spec_boolean("profiling", "Profiling",
							 "Enable time profiling of decoder and plugin",
							 FALSE, G_PARAM_READWRITE));
}


/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_base_init

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
static void mfw_gst_wma10dec_base_init(gpointer klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get(&mfw_gst_wma10dec_sink_template_factory));

    gst_element_class_add_pad_template(element_class,
	    gst_static_pad_template_get(&mfw_gst_wma10dec_src_template_factory));

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "wma audio decoder",
        "Codec/Decoder/Audio", "Decode compressed wma audio to raw data");
}


/*=============================================================================

FUNCTION:	mfw_gst_type_wma10dec_get_type

DESCRIPTION:	intefaces are initiated	in this	function.you can register one
				or more	interfaces after having	registered the type	itself.

ARGUMENTS PASSED:
		None

RETURN VALUE:
		A numerical	value ,which represents	the	unique identifier of this
		elment(wmadecoder)

PRE-CONDITIONS:
		None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
		None
=============================================================================*/
GType mfw_gst_wma10dec_get_type(void)
{
    static GType wmadec_type = 0;

    if (!wmadec_type) {
	static const GTypeInfo gstwmadec_info = {
	    sizeof(MFW_GST_WMA10DEC_INFO_CLASS_T),
	    mfw_gst_wma10dec_base_init, NULL, (GClassInitFunc) mfw_gst_wma10dec_class_init,
	    NULL, NULL, sizeof(MFW_GST_WMA10DEC_INFO_T), 0,
	    (GInstanceInitFunc) mfw_gst_wma10dec_init,
	};
	wmadec_type = g_type_register_static(GST_TYPE_ELEMENT,
					     "MFW_GST_WMA10DEC_INFO_T", &gstwmadec_info, 0);
    }

    GST_DEBUG_CATEGORY_INIT(mfw_gst_wma10dec_debug, "mfw_wma10decoder", 0,
			    "FreeScale's WMA10 Decoder's Log");
    return wmadec_type;
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
    return gst_element_register(plugin, "mfw_wma10decoder", FSL_GST_DEFAULT_DECODER_RANK_LEGACY,
				MFW_GST_TYPE_WMA10DEC);
}

/*=============================================================================
FUNCTION:	mfw_gst_wma10dec_change_state

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
static GstStateChangeReturn mfw_gst_wma10dec_change_state(GstElement * element,
			                                        GstStateChange transition)
{

    MFW_GST_WMA10DEC_INFO_T *wmadec_struct = NULL;
    GstStateChangeReturn ret;
    wmadec_struct = MFW_GST_WMA10DEC(element);

    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
            MM_INIT_DBG_MEM("wma10dec");

	    break;

        case GST_STATE_CHANGE_READY_TO_PAUSED:

        gst_tag_register (GST_TAG_MFW_WMA_CHANNELS, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
            "number of channels","number of channels", NULL);
        gst_tag_register (GST_TAG_MFW_WMA_SAMPLING_RATE, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
            "sampling frequency (Hz)","sampling frequency (Hz)", NULL);

	    wmadec_struct->caps_set = FALSE;
	    wmadec_struct->new_seg_flag = FALSE;
	    wmadec_struct->is_decode_init = 0;
	    wmadec_struct->end_of_stream = 0;
	    wmadec_struct->time_offset = 0;
	    wmadec_struct->seek_flag = FALSE;
	    wmadec_struct->time_added = 0;
	    wmadec_struct->Time = 0;
	    wmadec_struct->chain_Time = 0;
	    wmadec_struct->no_of_frames_dropped = 0;
	    wmadec_struct->total_frames = 0;
        wmadec_struct->flush_complete=TRUE;
	    wmadec_struct->mem = NULL;
	    wmadec_struct->dec_config = NULL;
	    wmadec_struct->dec_param = NULL;
        wmadec_struct->pAdapter = gst_adapter_new();
	    break;

        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        break;
        default:
	    break;
    }
    ret = GST_ELEMENT_CLASS (parent_class)->
        change_state (element, transition);
    
    switch (transition) {

        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	    break;

        case GST_STATE_CHANGE_PAUSED_TO_READY:
            {
	    	    gfloat avg_mcps = 0, avg_plugin_time = 0, avg_dec_time = 0;
	            if (wmadec_struct->profile) {

		        g_print("\nWMA Decoder Plugin: Profile Figures\n");
		        g_print("\n Total decode time is  %ldus", wmadec_struct->Time);
		        g_print("\n Total plugin time is  %ldus", wmadec_struct->chain_Time);
		        g_print("\n frames decoded is  %d", wmadec_struct->total_frames);
		        g_print("\n frames dropped is  %d", wmadec_struct->no_of_frames_dropped);

		        avg_mcps = (((float) wmadec_struct->dec_param->us32SamplesPerSec /
		              WMA10D_FRAME_SIZE) * wmadec_struct->Time * PROCESSOR_CLOCK)
		            / (wmadec_struct->total_frames - wmadec_struct->no_of_frames_dropped)
                    / 1000000;

		        g_print("\nAverage decode MCPS is  %f\n", avg_mcps);

                avg_mcps = (((float) wmadec_struct->dec_param->us32SamplesPerSec /
		              WMA10D_FRAME_SIZE) * wmadec_struct->chain_Time * PROCESSOR_CLOCK)
		            / (wmadec_struct->total_frames - wmadec_struct->no_of_frames_dropped)
                    / 1000000;

		        g_print("\nAverage plugin MCPS is  %f\n", avg_mcps);

		        avg_dec_time = ((float) wmadec_struct->Time) / wmadec_struct->total_frames;
		        g_print("\nAverage decoding time is %fus", avg_dec_time);

                avg_plugin_time = ((float) wmadec_struct->chain_Time) /
                                                    wmadec_struct->total_frames;
		        g_print("\nAverage plugin time is %fus\n", avg_plugin_time);

    	        wmadec_struct->Time                 = 0;
		        wmadec_struct->chain_Time           = 0;
		        wmadec_struct->total_frames         = 0;
		        wmadec_struct->no_of_frames_dropped = 0;
            }
    	    wmadec_struct->caps_set = FALSE;
	        wmadec_struct->new_seg_flag = FALSE;

            gst_adapter_clear(wmadec_struct->pAdapter);
            g_object_unref(wmadec_struct->pAdapter);
            wmadec_struct->pAdapter = NULL;
       
	        wmadec_struct->time_offset = 0;
	        wmadec_struct->is_decode_init = 0;
	        wmadec_struct->end_of_stream = 0;

	        if (NULL != wmadec_struct->output_buff) {
	            MM_FREE(wmadec_struct->output_buff);
	            wmadec_struct->output_buff = NULL;
	        }


            if (wmadec_struct->fillbuf){
                gst_buffer_unref(wmadec_struct->fillbuf);
                wmadec_struct->fillbuf = NULL;
                
            }


            if (NULL != wmadec_struct->dec_config) {
                gint i_loop;

                for (i_loop = 0; i_loop < wmadec_struct->dec_config->sWMADMemInfo.s32NumReqs;
                i_loop++) {
                    wmadec_struct->mem = &(wmadec_struct->dec_config->sWMADMemInfo.sMemInfoSub[i_loop]);
                    if (NULL != wmadec_struct->mem->app_base_ptr) {
                        MM_FREE(wmadec_struct->mem->app_base_ptr);
                        wmadec_struct->mem->app_base_ptr = NULL;
                    }
                }
            }

            if (NULL != wmadec_struct->dec_config) {
                MM_FREE(wmadec_struct->dec_config);
                wmadec_struct->dec_config = NULL;
	        }

	        if (NULL != wmadec_struct->dec_param) {
	            MM_FREE(wmadec_struct->dec_param);
	            wmadec_struct->dec_param = NULL;
	        }
            wmadec_struct->flush_complete=TRUE;
            break;
	    }

        case GST_STATE_CHANGE_READY_TO_NULL:
            MM_DEINIT_DBG_MEM();
	    break;

        default:
	    break;
    }
    return ret;
}


FSL_GST_PLUGIN_DEFINE("wmadec", "wma audio decoder", plugin_init);


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

/*=============================================================================

    Module Name:            mfw_gst_amrdec.c

    General Description:    Gstreamer plugin for AMR decoder
                            capable of decoding AMR-NB, AMR-WB and AMR-WB+.

===============================================================================
Portability: This file is ported for Linux and GStreamer.
*/

/*===============================================================================
                            INCLUDE FILES
=============================================================================*/
#include <gst/base/gstadapter.h>
#include "mfw_gst_amrdec.h"
#include "nb_amr_dec_api.h"
#include "wbamr_dec_interface.h"
//#include "wbamrp_dec_interface.h"
#include "mfw_gst_utils.h"

#define GST_CAT_DEFAULT mfw_gst_amrdec_debug

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* size of packed frame for each mode */
static const gint nb_block_size_mms[]=
{
    13, 14, 16, 18, 20, 21, 27, 32,
    6,  0,  0,  0,  0,  0,  0,  1
};

static const gint nb_block_size_if1[] = {
    15, 16, 18, 20, 22, 23, 29, 34,
    8, 0, 0, 0, 0, 0, 0, 3
};

static const gint nb_block_size_if2[] = {
    13, 14, 16, 18, 19, 21, 26, 31,
    6, 0, 0, 0, 0, 0, 0, 1
};

static const gint wb_block_size_mms[]=
{
    18, 24, 33, 37, 41, 47, 51, 59,
    61,  6,  0,  0,  0,  0,  0,  1
};

static const gint wb_block_size_if1[] = {
    20, 26, 35, 39, 43, 49, 53, 61,
    63,  8,  1,  1,  1,  1,  1,  1
};

static const gint wb_block_size_if2[] = {
    18, 23, 33, 37, 41, 47, 51, 59,
    61,  6,  1,  1,  1,  1,  1,  1
};

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
typedef struct _AmrDecParams {
    union {
        sAMRDDecoderConfigType *nb_dec_config;	/* decoder context */
        WBAMRD_Decoder_Config *wb_dec_config;
    } u;
    const gchar *frame_format;
    enum {
        AMR_MIME_NB = 0,
        AMR_MIME_WB,
        AMR_MIME_WBP
    } mime;
} AmrDecParams;

struct _MfwGstAmrdecPrivate {
    GstElement element;
    GstPad *sinkpad;
    GstPad *srcpad;
    AmrDecParams dec_params;	/* parameters of the decoder used by 
					   the plugin */
    gboolean caps_set;		/* flag to check whether the source pad 
				   capabilities  are set or not */
    gboolean discont;
    gboolean eos;		/* flag to update end of stream staus */
    gint    rate;
    gint    channels;
    gint    duration;
    
    GstAdapter *adapter;
    
    guint64 time_stamp;
    guint64 frame_count;
};

#define MFW_GST_AMRDEC_GET_PRIVATE(o) \
      (G_TYPE_INSTANCE_GET_PRIVATE ((o), MFW_GST_TYPE_AMRDEC, MfwGstAmrdecPrivate))

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src",
								   GST_PAD_SRC,
                                   GST_PAD_ALWAYS,
                                   GST_STATIC_CAPS
                                   ("audio/x-raw-int, "
                                    "endianness	= (gint) " G_STRINGIFY(G_BYTE_ORDER) ", "
                                    "signed	= (boolean)	true, "
                                    "width = (gint) 16, "
                                    "depth = (gint) 16, "
                                    "rate =	(gint) [8000, 48000], "
                                    "channels = (gint) [1, 8]"));


/*=============================================================================
                                LOCAL MACROS
=============================================================================*/

/*=============================================================================
                        STATIC FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_amrdec_debug);
static void mfw_gst_amrdec_set_property(GObject *object,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void mfw_gst_amrdec_get_property(GObject *object,
					    guint prop_id, GValue *value,
					    GParamSpec *pspec);
static gboolean mfw_gst_amrdec_set_caps(GstPad *pad, GstCaps *caps);
static GstFlowReturn mfw_gst_amrdec_chain(GstPad *pad, GstBuffer *buf);
static gboolean mfw_gst_amrdec_sink_event(GstPad *pad, GstEvent *event);
static gboolean plugin_init(GstPlugin * plugin);

/*=============================================================================
                            STATIC VARIABLES
=============================================================================*/

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

GST_BOILERPLATE(MfwGstAmrdec, mfw_gst_amrdec, GstElement, GST_TYPE_ELEMENT);

/*=============================================================================
FUNCTION: mfw_gst_amrdec_set_property

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
hex_print(gchar *desc, gchar *buf, gint len)
{
    gint i=0;
    g_print("%s:dump buffer:%p,len:%d.\n",desc,buf, len);
    for (i=0; i<len; i++)
        while(i<len)
        {
            g_print("%0X:",*buf++);
            if ((i & 0xf) == 0xf)
                g_print("\n");
            i++;
        }
    g_print("\n");
}

static void
mfw_gst_amrdec_set_property(GObject * object, guint prop_id,
				const GValue * value, GParamSpec * pspec)
{
    GST_DEBUG(" in mfw_gst_amrdec_set_property routine \n");
    GST_DEBUG(" out of mfw_gst_amrdec_set_property routine \n");
}

/*=============================================================================
FUNCTION: mfw_gst_amrdec_set_property

DESCRIPTION: gets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property got from the application
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
mfw_gst_amrdec_get_property(GObject * object, guint prop_id,
				GValue * value, GParamSpec * pspec)
{
    GST_DEBUG(" in mfw_gst_amrdec_get_property routine \n");
    GST_DEBUG(" out of mfw_gst_amrdec_get_property routine \n");

}

static void
mfw_gst_amrdec_dispose (GObject *object)
{
    MfwGstAmrdec *dec = MFW_GST_AMRDEC(object);
    MfwGstAmrdecPrivate *priv = dec->priv;
    if(priv->adapter) {
        g_object_unref (priv->adapter);
        priv->adapter = NULL;
    }
    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gboolean
mfw_gst_amrdec_init_nb_codec(MfwGstAmrdec *self)
{
    MfwGstAmrdecPrivate *priv = self->priv;
    sAMRDDecoderConfigType *dec_config;
    gint i, nr;
    eAMRDReturnType rc;
    sAMRDMemAllocInfoSubType *mem;

    g_assert(priv->dec_params.u.nb_dec_config == NULL);

	GST_INFO("Init AMR-NB Codec %s\n", eAMRDVersionInfo());
    /* allocate memory for config structure */
    dec_config = (sAMRDDecoderConfigType *) g_malloc0 (sizeof(sAMRDDecoderConfigType));
    if (dec_config == NULL) {
        GST_ERROR("error in allocation of decoder config structure");
        return FALSE;
    }

    dec_config->u8BitStreamFormat = NBAMR_MMSIO; //default value
    if(g_strcmp0(priv->dec_params.frame_format, "if1") == 0)
        dec_config->u8BitStreamFormat = NBAMR_IF1IO;
    else if(g_strcmp0(priv->dec_params.frame_format, "if2") == 0)
        dec_config->u8BitStreamFormat = NBAMR_IF2IO;
    //else if(g_strcmp0(priv->dec_params.frame_format, "mms") == 0)
    //    dec_config->u8BitStreamFormat = NBAMR_MMSIO;

    dec_config->u8NumFrameToDecode = 1;
    dec_config->u8RXFrameType = 0;  //TX type

    /* call query mem function to know mem requirement of library */
    if (eAMRDQueryMem(dec_config) != E_NBAMRD_OK) {
        g_free(dec_config);
        GST_ERROR("Failed to get the memory configuration for the decoder\n");
        return FALSE;
    }

    /* Number of memory chunk requests by the decoder */
    nr = dec_config->sAMRDMemInfo.s32AMRDNumMemReqs;

    for(i = 0; i < nr; i++)
    {
        mem = &(dec_config->sAMRDMemInfo.asMemInfoSub[i]);
        if (mem->s32AMRDSize==0){
            continue;
        }
        mem->pvAPPDBasePtr = g_malloc (mem->s32AMRDSize);
        if (mem->pvAPPDBasePtr == NULL) {
            g_free(dec_config);
            //FIXME: some memory not freed
            GST_ERROR("Failed to get the memory or the decoder\n");
            return FALSE;
        }
    }

    rc = eAMRDDecodeInit(dec_config);
    if (rc != E_NBAMRD_OK) {
        GST_ERROR("Error in initializing the decoder");
		for(i = 0; i < nr; i++)
			g_free(dec_config->sAMRDMemInfo.asMemInfoSub[i].pvAPPDBasePtr);
        g_free(dec_config);
        return FALSE;
    }
    priv->dec_params.u.nb_dec_config = dec_config;
    return TRUE;
}

static void
mfw_gst_amrdec_term_nb_codec(MfwGstAmrdec *self)
{
    MfwGstAmrdecPrivate *priv = self->priv;
    sAMRDDecoderConfigType *dec_config;
    gint i, nr;

    dec_config = priv->dec_params.u.nb_dec_config;
    if (dec_config == NULL) {
        return;
    }
    priv->dec_params.u.nb_dec_config = NULL;

    /* Number of memory chunk requests by the decoder */
    nr = dec_config->sAMRDMemInfo.s32AMRDNumMemReqs;
    for(i = 0; i < nr; i++)
    {
        g_free(dec_config->sAMRDMemInfo.asMemInfoSub[i].pvAPPDBasePtr);
    }

    g_free(dec_config);
}

static gboolean
mfw_gst_amrdec_init_wb_codec(MfwGstAmrdec *self)
{
	MfwGstAmrdecPrivate *priv = self->priv;
	WBAMRD_Decoder_Config *dec_config;
	gint i, nr;
    WBAMRD_RET_TYPE rc;
    WBAMRD_Mem_Alloc_Info_Sub *mem;

    g_assert(priv->dec_params.u.wb_dec_config == NULL);
	GST_INFO("Init AMR-WB Codec %s\n", WBAMR_get_version_info());

    /* allocate memory for config structure */
    dec_config = (WBAMRD_Decoder_Config *) g_malloc0 (sizeof(WBAMRD_Decoder_Config));
    if (dec_config == NULL) {
        GST_ERROR("error in allocation of decoder config structure");
        return FALSE;
    }

	//FIXME: following bitstreamformat is not defined in codec with macro
    dec_config->bitstreamformat = 2; //default value
    if(g_strcmp0(priv->dec_params.frame_format, "if1") == 0)
        dec_config->bitstreamformat = 4;
    else if(g_strcmp0(priv->dec_params.frame_format, "if2") == 0)
        dec_config->bitstreamformat = 3;
    //else if(g_strcmp0(priv->dec_params.frame_format, "mms") == 0)
    //    dec_config->u8BitStreamFormat = 1;

    /* call query mem function to know mem requirement of library */
    if (wbamrd_query_dec_mem(dec_config) != WBAMRD_OK) {
        g_free(dec_config);
        GST_ERROR("Failed to get the memory configuration for the decoder\n");
        return FALSE;
    }

    /* Number of memory chunk requests by the decoder */
    nr = dec_config->wbamrd_mem_info.wbamrd_num_reqs;

    for(i = 0; i < nr; i++)
    {
        mem = &(dec_config->wbamrd_mem_info.mem_info_sub[i]);
        if (mem->wbamrd_size==0){
            continue;
        }
        mem->wbappd_base_ptr = g_malloc (mem->wbamrd_size);
        if (mem->wbappd_base_ptr == NULL) {
            g_free(dec_config);
            //FIXME: some memory not freed
            GST_ERROR("Failed to get the memory or the decoder\n");
            return FALSE;
        }
    }

    rc = wbamrd_decode_init(dec_config);
    if (rc != WBAMRD_OK) {
        GST_ERROR("Error in initializing the decoder");
		for(i=0; i<nr; i++)
			g_free(dec_config->wbamrd_mem_info.mem_info_sub[i].wbappd_base_ptr);
        g_free(dec_config);
        return FALSE;
    }
    priv->dec_params.u.wb_dec_config = dec_config;
    return TRUE;
}

static void
mfw_gst_amrdec_term_wb_codec(MfwGstAmrdec *self)
{
    MfwGstAmrdecPrivate *priv = self->priv;
    WBAMRD_Decoder_Config *dec_config;
    gint i, nr;

    dec_config = priv->dec_params.u.wb_dec_config;
    if (dec_config == NULL) {
        return;
    }
    priv->dec_params.u.wb_dec_config = NULL;

    /* Number of memory chunk requests by the decoder */
    nr = dec_config->wbamrd_mem_info.wbamrd_num_reqs;
    for(i = 0; i < nr; i++)
        g_free(dec_config->wbamrd_mem_info.mem_info_sub[i].wbappd_base_ptr);
    g_free(dec_config);
}

static gboolean
mfw_gst_amrdec_init_wbp_codec(MfwGstAmrdec *self)
{//TODO
    return TRUE;
}

static void
mfw_gst_amrdec_term_wbp_codec(MfwGstAmrdec *self)
{
}

static gboolean
mfw_gst_amrdec_init_codec(MfwGstAmrdec *self)
{
    MfwGstAmrdecPrivate *priv = self->priv;
    if(priv->dec_params.mime == AMR_MIME_NB)
        return mfw_gst_amrdec_init_nb_codec(self);
    else if(priv->dec_params.mime == AMR_MIME_WB)
        return mfw_gst_amrdec_init_wb_codec(self);
    else if(priv->dec_params.mime == AMR_MIME_WBP)
        return mfw_gst_amrdec_init_wbp_codec(self);
}

static void
mfw_gst_amrdec_term_codec(MfwGstAmrdec *self)
{
    MfwGstAmrdecPrivate *priv = self->priv;
    if(priv->dec_params.mime == AMR_MIME_NB)
        mfw_gst_amrdec_term_nb_codec(self);
    else if(priv->dec_params.mime == AMR_MIME_WB)
        mfw_gst_amrdec_term_wb_codec(self);
    else if(priv->dec_params.mime == AMR_MIME_WBP)
        mfw_gst_amrdec_term_wbp_codec(self);
    priv->dec_params.frame_format = NULL;
}

static gboolean
mfw_gst_amrdec_decode(MfwGstAmrdec *self, guint8 *in_buf,
        gint in_size, guint8 *out_buf, gint out_size)
{
    MfwGstAmrdecPrivate *priv = self->priv;
	//FIXME: alignment problem
    if(priv->dec_params.mime == AMR_MIME_NB) {
        eAMRDReturnType ret = eAMRDDecodeFrame(priv->dec_params.u.nb_dec_config,
                (NBAMR_S16 *)in_buf, (NBAMR_S16 *)out_buf);
        GST_DEBUG ("AMR-NB decode return %d\n", ret);
        return ret == E_NBAMRD_OK;
    }
    else if(priv->dec_params.mime == AMR_MIME_WB) {
        WBAMRD_RET_TYPE ret;
        guint8 *real_in_buf = in_buf;
        guint8 format = priv->dec_params.u.wb_dec_config->bitstreamformat;
        if(format == 2) {
            guint8 byte = *in_buf;
            guint8 quality = (byte>>2) & 0x1;
            gint mode = (byte>>3) & 0xF;
            real_in_buf = g_malloc(in_size+1);
            *real_in_buf = quality;
            *(real_in_buf+1) = mode;
            memcpy(real_in_buf+2, in_buf+1, in_size-1);
        }
        ret = wbamrd_decode_frame(priv->dec_params.u.wb_dec_config,
                (WBAMR_S16 *)real_in_buf, (WBAMR_S16 *)out_buf);
        if (real_in_buf != in_buf)
            g_free(real_in_buf);
        GST_DEBUG ("AMR-WB decode return %d\n", ret);
        return ret == WBAMRD_OK;
    }
    else if(priv->dec_params.mime == AMR_MIME_WBP) {
    	//TODO
    }
    return FALSE;
}



//return FALSE if frame invalid
static gboolean
get_in_out_frame_size(MfwGstAmrdec *self,
        guint8 *data, gint *in_size, gint *out_size)
{
    MfwGstAmrdecPrivate *priv = self->priv;
    gboolean ret = TRUE;
    guint8 byte = *data;
    gint mode;
	guint8 format;
    *in_size = 0;
    GST_DEBUG("header 0x%x\n", byte);
    switch(priv->dec_params.mime) {
        case AMR_MIME_NB:
            format = priv->dec_params.u.nb_dec_config->u8BitStreamFormat;
            GST_DEBUG("nb format %d\n", format);
            if(format == NBAMR_MMSIO) {
                mode = (byte>>3) & 0xF;
                GST_DEBUG("nb mode %d\n", mode);
                ret = (byte&4) == 4;    //quality indicator
                *in_size = nb_block_size_mms[mode];
			} else if(format == NBAMR_IF1IO) {
				mode = (byte>>4) & 0xF;
				ret = (byte&8) == 8;    //quality indicator
				*in_size = nb_block_size_if1[mode];
			} else if(format == NBAMR_IF2IO) {
				mode = byte & 0xF;
				*in_size = nb_block_size_if2[mode];
			}
			*out_size = 8000/50*2;
			break;
		case AMR_MIME_WB:
			format = priv->dec_params.u.wb_dec_config->bitstreamformat;
            GST_DEBUG("wb format %d\n", format);
			if(format == 2) {
				mode = (byte>>3) & 0xF;
				ret = (byte&4) == 4;    //quality indicator
				*in_size = wb_block_size_mms[mode];
			} else if(format == 4) {
				mode = (byte>>4) & 0xF;
				ret = (byte&8) == 8;    //quality indicator
				*in_size = wb_block_size_if1[mode];
			} else if(format == 3) {
				mode = byte & 0xF;
				*in_size = wb_block_size_if2[mode];
			}
			*out_size = 16000/50*2;
            break;
        case AMR_MIME_WBP:
			//TODO
            break;
        default:
            g_assert(FALSE);
    }
    return ret;
}

/*=============================================================================
FUNCTION: mfw_gst_amrdec_chain

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
mfw_gst_amrdec_chain(GstPad *pad, GstBuffer *buf)
{
    MfwGstAmrdec *dec;
    MfwGstAmrdecPrivate *priv;
    GstFlowReturn ret;
    GST_DEBUG(" in of mfw_gst_amrdec_chain routine \n");

    dec = MFW_GST_AMRDEC (gst_pad_get_parent (pad));
    priv = dec->priv;

    if (!priv->caps_set) {
        GST_ERROR ("decoder is not initialized");
        gst_object_unref (dec);
        return GST_FLOW_NOT_NEGOTIATED;
    }

    /* discontinuity, don't combine samples before and after the
     * DISCONT */
    if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT)) {
        gst_adapter_clear (priv->adapter);
        priv->time_stamp = -1;
        priv->discont = TRUE;
    }

    /* take latest timestamp, FIXME timestamp is the one of the
     * first buffer in the adapter. */
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buf))
        priv->time_stamp = GST_BUFFER_TIMESTAMP (buf);

    gst_adapter_push (priv->adapter, buf);

    ret = GST_FLOW_OK;

    GST_DEBUG("got buffer size %d\n", GST_BUFFER_SIZE(buf));
    while (TRUE) {
        GstBuffer *out;
        guint8 *data;
        gint in_size, out_size;
        gboolean rv;

        /* need to peek data to get the size */
        if (gst_adapter_available (priv->adapter) < 1) {
            GST_DEBUG("--------------------------\n");
            break;
        }
        data = (guint8 *) gst_adapter_peek (priv->adapter, 1);
        rv = get_in_out_frame_size(dec, data, &in_size, &out_size);
        GST_DEBUG("got in size %d, out size %d\n", in_size, out_size);
        if(in_size <= 0) {
            GST_ERROR("AMR frame %lld mode not supported, maybe data misaligned\n", priv->frame_count);
            gst_adapter_clear(priv->adapter);
            ret = GST_FLOW_ERROR;
            break;
        }
        
        if(gst_adapter_available (priv->adapter) < in_size)
            break;
        if (!rv) {
            gst_adapter_flush(priv->adapter, in_size);
            GST_ERROR("AMR frame %lld damaged\n", priv->frame_count);
            priv->frame_count++;
            ret = GST_FLOW_ERROR;
            continue;
        }
        /* the library seems to write into the source data, hence
         * the copy. */
        data = gst_adapter_take (priv->adapter, in_size);

        /* get output */
        out = gst_buffer_new_and_alloc (out_size);

        GST_BUFFER_DURATION (out) = priv->duration;
        GST_BUFFER_TIMESTAMP (out) = priv->time_stamp;

        if (priv->time_stamp != -1)
            priv->time_stamp += priv->duration;
        if (priv->discont) {
            GST_WARNING("--- AMR discontinous --- \n");
            GST_BUFFER_FLAG_SET (out, GST_BUFFER_FLAG_DISCONT);
            priv->discont = FALSE;
        }

        gst_buffer_set_caps (out, GST_PAD_CAPS (priv->srcpad));

        /* decode */
        rv = mfw_gst_amrdec_decode(dec, data, in_size, GST_BUFFER_DATA(out), out_size);
        g_free (data);
        if(!rv) {
            GST_ERROR("AMR decode frame %lld error\n", priv->frame_count);
            gst_buffer_unref(out);
            ret = GST_FLOW_ERROR;
            priv->frame_count++;
            continue;   //just skip a frame
        }

        /* send out */
        ret = gst_pad_push (priv->srcpad, out);
        priv->frame_count++;
    }

    gst_object_unref (dec);
    GST_DEBUG(" out of mfw_gst_amrdec_chain routine \n");
    return ret;
}

static GstStateChangeReturn
state_ready_to_paused(GstElement *self)
{
    MfwGstAmrdecPrivate *priv = MFW_GST_AMRDEC_GET_PRIVATE(self);
    priv->caps_set = FALSE;
    priv->eos = FALSE;
    priv->channels = 0;
    priv->rate = 0;
    priv->duration = 0;
    priv->discont = TRUE;
    priv->time_stamp = -1;
    priv->frame_count = 0;
    //don't init codec because no caps available,
    //codec initialization delayed to set_caps
    return GST_STATE_CHANGE_SUCCESS;
}

static GstStateChangeReturn
state_paused_to_ready(GstElement *self)
{
    MfwGstAmrdec *dec = MFW_GST_AMRDEC(self);
    MfwGstAmrdecPrivate *priv = dec->priv;
    if(priv->caps_set)
    {
        mfw_gst_amrdec_term_codec(dec);
        priv->caps_set = FALSE;
        gst_adapter_clear(priv->adapter);
    }
    return GST_STATE_CHANGE_SUCCESS;
}

/*=============================================================================
FUNCTION:   mfw_gst_amrdec_change_state

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
mfw_gst_amrdec_change_state(GstElement * self,
				GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GST_INFO(" in mfw_gst_amrdec_change_state routine: transition %d\n", transition);
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
	    break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    	ret = state_ready_to_paused(self);
        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    default:
	    break;
    }
    ret = parent_class->change_state(self, transition);
    switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        ret = state_paused_to_ready(self);
        break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_READY_TO_NULL:
    default:
	    break;
    }
    GST_INFO(" out of mfw_gst_amrdec_change_state routine: ret %d\n", ret);
    return ret;
}

/*=============================================================================
FUNCTION:   mfw_gst_amrdec_sink_event

DESCRIPTION:

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
mfw_gst_amrdec_sink_event(GstPad * pad, GstEvent * event)
{
    gboolean ret = TRUE;
    MfwGstAmrdec *dec;
    MfwGstAmrdecPrivate *priv;

    dec = MFW_GST_AMRDEC(GST_OBJECT_PARENT(pad));
    priv = dec->priv;

    GST_DEBUG(" in mfw_gst_amrdec_sink_event function \n");
    switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_EOS:
            GST_DEBUG("\nAMR Decoder: Got an EOS from Demuxer\n");
            priv->eos = TRUE;
            ret = gst_pad_event_default(pad, event);
            break;
        case GST_EVENT_FLUSH_STOP:
            GST_DEBUG(" GST_EVENT_FLUSH_STOP \n");
            gst_adapter_clear (priv->adapter);
            priv->time_stamp = -1;
            ret = gst_pad_event_default(pad, event);
            break;
        default:
            ret = gst_pad_event_default(pad, event);
    }

    GST_DEBUG(" out of mfw_gst_amrdec_sink_event \n");
    return ret;
}


/*=============================================================================
FUNCTION:   mfw_gst_amrdec_set_caps

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
mfw_gst_amrdec_set_caps(GstPad *pad, GstCaps *caps)
{
    //term old codec and init new codec
    MfwGstAmrdec *self;
    MfwGstAmrdecPrivate *priv;
    const gchar *mime;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    GstCaps *src_caps;
    gboolean ret;

    self = MFW_GST_AMRDEC(gst_pad_get_parent(pad));
    priv = self->priv;

    if(priv->caps_set) {
        mfw_gst_amrdec_term_codec(self);
        priv->caps_set = FALSE;
        gst_adapter_clear(priv->adapter);
    }

    GST_DEBUG(" in mfw_gst_amrdec_set_caps routine \n");
    mime = gst_structure_get_name(structure);

    if (g_strcmp0(mime, "audio/AMR") == 0) {
	    priv->dec_params.mime = AMR_MIME_NB;
    }
    else if(g_strcmp0(mime, "audio/AMR-WB") == 0) {
        priv->dec_params.mime = AMR_MIME_WB;
    }
    else if(g_strcmp0(mime, "audio/AMR-WB+") == 0) {
        priv->dec_params.mime = AMR_MIME_WBP;
    }
    else
    {
        GST_WARNING("Wrong mimetype %s provided, we only support %s",
                mime, "audio/AMR & audio/AMR-WB & audio/AMR-WB+");
        gst_object_unref(self);
        return FALSE;
    }

    priv->dec_params.frame_format = gst_structure_get_string (structure, "frame_format");
    
    /* get channel count */
    gst_structure_get_int (structure, "channels", &priv->channels);
    gst_structure_get_int (structure, "rate", &priv->rate);

    /* fix channels error in parser */
    if(priv->dec_params.mime == AMR_MIME_NB || 
            priv->dec_params.mime == AMR_MIME_WB)   //only AMR_MIME_WB+ contains stereo mode
        priv->channels = 1;
    
    GST_DEBUG("frame_format %s, channels %d, rate %d\n",
            priv->dec_params.frame_format, priv->channels, priv->rate);

    priv->duration = gst_util_uint64_scale_int (GST_SECOND, 1,
            50 * priv->channels);
    /* create reverse caps */
    src_caps = gst_caps_new_simple ("audio/x-raw-int",
            "channels", G_TYPE_INT, priv->channels,
            "width", G_TYPE_INT, 16,
            "depth", G_TYPE_INT, 16,
            "endianness", G_TYPE_INT, G_BYTE_ORDER,
            "rate", G_TYPE_INT, priv->rate,
            "signed", G_TYPE_BOOLEAN, TRUE,
            NULL);

    ret = gst_pad_set_caps (priv->srcpad, src_caps);
    gst_caps_unref (src_caps);
    if(!ret) {
        GST_WARNING("set caps to srcpad failed\n");
        gst_object_unref(self);
        return FALSE;
    }
    if(!mfw_gst_amrdec_init_codec(self)) {
        GST_WARNING("init codec failed\n");
        gst_object_unref(self);
        return FALSE;
    }

    priv->caps_set = TRUE;
    gst_object_unref(self);
    GST_DEBUG(" out of mfw_gst_amrdec_set_caps routine \n");
    return TRUE;
}

/*=============================================================================
FUNCTION:   mfw_gst_amrdec_init

DESCRIPTION:This function creates the pads on the elements and register the
			function pointers which operate on these pads.

ARGUMENTS PASSED:
        pointer the aac decoder element handle.

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
mfw_gst_amrdec_init(MfwGstAmrdec *self, MfwGstAmrdecClass *klass)
{
    MfwGstAmrdecPrivate *priv;
    self->priv = priv = MFW_GST_AMRDEC_GET_PRIVATE(self);
    GstElementClass *elem_class = GST_ELEMENT_CLASS(klass);
    GST_DEBUG(" \n in mfw_gst_amrdec_init routine \n");
    priv->sinkpad =
        gst_pad_new_from_template(gst_element_class_get_pad_template
                (elem_class, "sink"), "sink");

    priv->srcpad =
        gst_pad_new_from_template(gst_element_class_get_pad_template
                (elem_class, "src"), "src");

    priv->adapter = gst_adapter_new ();

    gst_element_add_pad(GST_ELEMENT(self), priv->sinkpad);
    gst_element_add_pad(GST_ELEMENT(self), priv->srcpad);

    gst_pad_set_setcaps_function(priv->sinkpad,
            GST_DEBUG_FUNCPTR(mfw_gst_amrdec_set_caps));
    gst_pad_set_chain_function(priv->sinkpad,
            GST_DEBUG_FUNCPTR(mfw_gst_amrdec_chain));
    gst_pad_set_event_function(priv->sinkpad,
            GST_DEBUG_FUNCPTR(mfw_gst_amrdec_sink_event));

    GST_DEBUG("\n out of mfw_gst_amrdec_init \n");
}

/*=============================================================================
FUNCTION:   mfw_gst_amrdec_class_init

DESCRIPTION:Initialise the class only once (specifying what signals,
            arguments and virtual functions the class has and setting up
            global state)
ARGUMENTS PASSED:
       	klass   - pointer to aac decoder's element class

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
mfw_gst_amrdec_class_init(MfwGstAmrdecClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *elem_class;

    gobject_class = G_OBJECT_CLASS(klass);
    elem_class = GST_ELEMENT_CLASS(klass);

    g_type_class_add_private (klass, sizeof(MfwGstAmrdecPrivate));

    gobject_class->set_property = mfw_gst_amrdec_set_property;
    gobject_class->get_property = mfw_gst_amrdec_get_property;
    gobject_class->dispose = mfw_gst_amrdec_dispose;

    elem_class->change_state = mfw_gst_amrdec_change_state;

    GST_DEBUG_CATEGORY_INIT(mfw_gst_amrdec_debug, "mfw_amrdecoder",
			    0, "FreeScale's AMR Decoder's Log");
}

/*=============================================================================
FUNCTION:  mfw_gst_amrdec_base_init

DESCRIPTION:
            aac decoder element details are registered with the plugin during
            _base_init ,This function will initialise the class and child
            class properties during each new child class creation


ARGUMENTS PASSED:
        Klass   -   pointer to aac decoder plug-in class

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
mfw_gst_amrdec_base_init(gpointer klass)
{
    GstCaps *capslist;
    GstPadTemplate *sink_template;
    capslist = gst_caps_new_empty ();
    GstElementClass *elem_class = GST_ELEMENT_CLASS(klass);
    gst_caps_append_structure (capslist,
            gst_structure_new ("audio/AMR",
                //"bitrate", GST_TYPE_INT_RANGE, 4750, 12200,
                "rate", G_TYPE_INT, 8000,
                "channels", GST_TYPE_INT_RANGE, 1, 8,
                NULL));
    gst_caps_append_structure (capslist,
            gst_structure_new ("audio/AMR-WB",
                //"bitrate", GST_TYPE_INT_RANGE, 6600, 23850,
                "rate", G_TYPE_INT, 16000,
                "channels", GST_TYPE_INT_RANGE, 1, 8,
                NULL));
    gst_caps_append_structure (capslist,
            gst_structure_new ("audio/AMR-WB+",
                //"bitrate", GST_TYPE_INT_RANGE, 5200, 48000,
                "rate", GST_TYPE_INT_RANGE, 16000, 48000,
                "channels", GST_TYPE_INT_RANGE, 1, 8,
                NULL));
    sink_template = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, capslist);

    gst_element_class_add_pad_template (elem_class, sink_template);
    gst_element_class_add_pad_template(elem_class,
            gst_static_pad_template_get(&src_factory));

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(elem_class, "amr audio decoder",
        "Codec/Decoder/Audio", "Decode compressed amr audio to raw data");
}


/*=============================================================================
FUNCTION:   plugin_init

DESCRIPTION:    special function , which is called as soon as the plugin or
                element is loaded and rmation returned by this function
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
    gst_tag_register ("channels", GST_TAG_FLAG_DECODED, G_TYPE_UINT,
            "number of channels","number of channels", NULL);
    gst_tag_register ("sampling_frequency", GST_TAG_FLAG_DECODED, G_TYPE_UINT,
            "sampling frequency (Hz)","sampling frequency (Hz)", NULL);
    return gst_element_register(plugin, "mfw_amrdecoder",
				GST_RANK_PRIMARY, MFW_GST_TYPE_AMRDEC);
}

FSL_GST_PLUGIN_DEFINE("amrdec", "amr audio decoder", plugin_init);



/*
 * Copyright (C) 2009, 2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_spdiftx.c
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Feb 23 2009 Dexter Ji <b01140@freescale.com>
 * - Initial version
 */

/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include <string.h>
#include "mfw_gst_utils.h"
#ifdef MEMORY_DEBUG
#include "mfw_gst_debug.h"
#endif

#include "mfw_gst_spdiftx.h"

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None. */

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
/* None. */

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/
#define MFW_GST_PCM_CAPS                    \
        "audio/x-raw-int, "                 \
        "channels=(int)[1,2], "             \
        "rate=(int) {32000,44100,48000} "   

#define MFW_GST_AC3_CAPS                    \
        "audio/x-ac3 "     


#define MFW_GST_SRC_CAPS                                        \
        "audio/x-raw-int, "                                     \
        "endianness = (gint) "G_STRINGIFY(G_BYTE_ORDER)", "     \
        "signed = (boolean) true, "                             \
        "width = (gint) 16,  "                                  \
        "depth = (gint) 16,  "                                  \
        "rate = (gint) { 32000,44100, 48000 }, "                \
        "channels = (gint) [ 1, 2 ]"
        
#ifdef MEMORY_DEBUG
    static Mem_Mgr mem_mgr = {0};
    
#define SPDIFTX_MALLOC( size)\
        dbg_malloc((&mem_mgr),(size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
#define SPDIFTX_FREE( ptr)\
        dbg_free(&mem_mgr, (ptr), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
    
#else
#define SPDIFTX_MALLOC(size)\
        g_malloc((size))
#define SPDIFTX_FREE( ptr)\
        g_free((ptr))
    
#endif
    
#define SPDIFTX_FATAL_ERROR(...) g_print(RED_STR(__VA_ARGS__))
#define SPDIFTX_FLOW(...) g_print(BLUE_STR(__VA_ARGS__))

#define DEFAULT_BITWIDTH        16
#define DEFAULT_BITDEPTH        16
#define DEFAULT_CHANNELS        2

#define SPDIF_DEFAULT_CHANNELS  2
#define SPDIF_DEFAULT_WIDTH     16
#define SPDIF_DEFAULT_DEPTH     16
#define SPDIF_DEFAULT_MEDIA_TYPE MEDIA_TYPE_AC3
#define SPDIF_DEFAULT_RATE      48000

enum {
    BURST_BIT_AC3_DATA = 0,
    BURST_BIT_RESERVED,
    BURST_BIT_PAUSE,
    BURST_BIT_MPEG1_L1,
    BURST_BIT_MPEG1_L23_MPEG2_WO_EXT,
    BURST_BIT_MPEG2_WI_EXT,
};

#define SPDIF_IEC937_SYNC1  0xF872
#define SPDIF_IEC937_SYNC2  0x4E1F

#define AC3_SYNC_WORD_LSB   0x770B
#define AC3_SYNC_WORD_MSB   0x0B77

#define DEFAULT_AC3_FRAME_LENGTH        1536
#define DEFAULT_BURST_PREAMBLE_HEAD_LEN 8

#define SPDIF_BURST_AC3_DATA 0x01<<BURST_BIT_AC3_DATA
#define SPDIF_BURST_PAUSE    0x01<<BURST_BIT_PAUSE


/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/
static GstStaticPadTemplate mfw_spdiftx_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(MFW_GST_PCM_CAPS ";" MFW_GST_AC3_CAPS)
);

static GstStaticPadTemplate mfw_spdiftx_src_factory =
    GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(MFW_GST_SRC_CAPS)
);

GST_DEBUG_CATEGORY_STATIC (gst_spdif_tx_debug);
#define GST_CAT_DEFAULT gst_spdif_tx_debug


/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/
static void mfw_gst_spdiftx_class_init(gpointer klass);
static void mfw_gst_spdiftx_base_init(gpointer klass);
static void mfw_gst_spdiftx_init(MfwGstSpdifTX *filter, gpointer gclass);

static void mfw_gst_spdiftx_set_property(GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec);
static void mfw_gst_spdiftx_get_property(GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec);

static gboolean mfw_gst_spdiftx_set_caps(GstPad *pad, GstCaps *caps);
static gboolean mfw_gst_spdiftx_sink_event(GstPad * pad, GstEvent * event);

static GstFlowReturn mfw_gst_spdiftx_chain (GstPad *pad, GstBuffer *buf);
static gboolean mfw_gst_find_ac3sync(MfwGstSpdifTX *filter);
static gboolean mfw_gst_spdiftx_pushavail(MfwGstSpdifTX *filter);


/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/
static GstElementClass *parent_class = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
 FUNCTION:          src_templ
 DESCRIPTION:       Generate the source pad template.    
 IMPORTANT NOTES:   None
=============================================================================*/
static GstPadTemplate *
src_templ(void)
{
    static GstPadTemplate *templ = NULL;
    if (!templ) {
        templ = gst_pad_template_new("src", 
                    GST_PAD_SRC, GST_PAD_ALWAYS, GST_CAPS_ANY);
    }
    return templ;
}


static void
mfw_gst_spdiftx_set_property(GObject *object, guint prop_id,
                             const GValue *value, GParamSpec *pspec)
{
    MfwGstSpdifTX *filter = MFW_GST_SPDIFTX (object);

    switch (prop_id)
    {
    case PROPER_ID_OUTPUT_CHANNELS:
        filter->channels = g_value_get_int(value);
        break;
    case PROPER_ID_OUTPUT_WIDTH:
        filter->width = g_value_get_int(value);
        break;
    case PROPER_ID_MEDIA_TYPE:
        filter->media_type = g_value_get_int(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
mfw_gst_spdiftx_get_property(GObject *object, guint prop_id,
                             GValue *value, GParamSpec *pspec)
{
    MfwGstSpdifTX *filter = MFW_GST_SPDIFTX (object);
    switch (prop_id)
    {
    case PROPER_ID_OUTPUT_CHANNELS:
        g_value_set_int(value, filter->channels);
        break;
    case PROPER_ID_OUTPUT_WIDTH:
        g_value_set_int(value, filter->width);
        break;
    case PROPER_ID_MEDIA_TYPE:
        g_value_set_int(value, filter->media_type);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/*=============================================================================
 FUNCTION:      mfw_gst_spdiftx_sink_event
 DESCRIPTION:       Handles an event on the sink pad.
 ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
 RETURN VALUE:
        TRUE       -    if event is sent to sink properly
        FALSE      -    if event is not sent to sink properly
 PRE-CONDITIONS:    None
 POST-CONDITIONS:   None
 IMPORTANT NOTES:   None
=============================================================================*/
static gboolean 
mfw_gst_spdiftx_sink_event(GstPad * pad, GstEvent * event)
{
    MfwGstSpdifTX *filter = MFW_GST_SPDIFTX (GST_PAD_PARENT(pad));
    gboolean result = TRUE;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
    {
        GstFormat format;
        gst_event_parse_new_segment(event, NULL, NULL, &format, NULL,
                    NULL, NULL);
        if (format == GST_FORMAT_TIME) {
            GST_DEBUG("\nCame to the FORMAT_TIME call\n");
        } else {
            GST_DEBUG("Dropping newsegment event in format %s",
                      gst_format_get_name(format));
            result = TRUE;
        }
        result = gst_pad_event_default(pad, event);
        break;
    }

    case GST_EVENT_EOS:
    {
        gboolean ret;
        GST_DEBUG("\nSPDIF TX: Get EOS event\n");
        if (filter->media_type == MEDIA_TYPE_AC3) {
            ret = mfw_gst_find_ac3sync(filter);
            while(ret == TRUE) {
                ret = mfw_gst_find_ac3sync(filter);
            };
            mfw_gst_spdiftx_pushavail(filter);
        }        
        result = gst_pad_event_default(pad, event);
        if (result != TRUE) {
            GST_ERROR("\n Error in pushing the event, result is %d\n",result);
        }
        break;
    }
    default:
    {
        result = gst_pad_event_default(pad, event);
        break;
    }

    }

    GST_DEBUG("Out of mfw_gst_spdiftx_sink_event() function \n ");
    return result;
}


static GstFlowReturn
mfw_gst_spdiftx_core_init(MfwGstSpdifTX *filter)
{
    filter->status = SPDIF_STATE_NULL;
    filter->sync1 = filter->sync2 = 0;
    return GST_FLOW_OK;
    
}
static void uint16to32(guint8 *in, guint8 *out,gint len)
{

    while (len > 0)
    {
        *out++ = *in++;
        *out++ = *in++;
        *out++ = 0;
        *out++ = 0;
        len -= 2;
    }
    return;
}

static GstFlowReturn mfw_gst_spdif_ac3_repack_push(MfwGstSpdifTX *filter,
                                   guint8 *data, guint32 len)
{
    GstBuffer *buf;
    GstFlowReturn ret;
    guint8 *pReSData;
    guint8 *pout;
    burst_struct *burst;
    GstCaps *caps = NULL;

    guint32 buf_len;

    if (filter->width == 32)
        buf_len = (DEFAULT_AC3_FRAME_LENGTH)<<(filter->channels+1);
    else
        buf_len = (DEFAULT_AC3_FRAME_LENGTH)<<(filter->channels);

    GST_DEBUG("SPDIF buffered length is %d.\n",buf_len);

    if (len > buf_len) {
        GST_WARNING("AC3 data length is exceed \
            the default SPDIF buffer length.\n");
        return GST_FLOW_ERROR;
    }
    /* 
     * Support width 16 and 32
     * The burst preamble is LSB mode 
     */
    if (filter->width == 32) {
        burst32_struct *burst;

        pReSData = (guint8 *)SPDIFTX_MALLOC(buf_len);
        memset(pReSData, 0, buf_len);
        uint16to32(data, pReSData + 16, len);
        burst = (burst32_struct *)pReSData;
        burst->pa = SPDIF_IEC937_SYNC1;
        burst->pb = SPDIF_IEC937_SYNC2;
        burst->pc = SPDIF_BURST_AC3_DATA;
        burst->pd = (len<<3);
       
    }
    else if (filter->width == 16) {
        pReSData = (guint8 *)SPDIFTX_MALLOC(DEFAULT_BURST_PREAMBLE_HEAD_LEN);
        burst = (burst_struct *)pReSData;
        memcpy((guint8 *)burst,(guint8 *)&filter->burst_info,sizeof(filter->burst_info));
        burst->pd = (len<<3);
        GST_DEBUG("Burst preamble:%4X,%4X,%4X,%4X\n",burst->pa,burst->pb,burst->pc,burst->pd);
    }
    else {
        GST_WARNING("Not support this width:%d.",filter->width);
        return FALSE;
    }

    if (!filter->capsSet) {
        caps = gst_caps_new_simple("audio/x-raw-int",
            "endianness", G_TYPE_INT, G_BYTE_ORDER, 
            "signed",     G_TYPE_BOOLEAN, TRUE, 
            "width",      G_TYPE_INT, filter->width, 
            "depth",      G_TYPE_INT, SPDIF_DEFAULT_DEPTH, 
            "rate",       G_TYPE_INT, SPDIF_DEFAULT_RATE,
            "channels",   G_TYPE_INT, SPDIF_DEFAULT_CHANNELS, 
            NULL);
        gst_pad_set_caps(filter->srcpad, caps);
        gst_caps_unref(caps);
        filter->capsSet = TRUE;
        
    }
    filter->src_caps = GST_PAD_CAPS(filter->srcpad);

    ret = gst_pad_alloc_buffer_and_set_caps(filter->srcpad, 0,
                                            buf_len,
                                            filter->src_caps, &buf);       

    if (ret != GST_FLOW_OK) {
        g_print("Could not alloc buffer, ret= %d.\n",ret);
        return FALSE;
    }   

    pout = (guint8 *)GST_BUFFER_DATA(buf);
    if (filter->width == 32) {
        memcpy(pout, pReSData, buf_len);

    }
    else if (filter->width == 16) {
        memset(pout, 0, buf_len);
        memcpy(pout,pReSData, DEFAULT_BURST_PREAMBLE_HEAD_LEN);
        memcpy(pout + DEFAULT_BURST_PREAMBLE_HEAD_LEN, data, len);
    }

    GST_BUFFER_SIZE(buf) = buf_len;
    GST_BUFFER_DATA(buf) = pout;

    SPDIFTX_FREE(pReSData);

    ret = gst_pad_push(filter->srcpad, buf);
    if (ret != GST_FLOW_OK) {
        GST_ERROR(" not able to push the data ,ret = %d.\n",ret);
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;

}

static gboolean mfw_gst_spdiftx_pushavail(MfwGstSpdifTX *filter) 
{
    guint len = gst_adapter_available(filter->pInAdapt);
    guint8 *data = gst_adapter_peek(filter->pInAdapt, len);

    guint16 *pIn = (guint16 *)data;

    /* Check if there is available data to push */
    if (len == 0)
        return TRUE;

    if ( (*pIn == AC3_SYNC_WORD_LSB) 
         || (*pIn == AC3_SYNC_WORD_MSB) ) {
         mfw_gst_spdif_ac3_repack_push(filter,data, len);
    }
    gst_adapter_flush(filter->pInAdapt,len);

    
    return TRUE;
}

static gboolean mfw_gst_find_ac3sync(MfwGstSpdifTX *filter) 
{
    guint len = gst_adapter_available(filter->pInAdapt);
    guint8 *data = gst_adapter_peek(filter->pInAdapt, len);
    guint8 *pout;
    guint32 buf_size;
    gint i;
    GstFlowReturn ret;

    i = 0;
  
    while (i < len - 1) {
        guint16 *pIn;
        pIn = (guint16 *)(data + i);
        switch (filter->status) {
        case SPDIF_STATE_NULL:
            if ( (*pIn == AC3_SYNC_WORD_LSB) 
                 || (*pIn == AC3_SYNC_WORD_MSB) ) {
                filter->status = SPDIF_STATE_SYNC1;
                filter->sync1 = i;
                GST_DEBUG("find sync word:%d\n",i);
            }
            break;
        case SPDIF_STATE_SYNC1:
           if ( (*pIn == AC3_SYNC_WORD_LSB) 
                 || (*pIn == AC3_SYNC_WORD_MSB) ) {
                filter->sync2 = i;
                if (filter->sync2 < filter->sync1)
                {
                    GST_WARNING("The second sync position is wrong");
                    filter->status = SPDIF_STATE_NULL;
                    gst_adapter_flush(filter->pInAdapt,filter->sync1);
                    return FALSE;
                    
                }
                buf_size = filter->sync2-filter->sync1;
                GST_DEBUG("find 2 sync word:%d,get valid data %d.\n",i,buf_size);
                ret = mfw_gst_spdif_ac3_repack_push(filter,
                    data+filter->sync1, buf_size);
                filter->status = SPDIF_STATE_NULL;
                gst_adapter_flush(filter->pInAdapt,filter->sync2);
                if (ret != GST_FLOW_OK)
                    return  FALSE;
                else
                    return TRUE;            

            }   
            break;
   
        default:
            break;
        }
        i++;
    }

    if (filter->status == SPDIF_STATE_SYNC1)
        filter->status = SPDIF_STATE_NULL;

    return FALSE;
}

static GstFlowReturn
mfw_gst_spdiftx_process_frame(MfwGstSpdifTX *filter, GstBuffer *buf)
{

    GstFlowReturn ret;

    if (filter->media_type == MEDIA_TYPE_PCM) {
        ret = gst_pad_push(filter->srcpad, buf);
        if (ret != GST_FLOW_OK) {
            GST_ERROR(" not able to push the data ,ret = %d.\n",ret);
            return GST_FLOW_ERROR;
        }
        return GST_FLOW_OK;
    }
    else if (filter->media_type == MEDIA_TYPE_AC3) {
        /* AC3 type handling */
        gst_adapter_push(filter->pInAdapt, buf);
        ret = mfw_gst_find_ac3sync(filter);

        while(ret == TRUE) {
            ret = mfw_gst_find_ac3sync(filter);
        };
        return GST_FLOW_OK;
        
        
    }
    else {
        /* FixME: Handle other media types.
         *
         */
        gst_buffer_unref(buf);
        GST_WARNING("Don't know how to handle this format.\n");
        return GST_FLOW_OK;
        
    }
    
}

gboolean mfw_gst_spdiftx_mediadetect(MfwGstSpdifTX *filter, GstBuffer *buf)
{
    guint8 *data = GST_BUFFER_DATA(buf);
    gint i;

    /* Set default media type is PCM */
    filter->media_type = MEDIA_TYPE_PCM;

    /* Check the first buffer, if there is syncword of AC3 */
    for (i = 0; i< GST_BUFFER_SIZE(buf); i++)
    {
        
        guint16 *p16 = (guint16 *)(data + i);
        if ( (*p16 == AC3_SYNC_WORD_LSB) || (*p16 == AC3_SYNC_WORD_MSB))
        {
            filter->media_type = MEDIA_TYPE_AC3;
            break;
        }
    }
    
    GST_DEBUG("\nDetected media type is:%d.\n",filter->media_type);
    return TRUE;

}

static GstFlowReturn
mfw_gst_spdiftx_chain (GstPad *pad, GstBuffer *buf)
{
    MfwGstSpdifTX *filter;
    GstFlowReturn ret;
    GstCaps *caps;
    g_return_if_fail (pad != NULL);
    g_return_if_fail (buf != NULL);
    
    filter = MFW_GST_SPDIFTX(GST_OBJECT_PARENT (pad));
    

    if (G_UNLIKELY(filter->init==FALSE)) {
        filter->pInAdapt = gst_adapter_new();
        ret = mfw_gst_spdiftx_core_init(filter);
        if (ret!=GST_FLOW_OK){
            gst_buffer_unref(buf);
            SPDIFTX_FATAL_ERROR("mfw_gst_spdiftx_core_init failed with return %d\n", ret);
            return ret;
        }

        /* Auto detect the input media type */
        mfw_gst_spdiftx_mediadetect(filter, buf);

        if (filter->media_type == MEDIA_TYPE_AC3) {
            GST_DEBUG
                ("Set SRC pad to AC3 type, need iec937 repack.");
            filter->burst_info.pa = SPDIF_IEC937_SYNC1;
            filter->burst_info.pb = SPDIF_IEC937_SYNC2;
            filter->burst_info.pc = SPDIF_BURST_AC3_DATA;

            if (!filter->capsSet) {
               caps = gst_caps_new_simple("audio/x-raw-int",
                        "endianness", G_TYPE_INT, G_BYTE_ORDER, 
                        "signed",     G_TYPE_BOOLEAN, TRUE, 
                        "width",      G_TYPE_INT, filter->width, 
                        "depth",      G_TYPE_INT, SPDIF_DEFAULT_DEPTH, 
                        "rate",       G_TYPE_INT, SPDIF_DEFAULT_RATE,
                        "channels",   G_TYPE_INT, SPDIF_DEFAULT_CHANNELS, 
                        NULL);
                gst_pad_set_caps(filter->srcpad, caps);
                gst_caps_unref(caps);
                filter->capsSet = TRUE;
        
            }

        }
        else if (filter->media_type == MEDIA_TYPE_PCM) {
            caps = gst_caps_new_simple("audio/x-raw-int",
                    "channel", G_TYPE_INT, 2, NULL);                    
            gst_pad_set_caps(filter->srcpad,caps);
            gst_caps_unref(caps);
            filter->capsSet = TRUE;
        }

        GST_DEBUG("src caps:%s\n",
            gst_caps_to_string(GST_PAD_CAPS(filter->srcpad)) );
        filter->init = TRUE;
    }
    
    ret = mfw_gst_spdiftx_process_frame(filter, buf);
    if (ret!=GST_FLOW_OK){
        GST_WARNING("failed with ret = %d\n", ret);
    }

    return ret;
}



static void
mfw_gst_spdiftx_base_init (gpointer klass)
{

    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&mfw_spdiftx_src_factory) );
    
    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&mfw_spdiftx_sink_factory) );
    
    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "spdif audio transmitter",
        "Filter/Converter/Audio", "Transfer audio raw data to spdif packet");
    
    return;
}


static void
mfw_gst_spdiftx_class_init (gpointer klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    int i;

    gobject_class = (GObjectClass*) klass;
    gstelement_class = (GstElementClass*) klass;

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = mfw_gst_spdiftx_set_property;
    gobject_class->get_property = mfw_gst_spdiftx_get_property;

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_OUTPUT_CHANNELS, 
        g_param_spec_int ("channels", "output channels", 
        "Output Channels of SPDIF", 
        1, 2,
        SPDIF_DEFAULT_CHANNELS, G_PARAM_READWRITE)
    );  

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_OUTPUT_WIDTH, 
        g_param_spec_int ("width", "output width", 
        "Output Width of SPDIF", 
        8, 32,
        SPDIF_DEFAULT_WIDTH, G_PARAM_READWRITE)
    );  

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_MEDIA_TYPE, 
        g_param_spec_int ("media-type", "input media type", 
        "Input Media Type: 0: PCM, 1: AC3", 
        0, 1,
        SPDIF_DEFAULT_MEDIA_TYPE, G_PARAM_READWRITE)
    ); 
    
    return;
}

static gboolean
mfw_gst_spdiftx_set_caps (GstPad *pad, GstCaps *caps)
{
    MfwGstSpdifTX *filter;
    GstPad *otherpad;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mime;

    filter = MFW_GST_SPDIFTX(gst_pad_get_parent (pad));
    gst_object_unref(filter);

    return TRUE;
}



static void 
mfw_gst_spdiftx_init (MfwGstSpdifTX *filter,
    gpointer gclass)

{

    filter->sinkpad = gst_pad_new_from_template (
    gst_static_pad_template_get (
        &mfw_spdiftx_sink_factory), 
        "sink");
    gst_pad_set_setcaps_function (filter->sinkpad, mfw_gst_spdiftx_set_caps);

    filter->srcpad = gst_pad_new_from_template (
    gst_static_pad_template_get (
        &mfw_spdiftx_src_factory), 
        "src");

    gst_pad_set_setcaps_function (filter->sinkpad, mfw_gst_spdiftx_set_caps);

    gst_pad_set_setcaps_function (filter->srcpad, mfw_gst_spdiftx_set_caps);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    gst_pad_set_chain_function (filter->sinkpad, mfw_gst_spdiftx_chain);

    filter->capsSet = FALSE;
    filter->init = FALSE;

    filter->media_type = MEDIA_TYPE_AC3;
    filter->channels = SPDIF_DEFAULT_CHANNELS;
    filter->width = SPDIF_DEFAULT_WIDTH;


    gst_pad_set_event_function(filter->sinkpad,
                   GST_DEBUG_FUNCPTR
                   (mfw_gst_spdiftx_sink_event));


#define MFW_GST_SPDIFTX_PLUGIN VERSION
    PRINT_PLUGIN_VERSION(MFW_GST_SPDIFTX_PLUGIN);
    return;
}

/*=============================================================================
FUNCTION:    mfw_gst_spdiftx_get_type
        
DESCRIPTION:    

ARGUMENTS PASSED:
        
  
RETURN VALUE:
        

PRE-CONDITIONS:
        None
 
POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

GType
mfw_gst_spdiftx_get_type(void)
{
    static GType mfw_spdiftx_type = 0;

    if (!mfw_spdiftx_type)
    {
        static const GTypeInfo mfw_spdiftx_info =
        {
            sizeof (MfwGstSpdifTXClass),
            (GBaseInitFunc) mfw_gst_spdiftx_base_init,
            NULL,
            (GClassInitFunc) mfw_gst_spdiftx_class_init,
            NULL,
            NULL,
            sizeof (MfwGstSpdifTX),
            0,
            (GInstanceInitFunc) mfw_gst_spdiftx_init,
        };
        
        mfw_spdiftx_type = g_type_register_static (GST_TYPE_ELEMENT,
            "MfwGstSpdifTX",
            &mfw_spdiftx_info, 
            0
        );

        GST_DEBUG_CATEGORY_INIT (gst_spdif_tx_debug, "mfw_spdiftx", 0,
            "spdif tx");
    }
    return mfw_spdiftx_type;
}


static gboolean
plugin_init (GstPlugin *plugin)
{
    return gst_element_register (plugin, "mfw_spdiftx",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_SPDIFTX);
}

FSL_GST_PLUGIN_DEFINE("spdiftx", "spdif audio transmitter", plugin_init);


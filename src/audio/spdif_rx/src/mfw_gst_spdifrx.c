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
 * Module Name:    mfw_gst_spdifrx.c
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Feb 16 2009 Dexter Ji <b01140@freescale.com>
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

#include "mfw_gst_spdifrx.h"

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None. */

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC (gst_spdif_rx_debug);
#define GST_CAT_DEFAULT gst_spdif_rx_debug

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/
#define MFW_GST_PCM_CAPS                    \
        "audio/x-raw-int, "                 \
        "channels=(int)[1,2], "             \
        "rate=(int) {32000,44100,48000} "   

#define MFW_GST_AC3_CAPS    \
        "audio/x-ac3 "     
    
#ifdef MEMORY_DEBUG
    static Mem_Mgr mem_mgr = {0};
    
#define SPDIFRX_MALLOC( size)\
        dbg_malloc((&mem_mgr),(size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
#define SPDIFRX_FREE( ptr)\
        dbg_free(&mem_mgr, (ptr), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
    
#else
#define SPDIFRX_MALLOC(size)\
        g_malloc((size))
#define SPDIFRX_FREE( ptr)\
        g_free((ptr))
    
#endif

enum {
    BURST_BIT_AC3_DATA = 0,
    BURST_BIT_RESERVED,
    BURST_BIT_PAUSE,
    BURST_BIT_MPEG1_L1,
    BURST_BIT_MPEG1_L23_MPEG2_WO_EXT,
    BURST_BIT_MPEG2_WI_EXT,
};
    
#define SPDIFRX_FATAL_ERROR(...) g_print(RED_STR(__VA_ARGS__))
#define SPDIFRX_FLOW(...) g_print(BLUE_STR(__VA_ARGS__))

#define DEFAULT_BITWIDTH 16
#define DEFAULT_BITDEPTH 16
#define DEFAULT_CHANNELS 2


#define SPDIF_IEC937_SYNC1 0xF872
#define SPDIF_IEC937_SYNC2 0x4E1F

#define DEFAULT_IEC_FRAME_LENGTH 1536
#define DEFAULT_BURST_PREAMBLE_HEAD_LEN 8

#define SPDIF_BURST_AC3_DATA 0x01<<BURST_BIT_AC3_DATA
#define SPDIF_BURST_PAUSE    0x01<<BURST_BIT_PAUSE

#define SPDIF_FORMAT_PCM    0
#define SPDIF_FORMAT_NONPCM 1

/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/
static GstStaticPadTemplate mfw_spdifrx_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(MFW_GST_PCM_CAPS)
);

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/
static void mfw_gst_spdifrx_class_init(gpointer klass);
static void mfw_gst_spdifrx_base_init(gpointer klass);
static void mfw_gst_spdifrx_init(MfwGstSpdifRX *filter, gpointer gclass);

static void mfw_gst_spdifrx_set_property(GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec);
static void mfw_gst_spdifrx_get_property(GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec);

static gboolean mfw_gst_spdifrx_set_caps(GstPad *pad, GstCaps *caps);
static gboolean mfw_gst_spdifrx_sink_event(GstPad * pad, GstEvent * event);

static GstFlowReturn mfw_gst_spdifrx_chain (GstPad *pad, GstBuffer *buf);
static gboolean mfw_gst_spdifrx_push(MfwGstSpdifRX *filter);
static gboolean mfw_gst_spdifrx_pushavail(MfwGstSpdifRX *filter); 


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
        templ = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_CAPS_ANY);

    }
    return templ;
}


static void
mfw_gst_spdifrx_set_property(GObject *object, guint prop_id,
                             const GValue *value, GParamSpec *pspec)
{
    MfwGstSpdifRX *filter = MFW_GST_SPDIFRX (object);

    switch (prop_id)
    {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
mfw_gst_spdifrx_get_property(GObject *object, guint prop_id,
                             GValue *value, GParamSpec *pspec)
{
    MfwGstSpdifRX *filter = MFW_GST_SPDIFRX (object);
    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/*=============================================================================
 FUNCTION:      mfw_gst_spdifrx_sink_event
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
mfw_gst_spdifrx_sink_event(GstPad * pad, GstEvent * event)
{
    MfwGstSpdifRX *filter = MFW_GST_SPDIFRX (GST_PAD_PARENT(pad));
    gboolean result = TRUE;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
    {
        GstFormat format;
        gint64 start, stop, position;
        gint64 nstart, nstop;
        gst_event_parse_new_segment(event, NULL, NULL, &format, &start,
                                    &stop, &position);

        if (format == GST_FORMAT_TIME) {
                result = gst_pad_push_event(filter->srcpad, event);
                if (TRUE != result) {
                    GST_ERROR
                         ("\n Error in pushing the event,result is %d\n", result);
            }

            GST_DEBUG("\nCame to the FORMAT_TIME call\n");
        } else {
            g_print("Newsegment event in format %s",
                      gst_format_get_name(format));
            result = gst_pad_push_event(filter->srcpad, event);
            if (TRUE != result) {
                GST_ERROR
                ("\n Error in pushing the event,result  is %d\n",
                 result);
            }
        }
        break;
    }

    case GST_EVENT_EOS:
    {
        gboolean ret;
        GST_DEBUG("\nSPDIF Receiver converter: Get EOS event\n");
        if (filter->format == SPDIF_FORMAT_NONPCM) {
            mfw_gst_spdifrx_push(filter);
            while(ret == TRUE) {
                ret = mfw_gst_spdifrx_push(filter);
            };
            mfw_gst_spdifrx_pushavail(filter);
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

    GST_DEBUG("Out of mfw_gst_spdifrx_sink_event() function \n ");
    return result;
}

static GstFlowReturn
mfw_gst_spdifrx_core_init(MfwGstSpdifRX *filter)
{
    filter->status = SPDIF_STATE_NULL;
    filter->sync1 = filter->sync2 = 0;

    return GST_FLOW_OK;
   
}

static void uint32to16(guint8 *in, guint8 *out,gint len)
{

    while (len > 3)
    {
        *out++ = *in++;
        *out++ = *in++;
        in +=2;
        len -= 4;
    }
    return;
}

static GstFlowReturn spdif_tx_push(MfwGstSpdifRX *filter,
                                   guint8 *data, gint len)
{
    GstBuffer *buf;
    GstFlowReturn ret;
    guint8 *pReSData;
    guint8 *pout;
    burst_struct *burst;
    GstCaps *caps = NULL;


    pReSData = (guint8 *)SPDIFRX_MALLOC(len);

    if (filter->width == 32)
        uint32to16(data, pReSData, len);
    else
        memcpy(pReSData, data, len);

    burst = (burst_struct *)pReSData;
    GST_DEBUG("burst:%4X,%4X,%4X,%4X\n",burst->pa,burst->pb,burst->pc,burst->pd);

    /* 
     *  
     * FIXME: Check the burst info, 
     *   only support AC3 media type currently.
     *
     */

    if (burst->pc & SPDIF_BURST_AC3_DATA) {
        GST_DEBUG("Found AC3 data,len:%d.\n",burst->pd>>3);
        filter->media_type = MEDIA_TYPE_AC3;

        caps = gst_caps_new_simple("audio/x-ac3",
                    "channel", G_TYPE_INT, 2, NULL);                    

        ret = gst_pad_alloc_buffer_and_set_caps(filter->srcpad, 0,
                                                burst->pd>>3,
                                                caps, &buf);       

        if (ret != GST_FLOW_OK) {
            g_print("Could not alloc buffer, ret= %d.\n",ret);
            return FALSE;
        }   

        pout = (guint8 *)GST_BUFFER_DATA(buf);
        memcpy(pout, pReSData + DEFAULT_BURST_PREAMBLE_HEAD_LEN, (burst->pd>>3));
        GST_BUFFER_SIZE(buf) = burst->pd >> 3;
        GST_BUFFER_DATA(buf) = pout;

        SPDIFRX_FREE(pReSData);

    }
    else {
        
        SPDIFRX_FREE(pReSData);
        
    }


    ret = gst_pad_push(filter->srcpad, buf);
    if (ret != GST_FLOW_OK) {
        GST_ERROR(" not able to push the data ,ret = %d.\n",ret);
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;

}

static gboolean mfw_gst_spdifrx_push(MfwGstSpdifRX *filter) 
{
    guint len = gst_adapter_available(filter->pInAdapt);
    guint8 *data = gst_adapter_peek(filter->pInAdapt, len);
    guint8 *pout;
    gint buf_size;
    gint i;
    GstFlowReturn ret;

    i = 0;
    len = len;
    
    while (i < len - 7) {
        guint16 *pIn;
        pIn = (guint16 *)(data + i);
        switch (filter->status) {
        case SPDIF_STATE_NULL:
            /*
             * FIXME:
             * The second sync word's width could be 32 or 16,
             * There is a case the sync word is 16bits while 
             * the width is 32bits.
             */
            if ( (*pIn == SPDIF_IEC937_SYNC1) 
                 && ((*(pIn+1) == SPDIF_IEC937_SYNC2) 
                      || (*(pIn+2) == SPDIF_IEC937_SYNC2)) ) {
                filter->status = SPDIF_STATE_SYNC1;
                filter->sync1 = i;
                GST_DEBUG("find sync word:%d\n",i);
 
            }
            break;
        case SPDIF_STATE_SYNC1:
            if ( (*pIn == SPDIF_IEC937_SYNC1) 
                 && ((*(pIn+1) == SPDIF_IEC937_SYNC2) 
                      || (*(pIn+2) == SPDIF_IEC937_SYNC2)) ) {
                filter->sync2 = i;
                buf_size = filter->sync2-filter->sync1;
                GST_DEBUG("find 2 sync word:%d,dump validate data %d.\n",i,buf_size);
                ret = spdif_tx_push(filter,data+filter->sync1, buf_size);
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

static gboolean mfw_gst_spdifrx_pushavail(MfwGstSpdifRX *filter) 
{
    guint len = gst_adapter_available(filter->pInAdapt);
    guint8 *data = gst_adapter_peek(filter->pInAdapt, len);
    GstFlowReturn ret;
    guint16 *pIn;

    if (len == 0)
        return TRUE;

    pIn = (guint16 *)data;

    if ( (*pIn == SPDIF_IEC937_SYNC1) 
         && ((*(pIn+1) == SPDIF_IEC937_SYNC2) 
              || (*(pIn+2) == SPDIF_IEC937_SYNC2)) ) {
     ret = spdif_tx_push(filter,data, len);
     gst_adapter_flush(filter->pInAdapt,len);

    }    

    return TRUE;
}

gboolean mfw_gst_spdif_formatdetect(MfwGstSpdifRX *filter, guint8* data, gint len)
{
    gint i;

    filter->format = SPDIF_FORMAT_PCM;

    /* Check the first buffer if there is syncword of AC3 */
    for (i = 0; i< len; i++)
    {
        guint16 *p16 = (guint16 *)(data + i);

        if (*p16 == SPDIF_IEC937_SYNC1) {
            if ((*(p16+1) == SPDIF_IEC937_SYNC2) || (*(p16+2) == SPDIF_IEC937_SYNC2))
            {
                filter->format = SPDIF_FORMAT_NONPCM;
                if (*(p16+1) == SPDIF_IEC937_SYNC2)
                    filter->width = 16;
                else if (*(p16+1) == SPDIF_IEC937_SYNC2)
                    filter->width = 32;
                break;
            }
        } 

    }
    
    g_print("\nDetect the spdif format:%d.\n",filter->format);
    return TRUE;

}



/*
 * It will first to find the sync word of non-PCM format,
 * Repack the buffer to AC3 raw data format, then resample it.
 *
 */
static GstFlowReturn
mfw_gst_spdifrx_process_frame(MfwGstSpdifRX *filter, GstBuffer *buf)
{
    gboolean ret;
    GstBuffer *outb;
    GstCaps *src_caps;
    guint len = gst_adapter_available(filter->pInAdapt);
    guint8 *data;

    if (!filter->format_detected) {
        if (len < (DEFAULT_IEC_FRAME_LENGTH<<2)) {
            GST_WARNING("get buffer len:%d, need more data.\n",len);
            return GST_FLOW_OK;
        }
        else {
            GST_DEBUG("get buffer len:%d, detect format.\n",len);
            data = gst_adapter_peek(filter->pInAdapt, len);
            mfw_gst_spdif_formatdetect(filter, data, len);
            filter->format_detected = TRUE;
            
        }        
    }

    filter->src_caps = GST_BUFFER_CAPS(buf);

    if (filter->format == SPDIF_FORMAT_PCM) {
        GstBuffer *buf;  
        gst_pad_alloc_buffer_and_set_caps(filter->srcpad, 0,
                                                len,
                                                filter->src_caps, &buf);           
        buf = gst_adapter_take_buffer(filter->pInAdapt,len);
        ret = gst_pad_push(filter->srcpad, buf);
        if (ret != GST_FLOW_OK) {
            GST_ERROR(" not able to push the data ,ret = %d.\n",ret);
            return GST_FLOW_ERROR;
        }

    }
    else {
        ret = mfw_gst_spdifrx_push(filter);

        while(ret == TRUE) {
            ret = mfw_gst_spdifrx_push(filter);
        };
    }
    return GST_FLOW_OK;
  
}


static GstFlowReturn
mfw_gst_spdifrx_chain (GstPad *pad, GstBuffer *buf)
{
    MfwGstSpdifRX *filter;
    GstFlowReturn ret;
    
    g_return_if_fail (pad != NULL);
    g_return_if_fail (buf != NULL);
    
    filter = MFW_GST_SPDIFRX(GST_OBJECT_PARENT (pad));

    if (G_UNLIKELY(filter->init==FALSE)) {

        filter->pInAdapt = gst_adapter_new();
        filter->pOutAdapt = gst_adapter_new();
        
        ret = mfw_gst_spdifrx_core_init(filter);
        if (ret!=GST_FLOW_OK){
            gst_buffer_unref(buf);
            SPDIFRX_FATAL_ERROR("mfw_gst_spdifrx_core_init failed with return %d\n", ret);
            return ret;
        }

        filter->init = TRUE;
    }

    gst_adapter_push(filter->pInAdapt, buf);
    
    ret = mfw_gst_spdifrx_process_frame(filter, buf);
    if (ret!=GST_FLOW_OK){
        GST_WARNING("mfw_gst_spdifrx_process_frame failed with ret=%d\n", ret);
    }

    return ret;
}



static void
mfw_gst_spdifrx_base_init (gpointer klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    gst_element_class_add_pad_template (
        element_class,
        src_templ()
    );


    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&mfw_spdifrx_sink_factory));

    
    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "spdif audio receiver",
        "Filter/Converter/Audio", "Transfer spdif packet to audio raw data");
    
    return;
}


static void
mfw_gst_spdifrx_class_init (gpointer klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    int i;

    gobject_class = (GObjectClass*) klass;
    gstelement_class = (GstElementClass*) klass;

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = mfw_gst_spdifrx_set_property;
    gobject_class->get_property = mfw_gst_spdifrx_get_property;

    return;
}

static gboolean
mfw_gst_spdifrx_set_caps (GstPad *pad, GstCaps *caps)
{
    MfwGstSpdifRX *filter;
    
    filter = MFW_GST_SPDIFRX(gst_pad_get_parent (pad));

    gst_object_unref(filter);

    return TRUE; 
}



static void 
mfw_gst_spdifrx_init     (MfwGstSpdifRX *filter,
    gpointer gclass)

{

    filter->sinkpad = gst_pad_new_from_template (
        gst_static_pad_template_get (
            &mfw_spdifrx_sink_factory), 
        "sink");
    gst_pad_set_setcaps_function (filter->sinkpad, mfw_gst_spdifrx_set_caps);

    filter->srcpad = gst_pad_new_from_template(src_templ(), "src");

    gst_pad_set_setcaps_function (filter->srcpad, mfw_gst_spdifrx_set_caps);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    gst_pad_set_chain_function (filter->sinkpad, mfw_gst_spdifrx_chain);

    filter->capsSet = FALSE;
    filter->init = FALSE;
    filter->format = SPDIF_FORMAT_PCM;
    filter->format_detected = FALSE;
    filter->width = 16;
    gst_pad_set_event_function(filter->sinkpad,
                   GST_DEBUG_FUNCPTR
                   (mfw_gst_spdifrx_sink_event));


#define MFW_GST_SPDIFRX_PLUGIN VERSION
    PRINT_PLUGIN_VERSION(MFW_GST_SPDIFRX_PLUGIN);
    return;
}

/*=============================================================================
FUNCTION:    mfw_gst_spdifrx_get_type
        
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
mfw_gst_spdifrx_get_type(void)
{
    static GType mfw_spdifrx_type = 0;

    if (!mfw_spdifrx_type)
    {
        static const GTypeInfo mfw_spdifrx_info =
        {
            sizeof (MfwGstSpdifRXClass),
            (GBaseInitFunc) mfw_gst_spdifrx_base_init,
            NULL,
            (GClassInitFunc) mfw_gst_spdifrx_class_init,
            NULL,
            NULL,
            sizeof (MfwGstSpdifRX),
            0,
            (GInstanceInitFunc) mfw_gst_spdifrx_init,
        };
        
        mfw_spdifrx_type = g_type_register_static (GST_TYPE_ELEMENT,
            "MfwGstSpdifRX",
            &mfw_spdifrx_info, 
            0
        );
        
        GST_DEBUG_CATEGORY_INIT (gst_spdif_rx_debug, "mfw_spdifrx", 0,
         "spdif rx");
    }
    return mfw_spdifrx_type;
}


static gboolean
plugin_init (GstPlugin *plugin)
{
    return gst_element_register (plugin, "mfw_spdifrx",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_SPDIFRX);
}



FSL_GST_PLUGIN_DEFINE("spdifrx", "spdif audio receiver", plugin_init);


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
 * Module Name:    mfw_gst_downmix.c
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Jan 20 2009 Sario HU <b01138@freescale.com>
 * - Initial version
 */

/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#include <gst/gst.h>
#include <string.h>
#include "mfw_gst_utils.h"
#ifdef MEMORY_DEBUG
#include "mfw_gst_debug.h"
#endif
#include "downmix_dec_interface.h"

#include "mfw_gst_downmix.h"

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
#define MFW_GST_DOWNMIX_CAPS    \
        "audio/x-raw-int"                  
    
#ifdef MEMORY_DEBUG
    static Mem_Mgr mem_mgr = {0};
    
#define DOWNMIX_MALLOC( size)\
        dbg_malloc((&mem_mgr),(size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
#define DOWNMIX_FREE( ptr)\
        dbg_free(&mem_mgr, (ptr), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
    
#else
#define DOWNMIX_MALLOC(size)\
        g_malloc((size))
#define DOWNMIX_FREE( ptr)\
        g_free((ptr))
    
#endif
    
#define DOWNMIX_FATAL_ERROR(...) g_print(RED_STR(__VA_ARGS__))
#define DOWNMIX_FLOW(...) g_print(BLUE_STR(__VA_ARGS__))

#define DEFAULT_BITWIDTH 16
#define DEFAULT_BITDEPTH 16
#define DEFAULT_CHANNELS 2

/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/
static GstStaticPadTemplate mfw_downmix_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(MFW_GST_DOWNMIX_CAPS)
);




/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/
static void	mfw_gst_downmix_class_init(gpointer klass);
static void	mfw_gst_downmix_base_init(gpointer klass);
static void	mfw_gst_downmix_init(MfwGstDownMix *filter, gpointer gclass);

static void	mfw_gst_downmix_set_property(GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec);
static void	mfw_gst_downmix_get_property(GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec);

static gboolean mfw_gst_downmix_set_caps(GstPad *pad, GstCaps *caps);
static gboolean mfw_gst_downmix_sink_event(GstPad * pad, GstEvent * event);

static GstFlowReturn mfw_gst_downmix_chain (GstPad *pad, GstBuffer *buf);

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
        GstCaps *caps;
        
        caps = gst_caps_new_simple("audio/x-raw-int", NULL);
        templ = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
    }
    return templ;
}


static void
mfw_gst_downmix_set_property(GObject *object, guint prop_id,
                             const GValue *value, GParamSpec *pspec)
{
    MfwGstDownMix *filter = MFW_GST_DOWNMIX (object);

    switch (prop_id)
    {
    case PROPER_ID_OUTPUT_CHANNEL_NUMBER:
        filter->disiredOutChannels = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
mfw_gst_downmix_get_property(GObject *object, guint prop_id,
                             GValue *value, GParamSpec *pspec)
{
    MfwGstDownMix *filter = MFW_GST_DOWNMIX (object);
    switch (prop_id)
    {
    case PROPER_ID_OUTPUT_CHANNEL_NUMBER:
        g_value_set_int(value, filter->disiredOutChannels);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/*=============================================================================
 FUNCTION:      mfw_gst_downmix_sink_event
 DESCRIPTION:       Handles an event on the sink pad.
 ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
 RETURN VALUE:
        TRUE       -	if event is sent to sink properly
        FALSE	   -	if event is not sent to sink properly
 PRE-CONDITIONS:    None
 POST-CONDITIONS:   None
 IMPORTANT NOTES:   None
=============================================================================*/
static gboolean 
mfw_gst_downmix_sink_event(GstPad * pad, GstEvent * event)
{
    MfwGstDownMix *filter = MFW_GST_DOWNMIX (GST_PAD_PARENT(pad));
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
        GST_DEBUG("\nAudio post processor: Get EOS event\n");
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

    GST_DEBUG("Out of mfw_gst_downmix_sink_event() function \n ");
    return result;
}

static void 
mfw_gst_downmix_free_mem(DM_Mem_Alloc_Info * meminfo)
{
    int i;
    void * buf;
    for (i=0;i<meminfo->dm_num_reqs;i++){
        if (buf=meminfo->mem_info_sub[i].app_base_ptr){
            DOWNMIX_FREE(buf);
            meminfo->mem_info_sub[i].app_base_ptr = NULL;
        }
    }
}


static GstFlowReturn
mfw_gst_downmix_alloc_mem(DM_Mem_Alloc_Info * meminfo)
{
    int i;
    void * buf;
    
    for (i=0;i<meminfo->dm_num_reqs;i++){
        buf = DOWNMIX_MALLOC(meminfo->mem_info_sub[i].dm_size);
        if (buf==NULL)
            goto allocErr;
        meminfo->mem_info_sub[i].app_base_ptr = buf;
    }
    return GST_FLOW_OK;
    
allocErr:
    mfw_gst_downmix_free_mem(meminfo);
    return GST_FLOW_ERROR;
    
}

static GstFlowReturn
mfw_gst_downmix_core_init(MfwGstDownMix *filter, GstBuffer *buf)
{
    GstCaps * caps;
    GstStructure * s;
    DM_RET_TYPE dmret = DM_OK;
    DM_Decode_Config * dmConfig;
    PPP_INPUTPARA *pppInput;
    PPP_INFO *pppInfo;
    gboolean ret;

    dmConfig = DOWNMIX_MALLOC(sizeof(DM_Decode_Config));
    pppInput = DOWNMIX_MALLOC(sizeof(PPP_INPUTPARA));
    pppInfo = DOWNMIX_MALLOC(sizeof(PPP_INFO));

    if ((dmConfig==NULL)||(dmConfig==NULL)||(dmConfig==NULL))
        goto InitErr;

    memset(dmConfig, 0, sizeof(DM_Decode_Config));
    memset(pppInput, 0, sizeof(PPP_INPUTPARA));
    memset(pppInfo, 0, sizeof(PPP_INFO));
    
    dmret = dm_query_dec_mem(dmConfig);
    ret = mfw_gst_downmix_alloc_mem(&dmConfig->dm_mem_info);
    if (ret!=GST_FLOW_OK)
        goto InitErr;
    
    dmConfig->dm_output_bitdepth = dmConfig->dm_input_bitdepth = DEFAULT_BITDEPTH;
    dmConfig->dm_output_bitwidth = dmConfig->dm_input_bitwidth = DEFAULT_BITWIDTH;
    dmConfig->dm_input_channels = DEFAULT_CHANNELS;
    dmConfig->dm_output_channels = filter->disiredOutChannels;
    dmConfig->dm_input_freq = 44100;
    dmConfig->dm_output_ch_mask = 
        dm_default_channel_mask(dmConfig->dm_output_channels);

    caps = gst_buffer_get_caps(buf);
    s = gst_caps_get_structure(caps, 0);

    gst_structure_get_int(s, "depth", &dmConfig->dm_input_bitdepth);
    gst_structure_get_int(s, "width", &dmConfig->dm_input_bitwidth);
    gst_structure_get_int(s, "rate", &dmConfig->dm_input_freq);
    gst_structure_get_int(s, "channels", &dmConfig->dm_input_channels);
    
    dmConfig->dm_input_ch_mask =
        dm_default_channel_mask(dmConfig->dm_input_channels);

    pppInput->bitwidth = dmConfig->dm_input_bitwidth;
    pppInput->blockmode = PPP_INTERLEAVE;
    pppInput->channelmask = dmConfig->dm_input_ch_mask;
    pppInput->decodertype = DecoderTypeAuto;
    pppInput->pppcontrolsize = 0;
    pppInput->samplerate = dmConfig->dm_input_freq;

    dmret  = dm_decode_init(dmConfig);
    if (ret!=GST_FLOW_OK){
        mfw_gst_downmix_free_mem(&dmConfig->dm_mem_info);    
        goto InitErr;
    }
    
    filter->dmConfig = dmConfig;
    filter->pppInput = pppInput;
    filter->pppInfo = pppInfo;

    return GST_FLOW_OK;

    
InitErr:
    if (dmConfig)
        DOWNMIX_FREE(dmConfig);
    if (pppInput)
        DOWNMIX_FREE(pppInput);
    if (pppInfo)
        DOWNMIX_FREE(pppInfo);
    
    return GST_FLOW_ERROR;
    
}

static GstFlowReturn
mfw_gst_downmix_process_frame(MfwGstDownMix *filter, GstBuffer *buf)
{
    gint samples;
    gint outsize;
    DM_Decode_Config * dmConfig = filter->dmConfig;
    PPP_INPUTPARA * pppInput = filter->pppInput;
    GstBuffer * outb;
    GstFlowReturn ret;
    DM_RET_TYPE dmret;
    
    if (G_UNLIKELY(filter->capsSet==FALSE)){
        GstCaps * caps;
        
        caps = gst_caps_new_simple("audio/x-raw-int",
            				       "endianness", G_TYPE_INT, G_BYTE_ORDER,
            				       "signed", G_TYPE_BOOLEAN, TRUE,
            				       "width", G_TYPE_INT, dmConfig->dm_output_bitwidth, 
            				       "depth", G_TYPE_INT, dmConfig->dm_output_bitdepth,
            				       "rate", G_TYPE_INT, dmConfig->dm_input_freq,
            				       "channels", G_TYPE_INT, dmConfig->dm_output_channels,
            				       NULL); 
        gst_pad_set_caps(filter->srcpad, caps);
        filter->capsSet = TRUE;
    }

    samples = GST_BUFFER_SIZE(buf)/(dmConfig->dm_input_bitwidth/8)/dmConfig->dm_input_channels;
    outsize = samples*(dmConfig->dm_output_bitwidth/8)*dmConfig->dm_output_channels;

    ret = gst_pad_alloc_buffer(filter->srcpad, 0, outsize, GST_PAD_CAPS(filter->srcpad), &outb);
    if (ret!=GST_FLOW_OK)
        return ret;

    pppInput->iptr = GST_BUFFER_DATA(buf);
    pppInput->optr = GST_BUFFER_DATA(outb);
    pppInput->inputsamples = pppInput->outputsamples = samples;

    dmret = dm_decode_frame(dmConfig, pppInput, filter->pppInfo);

    if (dmret!=DM_OK){
        DOWNMIX_FATAL_ERROR("dm_decode_frame failed with ret=%d\n", dmret);
        goto processErr;
    }

    ret = gst_pad_push(filter->srcpad, outb);
    gst_buffer_unref(buf);
    return ret;

processErr:
    gst_buffer_unref(buf);
    gst_buffer_unref(outb);
    return GST_FLOW_ERROR;
    
}


static GstFlowReturn
mfw_gst_downmix_chain (GstPad *pad, GstBuffer *buf)
{
    MfwGstDownMix *filter;
    GstFlowReturn ret;
    
    g_return_if_fail (pad != NULL);
    g_return_if_fail (buf != NULL);
    
    filter = MFW_GST_DOWNMIX(GST_OBJECT_PARENT (pad));
    

    if (G_UNLIKELY(filter->init==FALSE)) {
        
    	ret = mfw_gst_downmix_core_init(filter, buf);
        if (ret!=GST_FLOW_OK){
            gst_buffer_unref(buf);
            DOWNMIX_FATAL_ERROR("mfw_gst_downmix_core_init failed with return %d\n", ret);
            return ret;
        }

        filter->init = TRUE;
    }
    
    ret = mfw_gst_downmix_process_frame(filter, buf);
    if (ret!=GST_FLOW_OK){
        GST_WARNING("mfw_gst_downmix_process_frame failed with ret=%d\n", ret);
    }

    return ret;
}

/*=============================================================================
FUNCTION:    mfw_gst_downmix_get_type
        
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
mfw_gst_downmix_get_type(void)
{
    static GType mfw_downmix_type = 0;

    if (!mfw_downmix_type)
    {
        static const GTypeInfo mfw_downmix_info =
        {
            sizeof (MfwGstDownMixClass),
            (GBaseInitFunc) mfw_gst_downmix_base_init,
            NULL,
            (GClassInitFunc) mfw_gst_downmix_class_init,
            NULL,
            NULL,
            sizeof (MfwGstDownMix),
            0,
            (GInstanceInitFunc) mfw_gst_downmix_init,
        };
        
        mfw_downmix_type = g_type_register_static (GST_TYPE_ELEMENT,
            "MfwGstDownMix",
            &mfw_downmix_info, 
            0
        );
    }
    return mfw_downmix_type;
}

static void
mfw_gst_downmix_base_init (gpointer klass)
{

    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    gst_element_class_add_pad_template (
        element_class,
        src_templ()
    );
    
    gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&mfw_downmix_sink_factory));

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "audio down mixer",
        "Filter/Converter/Audio", "Downmix audio channels");
    
    return;
}


static void
mfw_gst_downmix_class_init (gpointer klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    int i;

    gobject_class = (GObjectClass*) klass;
    gstelement_class = (GstElementClass*) klass;

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = mfw_gst_downmix_set_property;
    gobject_class->get_property = mfw_gst_downmix_get_property;

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_OUTPUT_CHANNEL_NUMBER, 
        g_param_spec_int ("ochannels", "output channels", 
        "Desired Output Channels", 
        1, 6,
        DEFAULT_CHANNELS, G_PARAM_READWRITE)
    );  

    return;
}

static gboolean
mfw_gst_downmix_set_caps (GstPad *pad, GstCaps *caps)
{
    MfwGstDownMix *filter;
    GstPad *otherpad;


    
    filter = MFW_GST_DOWNMIX(gst_pad_get_parent (pad));

    gst_object_unref(filter);

    return TRUE;
}



static void 
mfw_gst_downmix_init	 (MfwGstDownMix *filter,
    gpointer gclass)

{

    filter->sinkpad = gst_pad_new_from_template (
    gst_static_pad_template_get (
        &mfw_downmix_sink_factory), 
        "sink");
    gst_pad_set_setcaps_function (filter->sinkpad, mfw_gst_downmix_set_caps);

    filter->srcpad = gst_pad_new_from_template(src_templ(), "src");

    gst_pad_set_setcaps_function (filter->srcpad, mfw_gst_downmix_set_caps);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    gst_pad_set_chain_function (filter->sinkpad, mfw_gst_downmix_chain);

    filter->capsSet = FALSE;
    filter->init = FALSE;

    gst_pad_set_event_function(filter->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_downmix_sink_event));

    filter->disiredOutChannels = DEFAULT_CHANNELS;

#define MFW_GST_DOWNMIX_PLUGIN VERSION
    PRINT_CORE_VERSION(DownmixCodecVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_DOWNMIX_PLUGIN);
    return;
}



static gboolean
plugin_init (GstPlugin *plugin)
{
    return gst_element_register (plugin, "mfw_downmixer",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_DOWNMIX);
}



FSL_GST_PLUGIN_DEFINE("adownmix", "audio down mixer", plugin_init);



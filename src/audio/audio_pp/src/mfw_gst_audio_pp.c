/*
 * Copyright (C) 2008-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_audio_pp.c
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Nov 03 2008 Sario HU <b01138@freescale.com>
 * - Initial version
 */




/*=============================================================================
                            INCLUDE FILES
=============================================================================*/


#include <gst/gst.h>
#include <string.h>
#include "peq_ppp_interface.h"
#include "mfw_gst_utils.h"
#ifdef MEMORY_DEBUG
#include "mfw_gst_debug.h"
#endif


#include "mfw_gst_audio_pp.h"




/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/



#define MFW_GST_AUDIO_PP_CAPS    \
    "audio/x-raw-int"                  


static char * effectname[] = {
    "user defined",
    "acoustic",
    "bass booster",
    "bass reducer",
    "classical",
    "dance",
    "deep",
    "electronic",
    "hip hop",
    "jazz",
    "latin",
    "loudness",
    "lounge",
    "piano",
    "pop",
    "R&B",
    "rock",
    "small speakers",
    "spoken word",
    "treble booster",
    "treble reducer",
    "vocal booster",
    "flat"
};


#ifdef MEMORY_DEBUG
static Mem_Mgr mem_mgr = {0};

#define AUDIO_PP_MALLOC( size)\
    dbg_malloc((&mem_mgr),(size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )

#define AUDIO_PP_FREE( ptr)\
    dbg_free(&mem_mgr, (ptr), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
    
#else
#define AUDIO_PP_MALLOC(size)\
    g_malloc((size))
#define AUDIO_PP_FREE( ptr)\
    g_free((ptr))
#endif


#define AUDIO_PP_FATAL_ERROR(...) g_print(RED_STR(__VA_ARGS__))
#define AUDIO_PP_FLOW(...) g_print(BLUE_STR(__VA_ARGS__))

#define PEQ_PREMODE_DEFAULT 0
#define PEQ_ATTENUATION_DEFAULT -40

#define PEQ_PARAMETERS_NUM  2

static GstStaticPadTemplate mfw_audio_pp_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(MFW_GST_AUDIO_PP_CAPS)
);

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/

/* None. */

/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/

enum {
    AUDIO_PP_ENABLED = 1, 
    PEQ_PREMODE,
    PEQ_ATTENUATION,
};

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/

static void	mfw_gst_audio_pp_class_init	 (gpointer klass);
static void	mfw_gst_audio_pp_base_init	 (gpointer klass);
static void	mfw_gst_audio_pp_init	 (MfwGstAudioPP *filter,
                                                  gpointer gclass);

static void	mfw_gst_audio_pp_set_property (GObject *object, guint prop_id,
                                                  const GValue *value,
					          GParamSpec *pspec);
static void	mfw_gst_audio_pp_get_property (GObject *object, guint prop_id,
                                                  GValue *value,
						  GParamSpec *pspec);

static gboolean mfw_gst_audio_pp_set_caps (GstPad *pad, GstCaps *caps);
static gboolean mfw_gst_audio_pp_sink_event(GstPad * pad,
					      GstEvent * event);
static GstFlowReturn mfw_gst_audio_pp_chain (GstPad *pad, GstBuffer *buf);
static GstPadLinkReturn mfw_gst_audio_pp_link( GstPad * pad, 
           const GstCaps * caps);

/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/
static GstElementClass *parent_class = NULL;

//static PPFeature * pp_feature_list = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
FUNCTION:    src_templ
        
DESCRIPTION: Generate the source pad template.    

IMPORTANT NOTES:
   	    None
=============================================================================*/

static GstPadTemplate *src_templ(void)
{
    static GstPadTemplate *templ = NULL;

    if (!templ) {
        GstCaps *caps;
        caps = gst_caps_new_simple("audio/x-raw-int", NULL);

        templ =
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
    }
    return templ;
}
#if 0

static PPFeatureControl * parametric_eq_probe(PPMallocMemory mallocfunc, PPFreeMemory freefunc)
{
    PPFeatureControl pcontrol = mallocfunc(
}

static void mfw_gst_audio_pp_register_feature(void * probe)
{
    PPFeature * ppfeature = AUDIO_PP_MALLOC(sizeof(PPFeature));
    
    ppfeature->next_feature = NULL;
    ppfeature->probe = probe;
    ppfeature->state = PP_FEATURE_DISABLED;
    ppfeature->status = PP_FEATURE_STATE_NOPROBE;
    if (pp_feature_list){
        PPFeature * tmp = pp_feature_list;
        while(tmp->next_feature)
            tmp=tmp->next_feature;
        tmp->next_feature = ppfeature;
    }else{
        pp_feature_list = ppfeature;
    }
}
#endif

static void
mfw_gst_audio_pp_set_property (GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    MfwGstAudioPP *filter = MFW_GST_AUDIO_PP (object);
    switch (prop_id)
    {
    case AUDIO_PP_ENABLED:
        
        if (g_value_get_boolean(value)){
            filter->status = PP_ENABLED;
            g_print(YELLOW_STR("Parametric EQ Enabled.\n", 0));
        }else{
            filter->status = PP_DISABLED;
            g_print(YELLOW_STR("Parametric EQ Disabled.\n", 0));
        }
        break;
    case PEQ_PREMODE:
        filter->premode = g_value_get_int(value);
        filter->ppfeatureinstance[0].newparameterapplied = TRUE;
        break;

    case PEQ_ATTENUATION:
        filter->attenuation = g_value_get_int(value);
        filter->ppfeatureinstance[0].newparameterapplied = TRUE;
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}

static void
mfw_gst_audio_pp_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    MfwGstAudioPP *filter = MFW_GST_AUDIO_PP (object);

    switch (prop_id) {
    case AUDIO_PP_ENABLED:
        g_value_set_boolean(value, filter->status);
        break;  
    case PEQ_PREMODE:
        g_value_set_int(value, filter->premode);
        break;
    case PEQ_ATTENUATION:
        g_value_set_int(value, filter->attenuation);
        break;        
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}

/*=============================================================================
FUNCTION:   	mfw_gst_audio_pp_sink_event

DESCRIPTION:	Handles an event on the sink pad.

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
static gboolean mfw_gst_audio_pp_sink_event(GstPad * pad,
					      GstEvent * event)
{

    MfwGstAudioPP *filter = MFW_GST_AUDIO_PP (GST_PAD_PARENT(pad));

    gboolean result = TRUE;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
	{
	    GstFormat format;
	    gst_event_parse_new_segment(event, NULL, NULL, &format, NULL,
					NULL, NULL);
	    if (format == GST_FORMAT_TIME) {
    		GST_DEBUG("\nCame to the FORMAT_TIME call\n");
            /* Handling the NEW SEGMENT EVENT */

           
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
    		GST_ERROR("\n Error in pushing the event, result is %d\n",
    			  result);
	    }
eos_exit:

	    break;
	}
    case GST_STATE_CHANGE_READY_TO_NULL:
	{
        filter->caps_set = FALSE;
        break;
	}
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    break;
	}

    }

    GST_DEBUG("Out of mfw_gst_audio_pp_sink_event() function \n ");
    return result;
}

void default_paralist(PEQ_PL *para_list_ptr)
{
	PEQ_I32 i,j;	
	/* init common parameters */
	para_list_ptr->ppp_inputpara.iptr = PEQ_NULL;				
	para_list_ptr->ppp_inputpara.optr = PEQ_NULL;						
	para_list_ptr->ppp_inputpara.inputsamples = 256; 			/* input samples need to process */
	para_list_ptr->ppp_inputpara.outputsamples = 256;			/* output samples */
	para_list_ptr->ppp_inputpara.bitwidth = 16;				
	para_list_ptr->ppp_inputpara.blockmode = PPP_INTERLEAVE;
	para_list_ptr->ppp_inputpara.decodertype = DecoderTypePCM;
	para_list_ptr->ppp_inputpara.samplerate = 44100;
    
	for(i=0;i<CHANNUM_MAX;i++)						/* set default VOR as 0 */
	{
		para_list_ptr->ppp_inputpara.VOR[i] = 0;
	}
	para_list_ptr->ppp_inputpara.pppcontrolsize = 0x158;			/* total private parameter size in Bytes */
	
	
	/* init PEQ private parameters */
	para_list_ptr->channelnumber = 2;					/* total input channel number is 2*/
	para_list_ptr->peqenablemask = 0x3;//both 2 channel currently 0x0;					/* all the channel is disable for peq */
	para_list_ptr->chennelfilterselect = 0;//0xFFFFFFFF;			/* do not select filter for every channel */
	para_list_ptr->premode = 0;						/* do not select predetermined scenes */
	para_list_ptr->calbandsperfrm = 4;					/* calculate 4 bands coeff per frame */
	for (i=0;i<NPCMCHANS;i++)						/* init bands number */
	{
		para_list_ptr->bandspergroup[i] = 0; 				/*every group has 0 band */
						
	}					
	for (i=0;i<NPCMCHANS;i++)						/* init every filter's parameters */
	{
		for(j=0;j<BANDSINGRP;j++)
		{
			para_list_ptr->group_band[i][j].Gain = 0;		/* gain is 0dB */			
			para_list_ptr->group_band[i][j].Q_value = 50;		/* Q is 0.5 */			
			para_list_ptr->group_band[i][j].FilterType = 0;		/* Peak Filter */			
			para_list_ptr->group_band[i][j].Fc = 320*(j+1);		/* Fc is 32Hz,64,128,256,1024,2048,... */				
		}								/* end of band loop */
	}									/* end of group loop */
	return;
}

static gboolean parametric_eq_set_parameter(PPFeatureInstance * pfinstance, PPFeatureParametersList* pmlist)
{
    PEQ_PL * ppl;
    int i;
    
    ppl = &(((PEQ_INSTANCE_CONTEXT *)pfinstance->priv)->peqpl);

    for (i=0;i<pmlist->numofparater;i++){
        switch(pmlist->parameters[i].pmid){
            case PEQ_PREMODE:
                ppl->premode = *((PEQ_I32 *)(pmlist->parameters[i].pparameter));
                g_print(YELLOW_STR("Set effect: %s\n", effectname[ppl->premode]));
                break;
            case PEQ_ATTENUATION:
                ppl->attenuation = *((PEQ_I32 *)(pmlist->parameters[i].pparameter));
                g_print(YELLOW_STR("Set attenuation: %d\n", ppl->attenuation));
                break;
            default:
                g_print(RED_STR("Unknown parameter id %d for PEQ\n", (pmlist->parameters[i].pmid)));
                break;
        }
    }
    return TRUE;
}

static gboolean parametric_eq_do_process(PPFeatureInstance * pfinstance, GstBuffer * gstbuf)
{
    PEQ_PL * ppl = &(((PEQ_INSTANCE_CONTEXT *)pfinstance->priv)->peqpl);
    PEQ_PPP_Config * ppconfig = &(((PEQ_INSTANCE_CONTEXT *)pfinstance->priv)->peqconfig);
    PEQ_INFO * ppinfo = &(((PEQ_INSTANCE_CONTEXT *)pfinstance->priv)->pqeinfo);
    
    gint samples = GST_BUFFER_SIZE(gstbuf)/(ppl->ppp_inputpara.bitwidth/8)/ppl->channelnumber;
    
    //AUDIO_PP_FLOW("doprocess %ld\n", samples);
    
    ppl->ppp_inputpara.iptr = ppl->ppp_inputpara.optr = GST_BUFFER_DATA(gstbuf);
    ppl->ppp_inputpara.inputsamples = ppl->ppp_inputpara.outputsamples = samples;

    peq_ppp_frame(ppconfig, ppl, ppinfo);
    return TRUE;
}

static gboolean parametric_eq_instancelize(PPFeatureInstance * pfinstance, GstBuffer * gstbuf)
{
    PEQ_PPP_Config * pconfig;
    PEQ_PL * ppl;
    PEQ_Mem_Alloc_Info_Sub * mem_sub;

    char * tmp; 
    int i;

    if (pfinstance->priv = AUDIO_PP_MALLOC(sizeof(PEQ_INSTANCE_CONTEXT))){
        memset(pfinstance->priv, 0, sizeof(PEQ_INSTANCE_CONTEXT));
    }else{
        goto ErrClean;
    }

    if (pfinstance->pmlist = AUDIO_PP_MALLOC(sizeof(PPFeatureParameter)*PEQ_PARAMETERS_NUM+sizeof(guint))){
        memset(pfinstance->pmlist, 0, (sizeof(PPFeatureParameter)*PEQ_PARAMETERS_NUM+sizeof(guint)));
    }else{
        goto ErrClean;
    }

    pconfig = &(((PEQ_INSTANCE_CONTEXT *)pfinstance->priv)->peqconfig);
    
    peq_query_ppp_mem(pconfig);
    
    for (i=0;i<pconfig->peq_mem_info.peq_num_reqs;i++){
        mem_sub = &(pconfig->peq_mem_info.mem_info_sub[i]);
        tmp = AUDIO_PP_MALLOC(mem_sub->peq_size);
        if (tmp){
            mem_sub->app_base_ptr = tmp;
        }else{
            goto ErrClean;
        }
    }

    peq_ppp_init(pconfig);

    ppl = &(((PEQ_INSTANCE_CONTEXT *)pfinstance->priv)->peqpl);
    
    default_paralist(ppl);

    GstCaps* caps = gst_buffer_get_caps(gstbuf);
    GstStructure *structure = gst_caps_get_structure(caps, 0);

    guint samplerate = 0;
    gst_structure_get_int(structure, "rate",
    			      &samplerate);
    if (samplerate){
        //AUDIO_PP_FLOW("get sample rate %ld\n", samplerate);
        ppl->ppp_inputpara.samplerate = samplerate;
    }
        
    return TRUE;
    
ErrClean:
    if (pfinstance->priv){
        pconfig = &(((PEQ_INSTANCE_CONTEXT *)pfinstance->priv)->peqconfig);
        for (i=0;i<pconfig->peq_mem_info.peq_num_reqs;i++){
            mem_sub = &(pconfig->peq_mem_info.mem_info_sub[i]);
            if (mem_sub->app_base_ptr){
                AUDIO_PP_FREE(mem_sub->app_base_ptr);
            }
        }
        AUDIO_PP_FREE(pfinstance->priv);
        pfinstance->priv = NULL;
    }

    if (pfinstance->pmlist){
        AUDIO_PP_FREE(pfinstance->pmlist);
        pfinstance->pmlist = NULL;
    }
    return FALSE;
}

/* chain function
 * this function does the actual processing
 */

static GstFlowReturn
mfw_gst_audio_pp_chain (GstPad *pad, GstBuffer *buf)
{
    MfwGstAudioPP *filter;
    GstFlowReturn ret = GST_FLOW_OK;
    g_return_if_fail (pad != NULL);
    g_return_if_fail (buf != NULL);
    //AUDIO_PP_FLOW("enter %s\n", __FUNCTION__);
    filter = MFW_GST_AUDIO_PP(GST_OBJECT_PARENT (pad));
    if (filter->status==PP_ENABLED){
        PPFeatureInstance * pfeatureinstance0 = &filter->ppfeatureinstance[0];
        if (G_UNLIKELY(pfeatureinstance0->state==PP_EMPTY)){
            if (parametric_eq_instancelize(pfeatureinstance0, buf)==TRUE){   
                pfeatureinstance0->newparameterapplied = TRUE;
                filter->caps_set=TRUE;
            }else{
                ret = GST_FLOW_ERROR;
                gst_buffer_unref(buf);
                goto ErrChain;
            }
            
        }

        if (G_UNLIKELY(pfeatureinstance0->newparameterapplied)){
            PPFeatureParametersList * pmlist = pfeatureinstance0->pmlist;
            
            pmlist->numofparater = PEQ_PARAMETERS_NUM;
            
            pmlist->parameters[0].pmid = PEQ_PREMODE;
            pmlist->parameters[0].pparameter = (void *)&filter->premode;
            pmlist->parameters[1].pmid = PEQ_ATTENUATION;
            pmlist->parameters[1].pparameter = (void *)&filter->attenuation;

            parametric_eq_set_parameter(pfeatureinstance0,pmlist);
            pfeatureinstance0->newparameterapplied = FALSE;
        }
        pfeatureinstance0->state = PP_BUSY;
        parametric_eq_do_process(pfeatureinstance0,buf);
        pfeatureinstance0->state = PP_IDLE;
         
    }else{
    }
    gst_pad_push(filter->srcpad, buf);    


ErrChain:
    return ret;
    
}

/*=============================================================================
FUNCTION:    mfw_gst_audio_pp_get_type
        
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
mfw_gst_audio_pp_get_type(void)
{
    static GType mfw_audio_pp_type = 0;

    if (!mfw_audio_pp_type)
    {
        static const GTypeInfo mfw_audio_pp_info =
        {
            sizeof (MfwGstAudioPPClass),
            (GBaseInitFunc) mfw_gst_audio_pp_base_init,
            NULL,
            (GClassInitFunc) mfw_gst_audio_pp_class_init,
            NULL,
            NULL,
            sizeof (MfwGstAudioPP),
            0,
            (GInstanceInitFunc) mfw_gst_audio_pp_init,
        };
        
        mfw_audio_pp_type = g_type_register_static (GST_TYPE_ELEMENT,
            "MfwGstAudioPP",
            &mfw_audio_pp_info, 
            0
        );
    }
    return mfw_audio_pp_type;
}

static void
mfw_gst_audio_pp_base_init (gpointer klass)
{

    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    gst_element_class_add_pad_template (
        element_class,
        src_templ()
    );
    
    gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&mfw_audio_pp_sink_factory));
    
    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "audio post equalizer",
        "Filter/Converter/Audio", "Audio post equalizer");
    
    return;
}

/* Initialize the plugin's class */
static void
mfw_gst_audio_pp_class_init (gpointer klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class = (GObjectClass*) klass;
    gstelement_class = (GstElementClass*) klass;

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = mfw_gst_audio_pp_set_property;
    gobject_class->get_property = mfw_gst_audio_pp_get_property;

    /* CHECKME */
    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        AUDIO_PP_ENABLED, 
        g_param_spec_boolean ("enable", "enable", 
        "Enable Audio Post-processor.", 
        TRUE, G_PARAM_READWRITE)
    ); 

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PEQ_PREMODE, 
        g_param_spec_int ("eqmode", "eqmode fmt", 
        "EQ Predefined Mode. 0(disable) ~ 22", 
        0, 22,
        PEQ_PREMODE_DEFAULT, G_PARAM_READWRITE)
    );  

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PEQ_ATTENUATION, 
        g_param_spec_int ("attenuation", "pre attenuation", 
        "EQ Attenuation Mode in dB. -100~20", 
        -100, 20,
        PEQ_ATTENUATION_DEFAULT, G_PARAM_READWRITE)
    ); 

    return;
}



/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
mfw_gst_audio_pp_set_caps (GstPad *pad, GstCaps *caps)
{
    MfwGstAudioPP *filter;
    GstPad *otherpad;


    
    filter = MFW_GST_AUDIO_PP(gst_pad_get_parent (pad));

    if (!gst_pad_set_caps (filter->srcpad, caps)) {
        GST_ERROR("set caps error\n");
        gst_object_unref(filter);
        return FALSE;
    }

    

    gst_object_unref(filter);

    return TRUE;
}


/* Initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void 
mfw_gst_audio_pp_init	 (MfwGstAudioPP *filter,
    gpointer gclass)

{

    filter->sinkpad = gst_pad_new_from_template (
    gst_static_pad_template_get (
        &mfw_audio_pp_sink_factory), 
        "sink");
    gst_pad_set_setcaps_function (filter->sinkpad, mfw_gst_audio_pp_set_caps);

    filter->srcpad = gst_pad_new_from_template(src_templ(), "src");

    gst_pad_set_setcaps_function (filter->srcpad, mfw_gst_audio_pp_set_caps);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    gst_pad_set_chain_function (filter->sinkpad, mfw_gst_audio_pp_chain);
    /* Set buffer allocation to pass the buffer allocation 
     *   to the SINK element 
     */
    //gst_pad_set_bufferalloc_function(filter->sinkpad,
    //    mfw_gst_audio_pp_bufferalloc);

    filter->caps_set = FALSE;
    filter->status = PP_DISABLED;

    gst_pad_set_event_function(filter->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_audio_pp_sink_event));

    memset(&filter->ppfeatureinstance, 0, sizeof(PPFeatureInstance)*MAX_AUDIO_PP_FEATURE_INSTANCE);

    filter->premode = PEQ_PREMODE_DEFAULT;
    filter->attenuation = PEQ_ATTENUATION_DEFAULT;

    #define MFW_GST_AUDIO_PP_PLUGIN VERSION
    PRINT_CORE_VERSION(PEQPPPVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_AUDIO_PP_PLUGIN);
    return;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 *
 * exchange the string 'plugin' with your elemnt name
 */
static gboolean
plugin_init (GstPlugin *plugin)
{
    return gst_element_register (plugin, "mfw_audio_pp",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_AUDIO_PP);
}

FSL_GST_PLUGIN_DEFINE("audiopeq", "audio post equalizer", plugin_init);

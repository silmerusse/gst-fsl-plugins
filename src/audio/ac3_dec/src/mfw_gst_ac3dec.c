/*
 * Copyright (c) 2008-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_ac3dec.c
 *
 * Description:    Implementation of ac3 plugin for Gstreamer using push
 *                 based method.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 */


/*==================================================================================================
                            INCLUDE FILES
===================================================================================================*/
#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <gst/audio/multichannel.h>
#include "ac3d_dec_interface.h"
#include "mfw_gst_ac3dec.h"
#include "mfw_gst_utils.h"

/*==================================================================================================
                                     LOCAL CONSTANTS
==================================================================================================*/
#define		NFSCOD			3			/* # defined sample rates */
#define		NDATARATE		38			/* # defined data rates */

static const AC3D_INT16
frmsizetab[NFSCOD][NDATARATE] =
{	{	64, 64, 80, 80, 96, 96, 112, 112,
		128, 128, 160, 160, 192, 192, 224, 224,
		256, 256, 320, 320, 384, 384, 448, 448,
		512, 512, 640, 640, 768, 768, 896, 896,
		1024, 1024, 1152, 1152, 1280, 1280 },
	{	69, 70, 87, 88, 104, 105, 121, 122,
		139, 140, 174, 175, 208, 209, 243, 244,
		278, 279, 348, 349, 417, 418, 487, 488,
		557, 558, 696, 697, 835, 836, 975, 976,
		1114, 1115, 1253, 1254, 1393, 1394 },
	{	96, 96, 120, 120, 144, 144, 168, 168,
		192, 192, 240, 240, 288, 288, 336, 336,
		384, 384, 480, 480, 576, 576, 672, 672,
		768, 768, 960, 960, 1152, 1152, 1344, 1344,
		1536, 1536, 1728, 1728, 1920, 1920 } };

static const AC3D_INT32 sampleratetab[NFSCOD] = {48000,44100,32000};


/*==================================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/


typedef struct
{
        MfwGstAc3DecInfo    *ac3dec_info;
	AC3D_UINT8          *input_ptr;

}AC3_callback;


enum {
    ID_0,
    ID_OUTPUT_MODE,
    ID_PCM_DEPTH,
    ID_OUT_LFE,
    ID_STEREO_MODE,
    ID_DUAL_MONO_MODE,
    ID_KARAOKE,
    ID_PCM_SCALE,
    ID_COMPRESS_MODE,
    ID_DRC_LOW,
    ID_DRC_HI,
    ID_DEBUG,
    ID_PROFILE
};


/* defines sink pad  properties of ac3decoder element */
static GstStaticPadTemplate mfw_gst_ac3dec_sink_factory =
GST_STATIC_PAD_TEMPLATE("sink",
			GST_PAD_SINK,
			GST_PAD_ALWAYS,
			GST_STATIC_CAPS("audio/x-ac3, channels = (int) [ 1, 6 ];"
			"audio/x-3ca, channels = (int) [ 1, 6 ]")
    );



/* defines the source pad  properties of ac3decoder element */
static GstStaticPadTemplate mfw_gst_ac3dec_src_factory =
GST_STATIC_PAD_TEMPLATE("src",
			GST_PAD_SRC,
			GST_PAD_ALWAYS,
			GST_STATIC_CAPS("audio/x-raw-int, "
					"endianness = (int) "
					G_STRINGIFY(G_BYTE_ORDER) ", "
					"signed = (boolean) true, "
					"width = (int) 16, "
					"depth = (int) 16, "
					"rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
					"channels = (int) [ 1, 6 ];"
			                "audio/x-raw-int, "
					"endianness = (int) "
					G_STRINGIFY(G_BYTE_ORDER) ", "
					"signed = (boolean) true, "
					"width = (int) 32, "
					"depth = (int) 32, "
					"rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
					"channels = (int) [ 1, 6 ]")
    );

/*==================================================================================================
                                        LOCAL MACROS
==================================================================================================*/
/*alignment for memory address */
#define LONG_BOUNDARY    4
#define MEMORY_ALIGNMENT 4

/* bala for memory fix */
#define AC3_MAX_ADAPTER_SIZE 3840

#define AC3_BIG_STARTCODE 0x00000B77
#define AC3_LITTLE_STARTCODE 0x0000770B

/* the clock in MHz for IMX31 to be changed for other platforms */
#define PROCESSOR_CLOCK 532

#define	GST_CAT_DEFAULT	mfw_gst_ac3dec_debug

#define	GST_TAG_MFW_AC3_CHANNELS		"channels"
#define GST_TAG_MFW_AC3_SAMPLING_RATE	        "sampling_frequency"

/*==================================================================================================
                                      STATIC VARIABLES
==================================================================================================*/

static GstElementClass *parent_class = NULL;

/*==================================================================================================
                                 STATIC FUNCTION PROTOTYPES
==================================================================================================*/

GST_DEBUG_CATEGORY_STATIC(mfw_gst_ac3dec_debug);
static void mfw_gst_ac3dec_class_init(MfwGstAc3DecInfoClass * klass);
static void mfw_gst_ac3dec_base_init(gpointer klass);
static void mfw_gst_ac3dec_init(MfwGstAc3DecInfo * ac3dec_info);
GType mfw_gst_type_ac3dec_get_type(void);
static GstStateChangeReturn mfw_gst_ac3dec_change_state(GstElement *,
							GstStateChange);
static gboolean mfw_gst_ac3dec_sink_event(GstPad * pad, GstEvent * event);
static GstFlowReturn mfw_gst_ac3dec_chain(GstPad * pad, GstBuffer * buf);
static GstFlowReturn decode_ac3_chunk(MfwGstAc3DecInfo * ac3dec_info);
static void *alloc_fast(gint size);
static void *alloc_slow(gint size);


/* Functions used for seeking */
static gboolean mfw_gst_ac3dec_src_query(GstPad * pad, GstQuery * query);
static gboolean mfw_gst_ac3dec_seek(MfwGstAc3DecInfo *, GstPad *,
				    GstEvent *);
static gboolean mfw_gst_ac3dec_src_event(GstPad *pad, GstEvent *event);
static void mfw_gst_ac3dec_dispose(GObject * object);
static void mfw_gst_ac3dec_set_index(GstElement * element,
				     GstIndex * index);
static GstIndex *mfw_gst_ac3dec_get_index(GstElement * element);
static void mfw_gst_ac3dec_set_property(GObject * object, guint prop_id,
					const GValue * value,
					GParamSpec * pspec);
static void mfw_gst_ac3dec_get_property(GObject * object,
					guint prop_id, GValue * value,
					GParamSpec * pspec);
static const GstQueryType *mfw_gst_ac3dec_get_query_types(GstPad * pad);

/*==================================================================================================
                                     GLOBAL VARIABLES
==================================================================================================*/


/*==================================================================================================
                                     LOCAL FUNCTIONS
==================================================================================================*/


/*=============================================================================
FUNCTION: mfw_gst_ac3dec_set_property

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
mfw_gst_ac3dec_set_property(GObject * object, guint prop_id,
			    const GValue * value, GParamSpec * pspec)
{
    MfwGstAc3DecInfo *ac3dec_info = MFW_GST_AC3DEC(object);
    switch (prop_id)
    {
        case ID_OUTPUT_MODE:
            ac3dec_info->outputmode = g_value_get_int(value);
            GST_DEBUG("outputmode=%d\n", ac3dec_info->outputmode);
            break;

        case ID_PCM_DEPTH:
            ac3dec_info->wordsize = g_value_get_int(value);
            GST_DEBUG("pcm depth=%d\n", ac3dec_info->wordsize);
            break;

        case ID_OUT_LFE:
            ac3dec_info->outlfeon = g_value_get_int(value);
            GST_DEBUG("out lfe=%d\n", ac3dec_info->outlfeon);
            break;

        case ID_STEREO_MODE:
            ac3dec_info->stereomode = g_value_get_int(value);
            GST_DEBUG("stereo mode=%d\n", ac3dec_info->stereomode);
            break;

        case ID_DUAL_MONO_MODE:
            ac3dec_info->dualmonomode = g_value_get_int(value);
            GST_DEBUG("dual mono mode=%d\n", ac3dec_info->dualmonomode);
            break;

        case ID_KARAOKE:
#ifdef KCAPABLE
            ac3dec_info->kcapablemode = g_value_get_int(value);
            GST_DEBUG("karaok capable=%d\n", ac3dec_info->kcapablemode);
#endif
            break;

        case ID_PCM_SCALE:
            ac3dec_info->pcmscalefac = g_value_get_float(value);
            GST_DEBUG("pcm scale factor=%f\n", ac3dec_info->pcmscalefac);
            break;

        case ID_COMPRESS_MODE:
            ac3dec_info->compmode = g_value_get_int(value);
            GST_DEBUG("compress mode=%d\n", ac3dec_info->compmode);
            break;

        case ID_DRC_LOW:
            ac3dec_info->dynrngscalelow = g_value_get_float(value);
            GST_DEBUG("drc range low=%f\n", ac3dec_info->dynrngscalelow);
            break;

        case ID_DRC_HI:
            ac3dec_info->dynrngscalehi = g_value_get_float(value);
            GST_DEBUG("drc range hi=%f\n", ac3dec_info->dynrngscalehi);
            break;

        case ID_DEBUG:
            ac3dec_info->debug_arg = g_value_get_int(value);
            GST_DEBUG("debug=%d\n", ac3dec_info->debug_arg);
            break;

        case ID_PROFILE:
            ac3dec_info->profile = g_value_get_boolean(value);
            GST_DEBUG("profile=%d\n", ac3dec_info->profile);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


/*=============================================================================
FUNCTION: mfw_gst_ac3dec_get_property

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
mfw_gst_ac3dec_get_property(GObject * object, guint prop_id,
			    GValue * value, GParamSpec * pspec)
{

    MfwGstAc3DecInfo *ac3dec_info = MFW_GST_AC3DEC(object);

    switch (prop_id)
    {
        case ID_OUTPUT_MODE:
            GST_DEBUG("outputmode=%d\n", ac3dec_info->outputmode);
            g_value_set_int(value, ac3dec_info->outputmode);
            break;

        case ID_PCM_DEPTH:
            GST_DEBUG("pcm depth=%d\n", ac3dec_info->wordsize);
            g_value_set_int(value, ac3dec_info->wordsize);
            break;

        case ID_OUT_LFE:
            GST_DEBUG("out lfe=%d\n", ac3dec_info->outlfeon);
            g_value_set_int(value, ac3dec_info->outlfeon);
            break;

        case ID_STEREO_MODE:
            GST_DEBUG("stereo mode=%d\n", ac3dec_info->stereomode);
            g_value_set_int(value, ac3dec_info->stereomode);
            break;

        case ID_DUAL_MONO_MODE:
            GST_DEBUG("dual mono mode=%d\n", ac3dec_info->dualmonomode);
            g_value_set_int(value, ac3dec_info->dualmonomode);
            break;

        case ID_KARAOKE:
#ifdef KCAPABLE
            GST_DEBUG("karaok capable=%d\n", ac3dec_info->kcapablemode);
            g_value_set_int(value, ac3dec_info->kcapablemode);
#endif
            break;

        case ID_PCM_SCALE:
            GST_DEBUG("pcm scale factor=%f\n", ac3dec_info->pcmscalefac);
            g_value_set_float(value, ac3dec_info->pcmscalefac);
            break;

        case ID_COMPRESS_MODE:
            GST_DEBUG("compress mode=%d\n", ac3dec_info->compmode);
            g_value_set_int(value, ac3dec_info->compmode);
            break;

        case ID_DRC_LOW:
            GST_DEBUG("drc range low=%f\n", ac3dec_info->dynrngscalelow);
            g_value_set_float(value, ac3dec_info->dynrngscalelow);
            break;

        case ID_DRC_HI:
            GST_DEBUG("drc range hi=%f\n", ac3dec_info->dynrngscalehi);
            g_value_set_float(value, ac3dec_info->dynrngscalehi);
            break;

        case ID_DEBUG:
            GST_DEBUG("debug=%d\n", ac3dec_info->debug_arg);
            g_value_set_int(value, ac3dec_info->debug_arg);
            break;

        case ID_PROFILE:
            GST_DEBUG("profile=%d\n", ac3dec_info->profile);
            g_value_set_boolean(value, ac3dec_info->profile);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


/*==================================================================================================

FUNCTION:          mfw_gst_ac3dec_base_init

DESCRIPTION:       Element details are registered with the plugin during
                   _base_init ,This function will initialise the class and child
                   class properties during each new child class creation

ARGUMENTS PASSED:  Klass  -  void pointer


RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
static void mfw_gst_ac3dec_base_init(gpointer klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&mfw_gst_ac3dec_src_factory));

    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&mfw_gst_ac3dec_sink_factory));

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "ac3 audio decoder",
        "Codec/Decoder/Audio", "Decode compressed ac3 audio to raw data");
}

/*==================================================================================================

FUNCTION:       mfw_gst_ac3dec_class_init

DESCRIPTION:    Initialise the class.(specifying what signals,
                 arguments and virtual functions the class has and setting up
                 global states)

ARGUMENTS PASSED:
        klass    -  pointer to ac3decoder element class

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/

static void mfw_gst_ac3dec_class_init(MfwGstAc3DecInfoClass * klass)
{
    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
    gstelement_class->change_state = mfw_gst_ac3dec_change_state;
    gobject_class->set_property = mfw_gst_ac3dec_set_property;
    gobject_class->get_property = mfw_gst_ac3dec_get_property;
//    gobject_class->dispose = mfw_gst_ac3dec_dispose;
//    gstelement_class->set_index = mfw_gst_ac3dec_set_index;
//    gstelement_class->get_index = mfw_gst_ac3dec_get_index;


    g_object_class_install_property(gobject_class, ID_OUTPUT_MODE,
				    g_param_spec_int("output_mode",
						     "Output_mode",
						     "Output mode from 0 to 7",
						     0,7,7,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_PCM_DEPTH,
				    g_param_spec_int("pcm_depth",
						     "pcm_depth",
						     "valid bits of pcm per sample",
						     16,24,24,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_OUT_LFE,
				    g_param_spec_int("out_lfe",
						     "out_lfe",
						     "Output lfe channel present",
						     0,1,1,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_STEREO_MODE,
				    g_param_spec_int("stereo_mode",
						     "stereo_mode",
						     "Stereo output mode",
						     0,2,0,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_DUAL_MONO_MODE,
				    g_param_spec_int("dual_mono_mode",
						     "dual_mono_mode",
						     "Dual mono reproduction mode",
						     0,3,0,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_KARAOKE,
				    g_param_spec_int("karaoke_capable",
						     "karaoke_capable",
						     "Karaoke capable reproduction mode",
						     0,3,3,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_PCM_SCALE,
				    g_param_spec_float("pcm_scale",
						       "pcm_scale",
                                                       "PCM scale factor",
						        0.0,1.0,1.0,
						        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_COMPRESS_MODE,
				    g_param_spec_int("compress_mode",
						     "compress_mode",
						     "Dynamic range compression mode",
						     0,3,2,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_DRC_LOW,
				    g_param_spec_float("drc_range_low",
						       "drc_range_low",
						       "Dynamic range compression cut scale factor",
						       0.0, 1.0, 1.0,
						       G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_DRC_HI,
				    g_param_spec_float("drc_range_hi",
						       "drc_range_hi",
						       "Dynamic range compression boost scale factor",
						       0.0, 1.0, 1.0,
						       G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_DEBUG,
				    g_param_spec_int("debug_level",
						     "compress_mode",
						     "Dynamic range compression mode",
						     0,3,0,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_PROFILE,
				    g_param_spec_boolean("profiling",
							 "Profiling",
							 "Enable time profiling of decoder and plugin",
							 FALSE,
							 G_PARAM_READWRITE));
}

/*==================================================================================================

FUNCTION:       mfw_gst_ac3dec_init

DESCRIPTION:    create the pad template that has been registered with the
                element class in the _base_init

ARGUMENTS PASSED:
        ac3dec_info -    pointer to ac3decoder element structure

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/

static void mfw_gst_ac3dec_init(MfwGstAc3DecInfo * ac3dec_info)
{

    GstElementClass *klass = GST_ELEMENT_GET_CLASS(ac3dec_info);


    /* create the sink and src pads */
    ac3dec_info->sinkpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "sink"), "sink");

    ac3dec_info->srcpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "src"), "src");


    gst_element_add_pad(GST_ELEMENT(ac3dec_info), ac3dec_info->sinkpad);
    gst_element_add_pad(GST_ELEMENT(ac3dec_info), ac3dec_info->srcpad);
    gst_pad_set_chain_function(ac3dec_info->sinkpad, mfw_gst_ac3dec_chain);


    gst_pad_set_event_function(ac3dec_info->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_ac3dec_sink_event));

    gst_pad_set_event_function(ac3dec_info->srcpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_ac3dec_src_event));

    /* initialize ac3dec_info  */
    ac3dec_info->eos_event = FALSE;
    ac3dec_info->init_flag = FALSE;
    ac3dec_info->dec_config = NULL;
    ac3dec_info->dec_param = NULL;
    ac3dec_info->tags_set = FALSE;
    ac3dec_info->frame_no = 0;
    ac3dec_info->time_offset = 0;
    ac3dec_info->total_samples = 0;
    ac3dec_info->send_buff = NULL;
    ac3dec_info->seeked_time = 0;
    ac3dec_info->seek_flag = FALSE;
    ac3dec_info->Time = 0;
    ac3dec_info->chain_Time = 0;
    ac3dec_info->no_of_frames_dropped = 0;

    ac3dec_info->totalBytes = 0;
    ac3dec_info->duration = 0;
    {
        gint loopctr;
        for(loopctr=0; loopctr<10; loopctr++)
            ac3dec_info->seek_index[loopctr] = 0;
    }

    ac3dec_info->sampling_freq_pre = 0;
    ac3dec_info->num_channels_pre = 0;
    ac3dec_info->outputmask_pre = 0;

    /* set default codec property */
    ac3dec_info->outputmode = 7;
    ac3dec_info->wordsize =  16;
    ac3dec_info->outlfeon = 1;
    ac3dec_info->stereomode = 0;
    ac3dec_info->dualmonomode = 0;
#ifdef KCAPABLE
    ac3dec_info->kcapablemode = 3;
#endif
    ac3dec_info->pcmscalefac = 1.0;
    ac3dec_info->compmode = 2;
    ac3dec_info->dynrngscalelow = 1.0;
    ac3dec_info->dynrngscalehi = 1.0;
    ac3dec_info->debug_arg = 0;

    ac3dec_info->profile = 0;

    /* Registers a new tag type for the use with GStreamer's type system
       to describe ac3 media metadata*/
    gst_tag_register (GST_TAG_MFW_AC3_CHANNELS, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
            "number of channels","number of channels", NULL);
    gst_tag_register (GST_TAG_MFW_AC3_SAMPLING_RATE, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
            "sampling frequency (Hz)","sampling frequency (Hz)", NULL);

#define MFW_GST_AC3_DECODER_PLUGIN VERSION
    PRINT_CORE_VERSION(AC3D_get_version_info());
    PRINT_PLUGIN_VERSION(MFW_GST_AC3_DECODER_PLUGIN);

    INIT_DEMO_MODE(AC3D_get_version_info(), ac3dec_info->demo_mode);
}


/*==================================================================================================

FUNCTION:       plugin_init

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

==================================================================================================*/

static gboolean plugin_init(GstPlugin * plugin)
{
    return gst_element_register(plugin, "mfw_ac3decoder",
				FSL_GST_DEFAULT_DECODER_RANK_LEGACY, MFW_GST_TYPE_AC3DEC);
}

/*==================================================================================================

FUNCTION:       mfw_gst_type_ac3dec_get_type

DESCRIPTION:    Intefaces are initiated in this function.you can register one
                or more interfaces after having registered the type itself.

ARGUMENTS PASSED:
        None

RETURN VALUE:
        A numerical value ,which represents the unique identifier of this
        elment(ac3decoder)

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
GType mfw_gst_type_ac3dec_get_type(void)
{
    static GType mfwgstac3dec_type = 0;
    if (!mfwgstac3dec_type)
    {
	static const GTypeInfo mfwgstac3dec_info =
        {
	    sizeof(MfwGstAc3DecInfoClass),
	    (GBaseInitFunc) mfw_gst_ac3dec_base_init,
	    NULL,
	    (GClassInitFunc) mfw_gst_ac3dec_class_init,
	    NULL,
	    NULL,
	    sizeof(MfwGstAc3DecInfo),
	    0,
	    (GInstanceInitFunc) mfw_gst_ac3dec_init,
	};
	mfwgstac3dec_type = g_type_register_static(GST_TYPE_ELEMENT,
						   "MfwGstAc3DecInfo",
						   &mfwgstac3dec_info, 0);
    }
    GST_DEBUG_CATEGORY_INIT(mfw_gst_ac3dec_debug, "mfw_ac3decoder", 0,
			    "FreeScale's AC3 Decoder's Log");
    return mfwgstac3dec_type;
}



/*=============================================================================
FUNCTION: mfw_ac3_mem_flush

DESCRIPTION: this function flushes the current memory and allocate the new memory
                for decoder .

ARGUMENTS PASSED:
        ac3dec_info -   pointer to ac3decoder element structure

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static gboolean mfw_ac3_mem_flush(MfwGstAc3DecInfo * ac3dec_info)
{
    gint loopctr = 0;
    AC3DMemAllocInfoSub *mem = NULL;
    AC3_callback    *pCallbackStruct;
    AC3D_PARAM       *p_inparam;
    AC3D_RET_TYPE retval = AC3D_OK;
    gboolean result = TRUE;
    gint num;


    /*if ac3 decoder does not init, no need to flush again */
    if (!ac3dec_info->init_flag)
	goto done;

    gst_adapter_clear(ac3dec_info->adapter);


    GST_PAD_STREAM_LOCK(ac3dec_info->sinkpad);
    num = ac3dec_info->dec_config->sAC3DMemInfo.s32NumReqs;
    for (loopctr = 0; loopctr < num; loopctr++) {
        mem =
            &(ac3dec_info->dec_config->sAC3DMemInfo.
                    sMemInfoSub[loopctr]);
        if (mem->app_base_ptr != NULL) {
            g_free(mem->app_base_ptr);
            mem->app_base_ptr = NULL;
        }
    }
    if (ac3dec_info->dec_param != NULL) {
        g_free(ac3dec_info->dec_param);
        ac3dec_info->dec_param = NULL;
    }
    if (ac3dec_info->send_buff != NULL) {
        g_free(ac3dec_info->send_buff);
        ac3dec_info->send_buff = NULL;
    }
#if 0
    if (ac3dec_info->dec_config->pContext != NULL) {
        g_free(ac3dec_info->dec_config->pContext);
        ac3dec_info->dec_config->pContext = NULL;
    }
#endif
    if (ac3dec_info->dec_config != NULL) {
        g_free(ac3dec_info->dec_config);
        ac3dec_info->dec_config = NULL;
    }
    GST_PAD_STREAM_UNLOCK(ac3dec_info->sinkpad);


    pCallbackStruct = (AC3_callback *)alloc_fast(sizeof(AC3_callback));
    if (pCallbackStruct == NULL)
    {
        GST_ERROR("\n Could not allocate memory for the \
                callback structure\n");
        return GST_STATE_NULL;
    }

    ac3dec_info->dec_param = (AC3D_PARAM *)alloc_fast(sizeof(AC3D_PARAM));
    if (ac3dec_info->dec_param == NULL)
    {
        GST_ERROR("\n Could not allocate memory for the \
                AC3Decoder Parameter structure\n");
        return GST_STATE_NULL;
    }

    ac3dec_info->dec_config = (AC3DDecoderConfig *)alloc_fast(sizeof(AC3DDecoderConfig));
    if (ac3dec_info->dec_config == NULL)
    {
        GST_ERROR("\n Could not allocate memory \
                for the Ac3Decoder Configuration structure\n");
        return GST_STATE_NULL;
    }

    /* set decoder input parameters */
    p_inparam = ac3dec_info->dec_param;
    p_inparam->numchans = 6;
    p_inparam->chanptr[0] = 0;
    p_inparam->chanptr[1] = 1;
    p_inparam->chanptr[2] = 2;
    p_inparam->chanptr[3] = 3;
    p_inparam->chanptr[4] = 4;
    p_inparam->chanptr[5] = 5;
    p_inparam->wordsize = ac3dec_info->wordsize;
    p_inparam->dynrngscalelow = (AC3D_INT32)(ac3dec_info->dynrngscalelow * 2147483647.0);
    p_inparam->dynrngscalehi = (AC3D_INT32)(ac3dec_info->dynrngscalehi * 2147483647.0);
    p_inparam->pcmscalefac = (AC3D_INT32)(ac3dec_info->pcmscalefac * 2147483647.0);
    p_inparam->compmode = ac3dec_info->compmode;
    p_inparam->stereomode = ac3dec_info->stereomode;
    p_inparam->dualmonomode = ac3dec_info->dualmonomode;
    p_inparam->outputmode = ac3dec_info->outputmode;
    p_inparam->outlfeon = ac3dec_info->outlfeon;
    p_inparam->outputflg = 1;	    		/* enable output file flag */
    p_inparam->framecount = 0;			/* frame counter */
    p_inparam->blockcount = 0;			/* block counter */
    p_inparam->framestart = 0;			/* starting frame */
    p_inparam->frameend = -1;			/* ending frame */
    p_inparam->useverbose = 0;			/* verbose messages flag */
    p_inparam->debug_arg = ac3dec_info->debug_arg;
#ifdef KCAPABLE
    p_inparam->kcapablemode = ac3dec_info->kcapablemode;
#endif
    /*output param  init*/
    p_inparam->ac3d_endianmode = 2;     //endiamode checked by decoeder
    p_inparam->ac3d_sampling_freq = 0;
    p_inparam->ac3d_num_channels = 0;
    p_inparam->ac3d_frame_size = 0;
    p_inparam->ac3d_acmod = 0;
    p_inparam->ac3d_bitrate = 0;
    p_inparam->ac3d_outputmask = 0;


    /* Query for memory */
    if ((retval = AC3D_QueryMem(ac3dec_info->dec_config)) != AC3D_OK)
    {
        GST_ERROR
            ("Could not Query the Memory from the Decoder library\n");
        return GST_STATE_NULL;
    }

    /* number of memory chunks requested by decoder */
    num = ac3dec_info->dec_config->sAC3DMemInfo.s32NumReqs;

    for (loopctr = 0; loopctr < num; loopctr++)
    {
        mem = &(ac3dec_info->dec_config->sAC3DMemInfo.
                sMemInfoSub[loopctr]);

        if(mem->s32AC3DType == AC3D_FAST_MEMORY)
        {
            /*allocates memory in internal memory */
            mem->app_base_ptr = alloc_fast(mem->s32AC3DSize);

            if (mem->app_base_ptr == NULL) {
                GST_ERROR
                    ("Could not allocate memory for the Decoder\n");
                return GST_STATE_NULL;
            }
        }
        else
        {
            /*allocates memory in external memory */
            mem->app_base_ptr = alloc_slow(mem->s32AC3DSize);
            if (mem->app_base_ptr == NULL) {
                GST_ERROR
                    ("Could not allocate memory for the Decoder\n");
                return GST_STATE_NULL;
            }
        }
    }

    /* register the call back function in the decoder config structure */
    ac3dec_info->dec_config->pAC3param = p_inparam;

    /* allocate the buffer used to send the data during call back */
    ac3dec_info->send_buff = g_malloc(AC3_MAX_ADAPTER_SIZE);
    if (ac3dec_info->send_buff == NULL) {
        GST_ERROR("\n Could not allocate memory for the \
                buffer sent during callback\n");

        return GST_STATE_NULL;
    }
    memset(ac3dec_info->send_buff, 0, AC3_MAX_ADAPTER_SIZE);

    pCallbackStruct->ac3dec_info = ac3dec_info;
    pCallbackStruct->input_ptr = ac3dec_info->send_buff;

    ac3dec_info->init_flag = FALSE;

    ac3dec_info->sampling_freq_pre = 0;
    ac3dec_info->num_channels_pre = 0;
    ac3dec_info->outputmask_pre = 0;

  done:
    return result;
}


/*==================================================================================================

FUNCTION:       mfw_gst_ac3dec_sink_event

DESCRIPTION:    send an event to sink  pad of ac3decoder element

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -    event is handled properly
        FALSE      -	event is not handled properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean mfw_gst_ac3dec_sink_event(GstPad * pad, GstEvent * event)
{
    MfwGstAc3DecInfo *ac3dec_info = MFW_GST_AC3DEC(GST_PAD_PARENT(pad));
    gboolean result = TRUE;
    GstAdapter *adapter = NULL;
    AC3D_RET_TYPE retval = AC3D_OK;
    AC3D_INT32 size = 0;
    guint8 *data = NULL;
    GstFlowReturn res = GST_FLOW_ERROR;
    struct timeval tv_prof2, tv_prof3;
    long time_before = 0, time_after = 0;


    GST_DEBUG("handling %s event", GST_EVENT_TYPE_NAME(event));

    /* gets the event type */
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
	{

	    GstFormat format;
	    gint64 start, stop, position;
	    gint64 nstart, nstop;
	    GstEvent *nevent;

	    gst_event_parse_new_segment(event, NULL, NULL, &format, &start,
					&stop, &position);

	    if (format == GST_FORMAT_BYTES) {
		format = GST_FORMAT_TIME;

                nstart = ac3dec_info->seeked_time;
                nstop  = ac3dec_info->duration;
            if (nstop == 0)
                nstop = -1;

		nevent =
		    gst_event_new_new_segment(FALSE, 1.0, GST_FORMAT_TIME,
					      nstart, nstop, nstart);
		gst_event_unref(event);
		ac3dec_info->time_offset = (gfloat) nstart / GST_SECOND;
		result = gst_pad_push_event(ac3dec_info->srcpad, nevent);
		if (TRUE != result) {
		    GST_ERROR
			("\n Error in pushing the event,result	is %d\n",
			 result);

		}
	    } else if (format == GST_FORMAT_TIME) {
		ac3dec_info->time_offset = (gfloat) start / GST_SECOND;

		result = gst_pad_push_event(ac3dec_info->srcpad, event);
		if (TRUE != result) {
		    GST_ERROR
			("\n Error in pushing the event,result	is %d\n",
			 result);
		    gst_event_unref(event);
		}
	    }


	    break;
	}

    case GST_EVENT_EOS:
        {
            /* decode the remaining data. */
            GST_DEBUG("\n Got the EOS from sink\n");

            /* initialise the decoder */
            if (!ac3dec_info->init_flag)
            {
                retval = AC3D_dec_init(ac3dec_info->dec_config,NULL,0);
        	if(retval != AC3D_OK)
        	{
        	    GST_ERROR("\n Could not Initialize the AC3Decoder\n");
        	    return GST_FLOW_ERROR;
        	}

        	ac3dec_info->init_flag = TRUE;
            }

            /* End of stream , processing all the bufferd data */
            if(ac3dec_info->init_flag){
                ac3dec_info->eos_event = TRUE;
                adapter = (GstAdapter *) ac3dec_info->adapter;
                if (ac3dec_info->profile) {
                    gettimeofday(&tv_prof2, 0);
                }

                do {
                    if (ac3dec_info->stopped) {
                        ac3dec_info->stopped = FALSE;
                        break;
                    }

                    res = decode_ac3_chunk(ac3dec_info);
                    if (res != GST_FLOW_OK) {
                        GST_ERROR("Fatal error stoping decode");
                        break;
                    }
                } while (gst_adapter_available(adapter) > 0);

                if (ac3dec_info->profile) {

                    gettimeofday(&tv_prof3, 0);
                    time_before =
                        (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
                    time_after =
                        (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
                    ac3dec_info->chain_Time += time_after - time_before;
                }
            }
            else {
                GST_WARNING("Got EOS event without init");
            }

            result = gst_pad_push_event(ac3dec_info->srcpad, event);
            if (TRUE != result) {
                GST_ERROR("\n Error in pushing the event,result	is %d\n",
                        result);
            }

            break;
        }

    case GST_EVENT_FLUSH_STOP:
        {
	    if (result = mfw_ac3_mem_flush(ac3dec_info) == FALSE)
	        break;

            //gst_adapter_clear(ac3dec_info->adapter);

            result = gst_pad_push_event(ac3dec_info->srcpad, event);

            if (TRUE != result) {
                GST_ERROR("\n Error in pushing the event,result	is %d\n",
                        result);
                gst_event_unref(event);
            }
            break;
        }

    case GST_EVENT_FLUSH_START:
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    break;
	}
    }
    return result;
}



/*==================================================================================================

FUNCTION:   mfw_gst_ac3dec_seek

DESCRIPTION:    performs seek operation

ARGUMENTS PASSED:
        ac3dec_info -   pointer to decoder element
        pad         -   pointer to GstPad
        event       -   pointer to GstEvent

RETURN VALUE:
        TRUE    -   sucess
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean
mfw_gst_ac3dec_seek(MfwGstAc3DecInfo * ac3dec_info,
			GstPad * pad, GstEvent * event)
{
    gdouble rate;
    GstFormat format, conv;
    GstSeekFlags flags;
    GstSeekType cur_type, stop_type;
    gint64 cur = 0, stop = 0;
    gint64 time_cur = 0, time_stop = 0;
    gint64 bytes_cur = 0, bytes_stop = 0;
    gboolean flush;
    gboolean res;
    guint bytesavailable;
    GstEvent *seek_event;
    gint secctr, minctr, hourctr;

    gst_event_parse_seek(event, &rate, &format, &flags, &cur_type, &cur,
            &stop_type, &stop);

    switch(format)
    {
        case GST_FORMAT_TIME:
            {
                GST_DEBUG("\nseek from  %" GST_TIME_FORMAT "--------------- to %"
                        GST_TIME_FORMAT, GST_TIME_ARGS(cur), GST_TIME_ARGS(stop));

                if(cur > ac3dec_info->duration)
                    return FALSE;

                ac3dec_info->seeked_time = cur;

                secctr = (cur / GST_SECOND) % 60;
                minctr = (cur / (GST_SECOND * 60)) % 60;
                hourctr = cur / (GST_SECOND * 60 * 60);

                bytes_cur = ac3dec_info->seek_index[hourctr][minctr][secctr];
                bytes_stop = ac3dec_info->totalBytes;
                if (-1 == bytes_cur) {
                    GST_WARNING ("seek to EOS");
                    bytes_cur = bytes_stop;
                }


            }
            break;

        case GST_FORMAT_BYTES:
            {
                GST_DEBUG("\nseek from  %lu--------------- to %lu", cur, stop);
                bytes_cur = cur;
                bytes_stop = stop;
            }
            break;

        default:
            GST_ERROR("failed to convert format %u into GST_FORMAT_TIME",
                    format);
            return FALSE;
    }

    seek_event =
        gst_event_new_seek(rate, GST_FORMAT_BYTES, flags, cur_type,
                bytes_cur, stop_type, bytes_stop);

    /* do the seek */
    res = gst_pad_push_event(ac3dec_info->sinkpad, seek_event);


    return TRUE;
}


/*==================================================================================================

FUNCTION:   mfw_gst_ac3dec_src_event

DESCRIPTION:    send an event to src  pad of ac3decoder element

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

==================================================================================================*/
static gboolean
mfw_gst_ac3dec_src_event(GstPad * pad, GstEvent * event)
{
    gboolean res;
    MfwGstAc3DecInfo *ac3dec_info;
    GST_DEBUG(" in mfw_gst_ac3dec_src_event routine \n");
    ac3dec_info = MFW_GST_AC3DEC(GST_PAD_PARENT(pad));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEEK:
	if (ac3dec_info->seek_flag == TRUE) {
	    res = mfw_gst_ac3dec_seek(ac3dec_info, pad, event);

	} else {
	    res = gst_pad_push_event(ac3dec_info->sinkpad, event);

	}
	break;

    default:
	res = gst_pad_event_default(pad, event);
	break;


    }
    GST_DEBUG(" out of mfw_gst_ac3dec_src_event routine \n");
    return res;

}


/*==================================================================================================

FUNCTION:   mfw_gst_ac3dec_src_query

DESCRIPTION:    performs query on src pad.

ARGUMENTS PASSED:
        pad     -   pointer to GstPad
        query   -   pointer to GstQuery

RETURN VALUE:
        TRUE    -   success
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean mfw_gst_ac3dec_src_query(GstPad * pad, GstQuery * query)
{
    gboolean res = TRUE;
    GstPad *peer;
    MfwGstAc3DecInfo *ac3dec_info;
    ac3dec_info = MFW_GST_AC3DEC(GST_PAD_PARENT(pad));

    peer = gst_pad_get_peer(ac3dec_info->sinkpad);
    if (peer  == NULL)
        goto error;

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_POSITION:
	{
	    GstFormat format;

	    /* save requested format */
	    gst_query_parse_duration(query, &format, NULL);

            switch(format)
            {
                case GST_FORMAT_TIME:
                    {
                        gint64 time_cur;

                        time_cur = ac3dec_info->time_offset * GST_SECOND;
                        gst_query_set_duration(query, format, time_cur);

                        GST_DEBUG("duration=%" GST_TIME_FORMAT,
                                GST_TIME_ARGS(time_cur));
                    }
                    break;

                case GST_FORMAT_BYTES:
                    {
                        gint64 byte_cur;

                        if(gst_pad_query_position(peer, &format, &byte_cur) !=  GST_FLOW_OK);
                        {
                            GST_ERROR("error in gst_pad_query_position\n");
                            return FALSE;
                        }

                        gst_query_set_duration(query, format, byte_cur);

                        GST_DEBUG("duration=%" G_GINT64_FORMAT ",format=%u",
                                                          byte_cur, format);
                    }
                    break;

                default:
                    return FALSE;
            }
	}
	break;
    case GST_QUERY_DURATION:
	{
	    GstFormat format;
	    GstFormat rformat;
	    gint64 total, total_bytes;

	    /* save requested format */
	    gst_query_parse_duration(query, &format, NULL);

            switch(format)
            {
                case GST_FORMAT_TIME:
                    total = ac3dec_info->duration;
                    gst_query_set_duration(query, format, total);

                    GST_DEBUG("duration=%" GST_TIME_FORMAT,
                                            GST_TIME_ARGS(total));

                    break;

                case GST_FORMAT_BYTES:
                    total_bytes = ac3dec_info->totalBytes;
                    gst_query_set_duration(query, format, total_bytes);

                    GST_DEBUG("duration=%" G_GINT64_FORMAT , total_bytes);

                    break;

                default:
                    return FALSE;
            }
            break;
	}

    default:
	res = FALSE;
	break;
    }
    gst_object_unref(peer);
    return res;

  error:
    GST_ERROR("error handling query");
    gst_object_unref(peer);
    return FALSE;
}



/*==================================================================================================

FUNCTION:     app_calc_seek_index

DESCRIPTION: this function parse ac3 bitstream to get duration and seek index.

ARGUMENTS PASSED:
        samplerate  - sample rate for this frame
        framesize   - frame size for this frame
        buffer      - sync info data

RETURN VALUE:
        0           - normal
        1           - error

==================================================================================================*/
gint app_calc_seek_index(gint *samplerate, gint *framesize, gchar *buffer)
{
    gint ret = 0;
    gint fscod, frmsizecod;


    if(buffer[0] == 0x0b && buffer[1] == 0x77)
    {
        fscod = buffer[4] >> 6;
        frmsizecod = buffer[4] & 0x3f;
    }
    else if(buffer[0] == 0x77 && buffer[1] == 0x0b)
    {
        fscod = buffer[5] >> 6;
        frmsizecod = buffer[5] & 0x3f;
    }
    else
        return 1;

    *samplerate = sampleratetab[fscod];
    *framesize = 2 * frmsizetab[fscod][frmsizecod];

    return ret;
}



/*==================================================================================================

FUNCTION:     mfw_gst_ac3decoder_create_seek_index

DESCRIPTION: this function creates index for seek and duration.

ARGUMENTS PASSED:
        ac3dec_info - pointer to the plugin context

RETURN VALUE:
       0            - execution succesful
       1            - error in execution

==================================================================================================*/

gint mfw_gst_ac3decoder_create_seek_index(MfwGstAc3DecInfo * ac3dec_info)
{
    GstFlowReturn ret = GST_FLOW_OK;

    GstPad *pad = NULL;
    GstPad *peer_pad = NULL;

    GstFormat fmt = GST_FORMAT_BYTES;
    guint64 totalBytes = 0;
    GstBuffer *pullbuffer = NULL;
    guint64  offset = 0;

    gint samplerate, framesize;
    guint frameduration;

    guint temp = 0;
    gint FileType=0;
    GstClockTime file_duration;

    gint secctr, minctr, hourctr ;
    guint durationsec = 0;

    ac3dec_info->totalBytes = 0;
    ac3dec_info->duration = 0;
    pad = ac3dec_info->sinkpad;

    if (gst_pad_check_pull_range(pad))
    {
        if (gst_pad_activate_pull(GST_PAD_PEER(pad), TRUE))
        {
            peer_pad = gst_pad_get_peer(ac3dec_info->sinkpad);
            gst_pad_query_duration(peer_pad, &fmt, &totalBytes);
            gst_object_unref(GST_OBJECT(peer_pad));

            ac3dec_info->totalBytes = totalBytes;

            ac3dec_info->seek_index[0] = (guint64 **)g_malloc(60 * sizeof(guint64 *));
            if(ac3dec_info->seek_index[0] == NULL)
            {
                GST_ERROR("Failed in allocate memory for seek_index\n");
                return 1;
            }
            memset(ac3dec_info->seek_index[0], 0, 60 * sizeof(guint64 *));

            ac3dec_info->seek_index[0][0] = (guint64 *)g_malloc(60 * sizeof(guint64));
            if(ac3dec_info->seek_index[0][0] == NULL)
            {
                GST_ERROR("Failed in allocate memory for seek_index\n");
                return 1;
            }
            memset(ac3dec_info->seek_index[0][0], 0, 60 * sizeof(guint64));

            ac3dec_info->seek_index[0][0][0] = 0;
            secctr = 1;
            minctr = 0;
            hourctr =0;

            while(offset < totalBytes)
            {
                ret = gst_pad_pull_range(pad, offset, 6, &pullbuffer);
                if(ret != GST_FLOW_OK)
                    GST_ERROR("error while pull data\n");

                if(app_calc_seek_index(&samplerate, &framesize,(gchar *) GST_BUFFER_DATA(pullbuffer)))
                    break;

                frameduration = (guint)((1000000000.0 * 1536.0)/(gfloat)samplerate);

                ac3dec_info->duration += frameduration;
                durationsec += frameduration;

                if(durationsec >= GST_SECOND)
                {
                    ac3dec_info->seek_index[hourctr][minctr][secctr] = offset;
                    durationsec = 0;

                    secctr ++;
                    if(secctr == 60)
                    {
                        minctr ++;
                        secctr = 0;

                        ac3dec_info->seek_index[hourctr][minctr] = (guint64 *)g_malloc(60 * sizeof(guint64));
                        if(ac3dec_info->seek_index[hourctr][minctr] == NULL)
                        {
                            GST_ERROR("Failed in allocate memory for seek_index\n");
                            return 1;
                        }
                        memset(ac3dec_info->seek_index[hourctr][minctr], -1, 60 * sizeof(guint64));
                    }

                    if(minctr == 60)
                    {
                        hourctr ++;
                        minctr == 0;

                        ac3dec_info->seek_index[hourctr] = (guint64 **)g_malloc(60 * sizeof(guint64 *));
                        if(ac3dec_info->seek_index[hourctr] == NULL)
                        {
                            GST_ERROR("Failed in allocate memory for seek_index\n");
                            return 1;
                        }
                        memset(ac3dec_info->seek_index[hourctr], 0, 60 * sizeof(guint64 *));
                    }

                    if(hourctr >= 10)
                        break;
                }

                offset += framesize ;
            }

            gst_pad_activate_push(GST_PAD_PEER(pad), TRUE);
        }
    }

    return 0;
}



/*==================================================================================================

FUNCTION:     mfw_gst_ac3dec_change_state

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

==================================================================================================*/

static GstStateChangeReturn
mfw_gst_ac3dec_change_state(GstElement * element,
			    GstStateChange transition)
{
    GstStateChangeReturn ret;
    MfwGstAc3DecInfo *ac3dec_info;
    ac3dec_info = MFW_GST_AC3DEC(element);
    AC3DMemAllocInfoSub *mem = NULL;
    AC3D_RET_TYPE retval = AC3D_OK;
    gint loopctr = 0;
    gint num = 0;
    AC3D_PARAM       *p_inparam;
    AC3_callback    *pCallbackStruct;

    GST_DEBUG("transistion is %d\n", transition);
    switch (transition)
    {
        case GST_STATE_CHANGE_NULL_TO_READY:
            {

                /* allocate the adapter */
                ac3dec_info->adapter = gst_adapter_new();
                if (ac3dec_info->adapter == NULL)
                {
                    GST_ERROR
                        ("\n Could not allocate memory for the Adapter\n");
                    return GST_STATE_NULL;
                }


                /* allocate static resources for ac3 decoder */
                ac3dec_info->dec_param = (AC3D_PARAM *)alloc_fast(sizeof(AC3D_PARAM));
                if (ac3dec_info->dec_param == NULL)
                {
                    GST_ERROR("\n Could not allocate memory for the \
                                  AC3Decoder Parameter structure\n");
                    return GST_STATE_NULL;
                }

                ac3dec_info->dec_config = (AC3DDecoderConfig *)alloc_fast(sizeof(AC3DDecoderConfig));
                if (ac3dec_info->dec_config == NULL)
                {
                    GST_ERROR("\n Could not allocate memory \
                                  for the Ac3Decoder Configuration structure\n");
                    return GST_STATE_NULL;
                }

                /* set decoder input parameters */
                p_inparam = ac3dec_info->dec_param;

                p_inparam->numchans = 6;
                p_inparam->chanptr[0] = 0;
                p_inparam->chanptr[1] = 1;
                p_inparam->chanptr[2] = 2;
                p_inparam->chanptr[3] = 3;
                p_inparam->chanptr[4] = 4;
                p_inparam->chanptr[5] = 5;
                p_inparam->wordsize = ac3dec_info->wordsize;
                p_inparam->dynrngscalelow = (AC3D_INT32)(ac3dec_info->dynrngscalelow * 2147483647.0);
                p_inparam->dynrngscalehi = (AC3D_INT32)(ac3dec_info->dynrngscalehi * 2147483647.0);
                p_inparam->pcmscalefac = (AC3D_INT32)(ac3dec_info->pcmscalefac * 2147483647.0);
                p_inparam->compmode = ac3dec_info->compmode;
                p_inparam->stereomode = ac3dec_info->stereomode;
                p_inparam->dualmonomode = ac3dec_info->dualmonomode;
                p_inparam->outputmode = ac3dec_info->outputmode;
                p_inparam->outlfeon = ac3dec_info->outlfeon;
                p_inparam->outputflg = 1;	    		/* enable output file flag */
                p_inparam->framecount = 0;			/* frame counter */
                p_inparam->blockcount = 0;			/* block counter */
                p_inparam->framestart = 0;			/* starting frame */
                p_inparam->frameend = -1;			/* ending frame */
                p_inparam->useverbose = 0;			/* verbose messages flag */
                p_inparam->debug_arg = ac3dec_info->debug_arg;
#ifdef KCAPABLE
                p_inparam->kcapablemode = ac3dec_info->kcapablemode;
#endif
                /*output param  init*/
                p_inparam->ac3d_endianmode = 2;     //endiamode checked by decoeder
                p_inparam->ac3d_sampling_freq = 0;
                p_inparam->ac3d_num_channels = 0;
                p_inparam->ac3d_frame_size = 0;
                p_inparam->ac3d_acmod = 0;
                p_inparam->ac3d_bitrate = 0;
                p_inparam->ac3d_outputmask = 0;


                /* Query for memory */
                if ((retval = AC3D_QueryMem(ac3dec_info->dec_config)) != AC3D_OK)
                {
                    GST_ERROR
                        ("Could not Query the Memory from the Decoder library\n");
                    return GST_STATE_NULL;
                }

                /* number of memory chunks requested by decoder */
                num = ac3dec_info->dec_config->sAC3DMemInfo.s32NumReqs;


                for (loopctr = 0; loopctr < num; loopctr++)
                {
                    mem = &(ac3dec_info->dec_config->sAC3DMemInfo.
                            sMemInfoSub[loopctr]);

                    if(mem->s32AC3DType == AC3D_FAST_MEMORY)
                    {
                        /*allocates memory in internal memory */
                        mem->app_base_ptr = alloc_fast(mem->s32AC3DSize);

                        if (mem->app_base_ptr == NULL) {
                            GST_ERROR
                                ("Could not allocate memory for the Decoder\n");
                            return GST_STATE_NULL;
                        }
                    }
                    else
                    {
                        /*allocates memory in external memory */
                        mem->app_base_ptr = alloc_slow(mem->s32AC3DSize);
                        if (mem->app_base_ptr == NULL) {
                            GST_ERROR
                                ("Could not allocate memory for the Decoder\n");
                            return GST_STATE_NULL;
                        }
                    }
                }

                ac3dec_info->dec_config->pAC3param = p_inparam;

                ac3dec_info->dec_config->pContext = NULL;
                break;
            }
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            {

                if (gst_pad_check_pull_range(ac3dec_info->sinkpad)) {
                    // check if upstream element can support random access,
                    // if true, seek and query can be done in decoder element.
                    gst_pad_set_query_function(ac3dec_info->srcpad,
                            GST_DEBUG_FUNCPTR
                            (mfw_gst_ac3dec_src_query));

                    ac3dec_info->seek_flag = TRUE;

                    if(mfw_gst_ac3decoder_create_seek_index(ac3dec_info) != 0)
                        GST_ERROR("error in Create seek index\n");
                }

                break;
            }
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:

            ac3dec_info->stopped = FALSE;
            break;
        default:
            break;
    }

    ret = parent_class->change_state(element, transition);
    switch (transition) {
        gfloat avg_mcps = 0, avg_plugin_time = 0, avg_dec_time = 0;
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        ac3dec_info->stopped = TRUE;
        break;
        case GST_STATE_CHANGE_PAUSED_TO_READY:
        break;
        case GST_STATE_CHANGE_READY_TO_NULL:
        {
            if (ac3dec_info->profile) {

                g_print
                    ("\nProfile Figures for FSL AC3 Decoder gstreamer Plugin\n");

                g_print("\nTotal decode time is                   %ldus",
                        ac3dec_info->Time);
                g_print("\nTotal plugin time is                   %ldus",
                        ac3dec_info->chain_Time);
                g_print("\nTotal number of frames decoded is      %d",
                        ac3dec_info->frame_no);
                g_print("\nTotal number of frames dropped is      %d\n",
                        ac3dec_info->no_of_frames_dropped);

                avg_mcps =
                    (((float) ac3dec_info->dec_param->ac3d_sampling_freq /
                      AC3D_FRAME_SIZE) * ac3dec_info->Time *
                     PROCESSOR_CLOCK)
                    / (ac3dec_info->frame_no -
                            ac3dec_info->no_of_frames_dropped) / 1000000;

                g_print("\nAverage decode MCPS is  %f\n", avg_mcps);

                avg_mcps =
                    (((float) ac3dec_info->dec_param->ac3d_sampling_freq /
                      AC3D_FRAME_SIZE) * ac3dec_info->chain_Time *
                     PROCESSOR_CLOCK)
                    / (ac3dec_info->frame_no -
                            ac3dec_info->no_of_frames_dropped) / 1000000;

                g_print("\nAverage plugin MCPS is  %f\n", avg_mcps);


                avg_dec_time =
                    ((float) ac3dec_info->Time) / ac3dec_info->frame_no;
                g_print("\nAverage decoding time is               %fus",
                        avg_dec_time);
                avg_plugin_time =
                    ((float) ac3dec_info->chain_Time) /
                    ac3dec_info->frame_no;
                g_print("\nAverage plugin time is                 %fus\n",
                        avg_plugin_time);

                ac3dec_info->Time = 0;
                ac3dec_info->chain_Time = 0;
                ac3dec_info->frame_no = 0;
                ac3dec_info->no_of_frames_dropped = 0;
            }

            gst_adapter_clear(ac3dec_info->adapter);
            g_object_unref(ac3dec_info->adapter);

            ac3dec_info->adapter = NULL;
            GST_PAD_STREAM_LOCK(ac3dec_info->sinkpad);
            num = ac3dec_info->dec_config->sAC3DMemInfo.s32NumReqs;
            for (loopctr = 0; loopctr < num; loopctr++) {
                mem =
                    &(ac3dec_info->dec_config->sAC3DMemInfo.
                            sMemInfoSub[loopctr]);
                if (mem->app_base_ptr != NULL) {
                    g_free(mem->app_base_ptr);
                    mem->app_base_ptr = NULL;
                }
            }
            if (ac3dec_info->dec_param != NULL) {
                g_free(ac3dec_info->dec_param);
                ac3dec_info->dec_param = NULL;
            }
            if (ac3dec_info->send_buff != NULL) {
                g_free(ac3dec_info->send_buff);
                ac3dec_info->send_buff = NULL;
            }
#if 0
            if (ac3dec_info->dec_config->pContext != NULL) {
                g_free(ac3dec_info->dec_config->pContext);
                ac3dec_info->dec_config->pContext = NULL;
            }
#endif
            if (ac3dec_info->dec_config != NULL) {
                g_free(ac3dec_info->dec_config);
                ac3dec_info->dec_config = NULL;
            }
            GST_PAD_STREAM_UNLOCK(ac3dec_info->sinkpad);
            {
                gint secctr, minctr, hourctr;

                for(hourctr=0; hourctr<10; hourctr++)
                {
                    if(ac3dec_info->seek_index[hourctr] != NULL)
                    {
                        for(minctr=0; minctr<60; minctr++)
                            if(ac3dec_info->seek_index[hourctr][minctr] != NULL)
                                g_free(ac3dec_info->seek_index[hourctr][minctr]);

                        g_free(ac3dec_info->seek_index[hourctr]);
                    }
                    else
                        break;
                }
            }

            ac3dec_info->eos_event = FALSE;
            ac3dec_info->init_flag = FALSE;
            ac3dec_info->tags_set = FALSE;
            ac3dec_info->time_offset = 0;
            ac3dec_info->seeked_time = 0;
            ac3dec_info->total_samples = 0;

            break;
        }
        default:
        break;
    }
    return ret;
}


static gint
mfw_gst_ac3dec_find_startcode (char *buffer, int len)
{
    guint32 off = 0;
    guint32 lastword = 0;
    gint ret = -1;

    while (off < len) {
        lastword = ((lastword << 8) | buffer[off++]);
        if (((lastword & 0x0000ffff) == AC3_BIG_STARTCODE) || 
            ((lastword & 0x0000ffff) == AC3_LITTLE_STARTCODE)) {
            ret = off - 2;
            GST_DEBUG ("Found startcode, offset is %d.\n", ret);
            break;
        }
    }

    return ret;
}



/*==================================================================================================

FUNCTION:   mfw_gst_ac3dec_chain

DESCRIPTION:    this function called to get data from sink pad ,it also
				initialize the ac3decoder first time and starts the decoding
				process.

ARGUMENTS PASSED:
            buffer ---  pointer to the GstBuffer
			pad    ---- pointer to pad

RETURN VALUE:
           GstFlowReturn

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None

==================================================================================================*/
static GstFlowReturn mfw_gst_ac3dec_chain(GstPad * pad, GstBuffer * buffer)
{
    //const guint8 *data = NULL;
    gint loopctr = 0;
    AC3D_RET_TYPE retval = AC3D_OK;
    GstAdapter *adapter = NULL;
    guint size_twoframes = 0;
    guint8 *indata = NULL;
    struct timeval tv_prof2, tv_prof3;
    long time_before = 0, time_after = 0;
    MfwGstAc3DecInfo *ac3dec_info;
    GstFlowReturn result = GST_FLOW_OK;

    gint inBuffSize = GST_BUFFER_SIZE(buffer);;
    guint8 *inBufPtr = GST_BUFFER_DATA(buffer);

    GST_DEBUG("mfw_gst_ac3dec_chain\n");

    ac3dec_info = MFW_GST_AC3DEC(GST_OBJECT_PARENT(pad));

    if (ac3dec_info->demo_mode == 2)
        return GST_FLOW_ERROR;

    if (ac3dec_info->profile) {
        gettimeofday(&tv_prof2, 0);
    }

    adapter = (GstAdapter *) ac3dec_info->adapter;

    /* push input data from buffer to adapter */
    gst_adapter_push(adapter, buffer);

    /* initialise the decoder */
    if (!ac3dec_info->init_flag)
    {
        /* find the first start code and strip the leading bytes */
        gint buflen = gst_adapter_available (adapter);
        guint8 *data = (guint8 *) gst_adapter_peek (adapter, buflen);
        gint leading_bytes = mfw_gst_ac3dec_find_startcode(data, buflen);
        if (-1 == leading_bytes)
        {
            gst_adapter_flush(adapter, buflen);
        	return GST_FLOW_OK;
        }
        gst_adapter_flush(adapter, leading_bytes);

        retval = AC3D_dec_init(ac3dec_info->dec_config,NULL,0);
    	if(retval != AC3D_OK)
    	{
    	    GST_ERROR("\n Could not Initialize the AC3Decoder\n");
    	    return GST_FLOW_ERROR;
    	}

        if (NULL != buffer)
        {
            if (GST_BUFFER_TIMESTAMP_IS_VALID(buffer) &&
                GST_BUFFER_DURATION_IS_VALID(buffer))
            {
                ac3dec_info->time_offset =
                    (gfloat)GST_BUFFER_DURATION(buffer) * leading_bytes / buflen;
                ac3dec_info->time_offset += GST_BUFFER_TIMESTAMP(buffer);
                ac3dec_info->time_offset /= GST_SECOND;
            }
        }
	    ac3dec_info->init_flag = TRUE;
    }


    if (gst_adapter_available(adapter) < AC3_MAX_ADAPTER_SIZE)
	return GST_FLOW_OK;

    /* start decode process until the input buffer is not enough */
    do
    {
	result = decode_ac3_chunk(ac3dec_info);
	if (result != GST_FLOW_OK)
	    break;
    } while (gst_adapter_available(adapter) >= AC3_MAX_ADAPTER_SIZE);

    if (result != GST_FLOW_OK) {
	GST_ERROR(" Error in decding\n");
    }

    if (ac3dec_info->profile) {

	gettimeofday(&tv_prof3, 0);
	time_before = (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
	time_after = (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
	ac3dec_info->chain_Time += time_after - time_before;
    }

    return GST_FLOW_OK;
}

/*==================================================================================================

FUNCTION: decode_ac3_chunk

DESCRIPTION:
   This function calls the main ac3decoder function ,sends the decoded data to osssink

ARGUMENTS PASSED:
   pointer to the ac3decoderInfo structure.

RETURN VALUE:
   None

PRE-CONDITIONS:
   None

POST-CONDITIONS:
   None

IMPORTANT NOTES:
   None

==================================================================================================*/

static GstFlowReturn decode_ac3_chunk(MfwGstAc3DecInfo * ac3dec_info)
{
    AC3D_RET_TYPE retval = AC3D_OK;
    GstBuffer *outbuffer = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    gint loopctr = 0;
    GstCaps *src_caps = NULL;
    GstCaps *caps = NULL;
    AC3D_INT32 *decode_out = NULL;
    gfloat time_duration = 0.0;
    gint total_samples = 0;
    guint bytesavailable = 0;
    gint pcm_width = ac3dec_info->wordsize > 16 ? 32 : 16;
    gboolean out_L, out_R, out_C, out_Ls, out_Rs, out_lfe;

    struct timeval tv_prof, tv_prof1;
    long time_before = 0, time_after = 0;
    GstAdapter *adpt;
    gint16 *in_buf_data;
    gint in_buf_len;
    DEMO_LIVE_CHECK(ac3dec_info->demo_mode,
        (ac3dec_info->time_offset*GST_SECOND),
        ac3dec_info->srcpad);
    if (ac3dec_info->demo_mode == 2)
        return GST_FLOW_ERROR;


    decode_out = g_malloc(6 * AC3D_FRAME_SIZE * sizeof(AC3D_INT32));

    if (ac3dec_info->profile)
    {
	gettimeofday(&tv_prof, 0);
    }

    /* Get the input buffer */
    adpt = ac3dec_info->adapter;

    if (gst_adapter_available(adpt) >= AC3_MAX_ADAPTER_SIZE) {
        in_buf_data = (gint16 *)gst_adapter_peek(adpt, AC3_MAX_ADAPTER_SIZE);
        /* decode one frame of data */
        retval = AC3D_dec_Frame(ac3dec_info->dec_config,decode_out,in_buf_data,AC3_MAX_ADAPTER_SIZE);
    }
    else if (gst_adapter_available(adpt) > 0) {
        in_buf_len = gst_adapter_available(adpt);
        in_buf_data = (gint16 *)gst_adapter_peek(adpt, in_buf_len);

        /* decode one frame of data */
        retval = AC3D_dec_Frame(ac3dec_info->dec_config,decode_out,in_buf_data,in_buf_len);
    }
    else
        return GST_FLOW_OK;

    if (ac3dec_info->profile) {

	gettimeofday(&tv_prof1, 0);
	time_before = (tv_prof.tv_sec * 1000000) + tv_prof.tv_usec;
	time_after = (tv_prof1.tv_sec * 1000000) + tv_prof1.tv_usec;
	ac3dec_info->Time += time_after - time_before;

        ac3dec_info->frame_no++;
    }

    /* Get the consumped length, if consumed, then skip it, ignore return status */
	if(ac3dec_info->dec_config->pAC3param->ac3d_frame_size > 0)
    gst_adapter_flush(adpt,ac3dec_info->dec_config->pAC3param->ac3d_frame_size);

    if(retval < AC3D_ERR_FATAL )
    {


        out_L = (ac3dec_info->dec_param->ac3d_outputmask & 0x80) >> 7 ;
        out_C = (ac3dec_info->dec_param->ac3d_outputmask & 0x40) >> 6 ;
        out_R = (ac3dec_info->dec_param->ac3d_outputmask & 0x20) >> 5 ;
        out_Ls = (ac3dec_info->dec_param->ac3d_outputmask & 0x10) >> 4 ;
        out_Rs = (ac3dec_info->dec_param->ac3d_outputmask & 0x8) >> 3 ;
        out_lfe = (ac3dec_info->dec_param->ac3d_outputmask & 0x4) >> 2 ;

        GST_DEBUG("L:%d, C: %d, R: %d, Ls: %d, Lr: %d, lfe: %d\n", out_L, out_C, out_R, out_Ls, out_Rs, out_lfe);
        GST_DEBUG("Channel: %d\n", ac3dec_info->dec_param->ac3d_num_channels);
        GST_DEBUG("SampleRate: %d\n", ac3dec_info->dec_param->ac3d_sampling_freq);

        if( (ac3dec_info->dec_param->ac3d_sampling_freq != ac3dec_info->sampling_freq_pre)
                || (ac3dec_info->dec_param->ac3d_num_channels != ac3dec_info->num_channels_pre)
                || (ac3dec_info->dec_param->ac3d_outputmask != ac3dec_info->outputmask_pre) )
        {

            GValue chanpos = { 0 };
            GValue pos = { 0 };

        {
            GstTagList	*list = gst_tag_list_new();
            gchar  *codec_name = "AC3-Decoder";
            gst_tag_list_add(list,GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
                codec_name,NULL);
            gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
                (guint)(ac3dec_info->dec_param->ac3d_bitrate),NULL);
            gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_AC3_SAMPLING_RATE,
                (guint)ac3dec_info->dec_param->ac3d_sampling_freq,NULL);
            gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_AC3_CHANNELS,
                (guint)ac3dec_info->dec_param->ac3d_num_channels,NULL);

            gst_element_found_tags(GST_ELEMENT(ac3dec_info),list);


	    }

        g_value_init (&chanpos, GST_TYPE_ARRAY);
        g_value_init (&pos, GST_TYPE_AUDIO_CHANNEL_POSITION);

            if(out_C) {
                g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
                gst_value_array_append_value (&chanpos, &pos);
            }
            if(out_L) {
                g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
                gst_value_array_append_value (&chanpos, &pos);
            }
            if(out_R) {
                g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
                gst_value_array_append_value (&chanpos, &pos);
            }
            if(out_Ls) {
                g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
                gst_value_array_append_value (&chanpos, &pos);
            }
            if(out_Rs) {
                g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
                gst_value_array_append_value (&chanpos, &pos);
            }
            if(out_lfe) {
                g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_LFE);
                gst_value_array_append_value (&chanpos, &pos);
            }

            g_value_unset (&pos);

            caps = gst_caps_new_simple("audio/x-raw-int",
                    "endianness", G_TYPE_INT,G_LITTLE_ENDIAN,
                    "signed",G_TYPE_BOOLEAN, TRUE,
                    "width", G_TYPE_INT, pcm_width,
                    "depth", G_TYPE_INT, pcm_width,
                    "rate", G_TYPE_INT, ac3dec_info->dec_param->ac3d_sampling_freq,
                    "channels",G_TYPE_INT,ac3dec_info->dec_param->ac3d_num_channels,
                    NULL);

            gst_structure_set_value (gst_caps_get_structure (caps, 0), "channel-positions", &chanpos);
            g_value_unset (&chanpos);
            gst_pad_set_caps(ac3dec_info->srcpad, caps);
            gst_caps_unref(caps);

            ac3dec_info->sampling_freq_pre = ac3dec_info->dec_param->ac3d_sampling_freq;
            ac3dec_info->num_channels_pre = ac3dec_info->dec_param->ac3d_num_channels;
            ac3dec_info->outputmask_pre = ac3dec_info->dec_param->ac3d_outputmask;
        }

	src_caps = GST_PAD_CAPS(ac3dec_info->srcpad);


#if 0
    result = gst_pad_alloc_buffer_and_set_caps(ac3dec_info->srcpad, 0,
                                                    (pcm_width/8) *
                                                    ac3dec_info->dec_param->ac3d_num_channels *
                                                    AC3D_FRAME_SIZE,
                                                    src_caps, &outbuffer);

	if (result != GST_FLOW_OK)
        {
	    GST_ERROR("\n Error while allocating output buffer\n");
	    g_free(decode_out);
	    return result;
	}
#else
    outbuffer = gst_buffer_new_and_alloc((pcm_width/8) *
                                                    ac3dec_info->dec_param->ac3d_num_channels *
                                                    AC3D_FRAME_SIZE);
    gst_buffer_set_caps(outbuffer, src_caps);
#endif


        if(pcm_width > 16)
        {
            AC3D_INT32 *outdata = (AC3D_INT32 *) GST_BUFFER_DATA(outbuffer);
            if (outdata == NULL) {
                GST_ERROR("\n Could not allocate output buffer\n");
                g_free(decode_out);
                return result;
            }

            /* copy data from codec's output buffer to GST_BUFFER */
            for (loopctr = 0; loopctr < AC3D_FRAME_SIZE * 6; loopctr += 6)
            {
                if(out_L)
                    *outdata++ = ((decode_out[loopctr+0])<<8) ;
                if(out_R)
                    *outdata++ = ((decode_out[loopctr+2])<<8) ;
                if(out_C)
                    *outdata++ = ((decode_out[loopctr+1])<<8) ;
                if(out_lfe)
                    *outdata++ = ((decode_out[loopctr+5])<<8) ;
                if(out_Ls)
                    *outdata++ = ((decode_out[loopctr+3])<<8) ;
                if(out_Rs)
                    *outdata++ = ((decode_out[loopctr+4])<<8) ;
            }
        }
        else
        {
            AC3D_INT16 *outdata = (AC3D_INT16 *) GST_BUFFER_DATA(outbuffer);
            AC3D_INT16 *in_buf = (AC3D_INT16 *)decode_out;
            if (outdata == NULL) {
                GST_ERROR("\n Could not allocate output buffer\n");
                g_free(decode_out);
                return result;
            }

            /* copy data from codec's output buffer to GST_BUFFER */
            for (loopctr = 0; loopctr < AC3D_FRAME_SIZE * 6; loopctr += 6)
            {
                if(out_L)
                    *outdata++ = in_buf[loopctr+0] ;
                if(out_R)
                    *outdata++ = in_buf[loopctr+2];
                if(out_C)
                    *outdata++ = in_buf[loopctr+1];
                if(out_lfe)
                    *outdata++ = in_buf[loopctr+5];
                if(out_Ls)
                    *outdata++ = in_buf[loopctr+3];
                if(out_Rs)
                    *outdata++ = in_buf[loopctr+4];
            }
        }

	g_free(decode_out);

         GST_BUFFER_SIZE(outbuffer) = (pcm_width/8) *
                                       ac3dec_info->dec_param->ac3d_num_channels *
                                      AC3D_FRAME_SIZE,
	ac3dec_info->total_samples += AC3D_FRAME_SIZE;
	time_duration = (AC3D_FRAME_SIZE * 1.0 / ac3dec_info->dec_param->ac3d_sampling_freq);


     //   g_print("time_offset: %f\n", ac3dec_info->time_offset);

	GST_BUFFER_TIMESTAMP(outbuffer) =
	    ac3dec_info->time_offset * GST_SECOND;


	ac3dec_info->time_offset += time_duration;


	GST_BUFFER_DURATION(outbuffer) = time_duration * GST_SECOND;
	GST_BUFFER_OFFSET(outbuffer) = 0;

        if (ac3dec_info->profile)
        {
            GST_BUFFER_SIZE(outbuffer) = 0;
            GST_BUFFER_TIMESTAMP(outbuffer) = 0;
            GST_BUFFER_DURATION(outbuffer) = 0;
        }


        result = gst_pad_push(ac3dec_info->srcpad, outbuffer);
        if (result != GST_FLOW_OK)
        {
            GST_ERROR
                ("\n Error during pushing the buffer to sink, error is %d\n",
                 result);
            return result;
        }

	return GST_FLOW_OK;


    }
    else
    {
	g_free(decode_out);
	GST_ERROR("\n Exiting decode_ac3_chunk return value  %d\n",
		  retval);
	GST_ERROR("\n Error during decoding of a frame\n");
	return GST_FLOW_ERROR;
    }
}

/*==================================================================================================

FUNCTION: app_swap_buffers_ac3_dec

DESCRIPTION:
   This function fills and sends new set of input data to decoder, when it is requested

ARGUMENTS PASSED:
   Pointer to new input buffer,
   pointer to input buffer size
   pointer to decoder configuration parameters.

RETURN VALUE:
   None

PRE-CONDITIONS:
   None

POST-CONDITIONS:
   None

IMPORTANT NOTES:
   None

==================================================================================================*/
AC3D_INT32  app_swap_buffers_ac3_dec(AC3D_UINT8 **buf_ptr,
                                     AC3D_INT32 *buf_len,
                                     void *pAppContext)
{
    AC3_callback *pCallback = (AC3_callback *)pAppContext;
    AC3D_UINT8 *send_buff;
    const guint8 *indata;
    int loopctr;
    AC3D_INT32 req_len;
    GstAdapter *adapter;
    guint bytesavailable = 0;

    adapter = (GstAdapter *) pCallback->ac3dec_info->adapter;
    send_buff = pCallback->ac3dec_info->send_buff;
    req_len = *buf_len;

    bytesavailable = gst_adapter_available(adapter);
    if (bytesavailable < req_len)
    {
        *buf_ptr = NULL;
        *buf_len = 0;
        return -1;
    }

    indata = gst_adapter_peek(adapter, req_len);
    for (loopctr = 0; loopctr < req_len; loopctr ++)
    {
        //printf("[callback]: %2x\n", indata[loopctr]);
	send_buff[loopctr] = indata[loopctr];
    }

    *buf_ptr = (AC3D_UINT8 *) send_buff;
    *buf_len = req_len ;
    gst_adapter_flush(adapter, req_len);

    return 0;
}

/*==================================================================================================

FUNCTION: alloc_fast

DESCRIPTION:
   allocates the memory in internal memory

ARGUMENTS PASSED:
   size of the memory requested.

RETURN VALUE:
   Pointer to allocated memory

PRE-CONDITIONS:
   None

POST-CONDITIONS:
   None

IMPORTANT NOTES:
   None

==================================================================================================*/

static void *alloc_fast(int size)
{
    void *ptr = NULL;
    ptr = g_malloc(size + MEMORY_ALIGNMENT);

    if (ptr == NULL)
	return ptr;

    ptr =
	(void *) (((long) ptr + (long) (LONG_BOUNDARY - 1)) &
		  (long) (~(LONG_BOUNDARY - 1)));
    return ptr;
}

/*==================================================================================================

FUNCTION: alloc_slow

DESCRIPTION:
   allocates the requested memory in external memory

ARGUMENTS PASSED:
   size of the memory requested

RETURN VALUE:
   Pointer to allocated memory

PRE-CONDITIONS:
   None

POST-CONDITIONS:
   None

IMPORTANT NOTES:
   None

==================================================================================================*/
static void *alloc_slow(int size)
{
    void *ptr = NULL;

    ptr = g_malloc(size);
    if (ptr == NULL)
	return ptr;

    ptr =
	(void *) (((long) ptr + (long) LONG_BOUNDARY - 1) &
		  (long) (~(LONG_BOUNDARY - 1)));
    return ptr;
}

FSL_GST_PLUGIN_DEFINE("ac3dec", "ac3 audio decoder", plugin_init);


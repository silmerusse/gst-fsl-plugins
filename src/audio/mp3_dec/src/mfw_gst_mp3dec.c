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
 * Based on gstmad.c by
 * Copyright (c) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 */

/*
 * Module Name:    mfw_gst_mp3dec.c
 *
 * Description:    Implementation of mp3 plugin for Gstreamer using push
 *                 based method.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */

/*==================================================================================================
                            INCLUDE FILES
===================================================================================================*/
#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "mp3_dec_interface.h"
#include "mfw_gst_mp3dec.h"
#include "mp3_parser/mp3_parse.h"

#include "mfw_gst_utils.h"

/*==================================================================================================
                                     LOCAL CONSTANTS
==================================================================================================*/

/* None. */

/*==================================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/


enum {
    ID_0,
    ID_PARSE_FULL,
    ID_PROFILE_ENABLE
};




/* defines sink pad  properties of mp3decoder element */
static GstStaticPadTemplate mfw_gst_mp3dec_sink_factory =
GST_STATIC_PAD_TEMPLATE("sink",
			GST_PAD_SINK,
			GST_PAD_ALWAYS,
			GST_STATIC_CAPS("audio/mpeg, "
					"mpegversion = (int) 1, "
					"layer = (int) [1, 3], "
					"rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
					"channels = (int) [ 1, 2 ]")
    );



/* defines the source pad  properties of mp3decoder element */
static GstStaticPadTemplate mfw_gst_mp3dec_src_factory =
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
					"channels = (int) [ 1, 2 ]")
    );


/*==================================================================================================
                                        LOCAL MACROS
==================================================================================================*/
/*alignment for memory address */
#define LONG_BOUNDARY    4
#define MEMORY_ALIGNMENT 4
#define MFW_MP3DEC_CBKSIZE 512
#define PULL_SIZE   4096

/* threshold for timestamp resetting */
#define TS_RESET_THRESHOLD 2

/* fadein step */
#define FADEIN_STEP 1

/* the clock in MHz for IMX31 to be changed for other platforms */
#define PROCESSOR_CLOCK 532

#define	GST_CAT_DEFAULT	mfw_gst_mp3dec_debug

#define	GST_TAG_MFW_MP3_CHANNELS		"channels"
#define GST_TAG_MFW_MP3_SAMPLING_RATE	"sampling_frequency"

/*For error handling */
#define MP3DEC_ERR_MAXCNT 300
static gint err_count = 0;

/*==================================================================================================
                                      STATIC VARIABLES
==================================================================================================*/

static GstElementClass *parent_class = NULL;
//static MfwGstMp3DecInfo *mp3dec_global_info = NULL;

/*==================================================================================================
                                 STATIC FUNCTION PROTOTYPES
==================================================================================================*/

GST_DEBUG_CATEGORY_STATIC(mfw_gst_mp3dec_debug);
static void mfw_gst_mp3dec_class_init(MfwGstMp3DecInfoClass * klass);
static void mfw_gst_mp3dec_base_init(gpointer klass);
static void mfw_gst_mp3dec_init(MfwGstMp3DecInfo * mp3dec_info);
GType mfw_gst_type_mp3dec_get_type(void);
static GstStateChangeReturn mfw_gst_mp3dec_change_state(GstElement *,
							GstStateChange);
static gboolean mfw_gst_mp3dec_sink_event(GstPad * pad, GstEvent * event);
static GstFlowReturn mfw_gst_mp3dec_chain(GstPad * pad, GstBuffer * buf);
#ifndef PUSH_MODE
static GstFlowReturn decode_mp3_chunk(MfwGstMp3DecInfo * mp3dec_info);
#else
static GstFlowReturn decode_mp3_chunk(MfwGstMp3DecInfo * mp3dec_info, guint bufsize);
#endif


/* Functions used for seeking */
static gboolean mfw_gst_mp3dec_src_query(GstPad * pad, GstQuery * query);
static gboolean mfw_gst_mp3dec_convert_sink(GstPad *, GstFormat, gint64,
					    GstFormat *, gint64 *);
static gboolean mfw_gst_mp3dec_seek(MfwGstMp3DecInfo *, GstPad *,
				    GstEvent *);
static gboolean mfw_gst_mp3dec_convert_src(GstPad *, GstFormat, gint64,
					   GstFormat *, gint64 *);
static gboolean mfw_gst_mp3dec_src_event(GstPad *, GstEvent *);
static void mfw_gst_mp3dec_dispose(GObject * object);
static void mfw_gst_mp3dec_set_index(GstElement * element,
				     GstIndex * index);
static GstIndex *mfw_gst_mp3dec_get_index(GstElement * element);
static void mfw_gst_mp3dec_set_property(GObject * object, guint prop_id,
					const GValue * value,
					GParamSpec * pspec);
static void mfw_gst_mp3dec_get_property(GObject * object,
					guint prop_id, GValue * value,
					GParamSpec * pspec);
static const GstQueryType *mfw_gst_mp3dec_get_query_types(GstPad * pad);
static void decode_mp3_clear_list(MfwGstMp3DecInfo * mp3dec_info);
/*==================================================================================================
                                     GLOBAL VARIABLES
==================================================================================================*/


/*==================================================================================================
                                     LOCAL FUNCTIONS
==================================================================================================*/


/*=============================================================================
FUNCTION: mfw_gst_mp3dec_set_property

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
mfw_gst_mp3dec_set_property(GObject * object, guint prop_id,
			    const GValue * value, GParamSpec * pspec)
{
    MfwGstMp3DecInfo *mp3dec_info = MFW_GST_MP3DEC(object);
    switch (prop_id) {

    case ID_PARSE_FULL:
	mp3dec_info->full_parse = g_value_get_boolean(value);
	GST_DEBUG("fullparse=%d", mp3dec_info->full_parse);
	break;
    case ID_PROFILE_ENABLE:
	mp3dec_info->profile = g_value_get_boolean(value);
	GST_DEBUG("profile=%d", mp3dec_info->profile);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
}

/*=============================================================================
FUNCTION: mfw_gst_mp3dec_get_property

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
mfw_gst_mp3dec_get_property(GObject * object, guint prop_id,
			    GValue * value, GParamSpec * pspec)
{
    MfwGstMp3DecInfo *mp3dec_info = MFW_GST_MP3DEC(object);
    switch (prop_id) {
    case ID_PARSE_FULL:
	GST_DEBUG("fullparse=%d", mp3dec_info->full_parse);
	g_value_set_boolean(value, mp3dec_info->full_parse);
	break;

    case ID_PROFILE_ENABLE:
	GST_DEBUG("profile=%d", mp3dec_info->profile);
	g_value_set_boolean(value, mp3dec_info->profile);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
}



/*=============================================================================
FUNCTION: mfw_gst_mp3dec_get_query_types

DESCRIPTION: gets the different types of query supported by the plugin

ARGUMENTS PASSED:
        pad     - pad on which the function is registered

RETURN VALUE:
        query types ssupported by the plugin

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static const GstQueryType *mfw_gst_mp3dec_get_query_types(GstPad * pad)
{
    static const GstQueryType src_query_types[] = {
	GST_QUERY_POSITION,
	GST_QUERY_DURATION,
	GST_QUERY_CONVERT,
	GST_QUERY_SEEKING,
	0
    };

    return src_query_types;
}


/*=============================================================================
FUNCTION: mfw_gst_mp3dec_dispose

DESCRIPTION: cleans up the plug-in object

ARGUMENTS PASSED:
        object - plug-in object

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static void mfw_gst_mp3dec_dispose(GObject * object)
{
    MfwGstMp3DecInfo *mp3dec_info = MFW_GST_MP3DEC(object);
    mfw_gst_mp3dec_set_index(GST_ELEMENT(object), NULL);
    G_OBJECT_CLASS(parent_class)->dispose(object);
}
/*=============================================================================
FUNCTION: mfw_mp3_mem_flush

DESCRIPTION: this function flushes the current memory and allocate the new memory
                for decoder .

ARGUMENTS PASSED:
        mp3dec_info -   pointer to mp3decoder element structure

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static gboolean mfw_mp3_mem_flush(MfwGstMp3DecInfo * mp3dec_info)
{
    gint loopctr = 0;
    MP3D_Mem_Alloc_Info_Sub *mem = NULL;
    gboolean result = TRUE;
    gint num;

    /*if mp3 decoder does not init, no need to flush again */
    if (!mp3dec_info->init_flag)
	goto done;

    num = mp3dec_info->dec_config->mp3d_mem_info.mp3d_num_reqs;

    if (NULL != mp3dec_info->dec_config) {
	for (loopctr = 0; loopctr < num; loopctr++) {
	    mem =
		&(mp3dec_info->dec_config->mp3d_mem_info.
		  mem_info_sub[loopctr]);
	    if (NULL != mem->app_base_ptr) {
		MM_FREE(mem->app_base_ptr);
		mem->app_base_ptr = NULL;
	    }
	}
	MM_FREE(mp3dec_info->dec_config);
	mp3dec_info->dec_config = NULL;
    }

    if (NULL != mp3dec_info->dec_param) {
	MM_FREE(mp3dec_info->dec_param);
	mp3dec_info->dec_param = NULL;
    }

    mp3dec_info->dec_config =
	(MP3D_Decode_Config *) MM_MALLOC(sizeof(MP3D_Decode_Config));
    if (mp3dec_info->dec_config == NULL) {
	GST_ERROR
	    ("Could not allocate memory for the Mp3Decoder Configuration structure");

	result = FALSE;
	goto done;
    }

    mp3dec_info->dec_param =
	(MP3D_Decode_Params *) MM_MALLOC(sizeof(MP3D_Decode_Params));
    if (mp3dec_info->dec_param == NULL) {
	GST_ERROR
	    ("Could not allocate memory for the MP3Decoder Parameter structure");

	result = FALSE;
	goto done;
    }

    gst_adapter_clear(mp3dec_info->adapter);


    /* Assign output format */

    /* Query for memory */
    if ((mp3d_query_dec_mem(mp3dec_info->dec_config)) != MP3D_OK) {
	GST_ERROR
	    ("Could not Query the Memory from the Decoder library");
	result = FALSE;
	goto done;
    }

    /* number of memory chunks requested by decoder */
    num = mp3dec_info->dec_config->mp3d_mem_info.mp3d_num_reqs;
    for (loopctr = 0; loopctr < num; loopctr++) {
	mem =
	    &(mp3dec_info->dec_config->mp3d_mem_info.
	      mem_info_sub[loopctr]);
	if (mem->mp3d_type == MP3D_FAST_MEMORY) {
	    /*allocates memory in internal memory */
	    mem->app_base_ptr = MM_MALLOC(mem->mp3d_size);
	    if (mem->app_base_ptr == NULL) {
		GST_ERROR
		    ("Could not allocate fast memory for the Decoder");
		result = FALSE;
		goto done;
	    }
	} else {
	    /*allocates memory in external memory */
	    mem->app_base_ptr = MM_MALLOC(mem->mp3d_size);
	    if (mem->app_base_ptr == NULL) {
		GST_ERROR
		    ("Could not allocate slow memory for the Decoder");
		result = FALSE;
		goto done;
	    }
	}
    }
#ifndef PUSH_MODE
    /* Swap buffer function pointer initializations */
    mp3dec_info->dec_config->app_swap_buf = app_swap_buffers_mp3_dec;
#endif
    mp3dec_info->eos_event = FALSE;
    mp3dec_info->frame_no = 0;
    mp3dec_info->init_flag = FALSE;

  done:
    return result;
}

/*=============================================================================
FUNCTION: mfw_gst_mp3dec_set_index

DESCRIPTION: sets the index on the element

ARGUMENTS PASSED:
        element - plugin-element
        index - index to be set

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static void
mfw_gst_mp3dec_set_index(GstElement * element, GstIndex * index)
{
    MfwGstMp3DecInfo *mp3dec_info = MFW_GST_MP3DEC(element);
    mp3dec_info->index = index;
    if (index)
	gst_index_get_writer_id(index, GST_OBJECT(element),
				&mp3dec_info->index_id);
}


/*=============================================================================
FUNCTION: mfw_gst_mp3dec_get_index

DESCRIPTION: gets the index of the element

ARGUMENTS PASSED:
        element - plug-in element

RETURN VALUE:
        the index of the element

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static GstIndex *mfw_gst_mp3dec_get_index(GstElement * element)
{
    MfwGstMp3DecInfo *mp3dec_info = MFW_GST_MP3DEC(element);
    return mp3dec_info->index;
}

/*==================================================================================================

FUNCTION:          mfw_gst_mp3dec_base_init

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
static void mfw_gst_mp3dec_base_init(gpointer klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&mfw_gst_mp3dec_src_factory));

    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&mfw_gst_mp3dec_sink_factory));

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "mp3 audio decoder",
        "Codec/Decoder/Audio", "Decode compressed mp3 audio to raw data");
}

/*==================================================================================================

FUNCTION:       mfw_gst_mp3dec_class_init

DESCRIPTION:    Initialise the class.(specifying what signals,
                 arguments and virtual functions the class has and setting up
                 global states)

ARGUMENTS PASSED:
        klass    -  pointer to mp3decoder element class

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/

static void mfw_gst_mp3dec_class_init(MfwGstMp3DecInfoClass * klass)
{
    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
    gstelement_class->change_state = mfw_gst_mp3dec_change_state;
    gobject_class->set_property = mfw_gst_mp3dec_set_property;
    gobject_class->get_property = mfw_gst_mp3dec_get_property;
    gobject_class->dispose = mfw_gst_mp3dec_dispose;
    gstelement_class->set_index = mfw_gst_mp3dec_set_index;
    gstelement_class->get_index = mfw_gst_mp3dec_get_index;

    g_object_class_install_property(gobject_class, ID_PARSE_FULL,
				    g_param_spec_boolean("vbrduration",
							 "VBRDuration",
							 "Enable parsing of the mp3 stream before pre-roll to calculate average bitrate",
							 FALSE,
							 G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_PROFILE_ENABLE,
				    g_param_spec_boolean("profiling",
							 "Profiling",
							 "Enable time profiling of decoder and plugin",
							 FALSE,
							 G_PARAM_READWRITE));


}

/*==================================================================================================

FUNCTION:       mfw_gst_mp3dec_init

DESCRIPTION:    create the pad template that has been registered with the
                element class in the _base_init

ARGUMENTS PASSED:
        mp3dec_info -    pointer to mp3decoder element structure

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/

static void mfw_gst_mp3dec_init(MfwGstMp3DecInfo * mp3dec_info)
{

    GstElementClass *klass = GST_ELEMENT_GET_CLASS(mp3dec_info);

    /* create the sink and src pads */
    mp3dec_info->sinkpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "sink"), "sink");

    mp3dec_info->srcpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "src"), "src");


    gst_element_add_pad(GST_ELEMENT(mp3dec_info), mp3dec_info->sinkpad);
    gst_element_add_pad(GST_ELEMENT(mp3dec_info), mp3dec_info->srcpad);

    gst_pad_set_chain_function(mp3dec_info->sinkpad, mfw_gst_mp3dec_chain);

    gst_pad_set_event_function(mp3dec_info->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_mp3dec_sink_event));

    gst_pad_set_query_function(mp3dec_info->srcpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_mp3dec_src_query));

    gst_pad_set_query_type_function(mp3dec_info->srcpad,
				    GST_DEBUG_FUNCPTR
				    (mfw_gst_mp3dec_get_query_types));


    gst_pad_set_event_function(mp3dec_info->srcpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_mp3dec_src_event));

    mp3dec_info->profile = FALSE;
    mp3dec_info->full_parse = FALSE;

    /* For error handle */
    /*err handler count should init to 0*/
    err_count = 0;

    /* Get the decoder version */
#define MFW_GST_MP3_DECODER_PLUGIN VERSION
    PRINT_CORE_VERSION(mp3d_decode_versionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_MP3_DECODER_PLUGIN);

    INIT_DEMO_MODE(mp3d_decode_versionInfo(), mp3dec_info->demo_mode);

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
    return gst_element_register(plugin, "mfw_mp3decoder",
				FSL_GST_DEFAULT_DECODER_RANK_LEGACY, MFW_GST_TYPE_MP3DEC);
}


/*==================================================================================================

FUNCTION:       mfw_gst_type_mp3dec_get_type

DESCRIPTION:    Intefaces are initiated in this function.you can register one
                or more interfaces after having registered the type itself.

ARGUMENTS PASSED:
        None

RETURN VALUE:
        A numerical value ,which represents the unique identifier of this
        elment(mp3decoder)

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
GType mfw_gst_type_mp3dec_get_type(void)
{
    static GType mfwgstmp3dec_type = 0;
    if (!mfwgstmp3dec_type) {
	static const GTypeInfo mfwgstmp3dec_info = {
	    sizeof(MfwGstMp3DecInfoClass),
	    (GBaseInitFunc) mfw_gst_mp3dec_base_init,
	    NULL,
	    (GClassInitFunc) mfw_gst_mp3dec_class_init,
	    NULL,
	    NULL,
	    sizeof(MfwGstMp3DecInfo),
	    0,
	    (GInstanceInitFunc) mfw_gst_mp3dec_init,
	};
	mfwgstmp3dec_type = g_type_register_static(GST_TYPE_ELEMENT,
						   "MfwGstMp3DecInfo",
						   &mfwgstmp3dec_info, 0);
    }
    GST_DEBUG_CATEGORY_INIT(mfw_gst_mp3dec_debug, "mfw_mp3decoder", 0,
			    "FreeScale's MP3 Decoder's Log");
    return mfwgstmp3dec_type;
}

/*==================================================================================================

FUNCTION:       mfw_gst_mp3dec_calc_avgbitrate

DESCRIPTION:    header parses the complete input stream and computes the average \
                bitrate of the stream.

ARGUMENTS PASSED:
        mp3dec_info -    pointer to mp3decoder element structure

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
void mfw_gst_mp3dec_calc_avgbitrate(MfwGstMp3DecInfo * mp3dec_info)
{

    GstPad *pad = NULL;
    GstPad *my_peer_pad = NULL;
    GstFormat fmt = GST_FORMAT_BYTES;
    mp3_fr_info fr_size;

    pad = mp3dec_info->sinkpad;
    if (gst_pad_check_pull_range(pad)) {
	if (gst_pad_activate_pull(GST_PAD_PEER(pad), TRUE)) {

	    GstBuffer *pullbuffer = NULL;
	    GstFlowReturn ret = GST_FLOW_OK;
	    guint pullsize = PULL_SIZE;
	    GstAdapter *adapter = NULL;
	    const guint8 *data = NULL;
	    guint size = 0;
	    adapter = mp3dec_info->adapter;
	    gint first = 0;
	    gint64 totalduration = 0;
	    gint bitrate_mp3 = 0;
	    my_peer_pad = gst_pad_get_peer(mp3dec_info->sinkpad);

	    /* get total duration of file  in bytes */
	    gst_pad_query_duration(my_peer_pad, &fmt, &totalduration);
	    GST_DEBUG("Total duration of the file is =%f",
		      (gfloat) totalduration);
            gst_object_unref(GST_OBJECT(my_peer_pad));

	    do {

		ret =
		    gst_pad_pull_range(pad, mp3dec_info->offset, pullsize,
				       &pullbuffer);
		if (ret != GST_FLOW_OK
		    || (GST_BUFFER_SIZE(pullbuffer) == 0)) {
		    GST_ERROR("Error during pulling of data");
		    break;
		}
		pullsize = GST_BUFFER_SIZE(pullbuffer);
		gst_adapter_push(adapter, pullbuffer);
		mp3dec_info->offset += pullsize;
		size = gst_adapter_available(adapter);
		data = gst_adapter_peek(adapter, size);
		/* header parsing to get bitrate information */
        if (!mp3dec_info->strm_info_get) {
            fr_size = mp3_parser_parse_frame_header((gchar *) data, size, NULL);
            if (fr_size.index < size) {
                gst_adapter_flush(adapter, fr_size.index);
            } else {
                gst_adapter_flush(adapter, size);
                continue;
            }
            if(fr_size.flags == FLAG_NEEDMORE_DATA)
                continue;

            memcpy(&mp3dec_info->strm_info, &fr_size, sizeof(mp3_fr_info));
            mp3dec_info->strm_info_get = TRUE;
        }
        else {
            fr_size = mp3_parser_parse_frame_header((gchar *) data, size, &mp3dec_info->strm_info);
            if (fr_size.index < size) {
                gst_adapter_flush(adapter, fr_size.index);
            } else {
                gst_adapter_flush(adapter, size);
                continue;
            }
            if(fr_size.flags == FLAG_NEEDMORE_DATA)
		    continue;
		}
		bitrate_mp3 = fr_size.b_rate * 1000;
		mp3dec_info->total_bitrate += bitrate_mp3;
		mp3dec_info->total_frames++;
		/* flush out the bytes consumed */
        size = gst_adapter_available(adapter);
        if (size > (fr_size.index + fr_size.frm_size)) {
    		gst_adapter_flush(adapter,
    				  (fr_size.index + fr_size.frm_size));
        }
        else
            gst_adapter_flush(adapter, size);

		pullsize = (fr_size.index + fr_size.frm_size);
	    } while (mp3dec_info->offset < totalduration);
        mp3dec_info->strm_info_get = FALSE;
        memset(&mp3dec_info->strm_info, 0, sizeof(mp3_fr_info));
	}

	GST_DEBUG("total bitrate = %f",
		  (gfloat) mp3dec_info->total_bitrate);
	GST_DEBUG("total frames = %f",
		  (gfloat) mp3dec_info->total_frames);

	mp3dec_info->avg_bitrate = (gfloat) mp3dec_info->total_bitrate /
	    mp3dec_info->total_frames;

	GST_DEBUG("average bitrate = %f", mp3dec_info->avg_bitrate);
	/* flush all the buffers */
	gst_adapter_clear(mp3dec_info->adapter);
	gst_pad_activate_push(GST_PAD_PEER(pad), TRUE);


    } else {
	GST_DEBUG("cannot calulate the average bitrate during preroll \
        gst_pad_pull_range() cannot be performed on the peer source pad");

    }
}


/*==================================================================================================

FUNCTION:     mfw_gst_mp3dec_change_state

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
mfw_gst_mp3dec_change_state(GstElement * element,
			    GstStateChange transition)
{
    GstStateChangeReturn ret;
    MfwGstMp3DecInfo *mp3dec_info;
    mp3dec_info = MFW_GST_MP3DEC(element);
    MP3D_Mem_Alloc_Info_Sub *mem = NULL;
    MP3D_RET_TYPE retval = MP3D_OK;
    gint loopctr = 0;
    gint num = 0;


    GST_DEBUG("transistion is %d", transition);
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
	{
	    /* initialise all the variable */

        MM_INIT_DBG_MEM("mp3dec");
	    mp3dec_info->eos_event = FALSE;
	    mp3dec_info->init_flag = FALSE;
	    mp3dec_info->dec_config = NULL;
	    mp3dec_info->dec_param = NULL;
	    mp3dec_info->caps_set = FALSE;
	    mp3dec_info->frame_no = 0;
	    mp3dec_info->time_offset = 0;
	    mp3dec_info->offset = 0;
            mp3dec_info->mpeg_layer = 0;
	    mp3dec_info->new_seg_flag = FALSE;
	    mp3dec_info->total_bitrate = 0;
	    mp3dec_info->total_frames = 0;
	    mp3dec_info->total_samples = 0;
	    mp3dec_info->id3v2_size = 0;
	    mp3dec_info->send_buff = NULL;
	    mp3dec_info->avg_bitrate = 0.0f;
	    mp3dec_info->index = NULL;
	    mp3dec_info->index_id = 0;
	    mp3dec_info->total_samples = 0;
	    mp3dec_info->seeked_time = 0;
	    mp3dec_info->id3tag = FALSE;
	    mp3dec_info->Time = 0;
	    mp3dec_info->chain_Time = 0;
	    mp3dec_info->no_of_frames_dropped = 0;
	    mp3dec_info->layerchk = FALSE;
        mp3dec_info->strm_info_get = FALSE;
        mp3dec_info->new_segment = 1;
        mp3dec_info->flushed_bytes = 0;
        mp3dec_info->buf_list = NULL;
        mp3dec_info->fadein_factor = 0;
        memset(&mp3dec_info->strm_info, 0, sizeof(mp3_fr_info));
	    break;
	}
    case GST_STATE_CHANGE_READY_TO_PAUSED:
	{
        gst_tag_register (GST_TAG_MFW_MP3_CHANNELS, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
            "number of channels","number of channels", NULL);
        gst_tag_register (GST_TAG_MFW_MP3_SAMPLING_RATE, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
            "sampling frequency (Hz)","sampling frequency (Hz)", NULL);

        /* allocate the adapter */
	    mp3dec_info->adapter = gst_adapter_new();
	    if (mp3dec_info->adapter == NULL) {
		GST_ERROR
		    ("Could not allocate memory for the Adapter");
		return GST_STATE_NULL;
	    }
	    if (!mp3dec_info->init_flag) {

		/* allocate the decoder configuration structure */
		mp3dec_info->dec_config = (MP3D_Decode_Config *)
		    MM_MALLOC(sizeof(MP3D_Decode_Config));
		if (mp3dec_info->dec_config == NULL) {
		    GST_ERROR("Could not allocate memory \
                    for the Mp3Decoder Configuration structure");

		    return GST_STATE_NULL;
		}


		/* allocate the buffer used to send the data during call back */
		mp3dec_info->send_buff = MM_MALLOC(MP3D_INPUT_BUF_SIZE);
		if (mp3dec_info->send_buff == NULL) {
		    GST_ERROR("Could not allocate memory for the \
                    buffer sent during callback");

		    return GST_STATE_NULL;
		}
		memset(mp3dec_info->send_buff, 0, MP3D_INPUT_BUF_SIZE);
		/* allocate the decoder parameter structure */
		mp3dec_info->dec_param = (MP3D_Decode_Params *)
		    MM_MALLOC(sizeof(MP3D_Decode_Params));
		if (mp3dec_info->dec_param == NULL) {
		    GST_ERROR("Could not allocate memory for the \
                    MP3Decoder Parameter structure");
		    return GST_STATE_NULL;
		}


		/* Assign output format */

        /* Query for memory */
		if ((retval =
		     mp3d_query_dec_mem(mp3dec_info->dec_config)) !=
		    MP3D_OK) {
		    GST_ERROR
			("Could not Query the Memory from the Decoder library");
		    return GST_STATE_NULL;
		}

		/* number of memory chunks requested by decoder */
		num = mp3dec_info->dec_config->mp3d_mem_info.mp3d_num_reqs;

		for (loopctr = 0; loopctr < num; loopctr++) {
		    mem =
			&(mp3dec_info->dec_config->mp3d_mem_info.
			  mem_info_sub[loopctr]);

		    if (mem->mp3d_type == MP3D_FAST_MEMORY) {
			/*allocates memory in internal memory */
			mem->app_base_ptr = MM_MALLOC(mem->mp3d_size);
			if (mem->app_base_ptr == NULL) {
			    GST_ERROR
				("Could not allocate memory for the Decoder");
			    return GST_STATE_NULL;
			}
		    } else {
			/*allocates memory in external memory */
			mem->app_base_ptr = MM_MALLOC(mem->mp3d_size);
			if (mem->app_base_ptr == NULL) {
			    GST_ERROR
				("Could not allocate memory for the Decoder");
			    return GST_STATE_NULL;
			}
		    }
		}
#ifndef PUSH_MODE
		/* register the call back function in the decoder config structure */
		mp3dec_info->dec_config->app_swap_buf =
		    app_swap_buffers_mp3_dec;
#endif
	    }

	    /* header parsing to calulate the average bitrate */

	    if (mp3dec_info->full_parse) {
		if (!mp3dec_info->total_bitrate) {
		    mfw_gst_mp3dec_calc_avgbitrate(mp3dec_info);
		}
	    }
	    break;
	}
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	mp3dec_info->stopped = FALSE;
	break;
    default:
	break;
    }

    ret = parent_class->change_state(element, transition);
    switch (transition) {
	gfloat avg_mcps = 0, avg_plugin_time = 0, avg_dec_time = 0;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	mp3dec_info->stopped = TRUE;
	break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
	{
	    if (mp3dec_info->profile) {

		g_print
		    ("\nProfile Figures for FSL MP3 Decoder gstreamer Plugin\n");

		g_print("\nTotal decode time is                   %ldus",
			mp3dec_info->Time);
		g_print("\nTotal plugin time is                   %ldus",
			mp3dec_info->chain_Time);
		g_print("\nTotal number of frames decoded is      %d",
			mp3dec_info->frame_no);
		g_print("\nTotal number of frames dropped is      %d\n",
			mp3dec_info->no_of_frames_dropped);

		avg_mcps =
		    (((float) mp3dec_info->dec_param->mp3d_sampling_freq /
		      MP3D_FRAME_SIZE) * mp3dec_info->Time *
		     PROCESSOR_CLOCK)
		    / (mp3dec_info->frame_no -
		       mp3dec_info->no_of_frames_dropped) / 1000000;

		g_print("\nAverage decode MCPS is  %f\n", avg_mcps);

		avg_mcps =
		    (((float) mp3dec_info->dec_param->mp3d_sampling_freq /
		      MP3D_FRAME_SIZE) * mp3dec_info->chain_Time *
		     PROCESSOR_CLOCK)
		    / (mp3dec_info->frame_no -
		       mp3dec_info->no_of_frames_dropped) / 1000000;

		g_print("\nAverage plugin MCPS is  %f\n", avg_mcps);


		avg_dec_time =
		    ((float) mp3dec_info->Time) / mp3dec_info->frame_no;
		g_print("\nAverage decoding time is               %fus",
			avg_dec_time);
		avg_plugin_time =
		    ((float) mp3dec_info->chain_Time) /
		    mp3dec_info->frame_no;
		g_print("\nAverage plugin time is                 %fus\n",
			avg_plugin_time);

		mp3dec_info->Time = 0;
		mp3dec_info->chain_Time = 0;
		mp3dec_info->frame_no = 0;
		mp3dec_info->no_of_frames_dropped = 0;
	    }
	    gst_adapter_clear(mp3dec_info->adapter);
	    g_object_unref(mp3dec_info->adapter);

	    mp3dec_info->adapter = NULL;
	    num = mp3dec_info->dec_config->mp3d_mem_info.mp3d_num_reqs;
	    for (loopctr = 0; loopctr < num; loopctr++) {
		mem =
		    &(mp3dec_info->dec_config->mp3d_mem_info.
		      mem_info_sub[loopctr]);
		if (mem->app_base_ptr != NULL) {
		    MM_FREE(mem->app_base_ptr);
		    mem->app_base_ptr = NULL;
		}
	    }
	    if (mp3dec_info->dec_param != NULL) {
		MM_FREE(mp3dec_info->dec_param);
		mp3dec_info->dec_param = NULL;
	    }
	    if (mp3dec_info->send_buff != NULL) {
		MM_FREE(mp3dec_info->send_buff);
		mp3dec_info->send_buff = NULL;
	    }
	    if (mp3dec_info->dec_config != NULL) {
		MM_FREE(mp3dec_info->dec_config);
		mp3dec_info->dec_config = NULL;
	    }
	    mp3dec_info->eos_event = FALSE;
	    mp3dec_info->init_flag = FALSE;
	    mp3dec_info->caps_set = FALSE;
	    mp3dec_info->time_offset = 0;
            mp3dec_info->mpeg_layer = 0;
	    mp3dec_info->offset = 0;
	    mp3dec_info->new_seg_flag = FALSE;
	    mp3dec_info->seeked_time = 0;
	    mp3dec_info->total_samples = 0;
	    mp3dec_info->id3v2_size = 0;
	    mp3dec_info->index_id = 0;
	    mp3dec_info->total_samples = 0;
	    mp3dec_info->id3tag = FALSE;
	}
	break;
    case GST_STATE_CHANGE_READY_TO_NULL:
	{
	    mp3dec_info->total_bitrate = 0;
	    mp3dec_info->total_frames = 0;
	    mp3dec_info->avg_bitrate = 0.0f;
	    mp3dec_info->layerchk = FALSE;
        //mp3dec_global_info = NULL;
        MM_DEINIT_DBG_MEM();
	    break;
	}
    default:
	break;
    }
    return ret;
}

/*==================================================================================================

FUNCTION:       mfw_gst_mp3dec_sink_event

DESCRIPTION:    send an event to sink  pad of mp3decoder element

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

static gboolean mfw_gst_mp3dec_sink_event(GstPad * pad, GstEvent * event)
{
    MfwGstMp3DecInfo *mp3dec_info = MFW_GST_MP3DEC(GST_PAD_PARENT(pad));
    gboolean result = TRUE;
    GstAdapter *adapter = NULL;
    MP3D_RET_TYPE retval = MP3D_OK;
    mp3_fr_info fr_size;
    MP3D_INT32 size = 0;
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

        mp3dec_info->new_segment = 1;
	    if (format == GST_FORMAT_BYTES) {
		format = GST_FORMAT_TIME;
		if (start != 0)
		    mfw_gst_mp3dec_convert_sink(pad, GST_FORMAT_BYTES,
						start, &format, &nstart);
		else
		    nstart = start;
		if (stop != 0)
		    mfw_gst_mp3dec_convert_sink(pad, GST_FORMAT_BYTES,
						stop, &format, &nstop);
		else
		    nstop = stop;

		nevent =
		    gst_event_new_new_segment(FALSE, 1.0, GST_FORMAT_TIME,
					      nstart, nstop, nstart);
		gst_event_unref(event);
		mp3dec_info->time_offset = (guint64)nstart;
		result = gst_pad_push_event(mp3dec_info->srcpad, nevent);
		if (TRUE != result) {
		    GST_ERROR
			("Error in pushing the event,result	is %d",
			 result);

		}
	    } else if (format == GST_FORMAT_TIME) {
		mp3dec_info->time_offset = (guint64)start;
		result = gst_pad_push_event(mp3dec_info->srcpad, event);
		if (TRUE != result) {
		    GST_ERROR
			("Error in pushing the event,result	is %d",
			 result);
		}
	    }


	    break;
	}

    case GST_EVENT_EOS:
	{
	    /* decode the remaining data. */
	    GST_DEBUG("Got the EOS from sink");

        if(mp3dec_info->init_flag){
	    mp3dec_info->eos_event = TRUE;
	    adapter = (GstAdapter *) mp3dec_info->adapter;
	    if (mp3dec_info->profile) {
		gettimeofday(&tv_prof2, 0);
	    }
	    do {
        //if(mp3dec_global_info==NULL){
        //    break;
        //}

		if (mp3dec_info->stopped) {
		    mp3dec_info->stopped = FALSE;
		    break;
		}


#ifndef PUSH_MODE
    		res = decode_mp3_chunk(mp3dec_info);
#else
            res = decode_mp3_chunk(mp3dec_info, gst_adapter_available(adapter));
#endif
		if (res != GST_FLOW_OK) {
		    break;
		}
	    } while (gst_adapter_available(adapter) > 0);

            /* ENGR72798: continue decoding until decoder return end-of-stream */
        while (res == GST_FLOW_OK){
#ifndef PUSH_MODE
            res = decode_mp3_chunk(mp3dec_info);
#else
            res = decode_mp3_chunk(mp3dec_info, 0);
#endif
        }
	    if (mp3dec_info->profile) {

		gettimeofday(&tv_prof3, 0);
		time_before =
		    (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
		time_after =
		    (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
		mp3dec_info->chain_Time += time_after - time_before;
	    }
        }
	    result = gst_pad_push_event(mp3dec_info->srcpad, event);
	    if (TRUE != result) {
		GST_ERROR("Error in pushing the event,result	is %d",
			  result);
	    }
		/* this is useful for repeat mode, for null repeat mode, changestate will
		 * free these mp3 decoder memories. */
	    mfw_mp3_mem_flush(mp3dec_info);
        //mp3dec_info->strm_info_get = FALSE;
        err_count = 0;

	    break;
	}

    case GST_EVENT_FLUSH_STOP:
	{
#ifdef PUSH_MODE
        gst_adapter_clear(mp3dec_info->adapter);
        decode_mp3_clear_list(mp3dec_info);
#endif
	    result = gst_pad_push_event(mp3dec_info->srcpad, event);

	    if (TRUE != result) {
		GST_ERROR("Error in pushing the event,result	is %d",
			  result);
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

FUNCTION:   mfw_gst_mp3dec_src_query

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
static gboolean mfw_gst_mp3dec_src_query(GstPad * pad, GstQuery * query)
{
    gboolean res = TRUE;
    GstPad *peer;
    MfwGstMp3DecInfo *mp3dec_info;
    mp3dec_info = MFW_GST_MP3DEC(GST_PAD_PARENT(pad));

    peer = gst_pad_get_peer(mp3dec_info->sinkpad);
    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_POSITION:
	{

	    GstFormat format;
	    gint64 cur;
	    /* save requested format */
	    gst_query_parse_position(query, &format, NULL);

	    /* try any demuxer before us first */
	    if (format == GST_FORMAT_TIME && peer
		&& gst_pad_query(peer, query)) {
		gst_query_parse_position(query, NULL, &cur);
		GST_LOG_OBJECT(mp3dec_info,
			       "peer returned position %" GST_TIME_FORMAT,
			       GST_TIME_ARGS(cur));
		break;
	    }

	    /* and convert to the requested format */
	    if (format != GST_FORMAT_DEFAULT) {
		if (!mfw_gst_mp3dec_convert_src(pad, GST_FORMAT_DEFAULT,
						mp3dec_info->total_samples,
						&format, &cur))
		    goto error;
	    } else {
		cur = mp3dec_info->total_samples;
	    }

	    gst_query_set_position(query, format, cur);

	    if (format == GST_FORMAT_TIME) {
		GST_DEBUG("position=%" GST_TIME_FORMAT,
			  GST_TIME_ARGS(cur));
	    } else {
		GST_DEBUG("position=%" G_GINT64_FORMAT ", format=%u", cur,
			  format);
	    }
	    break;

	}
	break;
    case GST_QUERY_DURATION:
	{
	    GstFormat format;
	    GstFormat rformat;
	    gint64 total, total_bytes;

	    /* save requested format */
	    gst_query_parse_duration(query, &format, NULL);
	    if (peer  == NULL)
		goto error;

	    if (format == GST_FORMAT_TIME && gst_pad_query(peer, query)) {
		gst_query_parse_duration(query, NULL, &total);
		GST_LOG_OBJECT(mp3dec_info,
			       "peer returned duration %" GST_TIME_FORMAT,
			       GST_TIME_ARGS(total));
		break;
	    }
	    /* query peer for total length in bytes */
	    gst_query_set_duration(query, GST_FORMAT_BYTES, -1);


	    if (!gst_pad_query(peer, query)) {
		goto error;
	    }

	    /* get the returned format */
	    gst_query_parse_duration(query, &rformat, &total_bytes);

	    if (rformat == GST_FORMAT_BYTES) {
		GST_DEBUG("peer pad returned total bytes=%lld", total_bytes);
	    } else if (rformat == GST_FORMAT_TIME) {
		GST_DEBUG("peer pad returned total time=%",
			  GST_TIME_FORMAT, GST_TIME_ARGS(total_bytes));
	    }

	    /* Check if requested format is returned format */
	    if (format == rformat)
		return TRUE;

	    if (total_bytes != -1) {
		if (format != GST_FORMAT_BYTES) {
		    if (!mfw_gst_mp3dec_convert_sink
			(pad, GST_FORMAT_BYTES, total_bytes, &format,
			 &total))
			goto error;
		} else {
		    total = total_bytes;
		}
	    } else {
		total = -1;
	    }

	    gst_query_set_duration(query, format, total);

	    if (format == GST_FORMAT_TIME) {
		GST_DEBUG("duration=%" GST_TIME_FORMAT,
			  GST_TIME_ARGS(total));
	    } else {
		GST_DEBUG("duration=%" G_GINT64_FORMAT ",format=%u", total,
			  format);
	    }
	    break;
	}
    case GST_QUERY_CONVERT:
	{
	    GstFormat src_fmt, dest_fmt;
	    gint64 src_val, dest_val;
	    gst_query_parse_convert(query, &src_fmt, &src_val, &dest_fmt,
				    &dest_val);
	    if (!
		(res =
		 mfw_gst_mp3dec_convert_src(pad, src_fmt, src_val,
					    &dest_fmt, &dest_val)))
		goto error;

	    gst_query_set_convert(query, src_fmt, src_val, dest_fmt,
				  dest_val);
	    break;
	}
	case GST_QUERY_SEEKING:
        res = gst_pad_query(peer, query);
        break;
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

FUNCTION:   mfw_gst_mp3dec_convert_sink

DESCRIPTION:    converts the format of value from src format to destination format on sink pad .


ARGUMENTS PASSED:
        pad         -   pointer to GstPad
        src_format  -   format of source value
        src_value   -   value of soure
        dest_format -   format of destination value
        dest_value  -   value of destination

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
mfw_gst_mp3dec_convert_sink(GstPad * pad, GstFormat src_format,
			    gint64 src_value, GstFormat * dest_format,
			    gint64 * dest_value)
{
    gboolean res = TRUE;
    guint64 avg_bitrate = 0;
    MfwGstMp3DecInfo *mp3dec_info;
    mp3dec_info = MFW_GST_MP3DEC(GST_PAD_PARENT(pad));
    if(mp3dec_info->total_frames) {
        avg_bitrate =
	    mp3dec_info->total_bitrate / mp3dec_info->total_frames;
    }
    else
       avg_bitrate = mp3dec_info->avg_bitrate;

    switch (src_format) {
    case GST_FORMAT_BYTES:
	switch (*dest_format) {
	case GST_FORMAT_TIME:
        if(avg_bitrate) {
	        *dest_value = gst_util_uint64_scale(src_value, 8 * GST_SECOND,
						avg_bitrate);
        }
        else {
            /* use default bitrate */
            *dest_value=GST_CLOCK_TIME_NONE;
            GST_ERROR("Cannot get the duration, should not be here.");
        }
	    break;
	default:
	    res = FALSE;
	}
	break;
    case GST_FORMAT_TIME:
	switch (*dest_format) {
	case GST_FORMAT_BYTES:
            if (src_value == GST_CLOCK_TIME_NONE) {
              *dest_value = 0;
            }
            else
               if (avg_bitrate) {
	           *dest_value = gst_util_uint64_scale(src_value, avg_bitrate,
						8 * GST_SECOND);
               }
            else {
               *dest_value=0;
               }

	    break;

	default:
	    res = FALSE;
	}
	break;
    default:
	res = FALSE;
    }
    return res;
}


/*==================================================================================================

FUNCTION:   mfw_gst_mp3dec_seek

DESCRIPTION:    performs seek operation

ARGUMENTS PASSED:
        mp3dec_info -   pointer to decoder element
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
mfw_gst_mp3dec_seek(MfwGstMp3DecInfo * mp3dec_info, GstPad * pad,
		    GstEvent * event)
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
    gst_event_parse_seek(event, &rate, &format, &flags, &cur_type, &cur,
			 &stop_type, &stop);


    mp3dec_info->seeked_time = cur;



    GST_DEBUG("seek from  %" GST_TIME_FORMAT "--------------- to %"
	      GST_TIME_FORMAT, GST_TIME_ARGS(cur), GST_TIME_ARGS(stop));

    if (format != GST_FORMAT_TIME) {
	conv = GST_FORMAT_TIME;
	if (!mfw_gst_mp3dec_convert_src
	    (pad, format, cur, &conv, &time_cur))
	    goto convert_error;
	if (!mfw_gst_mp3dec_convert_src
	    (pad, format, stop, &conv, &time_stop))
	    goto convert_error;
    } else {
	time_cur = cur;
	time_stop = stop;
    }

    GST_DEBUG("seek from  %" GST_TIME_FORMAT "--------------- to %"
	      GST_TIME_FORMAT, GST_TIME_ARGS(time_cur),
	      GST_TIME_ARGS(time_stop));

    /* shave off the flush flag, we'll need it later */
    flush = ((flags & GST_SEEK_FLAG_FLUSH) != 0);
    res = FALSE;
    conv = GST_FORMAT_BYTES;
    if (!mfw_gst_mp3dec_convert_sink
	(pad, GST_FORMAT_TIME, time_cur, &conv, &bytes_cur))
	goto convert_error;

    if (!mfw_gst_mp3dec_convert_sink
	(pad, GST_FORMAT_TIME, time_stop, &conv, &bytes_stop))
	goto convert_error;

    if (FALSE == gst_pad_push_event(mp3dec_info->sinkpad, event))
    {
	GstEvent *seek_event;
	seek_event =
	    gst_event_new_seek(rate, GST_FORMAT_BYTES, flags, cur_type,
			       bytes_cur, stop_type, bytes_stop);

	/* do the seek */
	res = gst_pad_push_event(mp3dec_info->sinkpad, seek_event);
	/*if (res)
	   {
	   mp3dec_info->new_seg_flag = 0;
	   mp3dec_info->seeked_time = time_cur;
	   } */
    }

    return TRUE;

    /* ERRORS */
  convert_error:
    {
	/* probably unsupported seek format */
	GST_ERROR("failed to convert format %u into GST_FORMAT_TIME",
		  format);
	return FALSE;
    }
}



/*==================================================================================================
FUNCTION:   mfw_gst_mp3dec_convert_src

DESCRIPTION:    converts the format of value from src format to destination format on src pad .

ARGUMENTS PASSED:
        pad         -   pointer to GstPad
        src_format  -   format of source value
        src_value   -   value of soure
        dest_format -   format of destination value
        dest_value  -   value of destination

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
mfw_gst_mp3dec_convert_src(GstPad * pad, GstFormat src_format,
			   gint64 src_value, GstFormat * dest_format,
			   gint64 * dest_value)
{
    gboolean res = TRUE;
    guint scale = 1;
    gint bytes_per_sample;

    MfwGstMp3DecInfo *mp3dec_info;
    mp3dec_info = MFW_GST_MP3DEC(GST_PAD_PARENT(pad));
    bytes_per_sample = mp3dec_info->dec_param->mp3d_num_channels * 4;

    switch (src_format) {
    case GST_FORMAT_BYTES:
	switch (*dest_format) {
	case GST_FORMAT_DEFAULT:
	    if (bytes_per_sample == 0)
		return FALSE;
	    *dest_value = src_value / bytes_per_sample;
	    break;
	case GST_FORMAT_TIME:
	    {
		gint byterate = bytes_per_sample *
		    mp3dec_info->dec_param->mp3d_sampling_freq;
		if (byterate == 0)
		    return FALSE;
		*dest_value = src_value * GST_SECOND / byterate;
		break;
	    }
	default:
	    res = FALSE;
	}
	break;
    case GST_FORMAT_DEFAULT:
	switch (*dest_format) {
	case GST_FORMAT_BYTES:
	    *dest_value = src_value * bytes_per_sample;
	    break;
	case GST_FORMAT_TIME:
	    if (mp3dec_info->dec_param->mp3d_sampling_freq == 0)
		return FALSE;
	    *dest_value = src_value * GST_SECOND /
		mp3dec_info->dec_param->mp3d_sampling_freq;
	    break;
	default:
	    res = FALSE;
	}
	break;
    case GST_FORMAT_TIME:
	switch (*dest_format) {
	case GST_FORMAT_BYTES:
	    scale = bytes_per_sample;
	    /* fallthrough */
	case GST_FORMAT_DEFAULT:
	    *dest_value = src_value * scale *
		mp3dec_info->dec_param->mp3d_sampling_freq / GST_SECOND;
	    break;
	default:
	    res = FALSE;
	}
	break;
    default:
	res = FALSE;
    }

    return res;
}

/*==================================================================================================

FUNCTION:   mfw_gst_mp3dec_src_event

DESCRIPTION:    send an event to src  pad of mp3decoder element

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

static gboolean mfw_gst_mp3dec_src_event(GstPad * pad, GstEvent * event)
{

    gboolean res = TRUE;

    GST_DEBUG("coming in mfw_gst_mp3dec_src_event");

    MfwGstMp3DecInfo *mp3dec_info;
    mp3dec_info = MFW_GST_MP3DEC(GST_PAD_PARENT(pad));


    switch (GST_EVENT_TYPE(event)) {
	/* the all-formats seek logic */
    case GST_EVENT_SEEK:
        /* For error handle & send SEEK event to up-element will produce EOS while Backforward,we donot need send it, because when play with demuxer,we only send SEEK event to video branch */
	err_count = 0;
       	res = mfw_gst_mp3dec_seek (mp3dec_info, pad, event);
	break;
    default:
	res = FALSE;
	break;
    }

    if (FALSE == res) {
        gst_event_unref(event);
    }
    return res;
}


static void decode_mp3_add_buf_to_list(MfwGstMp3DecInfo * mp3dec_info, GstBuffer *buffer)
{
    gst_buffer_ref (buffer);
    mp3dec_info->buf_list = g_list_append(mp3dec_info->buf_list, buffer);
    GST_DEBUG("Add buffer to list : %" GST_TIME_FORMAT,
              GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)));
}


static void decode_mp3_flush_buf_from_list(MfwGstMp3DecInfo * mp3dec_info, gint size)
{
    GstBuffer *first_buf = (GstBuffer *)g_list_nth_data(mp3dec_info->buf_list, 0);
    mp3dec_info->flushed_bytes += size;
    if (GST_IS_BUFFER(first_buf)) {
        if (mp3dec_info->flushed_bytes >= GST_BUFFER_SIZE(first_buf)) {
            GST_DEBUG("Remove buffer from list : %" GST_TIME_FORMAT,
                      GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(first_buf)));
            mp3dec_info->buf_list = g_list_remove(mp3dec_info->buf_list, first_buf);
            gst_buffer_unref (first_buf);
            mp3dec_info->flushed_bytes -= GST_BUFFER_SIZE(first_buf);
        }
    }
}


static void decode_mp3_clear_list(MfwGstMp3DecInfo * mp3dec_info)
{
    GstBuffer *buffer = (GstBuffer *)g_list_nth_data(mp3dec_info->buf_list, 0);
    GST_DEBUG("Reset buffer list");
    while (GST_IS_BUFFER(buffer)) {
        mp3dec_info->buf_list = g_list_remove(mp3dec_info->buf_list, buffer);
        gst_buffer_unref (buffer);
        buffer = (GstBuffer *)g_list_nth_data(mp3dec_info->buf_list, 0);
    }
    mp3dec_info->flushed_bytes = 0;
}


static GstClockTime decode_mp3_get_timestamp(MfwGstMp3DecInfo * mp3dec_info)
{
    GstClockTime ts = GST_CLOCK_TIME_NONE;
    GstBuffer *first_buf = (GstBuffer *)g_list_nth_data(mp3dec_info->buf_list, 0);
    GstBuffer *second_buf = (GstBuffer *)g_list_nth_data(mp3dec_info->buf_list, 1);
    if (GST_IS_BUFFER(first_buf) && GST_IS_BUFFER(second_buf)) {
        GstClockTime first_ts = GST_BUFFER_TIMESTAMP(first_buf);
        GstClockTime second_ts = GST_BUFFER_TIMESTAMP(second_buf);
        GST_DEBUG("first buffer : %" GST_TIME_FORMAT ", "
                  "second buffer : %" GST_TIME_FORMAT,
                  GST_TIME_ARGS(first_ts), GST_TIME_ARGS(second_ts));
        if (GST_CLOCK_TIME_IS_VALID(first_ts) &&
            GST_CLOCK_TIME_IS_VALID(second_ts) &&
            (second_ts > first_ts)) {
            ts = first_ts +
                gst_util_uint64_scale_int(second_ts - first_ts,
                                          mp3dec_info->flushed_bytes,
                                          GST_BUFFER_SIZE(first_buf));
        }
    }

    return ts;
}


static void decode_mp3_fadein_mono(MfwGstMp3DecInfo * mp3dec_info, GstBuffer *buffer)
{
    gint size = GST_BUFFER_SIZE (buffer) / 2;
    gint16 *data = (gint16 *)GST_BUFFER_DATA (buffer);
    gint number = size;
    gint max = mp3dec_info->dec_param->mp3d_sampling_freq - mp3dec_info->fadein_factor;

    if (max > 0) {
        gint i = 0;
        number = MIN(size, max);
        for (i = 0; i < number; i++) {
            data[i] = (gint16)(data[i] * mp3dec_info->fadein_factor /
                                    mp3dec_info->dec_param->mp3d_sampling_freq);
            mp3dec_info->fadein_factor += FADEIN_STEP;
        }
    }
}


static void decode_mp3_fadein_steoro(MfwGstMp3DecInfo * mp3dec_info, GstBuffer *buffer)
{
    gint size = GST_BUFFER_SIZE (buffer) / 2;
    gint16 *data = (gint16 *)GST_BUFFER_DATA (buffer);
    gint number = 0;
    gint max = mp3dec_info->dec_param->mp3d_sampling_freq - mp3dec_info->fadein_factor;

    if (max > 0) {
        gint i = 0;
        number = MIN(size, max);
        for (i = 0; i < number; i += 2) {
             data[i] = (gint16)(data[i] * mp3dec_info->fadein_factor /
                                    mp3dec_info->dec_param->mp3d_sampling_freq);
            data[i + 1] = (gint16)(data[i + 1] * mp3dec_info->fadein_factor /
                                    mp3dec_info->dec_param->mp3d_sampling_freq);
            mp3dec_info->fadein_factor += FADEIN_STEP;
        }
    }
}


/*==================================================================================================

FUNCTION:   mfw_gst_mp3dec_chain

DESCRIPTION:    this function called to get data from sink pad ,it also
				initialize the mp3decoder first time and starts the decoding
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
static GstFlowReturn mfw_gst_mp3dec_chain(GstPad * pad, GstBuffer * buffer)
{
    const guint8 *data = NULL;
    gint loopctr = 0;
    MP3D_Mem_Alloc_Info_Sub *mem = NULL;
    MP3D_RET_TYPE retval = MP3D_OK;
    MP3D_INT32 size = 0;
    guint offset = 0;
    GstAdapter *adapter = NULL;
    guint size_twoframes = 0;
    guint8 *indata = NULL;
    mp3_fr_info fr_size;
    struct timeval tv_prof2, tv_prof3;
    long time_before = 0, time_after = 0;
    MfwGstMp3DecInfo *mp3dec_info;
    GstFlowReturn result = GST_FLOW_OK;
    GstClockTime ts;



    mp3dec_info = MFW_GST_MP3DEC(GST_PAD_PARENT(pad));
/*

    if (mp3dec_global_info == NULL)
	mp3dec_global_info = mp3dec_info;
    else {
	if (mp3dec_global_info != mp3dec_info) {
	    GstFlowReturn res;

	    res = gst_pad_push(mp3dec_info->srcpad, buffer);
	    if (res != GST_FLOW_OK) {
		GST_ERROR("could not push onto next element %d", res);
	    }
	    return GST_FLOW_OK;
	}
    }
*/
    if (mp3dec_info->profile) {
	gettimeofday(&tv_prof2, 0);
    }



    mp3dec_info = MFW_GST_MP3DEC(GST_OBJECT_PARENT(pad));
    adapter = (GstAdapter *) mp3dec_info->adapter;

    size = GST_BUFFER_SIZE(buffer);
    ts = GST_BUFFER_TIMESTAMP(buffer);
    decode_mp3_add_buf_to_list(mp3dec_info, buffer);

    gst_adapter_push(adapter, buffer);



    /* do the ID3 tag parssing */
    if (!mp3dec_info->id3tag) {
	size = gst_adapter_available(adapter);
	data = gst_adapter_peek(adapter, size);
	if (!mp3dec_info->id3v2_size) {
	    mp3dec_info->id3v2_size =
		mp3_parser_get_id3_v2_size((gchar *) data);
	    GST_DEBUG("Size of ID3 Data = %d", mp3dec_info->id3v2_size);
	}
	if (size > mp3dec_info->id3v2_size) {
        decode_mp3_flush_buf_from_list(mp3dec_info, mp3dec_info->id3v2_size);
	    gst_adapter_flush(adapter, mp3dec_info->id3v2_size);
	} else {
	    return GST_FLOW_OK;
	}
	mp3dec_info->id3tag = 1;
    }



    /* initialise the decoder */
    if (!mp3dec_info->init_flag) {
    	size = gst_adapter_available(adapter);
    	data = gst_adapter_peek(adapter, size);

      if (!mp3dec_info->strm_info_get) {
          fr_size = mp3_parser_parse_frame_header((gchar *) data, size, NULL);
#if 0
          if (fr_size.index < size) {
              gst_adapter_flush(adapter, fr_size.index);
          } else {
              gst_adapter_flush(adapter, size);
              return GST_FLOW_OK;
          }
#endif
      	if(fr_size.flags == FLAG_NEEDMORE_DATA){
      		return GST_FLOW_OK;
      	}
              memcpy(&mp3dec_info->strm_info, &fr_size, sizeof(mp3_fr_info));
              mp3dec_info->strm_info_get = TRUE;
      	if ((fr_size.xing_exist) || (fr_size.vbri_exist))
      	{
      	    gfloat duration = 0;

                  mp3dec_info->total_frames = fr_size.total_frame_num;
                  duration = fr_size.total_frame_num * (gfloat)fr_size.sample_per_fr/fr_size.sampling_rate;
                  mp3dec_info->total_bitrate = (guint64)(mp3dec_info->total_frames * (guint64)fr_size.total_bytes*8/duration);
                  mp3dec_info->avg_bitrate = (gfloat) mp3dec_info->total_bitrate /mp3dec_info->total_frames;

                  /*control the input adapter buffer size:MPEG1_32_VBR258.mp3*/
                  mp3dec_info->full_parse = TRUE;
      	}
      }
      else {
          fr_size = mp3_parser_parse_frame_header((gchar *) data, size, &mp3dec_info->strm_info);
          if (fr_size.index < size) {
              decode_mp3_flush_buf_from_list(mp3dec_info, fr_size.index);
              gst_adapter_flush(adapter, fr_size.index);
          }
#if 0
          else {
              gst_adapter_flush(adapter, size);
              return GST_FLOW_OK;
          }
#endif
          if(fr_size.flags == FLAG_NEEDMORE_DATA){
              return GST_FLOW_OK;
          }
      }

    	if ((retval =
    	     mp3d_decode_init(mp3dec_info->dec_config, (MP3D_UINT8 *) NULL,
    			      0)) != MP3D_OK) {
    	    GST_ERROR("Could not Initialize the MP3Decoder");
    	    return GST_FLOW_ERROR;
    	}
    	mp3dec_info->init_flag = TRUE;
    	return GST_FLOW_OK;
    }


    if (mp3dec_info->new_segment) {
	    if (GST_CLOCK_TIME_IS_VALID(ts)) {
	        mp3dec_info->time_offset = ts;
            mp3dec_info->new_segment = 0;
	    }
	}

    /*control the input adapter buffer size:MPEG1_32_VBR258.mp3*/
#ifdef PUSH_MODE
//    if (gst_adapter_available(adapter) < MP3D_INPUT_BUF_PUSH_SIZE)
    if (gst_adapter_available(adapter) < MP3D_INPUT_BUF_SIZE)  /*ENGR141650 for low bit rate case the thershhold shall not too large*/
#else
    if (gst_adapter_available(adapter) < MP3D_INPUT_BUF_SIZE)
#endif
	return GST_FLOW_OK;
    do {
        //if(mp3dec_global_info==NULL){
        //    break;
        //}
#ifdef PUSH_MODE
        result = decode_mp3_chunk(mp3dec_info, MP3D_INPUT_BUF_SIZE);
#else
        result = decode_mp3_chunk(mp3dec_info);
#endif

#ifdef PUSH_MODE
    } while (gst_adapter_available(adapter) >= MP3D_INPUT_BUF_SIZE);
#else
    } while (gst_adapter_available(adapter) >= MP3D_INPUT_BUF_SIZE);
#endif

    /*for error handler*/
    if (result != GST_FLOW_OK) {
	    GST_ERROR("Error in decoding");
	    if(err_count >= MP3DEC_ERR_MAXCNT)
        {
            /* post message  */
            GstMessage *message = NULL;
            GError *gerror = NULL;
            gerror = g_error_new_literal(1, 0,"Too many wrong data to codec!");
            message =
                gst_message_new_error(GST_OBJECT(GST_ELEMENT(mp3dec_info)),
	                              gerror, "debug none");
	    gst_element_post_message(GST_ELEMENT(mp3dec_info), message);
	    g_error_free(gerror);
	    return GST_FLOW_ERROR;
        }
        return result;
    }
    if (mp3dec_info->profile) {

	gettimeofday(&tv_prof3, 0);
	time_before = (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
	time_after = (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
	mp3dec_info->chain_Time += time_after - time_before;
    }

    return GST_FLOW_OK;
}

#define IS_MP3_DEC_ERROR(ret)\
    (((ret)!=MP3D_OK) && ((ret)!=MP3D_ERROR_SCALEFACTOR) && ((ret)!=MP3D_END_OF_STREAM))

/*==================================================================================================

FUNCTION: decode_mp3_chunk

DESCRIPTION:
   This function calls the main mp3decoder function ,sends the decoded data to osssink

ARGUMENTS PASSED:
   pointer to the mp3decoderInfo structure.

RETURN VALUE:
   None

PRE-CONDITIONS:
   None

POST-CONDITIONS:
   None

IMPORTANT NOTES:
   None

==================================================================================================*/
#ifndef PUSH_MODE
static GstFlowReturn decode_mp3_chunk(MfwGstMp3DecInfo * mp3dec_info)
#else
static GstFlowReturn decode_mp3_chunk(MfwGstMp3DecInfo * mp3dec_info, guint bufsize)
#endif
{
    gint num_samples = 0;
    MP3D_RET_TYPE retval = MP3D_OK;
    GstBuffer *outbuffer = NULL;
    gint16 *outdata = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    gint loopctr = 0;
    GstCaps *src_caps = NULL;
    GstCaps *caps = NULL;
    MP3D_INT32 *decode_out = NULL;
    guint64 time_duration = 0;
    gint total_samples = 0;
    struct timeval tv_prof, tv_prof1;
    long time_before = 0, time_after = 0;
    guint loop_cnt=0;
    GstClockTime ts, diff;

{
    if (mp3dec_info->demo_mode == 2)
    {
        return GST_FLOW_ERROR;
    }

    DEMO_LIVE_CHECK(mp3dec_info->demo_mode,
        mp3dec_info->time_offset,mp3dec_info->srcpad);

}


    src_caps = GST_PAD_CAPS(mp3dec_info->srcpad);
#if 0
    result = gst_pad_alloc_buffer(mp3dec_info->srcpad, 0,
            sizeof(MP3D_INT16) * 2 * MP3D_FRAME_SIZE,
            NULL, &outbuffer);

    if (result != GST_FLOW_OK) {
        GST_ERROR("Error while allocating output buffer");
        return result;
    }
#else
    outbuffer = gst_buffer_new_and_alloc(sizeof(MP3D_INT16) * 2 * MP3D_FRAME_SIZE);
    gst_buffer_set_caps(outbuffer, src_caps);
#endif
    decode_out = GST_BUFFER_DATA(outbuffer);

    if (mp3dec_info->profile) {

	    gettimeofday(&tv_prof, 0);
    }

#ifdef PUSH_MODE
    if (bufsize>0){
        mp3dec_info->dec_config->pInBuf =
            //gst_adapter_peek(mp3dec_global_info->adapter, bufsize);
            gst_adapter_peek(mp3dec_info->adapter, bufsize);
    }
    mp3dec_info->dec_config->inBufLen = bufsize;
    mp3dec_info->dec_config->consumedBufLen = 0;
#endif

    retval =
	mp3d_decode_frame(mp3dec_info->dec_config, mp3dec_info->dec_param,
			  decode_out);
    //g_print (RED_STR("mp3d_decode_frame() returns %d, error count %d\n", retval, err_count));

    ts = decode_mp3_get_timestamp(mp3dec_info);
#if 0
    if ((err_count > 0) && !IS_MP3_DEC_ERROR(retval)) {
        /* if this is the first correct output after error, reset the time offset */
	    if (GST_CLOCK_TIME_IS_VALID(ts)) {
            GST_ERROR ("Encountered error, "
                "reset time offset from %"GST_TIME_FORMAT" to %"GST_TIME_FORMAT"\n",
                GST_TIME_ARGS(mp3dec_info->time_offset), GST_TIME_ARGS(ts));
	        mp3dec_info->time_offset = ts;
	    }
    }
#endif

#ifdef PUSH_MODE
    if (mp3dec_info->dec_config->consumedBufLen>0){
		// below code should no use, core decoder can make sure that consumed <= bufsize
        //if (mp3dec_info->dec_config->consumedBufLen>bufsize){//bug for mp3d
             //mp3dec_info->dec_config->consumedBufLen = bufsize;
        //}
        //gst_adapter_flush(mp3dec_global_info->adapter, mp3dec_info->dec_config->consumedBufLen);
        decode_mp3_flush_buf_from_list(mp3dec_info, mp3dec_info->dec_config->consumedBufLen);
        gst_adapter_flush(mp3dec_info->adapter, mp3dec_info->dec_config->consumedBufLen);
    }


#endif
    if (IS_MP3_DEC_ERROR(retval)){
        err_count++;
	}else{
		err_count = 0;
	}
    if (mp3dec_info->profile) {

    	gettimeofday(&tv_prof1, 0);
    	time_before = (tv_prof.tv_sec * 1000000) + tv_prof.tv_usec;
    	time_after = (tv_prof1.tv_sec * 1000000) + tv_prof1.tv_usec;
    	mp3dec_info->Time += time_after - time_before;

    	if ((retval == MP3D_OK) || (retval == MP3D_ERROR_SCALEFACTOR)) {
    	    mp3dec_info->frame_no++;
    	} else if (retval != MP3D_END_OF_STREAM) {
    	    mp3dec_info->no_of_frames_dropped++;
    	}
    }

    if (!mp3dec_info->full_parse) {
            if ((retval == MP3D_OK) || (retval == MP3D_ERROR_SCALEFACTOR))
             {
            	mp3dec_info->total_bitrate +=
            	    (mp3dec_info->dec_param->mp3d_bit_rate * 1000);
            	mp3dec_info->total_frames++;
            	mp3dec_info->avg_bitrate = (gfloat) mp3dec_info->total_bitrate /
            	    mp3dec_info->total_frames;
            }
    }


    if  ((retval == MP3D_OK) || (retval == MP3D_ERROR_SCALEFACTOR)) {
        mp3dec_info->mpeg_layer = mp3dec_info->dec_param->layer;
        mp3dec_info->mpeg_version = mp3dec_info->dec_param->version;

	if (!mp3dec_info->caps_set) {

        GstTagList	*list = gst_tag_list_new();
        gchar  *codec_name;

        switch (mp3dec_info->mpeg_version) {
        case MPEG_VERSION1:
            codec_name = g_strdup_printf("MPEG-1 Layer %d",mp3dec_info->strm_info.layer);
            break;
        case MPEG_VERSION2:
            codec_name = g_strdup_printf("MPEG-2 Layer %d",mp3dec_info->strm_info.layer);
            break;
        case MPEG_VERSION25:
            codec_name = g_strdup_printf("MPEG-2.5 Layer %d",mp3dec_info->strm_info.layer);
            break;
        }

        gst_tag_list_add(list,GST_TAG_MERGE_APPEND,GST_TAG_AUDIO_CODEC,
                codec_name,NULL);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
                (guint)(mp3dec_info->avg_bitrate),NULL);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_MP3_SAMPLING_RATE,
                (guint)mp3dec_info->dec_param->mp3d_sampling_freq,NULL);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_MP3_CHANNELS,
                (guint)mp3dec_info->dec_param->mp3d_num_channels,NULL);

        g_free(codec_name);

        gst_element_found_tags(GST_ELEMENT(mp3dec_info),list);
	    mp3dec_info->caps_set = TRUE;
	    caps = gst_caps_new_simple("audio/x-raw-int",
				       "endianness", G_TYPE_INT, G_BYTE_ORDER,
				       "signed", G_TYPE_BOOLEAN, TRUE,
				       "width",  G_TYPE_INT, 16,
				       "depth", G_TYPE_INT, 16,
				       "rate", G_TYPE_INT, mp3dec_info->dec_param->mp3d_sampling_freq,
				       "channels", G_TYPE_INT, mp3dec_info->dec_param->mp3d_num_channels,
				       NULL);
	    gst_pad_set_caps(mp3dec_info->srcpad, caps);
        gst_buffer_set_caps(outbuffer, caps);
	    gst_caps_unref(caps);


	}

    loop_cnt = MP3D_FRAME_SIZE;

	if (mp3dec_info->dec_param->mp3d_num_channels == 1) {
	    GST_BUFFER_SIZE(outbuffer) = MP3D_FRAME_SIZE * 2;
	    num_samples = MP3D_FRAME_SIZE;
        decode_mp3_fadein_mono(mp3dec_info, outbuffer);

	}
	if (mp3dec_info->dec_param->mp3d_num_channels == 2) {
	    GST_BUFFER_SIZE(outbuffer) = MP3D_FRAME_SIZE * 2 * 2;
	    num_samples = MP3D_FRAME_SIZE*2;
        decode_mp3_fadein_steoro(mp3dec_info, outbuffer);

	}
	mp3dec_info->total_samples += num_samples;


    /* ENGR47756:AV not sync after about 30 minutes */
    time_duration =
        gst_util_uint64_scale_int(loop_cnt, GST_SECOND, mp3dec_info->dec_param->mp3d_sampling_freq);

    /* A/V sync for fuzzy stream:
     * Check the difference between calculated ts and accumulated ts. If it is
     * bigger than the threshold, reset the timestamp to the calculated one. */
    diff = (mp3dec_info->time_offset > ts) ?
           (mp3dec_info->time_offset - ts) : (ts - mp3dec_info->time_offset);
    if ((GST_CLOCK_TIME_IS_VALID(ts)) &&
        (diff > (time_duration * TS_RESET_THRESHOLD))) {
        GST_WARNING ("difference between calculated ts and accumulated ts is bigger than %d frames, "
            "reset time offset from %"GST_TIME_FORMAT" to %"GST_TIME_FORMAT"",
            TS_RESET_THRESHOLD,
            GST_TIME_ARGS(mp3dec_info->time_offset), GST_TIME_ARGS(ts));
        mp3dec_info->time_offset = ts;
        mp3dec_info->fadein_factor = 0;
    }

	GST_BUFFER_TIMESTAMP(outbuffer) = mp3dec_info->time_offset;
	GST_BUFFER_DURATION(outbuffer) = time_duration;
	GST_BUFFER_OFFSET(outbuffer) = 0;

    mp3dec_info->time_offset += time_duration;

    result = gst_pad_push(mp3dec_info->srcpad, outbuffer);
	if (result != GST_FLOW_OK) {
	    GST_ERROR
		("Error during pushing the buffer to osssink, error is %d",
		 result);
	    return result;
	}

	return GST_FLOW_OK;


    }
	else if(retval != MP3D_END_OF_STREAM)
	{
            /* if error occurs, but not end for file, we will try to decode more data,not to stop. */
            gst_buffer_unref(outbuffer);
            return GST_FLOW_OK;

    }
    /* end of stream case */
	else if (mp3dec_info->eos_event != TRUE)
	{
			GstEvent *event;
			GST_WARNING("Decoder return a error EOS value,send EOS event.");
            gst_buffer_unref(outbuffer);
			event = gst_event_new_eos();
			gst_pad_push_event ((mp3dec_info->srcpad), event);
			/* Stop the mp3 decoding, when it believe the vector has reached eos */
			mp3dec_info->demo_mode = 2;
			return GST_FLOW_ERROR;

	}
	else{
        gst_buffer_unref(outbuffer);
		return GST_FLOW_ERROR;
	}
}

#ifndef PUSH_MODE
/*==================================================================================================

FUNCTION: app_swap_buffers_mp3_dec

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
MP3D_INT8 app_swap_buffers_mp3_dec(MP3D_UINT8 ** new_buf_ptr,
				   MP3D_INT32 * new_buf_len,
				   MP3D_Decode_Config * dec_config)
{/*
    const guint8 *indata;
    int loopctr;
    GstAdapter *adapter;
    guint bytesavailable = 0;
    adapter = (GstAdapter *) mp3dec_global_info->adapter;

    bytesavailable = gst_adapter_available(adapter);

    if (bytesavailable == 0) {
        *new_buf_ptr = NULL;
        *new_buf_len = 0;
        return -1;
    }

    if (bytesavailable > MP3D_INPUT_BUF_SIZE)
       bytesavailable = MP3D_INPUT_BUF_SIZE;

    indata = gst_adapter_peek(adapter, bytesavailable);

    memcpy(mp3dec_global_info->send_buff, indata, bytesavailable);
    *new_buf_ptr = mp3dec_global_info->send_buff;
    *new_buf_len = bytesavailable;

    gst_adapter_flush(adapter, bytesavailable);*/
    return 0;
}
#endif

FSL_GST_PLUGIN_DEFINE("mp3dec", "mp3 audio decoder", plugin_init);

/*==================================================================================================*/

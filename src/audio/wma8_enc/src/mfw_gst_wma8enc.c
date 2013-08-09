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
 * Module Name:    mfw_gst_wma8enc.c
 *
 * Description:    Implementation of wma encoder plugin for Gstreamer
 *                 using push based mode.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Apr, 1 2008 Li Jian<b06256@freescale.com>
 * - Initial version
 *
 */



/*=========================================================================================
                            INCLUDE FILES
========================================================================================*/

#include <string.h>
#include "tchar.h"
#include "mfw_gst_wma8enc.h"
#include "mfw_gst_utils.h"
/*==================================================================================================
                                        LOCAL CONSTANTS
==================================================================================================*/

/* None. */

/*==================================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/

enum
{
  ARG_0,
  ARG_SAMPLERATE,
  ARG_BITRATE,
  ARG_CHANNEL,
  ARG_TITLE,
  ARG_AUTHOR,
  ARG_DESC,
  ARG_CR,
  ARG_RAT
};

static GstStaticPadTemplate mfw_gst_wma8enc_sink_template =
GST_STATIC_PAD_TEMPLATE ("wma8enc_sink",
			 GST_PAD_SINK,
			 GST_PAD_ALWAYS,
             GST_STATIC_CAPS (
                 "audio/x-raw-int, "
                 "rate = (int) {22050, 32000, 44100, 48000}, "
                 "channels = (int) [ 1, 2 ], "
                 "endianness = (int) LITTLE_ENDIAN, "
                 "width = (int) 16, "
                 "depth = (int) 16, "
                 "signed = (boolean) true"));

static GstStaticPadTemplate mfw_gst_wma8enc_src_template =
GST_STATIC_PAD_TEMPLATE ("wma8enc_src",
            GST_PAD_SRC,
            GST_PAD_ALWAYS,
            GST_STATIC_CAPS (
                "audio/x-wma"));

typedef guint8 U8;

/*==================================================================================================
                                        LOCAL MACROS
==================================================================================================*/

#define DEFAULT_SAMPLERATE	44100
#define DEFAULT_BITRATE		64000
#define DEFAULT_CHANNELS	2
#define DEFAULT_TITLE       NULL
#define DEFAULT_AUTHOR      NULL
#define DEFAULT_DESC        NULL
#define DEFAULT_CR          NULL
#define DEFAULT_RAT         NULL

#define ALIGN			   	4
#define MAX_METADATA_CHARS 255 // Max length of Metadata in chars

#define GST_CAT_DEFAULT mfw_gst_wma8enc_debug
GST_DEBUG_CATEGORY_STATIC (mfw_gst_wma8enc_debug);
#define DEBUG_INIT(bla)  \
    GST_DEBUG_CATEGORY_INIT (mfw_gst_wma8enc_debug, "mfw_wma8encoder", 0, \
                             "FreeScale's Wma8 Encoder's Log");

GST_BOILERPLATE_FULL (MfwGstWma8EncInfo, mfw_gst_wma8enc, GstElement,
		              GST_TYPE_ELEMENT, DEBUG_INIT);


/*==================================================================================================
                                      STATIC VARIABLES
==================================================================================================*/

/*==================================================================================================
                                 STATIC FUNCTION PROTOTYPES
==================================================================================================*/




static void mfw_gst_wma8enc_set_property (GObject * object, guint prop_id,
					 const GValue * value,
					 GParamSpec * pspec);
static void mfw_gst_wma8enc_get_property (GObject * object, guint prop_id,
					 GValue * value, GParamSpec * pspec);

static gboolean mfw_gst_wma8enc_set_caps (GstPad * pad, GstCaps * caps);
static gboolean mfw_gst_wma8enc_sink_event (GstPad * pad, GstEvent * event);
static GstStateChangeReturn mfw_gst_wma8enc_change_state (GstElement * element,
							 GstStateChange
							 transition);
static GstFlowReturn mfw_gst_wma8enc_encode_frame (MfwGstWma8EncInfo * wma8enc,
						  gboolean eos);
static GstFlowReturn mfw_gst_wma8enc_chain (GstPad * pad, GstBuffer * buf);

static void* alloc_align (int size);
static void free_Aligned(void *ptr);
static void CopyCharToWChar(word *dst, TCHAR *src, int length);
static void SetAsfParams(ASFParams *pAsfParams, WMAEEncoderParams *psWMAEncParams);
static gboolean mfw_gst_wma8enc_init_resource(MfwGstWma8EncInfo *wma8enc);
static gboolean mfw_gst_wma8enc_deinit_resource(MfwGstWma8EncInfo *wma8enc);
static GstFlowReturn mfw_gst_wma8enc_push_asf_header(MfwGstWma8EncInfo * wma8enc);
static GstFlowReturn mfw_gst_wma8enc_set_asf_header(MfwGstWma8EncInfo * wma8enc);
static GstFlowReturn mfw_gst_wma8enc_update_asf_header(MfwGstWma8EncInfo * wma8enc);

/*==================================================================================================
                                     GLOBAL VARIABLES
==================================================================================================*/


/*==================================================================================================
                                     LOCAL FUNCTIONS
==================================================================================================*/

/*==================================================================================================

FUNCTION:       plugin_init

DESCRIPTION:
special function , which is called as soon as the plugin or
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
static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "mfw_wma8encoder",
			       GST_RANK_PRIMARY, MFW_GST_TYPE_WMA8ENC);
}


/*==================================================================================================
 
  FUNCTION: mfw_gst_wma8enc_base_init          

  DESCRIPTION: 
  Element details are registered with the plugin during _base_init ,
  This function will initialise the class and child
  class properties during each new child class creation

  ARGUMENTS PASSED:
  gclass  -  pointer to element class

  RETURN VALUE:
  None

  PRE-CONDITIONS:
  None

  POST-CONDITIONS:
  None

  IMPORTANT NOTES:
  None

  ==================================================================================================*/
static void
mfw_gst_wma8enc_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
				      gst_static_pad_template_get
				      (&mfw_gst_wma8enc_sink_template));
  gst_element_class_add_pad_template (element_class,
				      gst_static_pad_template_get
				      (&mfw_gst_wma8enc_src_template));
  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "wma8 audio encoder",
      "Codec/Encode/Audio", "Encode audio raw data to compressed wma8 audio");
}



/* initialize the plugin's class */
/*==================================================================================================
 
FUNCTION: mfw_gst_wma8enc_class_init          

DESCRIPTION: 
Initialise the plugin's class.

ARGUMENTS PASSED:
  gclass  -  pointer to element class

RETURN VALUE:
None

PRE-CONDITIONS:
None

POST-CONDITIONS:
None

IMPORTANT NOTES:
None

==================================================================================================*/
static void
mfw_gst_wma8enc_class_init (MfwGstWma8EncInfoClass * gclass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) gclass;
  gstelement_class = (GstElementClass *) gclass;

  gobject_class->set_property = mfw_gst_wma8enc_set_property;
  gobject_class->get_property = mfw_gst_wma8enc_get_property;
  gstelement_class->change_state = mfw_gst_wma8enc_change_state;

  g_object_class_install_property (gobject_class, ARG_SAMPLERATE,
				   g_param_spec_int ("sample_rate",
						     "Sample_rate",
						     "Input sample rate(default 44100)",
						     22050, 48000,
						     DEFAULT_SAMPLERATE,
						     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_BITRATE,
				   g_param_spec_int ("bitrate", "Bitrate",
						     "Encode bitrate in bps",
						     16000, 220000, DEFAULT_BITRATE,
						     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_CHANNEL,
				   g_param_spec_int ("channels", "Channels",
						     "Input channels(default 2)",
						     1, 2, DEFAULT_CHANNELS,
						     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_TITLE,
				   g_param_spec_string ("title", "Title",
						     "Content title",
						     DEFAULT_TITLE,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, ARG_AUTHOR,
				   g_param_spec_string ("author", "Author",
						     "Content author",
						     DEFAULT_AUTHOR,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, ARG_DESC,
				   g_param_spec_string ("description", "Description",
						     "Content description",
						     DEFAULT_DESC,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, ARG_CR,
				   g_param_spec_string ("copyright", "Copyright",
						     "Content copyright",
						     DEFAULT_CR,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, ARG_RAT,
				   g_param_spec_string ("rating", "Rating",
						     "Content rating",
						     DEFAULT_RAT,
						     G_PARAM_WRITABLE));
}


/*==================================================================================================

FUNCTION: mfw_gst_wma8enc_init          

DESCRIPTION: 
create the pad template that has been registered with the
element class in the _base_init

ARGUMENTS PASSED:
wma8enc -  pointer to element structure
gclass  -  pointer to element class

RETURN VALUE:
None

PRE-CONDITIONS:
None

POST-CONDITIONS:
None

IMPORTANT NOTES:
None

==================================================================================================*/
static void
mfw_gst_wma8enc_init (MfwGstWma8EncInfo * wma8enc,
                      MfwGstWma8EncInfoClass * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (wma8enc);

  wma8enc->sinkpad =
    gst_pad_new_from_template (
            gst_element_class_get_pad_template (klass, "wma8enc_sink") ,
			"sink");

  wma8enc->srcpad =
    gst_pad_new_from_template (
            gst_element_class_get_pad_template (klass, "wma8enc_src") ,
			"src");

  gst_element_add_pad (GST_ELEMENT (wma8enc), wma8enc->sinkpad);
  gst_element_add_pad (GST_ELEMENT (wma8enc), wma8enc->srcpad);
  gst_pad_set_chain_function (wma8enc->sinkpad, mfw_gst_wma8enc_chain);
  gst_pad_set_event_function (wma8enc->sinkpad, mfw_gst_wma8enc_sink_event);
  gst_pad_set_setcaps_function(wma8enc->sinkpad, mfw_gst_wma8enc_set_caps);

  wma8enc->sampling_rate = DEFAULT_SAMPLERATE;
  wma8enc->bit_rate = DEFAULT_BITRATE;
  wma8enc->channels = DEFAULT_CHANNELS;
  wma8enc->title = NULL;
  wma8enc->author = NULL;
  wma8enc->desc = NULL;
  wma8enc->cr = NULL;
  wma8enc->rating = NULL;

#define MFW_GST_WMA8_ENCODER_PLUGIN VERSION    
    PRINT_CORE_VERSION(WMA8ECodecVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_WMA8_ENCODER_PLUGIN);
    INIT_DEMO_MODE(WMA8ECodecVersionInfo(), wma8enc->demo_mode);

}


static void
mfw_gst_wma8enc_set_property (GObject * object, guint prop_id,
			     const GValue * value, GParamSpec * pspec)
{
  MfwGstWma8EncInfo *wma8enc = MFW_GST_WMA8ENC (object);

  switch (prop_id)
  {
      case ARG_SAMPLERATE:
          wma8enc->sampling_rate = g_value_get_int (value);
          break;
      case ARG_BITRATE:
          wma8enc->bit_rate = g_value_get_int (value);
          break;
      case ARG_CHANNEL:
          wma8enc->channels = g_value_get_int (value);
          break;
      case ARG_TITLE: 
          {
              const gchar *s = g_value_get_string (value);
              if (wma8enc->title)
                  g_free(wma8enc->title);
              wma8enc->title = g_strdup(s);
          }
          break;
      case ARG_AUTHOR:
          {
              const gchar *s = g_value_get_string (value);
              if (wma8enc->author)
                  g_free(wma8enc->author);
              wma8enc->author = g_strdup(s);
          }
          break;
      case ARG_DESC:
          {
              const gchar *s = g_value_get_string (value);
              if (wma8enc->desc)
                  g_free(wma8enc->desc);
              wma8enc->desc = g_strdup(s);
          }
          break;
      case ARG_CR:
          {
              const gchar *s = g_value_get_string (value);
              if (wma8enc->cr)
                  g_free(wma8enc->cr);
              wma8enc->cr = g_strdup(s);
          }
          break;
      case ARG_RAT:
          {
              const gchar *s = g_value_get_string (value);
              if (wma8enc->rating)
                  g_free(wma8enc->rating);
              wma8enc->rating = g_strdup(s);
          }
          break;
      default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
  }
}

static void
mfw_gst_wma8enc_get_property (GObject * object, guint prop_id,
			     GValue * value, GParamSpec * pspec)
{
  MfwGstWma8EncInfo *wma8enc = MFW_GST_WMA8ENC (object);

  switch (prop_id)
    {
    case ARG_SAMPLERATE:
      g_value_set_int (value, wma8enc->sampling_rate);
      break;
    case ARG_BITRATE:
      g_value_set_int (value, wma8enc->bit_rate);
      break;
    case ARG_CHANNEL:
      g_value_set_int (value, wma8enc->channels);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static gboolean
mfw_gst_wma8enc_sink_event (GstPad * pad, GstEvent * event)
{
    MfwGstWma8EncInfo *wma8enc;
    gboolean ret = TRUE;
    GstBuffer *outbuf = NULL;
    guint8 *outdata = NULL;
    wma8enc = MFW_GST_WMA8ENC (GST_OBJECT_PARENT (pad));

    switch (GST_EVENT_TYPE (event))
    {
        case GST_EVENT_EOS:
            {
                GstFlowReturn result = GST_FLOW_OK;
                GstAdapter *adapter = wma8enc->adapter;

                while (gst_adapter_available(adapter) >= wma8enc->frameSize) {
                    wma8enc->inbuf = (guint8*)gst_adapter_peek(adapter, wma8enc->frameSize);
                    wma8enc->total_input += wma8enc->frameSize;
                    result = mfw_gst_wma8enc_encode_frame (wma8enc, FALSE);
                    if (result != GST_FLOW_OK) 
                        return result;
                    gst_adapter_flush(adapter, wma8enc->frameSize);
                }

                /* send the last frame */
                wma8enc->inbuf = g_malloc(wma8enc->frameSize);
                if (gst_adapter_available(adapter) > 0) {
                    gint size = gst_adapter_available(adapter);
                    wma8enc->total_input += size;
                    memset (wma8enc->inbuf, 0, wma8enc->frameSize);
                    memcpy (wma8enc->inbuf, gst_adapter_peek(adapter, size), size);
                    result = mfw_gst_wma8enc_encode_frame (wma8enc, FALSE);
                    if (result != GST_FLOW_OK) 
                        return result;

                }

                /* encode until all the input in core encoder working buffer been encoded */
                memset (wma8enc->inbuf, 0, wma8enc->frameSize);
                result = mfw_gst_wma8enc_encode_frame (wma8enc, TRUE);
                if (result != GST_FLOW_OK) 
                    return result;
                g_free (wma8enc->inbuf);

                /* update asf file header */
                /* seek to beginning of file */
                gst_pad_push_event (wma8enc->srcpad,
                        gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_BYTES,
                            0, GST_CLOCK_TIME_NONE, 0));

                result = mfw_gst_wma8enc_update_asf_header(wma8enc);
                if (result != GST_FLOW_OK) 
                    return result;

                gst_pad_push_event (wma8enc->srcpad, event);
                GST_DEBUG ("Wma8enc EOS event pushed!\n");
                break;
            }
        default:
            {
                ret = gst_pad_event_default (pad, event);
                break;
            }
    }

    return ret;
}

static gboolean
mfw_gst_wma8enc_set_caps (GstPad * pad, GstCaps * caps)
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    MfwGstWma8EncInfo *wma8enc = MFW_GST_WMA8ENC (GST_OBJECT_PARENT (pad));
    const gchar *mime = NULL;

    mime = gst_structure_get_name(structure);
    /* check for MIME type */
    if (strcmp(mime, "audio/x-raw-int") != 0) {
        GST_ERROR("Wrong  mimetype %s provided, supported is %s", mime, "audio/x-raw-int");
        return FALSE;
    }

    gst_structure_get_int(structure, "channels", &wma8enc->channels);
    gst_structure_get_int(structure, "rate", &wma8enc->sampling_rate);

    GST_DEBUG("channels: %d\n", wma8enc->channels);
    GST_DEBUG("rate: %d\n", wma8enc->sampling_rate);

    return TRUE;
}

/*==================================================================================================

FUNCTION: mfw_gst_wma8enc_change_state

DESCRIPTION: this function keeps track of different states of pipeline.

ARGUMENTS PASSED:
element     -   pointer to element
transition  -   state of the pipeline

RETURN VALUE:
GST_STATE_CHANGE_FAILURE    - the state change failed
GST_STATE_CHANGE_SUCCESS    - the state change succeeded
GST_STATE_CHANGE_ASYNC      - the state change will happen asynchronously
GST_STATE_CHANGE_NO_PREROLL - the state change cannot be prerolled

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
static GstStateChangeReturn
mfw_gst_wma8enc_change_state (GstElement * element, GstStateChange transition)
{
    GstStateChangeReturn ret;
    MfwGstWma8EncInfo *wma8enc = MFW_GST_WMA8ENC (element);

    switch (transition)
    {
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            wma8enc->adapter = NULL;
            wma8enc->psEncConfig = NULL;
            wma8enc->psASFParams = NULL;
            wma8enc->init = FALSE;
            wma8enc->frameSize = 0;
            wma8enc->inbuf = NULL;
            wma8enc->outbuf = NULL;
            wma8enc->total_input = 0;
            wma8enc->byte_rate = 0;
            mfw_gst_wma8enc_init_resource(wma8enc);
            break;
    }

    ret = parent_class->change_state (element, transition);

    switch (transition)
    {
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            wma8enc->adapter = NULL;
            wma8enc->psEncConfig = NULL;
            wma8enc->psASFParams = NULL;
            wma8enc->sampling_rate = DEFAULT_SAMPLERATE;
            wma8enc->bit_rate = DEFAULT_BITRATE;
            wma8enc->channels = DEFAULT_CHANNELS;
            wma8enc->init = FALSE;
            wma8enc->frameSize = 0;
            wma8enc->inbuf = NULL;
            wma8enc->outbuf = NULL;
            wma8enc->total_input = 0;
            wma8enc->byte_rate = 0;

            if (wma8enc->title)
                g_free(wma8enc->title);
            if (wma8enc->author)
                g_free(wma8enc->author);
            if (wma8enc->desc)
                g_free(wma8enc->desc);
            if (wma8enc->cr)
                g_free(wma8enc->cr);
            if (wma8enc->rating)
                g_free(wma8enc->rating);
            mfw_gst_wma8enc_deinit_resource(wma8enc);
            break;
    }

    return ret;
}

#define DEMO_LIVE_CHECK_WO_EOS(demomode,timestamp)              \
do {                                                            \
    if (                                                        \
        ( (demomode) == 1 ) &&                                  \
        ( (timestamp)  > DEMO_LIVE_TIME)                        \
        )                                                       \
    {                                                           \
        GST_WARNING("This is a demo version,                    \
        and the time exceed 2 minutes.                          \
            Sending EOS event.\n");                             \
        (demomode) = 2;                                         \
    }                                                           \
}while(0);
/*==================================================================================================

FUNCTION:   mfw_gst_wma8enc_chain

DESCRIPTION:    this function called to get data from sink pad ,it also
				initialize the encoder first time and starts the encoding
				process.

ARGUMENTS PASSED:
buffer -  pointer to the GstBuffer
pad    -  pointer to pad

RETURN VALUE:
GstFlowReturn

PRE-CONDITIONS:
None

POST-CONDITIONS:
None

IMPORTANT NOTES:
None

==================================================================================================*/
static GstFlowReturn
mfw_gst_wma8enc_chain (GstPad * pad, GstBuffer * buf)
{
    GstFlowReturn result = GST_FLOW_OK;
    GstAdapter *adapter = NULL;
    MfwGstWma8EncInfo *wma8enc;

    wma8enc = MFW_GST_WMA8ENC (GST_OBJECT_PARENT (pad));


  if (wma8enc->demo_mode == 2)
      return GST_FLOW_ERROR;


    if (FALSE == wma8enc->init) {
        tWMAEncodeStatus iStatus;
        ASFRESULTS rcAsf;
        WMAEEncoderConfig *psEncConfig = NULL;
        WMAEEncoderParams *psEncParams = NULL;
        ASFParams *psASFParams = NULL;
        WMAEMemAllocInfoSub *mem;
        gint i, nr;

        psEncConfig = wma8enc->psEncConfig;
        psEncParams = psEncConfig->psEncodeParams;
        psASFParams = wma8enc->psASFParams;

        psEncParams->App_iDstAudioSampleRate = wma8enc->sampling_rate;
        psEncParams->App_iDstAudioBitRate = wma8enc->bit_rate;
        psEncParams->App_iDstAudioChannels = wma8enc->channels;

        GST_DEBUG ("encoder src setting: sample_rate = %d Hz, bitrate = %d kbps, channels = %d\n",
                   psEncParams->App_iDstAudioSampleRate,
                   (psEncParams->App_iDstAudioBitRate + 500)/1000, 
                   psEncParams->App_iDstAudioChannels);


        /* query the element resource memory */
        if((iStatus = eWMAEQueryMem(psEncConfig))!= WMA_Succeeded) {
            GST_ERROR("Query Memory Failed.\n");
            return GST_FLOW_ERROR;
        }

        /* Number of memory chunk requests by the decoder */
        nr = psEncConfig->sWMAEMemInfo.s32NumReqs;
        for(i = 0; i < nr; i++) {
            mem = &(psEncConfig->sWMAEMemInfo.sMemInfoSub[i]);
            if (mem->s32WMAEType == WMAE_FAST_MEMORY) {
                mem->app_base_ptr = alloc_align (mem->s32WMAESize);
                if (mem->app_base_ptr == NULL)
                    return GST_FLOW_ERROR;
            }
            else {
                mem->app_base_ptr = alloc_align (mem->s32WMAESize);
                if (mem->app_base_ptr == NULL)
                    return GST_FLOW_ERROR;
            }
        }

        /* init the encoder */
        iStatus = eInitWMAEncoder(psEncConfig);
        if(iStatus != WMA_Succeeded) {
            GST_ERROR("Init WMA Encoder Failed!\n");
            return GST_FLOW_ERROR;
        }

        psEncParams->App_iDstAudioBitRate    = psEncParams->pFormat.nAvgBytesPerSec * 8;;
        psEncParams->App_iDstAudioSampleRate = psEncParams->pFormat.nSamplesPerSec;
        psEncParams->App_iDstAudioChannels   = psEncParams->pFormat.nChannels;

        wma8enc->frameSize = psEncParams->pFormat.nSamplesPerFrame *
                             psEncParams->pFormat.nChannels *
                             sizeof(short);

        wma8enc->byte_rate = wma8enc->sampling_rate * wma8enc->channels * sizeof(short);
        
        GST_DEBUG ("encoder dest setting: sample_rate = %d Hz, bitrate = %d kbps, channels = %d\n",
                   psEncParams->App_iDstAudioSampleRate,
                   (psEncParams->App_iDstAudioBitRate + 500)/1000, 
                   psEncParams->App_iDstAudioChannels);

        /* allocate wmapacket output buffer */
        wma8enc->outbuf = (guint8 *)alloc_align(sizeof(guint8) * psEncParams->WMAE_packet_byte_length);
        if(wma8enc->outbuf == NULL) {
            GST_ERROR("Allocate output buffer failed.\n");
            return GST_FLOW_ERROR;
        }

        /* push asf file header to next element*/
        /* set title para */
        if (wma8enc->title) {
            psASFParams->g_cTitle = strlen(wma8enc->title) + 1;
            if (psASFParams->g_cTitle > MAX_METADATA_CHARS + 1) {
                GST_ERROR("Content title is too long. Expect < %d\n", MAX_METADATA_CHARS+1);
                return GST_FLOW_ERROR;
            }
            if (psASFParams->g_wszTitle) free_Aligned( psASFParams->g_wszTitle );
            psASFParams->g_wszTitle = (word *)alloc_align( psASFParams->g_cTitle * sizeof(word) );
            if (psASFParams->g_wszTitle)
                CopyCharToWChar(psASFParams->g_wszTitle, wma8enc->title, psASFParams->g_cTitle);
            else {
                GST_ERROR("Could not allocate memory for [Title].\n" );
                return GST_FLOW_ERROR;
            }
        }

        /* set author para */
        if (wma8enc->author) {
            psASFParams->g_cAuthor = strlen(wma8enc->author) + 1;
            if (psASFParams->g_cAuthor > MAX_METADATA_CHARS + 1) {
                GST_ERROR("Content author is too long. Expect < %d\n", MAX_METADATA_CHARS+1);
                return GST_FLOW_ERROR;
            }
            if (psASFParams->g_wszAuthor) free_Aligned( psASFParams->g_wszAuthor );
            psASFParams->g_wszAuthor = (word*)alloc_align( psASFParams->g_cAuthor * sizeof(word) );
            if (psASFParams->g_wszAuthor)
                CopyCharToWChar(psASFParams->g_wszAuthor, wma8enc->author, psASFParams->g_cAuthor);
            else {
                GST_ERROR("Could not allocate memory for [Author].\n" );
                return GST_FLOW_ERROR;
            }
        }

        /* set description para */
        if (wma8enc->desc) {
            psASFParams->g_cDescription = strlen(wma8enc->desc) + 1;
            if (psASFParams->g_cDescription > MAX_METADATA_CHARS + 1) {
                GST_ERROR("Content description is too long. Expect < %d\n", MAX_METADATA_CHARS+1);
                return GST_FLOW_ERROR;
            }
            if (psASFParams->g_wszDescription) free_Aligned ( psASFParams->g_wszDescription );
            psASFParams->g_wszDescription = (word*)alloc_align( psASFParams->g_cDescription * sizeof(word) );
            if (psASFParams->g_wszDescription)
                CopyCharToWChar(psASFParams->g_wszDescription, wma8enc->desc, psASFParams->g_cDescription);
            else {
                GST_ERROR("Could not allocate memory for [Description].\n" );
                return GST_FLOW_ERROR;
            }
        }

        /* set copyright para */
        if (wma8enc->cr) {
            psASFParams->g_cCopyright = strlen(wma8enc->cr) + 1;
            if (psASFParams->g_cCopyright > MAX_METADATA_CHARS + 1) {
                GST_ERROR("Content copyright is too long. Expect < %d\n", MAX_METADATA_CHARS+1);
                return GST_FLOW_ERROR;
            }
            if (psASFParams->g_wszCopyright) free_Aligned ( psASFParams->g_wszCopyright );
            psASFParams->g_wszCopyright = (word*)alloc_align( psASFParams->g_cCopyright * sizeof(word) );
            if (psASFParams->g_wszCopyright)
                CopyCharToWChar(psASFParams->g_wszCopyright, wma8enc->cr, psASFParams->g_cCopyright);
            else {
                GST_ERROR("Could not allocate memory for [Copyright].\n" );
                return GST_FLOW_ERROR;
            }
        }

        /* set rating para */
        if (wma8enc->rating) {
            psASFParams->g_cRating = strlen(wma8enc->rating) + 1;
            if (psASFParams->g_cRating > MAX_METADATA_CHARS + 1) {
                GST_ERROR("Content rating is too long. Expect < %d\n", MAX_METADATA_CHARS+1);
                return GST_FLOW_ERROR;
            }
            if (psASFParams->g_wszRating) free_Aligned ( psASFParams->g_wszRating );
            psASFParams->g_wszRating = (word*)alloc_align( psASFParams->g_cRating * sizeof(word) );
            if (psASFParams->g_wszRating)
                CopyCharToWChar(psASFParams->g_wszRating, wma8enc->rating, psASFParams->g_cRating);
            else {
                GST_ERROR("Could not allocate memory for [Rating].\n" );
                return GST_FLOW_ERROR;
            }
        }

        SetAsfParams(psASFParams, psEncParams);
        result = mfw_gst_wma8enc_set_asf_header(wma8enc);
        if (result != GST_FLOW_OK) 
            return result;
        wma8enc->init = TRUE;
    }

    adapter = wma8enc->adapter;
    gst_adapter_push(adapter, buf);
{
    GstClockTime pts = (wma8enc->total_input / wma8enc->byte_rate);
    DEMO_LIVE_CHECK_WO_EOS(wma8enc->demo_mode, pts);
    if (wma8enc->demo_mode == 2) {
        GstEvent *event;
        gst_pad_push_event (wma8enc->srcpad,
            gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_BYTES,
            0, GST_CLOCK_TIME_NONE, 0));

        GST_DEBUG("Update the asf header in demo version.\n");
        result = mfw_gst_wma8enc_update_asf_header(wma8enc);
        if (result != GST_FLOW_OK) 
            return result;
        GST_DEBUG("Send eos event to pipeline.\n");
        event = gst_event_new_eos();                            
        gst_pad_push_event ((wma8enc->srcpad), event);                  

    }
}

    while (gst_adapter_available(adapter) >= wma8enc->frameSize) {
        wma8enc->inbuf = (guint8*)gst_adapter_peek(adapter, wma8enc->frameSize);
        wma8enc->total_input += wma8enc->frameSize;
        result = mfw_gst_wma8enc_encode_frame (wma8enc, FALSE);
        if (result != GST_FLOW_OK) 
            return result;
        gst_adapter_flush(adapter, wma8enc->frameSize);
    }

	return GST_FLOW_OK;
}


/****************************************************
 *  Following are private functions
 ***************************************************/
static void* alloc_align (int size)
{
    int mask = -1; //Initally set mask to 0xFFFFFFFF
    void *retBuffer;
    void *buffer = (void *)g_malloc(size+ALIGN);  
    mask <<= 2;//LOG2(ALIGN);                     //Generate mask to clear lsb's
    retBuffer = (void*)(((int)((U8*)buffer+ALIGN))&mask);//Generate aligned pointer
    ((U8*)retBuffer)[-1] = (U8)((U8*)retBuffer-(U8*)buffer);//Write offset to newPtr-1
    return retBuffer;
}

static void free_Aligned(void *ptr){
    U8* realBuffer = (U8*)ptr;
    U8 bytesBack;
    if (realBuffer == NULL) return; 
    bytesBack = ((U8*)ptr)[-1];      //Get offset to real pointer from -1 possition
    realBuffer -= bytesBack;    //Get original pointer address
    g_free(realBuffer);
}

static void CopyCharToWChar(word *dst, TCHAR *src, int length)
{
    while (length--) *dst++ = (word) *src++;
}

static void SetAsfParams(ASFParams *pAsfParams, WMAEEncoderParams *psWMAEncParams)
{
  pAsfParams->WMAE_packet_byte_length = psWMAEncParams->WMAE_packet_byte_length;

  pAsfParams->pFormat.nAudioDelaySizeMs = psWMAEncParams->pFormat.nAudioDelaySizeMs;  
  pAsfParams->pFormat.nSamplesPerSec = psWMAEncParams->pFormat.nSamplesPerSec;
  pAsfParams->pFormat.nAvgBytesPerSec = psWMAEncParams->pFormat.nAvgBytesPerSec;
  pAsfParams->pFormat.nChannels = psWMAEncParams->pFormat.nChannels;
  pAsfParams->pFormat.nSamplesPerSec = psWMAEncParams->pFormat.nSamplesPerSec;
  pAsfParams->pFormat.nAvgBytesPerSec = psWMAEncParams->pFormat.nAvgBytesPerSec;
  pAsfParams->pFormat.nSamplesPerFrame = psWMAEncParams->pFormat.nSamplesPerFrame;
  pAsfParams->pFormat.wEncodeOptions = psWMAEncParams->pFormat.wEncodeOptions;
  
}

/* Allocate element requried resources */
static gboolean
mfw_gst_wma8enc_init_resource(MfwGstWma8EncInfo *wma8enc)
{
    tWMAEncodeStatus iStatus;
    WMAEEncoderConfig *psEncConfig;

    wma8enc->psEncConfig = (WMAEEncoderConfig *)alloc_align(sizeof(WMAEEncoderConfig));
    if(wma8enc->psEncConfig == NULL) {
        GST_ERROR("Failed allocate WMAEEncoderConfig.\n");
        goto Cleanup;
    }
    memset(wma8enc->psEncConfig, 0, sizeof(WMAEEncoderConfig));
    psEncConfig = wma8enc->psEncConfig;

    psEncConfig->psEncodeParams = (WMAEEncoderParams*)alloc_align(sizeof(WMAEEncoderParams));;
    if (psEncConfig->psEncodeParams == NULL) {
        GST_ERROR("Failed allocate WMAEEncoderParams.\n");
        goto Cleanup;
    }
    memset(psEncConfig->psEncodeParams, 0, sizeof(WMAEEncoderParams));

    wma8enc->psASFParams = (ASFParams *)alloc_align(sizeof(ASFParams));
    if (wma8enc->psASFParams == NULL) {
        GST_ERROR("Failed allocate ASFParams.\n");
        goto Cleanup;
    }
    memset(wma8enc->psASFParams,0,sizeof(ASFParams));

    /* Allocate a input adapter to element */
    wma8enc->adapter = gst_adapter_new();
    if (wma8enc->adapter == NULL) {
        GST_ERROR ("\n Create input adapter failed.\n");
        goto Cleanup;
    }

    return TRUE;

Cleanup:
    mfw_gst_wma8enc_deinit_resource(wma8enc);
    return FALSE;
}


/* De-allocate element requried resources */
static gboolean
mfw_gst_wma8enc_deinit_resource(MfwGstWma8EncInfo *wma8enc)
{
    WMAEEncoderConfig *psEncConfig = wma8enc->psEncConfig;
    ASFParams *psASFParams = wma8enc->psASFParams;
    WMAEMemAllocInfoSub *mem;
    gint i, nr;

    if (psEncConfig == NULL)
        return FALSE;

    nr = psEncConfig->sWMAEMemInfo.s32NumReqs;
    for (i = 0; i < nr; i++) {
        if(psEncConfig->sWMAEMemInfo.sMemInfoSub[i].app_base_ptr)
            free_Aligned (psEncConfig->sWMAEMemInfo.sMemInfoSub[i].app_base_ptr);
    }

    if (psEncConfig->psEncodeParams)
        free_Aligned (psEncConfig->psEncodeParams);

    if (psEncConfig)
        free_Aligned (psEncConfig);

    if (psASFParams->g_wszTitle) 
        free_Aligned( psASFParams->g_wszTitle );
    if (psASFParams->g_wszAuthor) 
        free_Aligned( psASFParams->g_wszAuthor );
    if (psASFParams->g_wszDescription) 
        free_Aligned ( psASFParams->g_wszDescription );
    if (psASFParams->g_wszCopyright) 
        free_Aligned ( psASFParams->g_wszCopyright );
    if (psASFParams->g_wszRating) 
        free_Aligned ( psASFParams->g_wszRating );
    if (psASFParams)
        free_Aligned (wma8enc->psASFParams);

    if (wma8enc->adapter) {
        gst_adapter_clear(wma8enc->adapter);
        g_object_unref(wma8enc->adapter);
    }

    if (wma8enc->outbuf)
        free_Aligned (wma8enc->outbuf);

    return TRUE;
}


static GstFlowReturn
mfw_gst_wma8enc_encode_frame (MfwGstWma8EncInfo * wma8enc, gboolean eos)
{
    GstFlowReturn ret;
    guint8 *inbuf, *outbuf;
    GstBuffer *asfOutBuf;
    guint8 *asfOutData;
    GstAdapter *adapter = wma8enc->adapter;

    tWMAEncodeStatus iStatus;
    ASFRESULTS rcAsf;
    WMAEEncoderConfig *psEncConfig = NULL;
    WMAEEncoderParams *psEncParams = NULL;
    ASFParams *psASFParams = NULL;

    psEncConfig = wma8enc->psEncConfig;
    psEncParams = psEncConfig->psEncodeParams;
    psASFParams = wma8enc->psASFParams;

    /* get input buffer ptr */
    inbuf = wma8enc->inbuf;
    outbuf = wma8enc->outbuf;

    do {

        if (wma8enc->demo_mode == 2)
          return GST_FLOW_ERROR;

        iStatus = eWMAEncodeFrame(psEncConfig,inbuf,outbuf,eos);
        if((int)iStatus < 0) {
            GST_ERROR("WMA Encode Frame Failed!\n");
            return GST_FLOW_ERROR;
        }

        if (psEncParams->WMAE_isPacketReady) {
            /* allocate output buffer from downstream element */
            ret =
                gst_pad_alloc_buffer_and_set_caps (wma8enc->srcpad, GST_BUFFER_OFFSET_NONE,
                        psASFParams->g_asf_packet_size,
                        GST_PAD_CAPS (wma8enc->srcpad), &asfOutBuf);
            if (ret != GST_FLOW_OK) {
                GST_ERROR ("Can not allocate buffer from next element!\n");
                return GST_FLOW_ERROR;
            }

            asfOutData = GST_BUFFER_DATA (asfOutBuf);
            rcAsf = asf_packetize (psASFParams, outbuf, asfOutData, TRUE, psEncParams->WMAE_nEncodeSamplesDone);
            if(rcAsf != cASF_NoErr) {
                GST_ERROR("Add Asf packet failed.\n");
                return GST_FLOW_ERROR;
            }

            GST_BUFFER_OFFSET (asfOutBuf) = 0;
            GST_BUFFER_SIZE (asfOutBuf) = psASFParams->g_space_in_packet + ASF_HEADER;
            psASFParams->asf_file_size += GST_BUFFER_SIZE (asfOutBuf);

            ret = gst_pad_push (wma8enc->srcpad, asfOutBuf);
            if (ret != GST_FLOW_OK) {
                GST_ERROR("Failed push buffer to next element.\n");
                return ret;
            }
        }
    } while (eos && iStatus != WMA_NoMoreFrames);

    return GST_FLOW_OK;
}


static GstFlowReturn
mfw_gst_wma8enc_set_asf_header(MfwGstWma8EncInfo * wma8enc)
{
    wma8enc->asf_header_size = add_asf_file_header (wma8enc->psASFParams);
    if(wma8enc->asf_header_size <= 0) {
        GST_ERROR("Add Asf file header failed.\n");
        return GST_FLOW_ERROR;
    }

    return mfw_gst_wma8enc_push_asf_header (wma8enc);
}

static GstFlowReturn
mfw_gst_wma8enc_update_asf_header(MfwGstWma8EncInfo * wma8enc)
{
    ASFRESULTS rcAsf;
    WMAEEncoderConfig *psEncConfig = NULL;
    WMAEEncoderParams *psEncParams = NULL;
    ASFParams *psASFParams = NULL;

    psEncConfig = wma8enc->psEncConfig;
    psEncParams = psEncConfig->psEncodeParams;
    psASFParams = wma8enc->psASFParams;

    psASFParams->nSize = wma8enc->total_input;
    psASFParams->nSR = psEncParams->App_iDstAudioSampleRate;

    rcAsf = update_asf_file_header( psASFParams,
                                    psEncParams->App_iDstAudioBitRate / 1000,
                                    psEncParams->App_iDstAudioSampleRate / 1000,
                                    psEncParams->App_iDstAudioChannels,
                                    2 * psEncParams->App_iDstAudioChannels,
                                    psEncParams->pFormat.nSamplesPerFrame);
    if(rcAsf != cASF_NoErr) {
        GST_ERROR("Update Asf file header failed.\n");
        return GST_FLOW_ERROR;
    }

    return mfw_gst_wma8enc_push_asf_header (wma8enc);
}

static GstFlowReturn
mfw_gst_wma8enc_push_asf_header(MfwGstWma8EncInfo * wma8enc)
{
    GstFlowReturn ret;
    GstBuffer *asfHeaderBuf;
    guint8 *asfHeaderData;
    gint dataOffset;
    gint header_size;
    WMAEEncoderConfig *psEncConfig = NULL;
    WMAEEncoderParams *psEncParams = NULL;
    ASFParams *psASFParams = NULL;
    asf_header_type *pasfh;
    qword file_len,stream_len,ext_len,codec_len;

    psEncConfig = wma8enc->psEncConfig;
    psEncParams = psEncConfig->psEncodeParams;
    psASFParams = wma8enc->psASFParams;

    ret =
        gst_pad_alloc_buffer_and_set_caps (wma8enc->srcpad, GST_BUFFER_OFFSET_NONE,
                                           wma8enc->asf_header_size, GST_PAD_CAPS (wma8enc->srcpad), 
                                           &asfHeaderBuf);
    if (ret != GST_FLOW_OK) {
        GST_ERROR ("Can not allocate buffer from next element!\n");
        return GST_FLOW_ERROR;
    }

    write_asf_file_header(psASFParams, GST_BUFFER_DATA (asfHeaderBuf));
    GST_BUFFER_SIZE (asfHeaderBuf) = wma8enc->asf_header_size;
    psASFParams->asf_file_size += GST_BUFFER_SIZE (asfHeaderBuf);

    ret = gst_pad_push (wma8enc->srcpad, asfHeaderBuf);
    if (ret != GST_FLOW_OK) {
        GST_ERROR("Failed push buffer to next element.\n");
        return ret;
    }
    return ret;
}

FSL_GST_PLUGIN_DEFINE("wma8enc", "wma8 audio encoder", plugin_init);



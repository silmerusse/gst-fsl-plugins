/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/* GStreamer Adaptive Multi-Rate Narrow-Band (AMR-NB) plugin
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
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

/**
 * SECTION:element-amrnbenc
 * @see_also: #GstAmrnbDec, #GstAmrnbParse
 *
 * AMR narrowband encoder based on the 
 * <ulink url="http://sourceforge.net/projects/opencore-amr">opencore codec implementation</ulink>.
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch filesrc location=abc.wav ! wavparse ! audioresample ! audioconvert ! mfw_amrencoder ! filesink location=abc.amr
 * ]|
 * Please note that the above stream misses the header, that is needed to play
 * the stream.
 * </refsect2>
 */

#include "mfw_gst_amrenc.h"
#include "mfw_gst_utils.h"



typedef enum
{
    MR475,
    MR515,
    MR59, 
    MR67, 
    MR74, 
    MR795,
    MR102,
    MR122,
    MRDTX
} AMR_BANDMODE;

static const GEnumValue gst_amrnbenc_bandmode[] = {
    {MR475, "MR475",    "MR475"},
    {MR515, "MR515",    "MR515"},
    {MR59,  "MR59",     "MR59"},
    {MR67,  "MR67",     "MR67"},
    {MR74,  "MR74",     "MR74"},
    {MR795, "MR795",    "MR795"},
    {MR102, "MR102",    "MR102"},
    {MR122, "MR122",    "MR122"},
    {MRDTX, "MRDTX",    "MRDTX"},
    {0,     NULL,       NULL},
};

static GType
gst_amrnbenc_bandmode_get_type ()
{
  static GType gst_amrnbenc_bandmode_type = 0;
  if (!gst_amrnbenc_bandmode_type) {
    gst_amrnbenc_bandmode_type =
        g_enum_register_static ("MfwGstAmrnbEncBandMode", gst_amrnbenc_bandmode);
  }
  return gst_amrnbenc_bandmode_type;
}

static GType
gst_amrnbenc_format_get_type ()
{
  static GType gst_amrnbenc_format_type = 0;
  static const GEnumValue gst_amrnbenc_format[] = {
      {NBAMR_ETSI,  "ETSI", "ETSI"},
      {NBAMR_MMSIO, "MMS",  "MMS"},
      {NBAMR_IF1IO, "IF1",  "IF1"},
      {NBAMR_IF2IO, "IF2",  "IF2"},
      {0,           NULL,   NULL},
  };
  if (!gst_amrnbenc_format_type) {
    gst_amrnbenc_format_type =
        g_enum_register_static ("MfwGstAmrnbEncFormat", gst_amrnbenc_format);
  }
  return gst_amrnbenc_format_type;
}

#define GST_AMRNBENC_BANDMODE_TYPE (gst_amrnbenc_bandmode_get_type())
#define GST_AMRNBENC_FORMAT_TYPE   (gst_amrnbenc_format_get_type())

enum
{
  PROP_0,
  PROP_BANDMODE,
  PROP_FORMAT
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "signed = (boolean) TRUE, "
        "endianness = (int) BYTE_ORDER, "
        "rate = (int) 8000," "channels = (int) 1")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/AMR, " "rate = (int) 8000, " "channels = (int) 1")
    );

#define DEFAULT_DTX             FALSE
#define DEFAULT_NFRAMES         1
#define DEFAULT_BANDMODE        MR122
#define DEFAULT_FORMAT          NBAMR_MMSIO

#define MAX_SERIAL_SIZE         244
#define SERIAL_FRAMESIZE        (1+MAX_SERIAL_SIZE+5)
#define L_FRAME                 160


GST_DEBUG_CATEGORY_STATIC (gst_amrnbenc_debug);
#define GST_CAT_DEFAULT gst_amrnbenc_debug

static void gst_amrnbenc_finalize (GObject * object);

static GstFlowReturn gst_amrnbenc_chain (GstPad * pad, GstBuffer * buffer);
static gboolean gst_amrnbenc_setcaps (GstPad * pad, GstCaps * caps);
static GstStateChangeReturn gst_amrnbenc_state_change (GstElement * element,
    GstStateChange transition);

static void
_do_init (GType object_type)
{
  const GInterfaceInfo preset_interface_info = {
    NULL,                       /* interface init */
    NULL,                       /* interface finalize */
    NULL                        /* interface_data */
  };

  g_type_add_interface_static (object_type, GST_TYPE_PRESET,
      &preset_interface_info);

  GST_DEBUG_CATEGORY_INIT (gst_amrnbenc_debug, "mfw_amrencoder", 0,
      "Freescale AMR Encoder Plugin");
}

GST_BOILERPLATE_FULL (MfwGstAmrnbEnc, gst_amrnbenc, GstElement, GST_TYPE_ELEMENT,
    _do_init);

static void
gst_amrnbenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  MfwGstAmrnbEnc *self = GST_AMRNBENC (object);

  switch (prop_id) {
    case PROP_BANDMODE:
      self->bandmode = g_value_get_enum (value);
      self->mode_str = gst_amrnbenc_bandmode[self->bandmode].value_name;
      break;
    case PROP_FORMAT:
      self->bitstream_format = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}

static void
gst_amrnbenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  MfwGstAmrnbEnc *self = GST_AMRNBENC (object);

  switch (prop_id) {
    case PROP_BANDMODE:
      g_value_set_enum (value, self->bandmode);
      break;
    case PROP_FORMAT:
      g_value_set_enum (value, self->bitstream_format);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}

static void
gst_amrnbenc_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "amr NB audio encoder",
      "Codec/Encoder/Audio", "Encode raw audio to amr NB data");
}

static void
gst_amrnbenc_class_init (MfwGstAmrnbEncClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  object_class->set_property = gst_amrnbenc_set_property;
  object_class->get_property = gst_amrnbenc_get_property;
  object_class->finalize = gst_amrnbenc_finalize;

  g_object_class_install_property (object_class, PROP_BANDMODE,
      g_param_spec_enum ("band-mode", "Band Mode",
          "Encoding Band Mode (Kbps)", GST_AMRNBENC_BANDMODE_TYPE,
          DEFAULT_BANDMODE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_FORMAT,
      g_param_spec_enum ("format", "Bitstream Format",
          "Bitstream Format", GST_AMRNBENC_FORMAT_TYPE,
          DEFAULT_FORMAT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_amrnbenc_state_change);
}

static void
gst_amrnbenc_init (MfwGstAmrnbEnc * amrnbenc, MfwGstAmrnbEncClass * klass)
{
  /* create the sink pad */
  amrnbenc->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_setcaps_function (amrnbenc->sinkpad, gst_amrnbenc_setcaps);
  gst_pad_set_chain_function (amrnbenc->sinkpad, gst_amrnbenc_chain);
  gst_element_add_pad (GST_ELEMENT (amrnbenc), amrnbenc->sinkpad);

  /* create the src pad */
  amrnbenc->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_use_fixed_caps (amrnbenc->srcpad);
  gst_element_add_pad (GST_ELEMENT (amrnbenc), amrnbenc->srcpad);

  amrnbenc->adapter = gst_adapter_new ();

  amrnbenc->bandmode = MR122;
  amrnbenc->dtx_flag = DEFAULT_DTX;
  amrnbenc->bitstream_format = DEFAULT_FORMAT;
  amrnbenc->number_frame = DEFAULT_NFRAMES;

    /* Get the decoder version */
#define MFW_GST_AMR_ENCODER_PLUGIN VERSION
    PRINT_CORE_VERSION(eAMREVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_AMR_ENCODER_PLUGIN);
}

static void
gst_amrnbenc_finalize (GObject * object)
{
  MfwGstAmrnbEnc *amrnbenc;

  amrnbenc = GST_AMRNBENC (object);

  g_object_unref (G_OBJECT (amrnbenc->adapter));
  amrnbenc->adapter = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_amrnbenc_setcaps (GstPad * pad, GstCaps * caps)
{
  GstStructure *structure;
  MfwGstAmrnbEnc *amrnbenc;
  GstCaps *copy;
  gchar *frame_format[] = {"esti", "mms", "if1", "if2"};

  amrnbenc = GST_AMRNBENC (GST_PAD_PARENT (pad));

  GST_INFO ("sink pad caps : %"GST_PTR_FORMAT, caps);

  structure = gst_caps_get_structure (caps, 0);

  /* get channel count */
  gst_structure_get_int (structure, "channels", &amrnbenc->channels);
  gst_structure_get_int (structure, "rate", &amrnbenc->rate);

  /* this is not wrong but will sound bad */
  if (amrnbenc->channels != 1) {
    g_warning ("amrnbdec is only optimized for mono channels");
  }
  if (amrnbenc->rate != 8000) {
    g_warning ("amrnbdec is only optimized for 8000 Hz samplerate");
  }

  /* create reverse caps */
  copy = gst_caps_new_simple ("audio/AMR",
      "channels", G_TYPE_INT, amrnbenc->channels,
      "rate", G_TYPE_INT, amrnbenc->rate, 
      "frame_format", G_TYPE_STRING, frame_format[amrnbenc->bitstream_format], NULL);

  /* precalc duration as it's constant now */
  amrnbenc->duration = gst_util_uint64_scale_int (L_FRAME, GST_SECOND,
                                                  amrnbenc->rate);

  GST_INFO ("src pad caps : %"GST_PTR_FORMAT, copy);
  gst_pad_set_caps (amrnbenc->srcpad, copy);
  gst_caps_unref (copy);

  return TRUE;
}

static GstFlowReturn
gst_amrnbenc_chain (GstPad * pad, GstBuffer * buffer)
{
  MfwGstAmrnbEnc *amrnbenc;
  eAMREReturnType  eRetVal;
  gint outsize = ((NBAMR_MAX_PACKED_SIZE/2)+(NBAMR_MAX_PACKED_SIZE%2)) * 2;
  GstFlowReturn ret;

  amrnbenc = GST_AMRNBENC (GST_PAD_PARENT (pad));

  if (amrnbenc->rate == 0 || amrnbenc->channels == 0)
    goto not_negotiated;

  /* discontinuity clears adapter, FIXME, maybe we can set some
   * encoder flag to mask the discont. */
  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)) {
    gst_adapter_clear (amrnbenc->adapter);
    amrnbenc->ts = 0;
    amrnbenc->discont = TRUE;
  }

  GST_DEBUG("input buffer ts: %" GST_TIME_FORMAT ", size: %d, duration :%" 
            GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)), 
            GST_BUFFER_SIZE(buffer), 
            GST_TIME_ARGS(GST_BUFFER_DURATION(buffer)));

  /* take latest timestamp, FIXME timestamp is the one of the
   * first buffer in the adapter. */
  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    amrnbenc->ts = GST_BUFFER_TIMESTAMP (buffer);

  ret = GST_FLOW_OK;
  gst_adapter_push (amrnbenc->adapter, buffer);
  
  if (NBAMR_ETSI == amrnbenc->bitstream_format) {
    outsize = SERIAL_FRAMESIZE * 2;
  }

  /* Collect samples until we have enough for an output frame */
  while (gst_adapter_available (amrnbenc->adapter) >= L_FRAME * 2) {
    GstBuffer *out;
    guint8 *data;

    /* get output, max size is 34 */
    out = gst_buffer_new_and_alloc (outsize);
    GST_BUFFER_DURATION (out) = amrnbenc->duration;
    GST_BUFFER_TIMESTAMP (out) = amrnbenc->ts;
    if (GST_CLOCK_TIME_IS_VALID(amrnbenc->ts)) {
      amrnbenc->ts += amrnbenc->duration;
    }
    if (amrnbenc->discont) {
      GST_BUFFER_FLAG_SET (out, GST_BUFFER_FLAG_DISCONT);
      amrnbenc->discont = FALSE;
    }

    gst_buffer_set_caps (out, GST_PAD_CAPS (amrnbenc->srcpad));

    /* The AMR encoder actually writes into the source data buffers it gets */
    data = gst_adapter_take (amrnbenc->adapter, L_FRAME * 2);

    /* encode */
    eRetVal =
        eAMREEncodeFrame (amrnbenc->psEncConfig,
        (NBAMR_S16 *) data, (NBAMR_S16 *) GST_BUFFER_DATA (out));
    g_free (data);
    if (eRetVal != E_NBAMRE_OK) {
        if (eRetVal == E_NBAMRE_INVALID_MODE) {
            GST_ERROR("Invalid amr_mode specified: '%s'\n", amrnbenc->mode_str);
        }
        else {
            GST_ERROR("eAMREEncodeFrame failed, error code: %d\n", eRetVal);
        }

        ret = GST_FLOW_ERROR;
        break;
    }

    /* set data size */
    if (amrnbenc->psEncConfig->u8BitStreamFormat == NBAMR_IF1IO)
    {
        /*
         * IF1 frame format returns number of bits based on the mode used. This
         * includes the header part of 8 * 3 = 24 bits.
         * Following calculation is done to get the number of packed bytes to be
         * written into the file. 1 is added for the remainder bits.
         */
        amrnbenc->psEncConfig->pu32AMREPackedSize[0]=
                  (amrnbenc->psEncConfig->pu32AMREPackedSize[0] >> 3) + 1;
    }
    GST_BUFFER_SIZE(out) = amrnbenc->psEncConfig->pu32AMREPackedSize[0];

    /* play */
    if ((ret = gst_pad_push (amrnbenc->srcpad, out)) != GST_FLOW_OK)
      break;
    GST_DEBUG("push buffer ts: %" GST_TIME_FORMAT ", size: %d, duration :%" 
              GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(out)), 
              GST_BUFFER_SIZE(out), 
              GST_TIME_ARGS(GST_BUFFER_DURATION(out)));
  }
  return ret;

  /* ERRORS */
not_negotiated:
  {
    GST_ELEMENT_ERROR (amrnbenc, STREAM, TYPE_NOT_FOUND,
        (NULL), ("unknown type"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static gboolean
gst_amrnbenc_alloc_memory(MfwGstAmrnbEnc *enc)
{
    gint i = 0;
    NBAMR_S16  s16NumMemReqs;
    sAMREMemAllocInfoSubType *psMem;       /*pointer to encoder sub-memory */
    sAMREEncoderConfigType *psEncConfig = NULL;
    eAMREReturnType eRetVal = E_NBAMRE_OK;
    gboolean ret = TRUE;
  
    /* allocate fast memory for encoder config structure */
    enc->psEncConfig = (sAMREEncoderConfigType *)\
                            g_malloc0(sizeof(sAMREEncoderConfigType));
    if (enc->psEncConfig == NULL)
    {
        GST_ERROR ("Failed to allocate memory for psEncConfig");
        return FALSE;
    }
    psEncConfig = enc->psEncConfig;
    
    /* allocate memory for encoder to use */
    psEncConfig->pvAMREEncodeInfoPtr = NULL;
    /* Not used */
    psEncConfig->pu8APPEInitializedDataStart = NULL;
  
    /* Set DTX flag */
    psEncConfig->s16APPEDtxFlag = enc->dtx_flag;
  
    psEncConfig->u8BitStreamFormat = enc->bitstream_format;
    psEncConfig->u8NumFrameToEncode= enc->number_frame;
  
    /* encoded data size */
    psEncConfig->pu32AMREPackedSize = (NBAMR_U32 *)
                            g_malloc(enc->number_frame*sizeof(NBAMR_U32));
    if (psEncConfig->pu32AMREPackedSize == NULL)
    {
        GST_ERROR ("Failed to allocate memory for pu32AMREPackedSize");
        return FALSE;
    }
    /* user requested mode */
    psEncConfig->pps8APPEModeStr = (NBAMR_S8 **)
                            g_malloc(enc->number_frame * sizeof(NBAMR_S8 *));
    if (psEncConfig->pps8APPEModeStr == NULL)
    {
        GST_ERROR ("Failed to allocate memory for pps8APPEModeStr");
        return FALSE;
    }
    /* used mode by encoder */
    psEncConfig->pps8AMREUsedModeStr = (NBAMR_S8 **)
                            g_malloc(enc->number_frame * sizeof(NBAMR_S8*));
    if (psEncConfig->pps8AMREUsedModeStr == NULL)
    {
        GST_ERROR ("Failed to allocate memory for pps8AMREUsedModeStr");
        return FALSE;
    }
  
    for (i =0; i<enc->number_frame; i++)
    {
        /* set user requested encoding mode */
        psEncConfig->pps8APPEModeStr[i] = enc->mode_str;
    }
  
    /* initialize packed data variable for all the frames */
    for (i =0; i<enc->number_frame; i++)
    {
        *(psEncConfig->pu32AMREPackedSize+i) = 0;
    }
  
    /* initialize config structure memory to NULL */
    for(i = 0; i <NBAMR_MAX_NUM_MEM_REQS; i++)
    {
        (psEncConfig->sAMREMemInfo.asMemInfoSub[i].pvAPPEBasePtr) = NULL;
    }
  
    /* Find encoder memory requiremet */
    eRetVal = eAMREQueryMem (psEncConfig);
    if (eRetVal != E_NBAMRE_OK) {
        GST_ERROR ("Failed to query memory");
        return GST_STATE_CHANGE_FAILURE;
    }
    
    /* Number of memory chunk requested by the encoder */
   	s16NumMemReqs = (NBAMR_S16)(psEncConfig->sAMREMemInfo.s32AMRENumMemReqs);
    /* allocate memory requested by the encoder */
  	for(i = 0; i <s16NumMemReqs; i++)
    {
        psMem = &(psEncConfig->sAMREMemInfo.asMemInfoSub[i]);
        psMem->pvAPPEBasePtr = g_malloc(psMem->s32AMRESize);
        if (psMem->pvAPPEBasePtr == NULL)
        {
            GST_ERROR ("Failed to allocate memory for pvAPPEBasePtr");
            return FALSE;
        }
    }

    return TRUE;
}

static void
gst_amrnbenc_free_memory(MfwGstAmrnbEnc *enc)
{
    gint i = 0;
    NBAMR_S16  s16NumMemReqs;
    sAMREMemAllocInfoSubType *psMem;       /*pointer to encoder sub-memory */
    sAMREEncoderConfigType *psEncConfig = NULL;
    eAMREReturnType eRetVal = E_NBAMRE_OK;
    gboolean ret = TRUE;
  
    if (NULL == enc->psEncConfig)
        return;
    
    psEncConfig = enc->psEncConfig;
    
    if (NULL != psEncConfig->pu32AMREPackedSize) {
        g_free(psEncConfig->pu32AMREPackedSize);
        psEncConfig->pu32AMREPackedSize = NULL;
    }
    
    if (NULL != psEncConfig->pps8APPEModeStr) {
        g_free(psEncConfig->pps8APPEModeStr);
        psEncConfig->pps8APPEModeStr = NULL;
    }
    
    if (NULL != psEncConfig->pps8AMREUsedModeStr) {
        g_free(psEncConfig->pps8AMREUsedModeStr);
        psEncConfig->pps8AMREUsedModeStr = NULL;
    }
  
   	s16NumMemReqs = (NBAMR_S16)(psEncConfig->sAMREMemInfo.s32AMRENumMemReqs);
  	for(i = 0; i <s16NumMemReqs; i++) {
        psMem = &(psEncConfig->sAMREMemInfo.asMemInfoSub[i]);
        if (NULL != psMem->pvAPPEBasePtr) {
            g_free(psMem->pvAPPEBasePtr);
            psMem->pvAPPEBasePtr = NULL;
        }
    }
    
    g_free(enc->psEncConfig);

    return;
}

static GstStateChangeReturn
gst_amrnbenc_state_change (GstElement * element, GstStateChange transition)
{
  MfwGstAmrnbEnc *amrnbenc;
  GstStateChangeReturn ret;

  amrnbenc = GST_AMRNBENC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (FALSE == gst_amrnbenc_alloc_memory(amrnbenc)) {
        gst_amrnbenc_free_memory(amrnbenc);
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (E_NBAMRE_OK != eAMREEncodeInit (amrnbenc->psEncConfig)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      amrnbenc->rate = 0;
      amrnbenc->channels = 0;
      amrnbenc->ts = 0;
      amrnbenc->discont = FALSE;
      gst_adapter_clear (amrnbenc->adapter);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_amrnbenc_free_memory(amrnbenc);
      break;
    default:
      break;
  }
  return ret;
}

/*****************************************************************************/
/*    This is used to define the entry point and meta data of plugin         */
/*****************************************************************************/
static gboolean plugin_init(GstPlugin * plugin)
{
    return gst_element_register(plugin, "mfw_amrencoder",
				GST_RANK_PRIMARY, GST_TYPE_AMRNBENC);
}

FSL_GST_PLUGIN_DEFINE("amrenc", "amr audio encoder", plugin_init);




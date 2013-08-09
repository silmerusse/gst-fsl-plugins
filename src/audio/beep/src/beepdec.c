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
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. All rights reserved. 
 *
 */


/*
 * Module Name:   beepdec.c
 *
 * Description:   Implementation of unified audio decoder gstreamer plugin
 *
 * Portability:   This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */


#include <string.h>
#include <gst/audio/multichannel.h>

#include "beepdec.h"
#include "gstsutils/gstsutils.h"


#define BEEP_PCM_CAPS \
   "audio/x-raw-int, "\
   "width = (int){16, 24, 32}, depth = (int){16, 24, 32}, "\
   "rate = (int){7350, 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000}, "\
   "channels = (int)[1, 8]"

#define CORE_API(inf, name, elseroutine, err, ...)\
  do{\
      if (inf->name){\
          err = (inf->name)( __VA_ARGS__ );\
          if (err & CORE_FATAL_ERROR_MASK){\
            GST_ERROR("Call %s failed, ret = 0x%d", STR(name), (err));\
            do {\
                elseroutine;\
            }while(0);\
          }\
      }else{\
          GST_WARNING("Warning: API[" _STR(name) "] not implement!");\
          elseroutine;\
      }\
  }while(0)

#define CORE_FATAL_ERROR_MASK ((uint32)0xff)
#define CORE_STATUS_MASK (~(uint32)0xff)

#define BEEP_TIMEDIFF_DEFAULT_MS (1500)

#define AUDIO_BYTES2SAMPLE(bytes, width, channels) \
  ( ((width) && (channels)) ? (bytes*8/width/channels) : 0)

#define GST_CLOCK_ABS_DIFF(a,b)\
  ((a)>=(b)?GST_CLOCK_DIFF((b), (a)):GST_CLOCK_DIFF((a), (b)))

#define GST_TAG_BEEP_SAMPLING_RATE  "sampling_frequency"
#define GST_TAG_BEEP_CHANNELS       "channels"

#define BEEPDEC_ELEMENT_DEFAULT_LONGNAME "beep audio decoder"
#define BEEPDEC_ELEMENT_DEFAULT_DESCRIPTION "Decode compressed audio to raw data"

#define BEEP_NULL_REPLACE(value, defaultvalue)\
  ((value)? (value):(defaultvalue))

enum
{
  PROP_0,
  PROP_RESYNC_THRESHOLD,
  PROP_RESET_WHEN_RESYNC,
  PROP_SET_OUTPUT_LAYOUT,
};



typedef struct
{
  GType g_type;
  GTypeInfo g_typeinfo;
  BeepCoreDlEntry *entry;
} BeepGTypeMap;

typedef struct
{
  GstPadTemplate *pad_template;
  BeepCoreDlEntry *entry;
} BeepSinkPadTemplateMap;


static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (BEEP_PCM_CAPS)
    );

static GstsutilsOptionEntry g_beepdec_option_table[] = {
  {PROP_RESYNC_THRESHOLD, "resync-threshold", "resync-threshold",
        "set threshold for resync timestamp: -1, disable resync",
        G_TYPE_INT64,
        G_STRUCT_OFFSET (BeepDecOption, resync_threshold), "2000000000", "-1",
      G_MAXINT64_STR},
  {PROP_SET_OUTPUT_LAYOUT, "set-layout", "set layout",
        "enable/disable set output channel layout",
        G_TYPE_BOOLEAN, G_STRUCT_OFFSET (BeepDecOption, set_layout),
      "true"},
  {PROP_RESET_WHEN_RESYNC, "reset-when-resync", "reset when resync",
        "enable/disable reset when resync",
        G_TYPE_BOOLEAN, G_STRUCT_OFFSET (BeepDecOption, reset_when_resync),
      "true"},

  {-1, NULL, NULL, NULL, 0, 0, NULL}
};


GST_DEBUG_CATEGORY (gst_beepdec_debug);

static void gst_beepdec_finalize (GObject * object);

static GstFlowReturn gst_beepdec_chain (GstPad * pad, GstBuffer * buffer);
static gboolean gst_beepdec_setcaps (GstPad * pad, GstCaps * caps);
static GstStateChangeReturn gst_beepdec_state_change (GstElement * element,
    GstStateChange transition);
static gboolean gst_beepdec_sink_event (GstPad * pad, GstEvent * event);

static const GstQueryType *gst_beepdec_query_types (GstPad * pad);
static gboolean gst_beepdec_src_query (GstPad * pad, GstQuery * query);

static BeepSinkPadTemplateMap beepdecsubtemplate_map[GST_BEEP_SUBELEMENT_MAX] =
    { {NULL, NULL} };
static BeepGTypeMap beep_gtype_maps[GST_BEEP_SUBELEMENT_MAX] =
    { {G_TYPE_INVALID, {0}, NULL} };


static void *
beepdec_core_mem_alloc (uint32 size)
{
  void *ret = NULL;
  if (size) {
    ret = MM_MALLOC (size);
  }
  return ret;
}

static void
beepdec_core_mem_free (void *ptr)
{
  MM_FREE (ptr);
}

static void *
beepdec_core_mem_calloc (uint32 numElements, uint32 size)
{
  uint32 alloc_size = size * numElements;
  void *ret = NULL;
  if (alloc_size) {
    ret = MM_MALLOC (alloc_size);
    if (ret) {
      memset (ret, 0, alloc_size);
    }
  }
  return ret;
}

static void *
beepdec_core_mem_realloc (void *ptr, uint32 size)
{
  void *ret = MM_REALLOC (ptr, size);
  return ret;
}


static void
beepdec_core_close (GstBeepDec * beepdec)
{
  if (beepdec->beep_interface) {
    if (beepdec->handle) {
      if (beepdec->beep_interface->deleteDecoder) {
        beepdec->beep_interface->deleteDecoder (beepdec->handle);
        beepdec->handle = NULL;
      }
    }
    beep_core_destroy_interface (beepdec->beep_interface);
    beepdec->beep_interface = NULL;
  }
}


static GstCaps *
beepdec_outputformat_to_caps (GstBeepDec * beepdec)
{
  UniAcodecOutputPCMFormat *oformat = &beepdec->outputformat;
  GstCaps *caps = NULL, *supercaps;

  supercaps = gst_caps_from_string (BEEP_PCM_CAPS);

  caps = gst_caps_new_simple ("audio/x-raw-int",
      "endianness", G_TYPE_INT, G_LITTLE_ENDIAN,
      "signed", G_TYPE_BOOLEAN, TRUE,
      "width", G_TYPE_INT, oformat->width,
      "depth", G_TYPE_INT, oformat->depth,
      "rate", G_TYPE_INT, oformat->samplerate,
      "channels", G_TYPE_INT, oformat->channels, NULL);

  if (gst_caps_is_subset (caps, supercaps)) {

    if ((oformat->channels > 2) && (beepdec->options.set_layout)) {
      GValue chanpos = { 0 };
      GValue pos = { 0 };
      g_value_init (&chanpos, GST_TYPE_ARRAY);
      g_value_init (&pos, GST_TYPE_AUDIO_CHANNEL_POSITION);
      int i;

      for (i = 0; i < oformat->channels; i++) {
        switch (oformat->layout[i]) {
          case UA_CHANNEL_FRONT_MONO:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_MONO);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_FRONT_LEFT:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_FRONT_RIGHT:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_REAR_CENTER:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_CENTER);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_REAR_LEFT:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_REAR_RIGHT:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_LFE:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_LFE);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_FRONT_CENTER:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_FRONT_LEFT_CENTER:
            g_value_set_enum (&pos,
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_FRONT_RIGHT_CENTER:
            g_value_set_enum (&pos,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_SIDE_LEFT:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          case UA_CHANNEL_SIDE_RIGHT:
            g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT);
            gst_value_array_append_value (&chanpos, &pos);
            break;
          default:
            GST_WARNING ("Unsupport layout value %d at position %d",
                oformat->layout[i], i);
            goto bail;
            break;
        }
      }
      gst_structure_set_value (gst_caps_get_structure (caps, 0),
          "channel-positions", &chanpos);
    }
  } else {
    if (caps) {
      gst_caps_unref (caps);
      caps = NULL;
    }
  }
bail:
  if (supercaps) {
    gst_caps_unref (supercaps);
  }
  return caps;
}


static void
_do_init ()
{
  GST_DEBUG_CATEGORY_INIT (gst_beepdec_debug, "beepdec", 0,
      "Univisal audio decoder");
}


static void gst_beepdec_base_init (gpointer klass);
static void gst_beepdec_class_init (GstBeepDecClass * klass,
    gpointer class_data);
static void gst_beepdec_init (GstBeepDec * beepdec, GstBeepDecClass * klass);

static GstElementClass *parent_class = NULL;


GType
gst_beepdec_get_type (void)
{
  static GType beepdec_type = 0;

  if (G_UNLIKELY (!beepdec_type)) {
    static const GTypeInfo beepdec_info = {
      sizeof (GstBeepDecClass),
      (GBaseInitFunc) gst_beepdec_base_init, NULL,
      (GClassInitFunc) gst_beepdec_class_init,
      NULL, NULL, sizeof (GstBeepDec), 0,
      (GInstanceInitFunc) gst_beepdec_init,
    };

    beepdec_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstBeepDec",
        &beepdec_info, 0);

    _do_init ();
  }
  return beepdec_type;
}




#define CONSTRUCT_GTYPEINFO_SIMPLE(typeinfo, classsize, baseinitfunc, classinitfunc, classdata, instancesize, instanceinitfunc)\
  do{\
    (typeinfo).class_size=(classsize);\
    (typeinfo).base_init=(GBaseInitFunc)(baseinitfunc);\
    (typeinfo).class_init=(GClassInitFunc)(classinitfunc);\
    (typeinfo).class_data=(classdata);\
    (typeinfo).instance_size=(instancesize);\
    (typeinfo).instance_init=(GInstanceInitFunc)(instanceinitfunc);\
  }while(0)

static gint
gst_beepdec_subelement_find_type (BeepGTypeMap * map, BeepCoreDlEntry * entry)
{
  gint i = 0;

  while ((i <= GST_BEEP_SUBELEMENT_MAX) && (map[i].g_type != G_TYPE_INVALID)
      && (map[i].entry != entry)) {
    i++;
  }

  if (i > GST_BEEP_SUBELEMENT_MAX)
    i = -1;
  return i;
}


GType
gst_beepdec_subelement_get_type (BeepCoreDlEntry * entry)
{
  GType g_type = G_TYPE_INVALID;
  gint i = gst_beepdec_subelement_find_type (beep_gtype_maps, entry);

  if (i >= 0) {
    if (beep_gtype_maps[i].entry == entry) {
      g_type = beep_gtype_maps[i].g_type;
    } else {
      gchar *typename = g_strdup_printf ("GstBeepdec%d", i);
      beep_gtype_maps[i].entry = entry;
      CONSTRUCT_GTYPEINFO_SIMPLE (beep_gtype_maps[i].g_typeinfo,
          sizeof (GstBeepDecClass),
          gst_beepdec_base_init,
          gst_beepdec_class_init, entry, sizeof (GstBeepDec), gst_beepdec_init);
      g_type = beep_gtype_maps[i].g_type =
          g_type_register_static (GST_TYPE_ELEMENT, typename,
          &beep_gtype_maps[i].g_typeinfo, 0);

      g_free (typename);
    }
  }
  return g_type;
}


static GstEvent *
gst_beepdec_convert_segment (GstBeepDec * beepdec, GstEvent * event,
    GstFormat * destfmt)
{
#define BEEPSCALE(value, total, de) (((value)==((gint64)0))? 0 : \
  (((value)==((gint64)-1))? -1 : ((total)*(value)/(de))))

  gint64 start, stop, pos;
  gboolean update;
  gdouble rate;
  GstFormat fmt;
  GstEvent *nevent = event;

  gst_event_parse_new_segment (event, &update, &rate, &fmt, &start, &stop,
      &pos);


  if (fmt != *destfmt) {
    if (fmt == GST_FORMAT_BYTES) {
      if (*destfmt == GST_FORMAT_TIME) {

        if ((beepdec->byte_duration > 0) && (beepdec->byte_avg_rate > 0)) {
          start =
              BEEPSCALE (start, beepdec->byte_duration / beepdec->byte_avg_rate,
              beepdec->byte_duration);
          stop =
              BEEPSCALE (stop, beepdec->byte_duration / beepdec->byte_avg_rate,
              beepdec->byte_duration);
          pos =
              BEEPSCALE (pos, beepdec->byte_duration / beepdec->byte_avg_rate,
              beepdec->byte_duration);
        } else {
          if (start == ((gint64) 0))
            start = 0;
          else if ((start == ((gint64) - 1))
              || (start == beepdec->byte_duration))
            start = -1;
          else
            goto bail;
          if (stop == ((gint64) 0))
            stop = 0;
          else if ((stop == ((gint64) - 1)) || (stop == beepdec->byte_duration))
            stop = -1;
          else
            goto bail;
          if (pos == ((gint64) 0))
            pos = 0;
          else if ((pos == ((gint64) - 1)) || (pos == beepdec->byte_duration))
            pos = -1;
          else
            goto bail;
        }

        nevent = gst_event_new_new_segment (update, rate, GST_FORMAT_TIME,
            start, stop, pos);
        *destfmt = GST_FORMAT_TIME;
        gst_event_unref (event);
      }
    }
    *destfmt = fmt;
  }
bail:
  return nevent;
}



static void
gst_beepdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBeepDec *self = GST_BEEPDEC (object);

  switch (prop_id) {

    default:
      if (gstsutils_options_set_option (g_beepdec_option_table,
              (gchar *) & self->options, prop_id, value) == FALSE) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
  }
  return;
}

static void
gst_beepdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBeepDec *self = GST_BEEPDEC (object);

  switch (prop_id) {
    default:
      if (gstsutils_options_get_option (g_beepdec_option_table,
              (gchar *) & self->options, prop_id, value) == FALSE) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
  }

  return;
}


static GstPadTemplate *
gst_beepdec_sink_pad_template (void)
{
  static GstPadTemplate *templ = NULL;

  if (!templ) {
    GstCaps *caps = beep_core_get_caps ();

    if (caps) {
      templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    }
  }
  return templ;
}


static gint
gst_beepdec_subelement_find_sinkpad_template (BeepSinkPadTemplateMap * map,
    BeepCoreDlEntry * entry)
{
  gint i = 0;

  while ((i <= GST_BEEP_SUBELEMENT_MAX) && (map[i].pad_template != NULL)
      && (map[i].entry != entry)) {
    i++;
  }

  if (i > GST_BEEP_SUBELEMENT_MAX)
    i = -1;
  return i;
}



static GstPadTemplate *
gst_beepdec_subelement_sink_pad_template (BeepCoreDlEntry * entry)
{
  GstPadTemplate *sinkpadtemplate = NULL;

  int i = gst_beepdec_subelement_find_sinkpad_template (beepdecsubtemplate_map,
      entry);

  if (i >= 0) {
    if (beepdecsubtemplate_map[i].entry == entry) {
      sinkpadtemplate = beepdecsubtemplate_map[i].pad_template;
    } else {
      beepdecsubtemplate_map[i].entry = entry;
      sinkpadtemplate = beepdecsubtemplate_map[i].pad_template =
          gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          beep_core_get_cap (entry));
    }
  }
  return sinkpadtemplate;
}


static void
gst_beepdec_base_init (gpointer klass)
{
}

static void
gst_beepdec_class_init (GstBeepDecClass * klass, gpointer class_data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  BeepCoreDlEntry *entry = (BeepCoreDlEntry *) class_data;

  klass->entry = entry;

  if (entry) {
    gst_element_class_add_pad_template (element_class,
        gst_beepdec_subelement_sink_pad_template (entry));

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE (element_class,
        BEEP_NULL_REPLACE (entry->longname, BEEPDEC_ELEMENT_DEFAULT_LONGNAME),
        "Codec/Decoder/Audio", BEEP_NULL_REPLACE (entry->description,
            BEEPDEC_ELEMENT_DEFAULT_DESCRIPTION));
  } else {

    gst_element_class_add_pad_template (element_class,
        gst_beepdec_sink_pad_template ());

    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE (element_class,
        BEEPDEC_ELEMENT_DEFAULT_LONGNAME, "Codec/Decoder/Audio",
        BEEPDEC_ELEMENT_DEFAULT_DESCRIPTION);
  }

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);


  object_class->set_property = GST_DEBUG_FUNCPTR (gst_beepdec_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_beepdec_get_property);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_beepdec_finalize);

  gstsutils_options_install_properties_by_options (g_beepdec_option_table,
      object_class);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_beepdec_state_change);
}

static void
gst_beepdec_init (GstBeepDec * beepdec, GstBeepDecClass * klass)
{
  beepdec->entry = klass->entry;

  if (beepdec->entry) {
    beepdec->sinkpad =
        gst_pad_new_from_template (gst_beepdec_subelement_sink_pad_template
        (beepdec->entry), "sink");
  } else {
    beepdec->sinkpad =
        gst_pad_new_from_template (gst_beepdec_sink_pad_template (), "sink");
  }



  gst_pad_set_setcaps_function (beepdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_beepdec_setcaps));
  gst_pad_set_chain_function (beepdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_beepdec_chain));
  gst_pad_set_event_function (beepdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_beepdec_sink_event));
  gst_element_add_pad (GST_ELEMENT (beepdec), beepdec->sinkpad);

  beepdec->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_set_query_type_function (beepdec->srcpad,
      GST_DEBUG_FUNCPTR (gst_beepdec_query_types));
  gst_element_add_pad (GST_ELEMENT (beepdec), beepdec->srcpad);
  gst_pad_use_fixed_caps (beepdec->srcpad);

  gstsutils_options_load_default (g_beepdec_option_table,
      (gchar *) & beepdec->options);
  gstsutils_options_load_from_keyfile (g_beepdec_option_table,
      (gchar *) & beepdec->options, FSL_GST_CONF_DEFAULT_FILENAME, "beepdec");


}

static void
gst_beepdec_finalize (GObject * object)
{
  GstBeepDec *beepdec;

  beepdec = GST_BEEPDEC (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static gboolean
gst_beepdec_setcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret = TRUE;
  GstBeepDec *beepdec;
  GstCaps *src_caps;

  beepdec = GST_BEEPDEC (GST_PAD_PARENT (pad));

  GST_INFO ("Get upstream caps %" GST_PTR_FORMAT, caps);

  beepdec->beep_interface = beep_core_create_interface_from_caps (caps);

  if (beepdec->beep_interface) {

    UniACodecMemoryOps ops;
    ops.Malloc = beepdec_core_mem_alloc;
    ops.Calloc = beepdec_core_mem_calloc;
    ops.ReAlloc = beepdec_core_mem_realloc;
    ops.Free = beepdec_core_mem_free;

    beepdec->handle = beepdec->beep_interface->createDecoder (&ops);
    if (beepdec->handle) {
      GstStructure *structure;
      UniACodecParameter parameter;
      gint intvalue;
      const GValue *value = NULL;
	  gchar * stream_format;
      gint32 core_ret;
      structure = gst_caps_get_structure (caps, 0);
      if (gst_structure_get_int (structure, "rate", &intvalue)) {
        GST_INFO ("Set rate %d", intvalue);
        parameter.samplerate = intvalue;
        CORE_API (beepdec->beep_interface, setDecoderPara, goto error, core_ret,
            beepdec->handle, UNIA_SAMPLERATE, &parameter);
      }
      if (gst_structure_get_int (structure, "bitrate", &intvalue)) {
        GST_INFO ("Set bitrate %d", intvalue);
        parameter.bitrate = intvalue;
        CORE_API (beepdec->beep_interface, setDecoderPara, goto error, core_ret,
            beepdec->handle, UNIA_BITRATE, &parameter);
      }
      if (gst_structure_get_int (structure, "channels", &intvalue)) {
        GST_INFO ("Set channel %d", intvalue);
        parameter.channels = intvalue;
        CORE_API (beepdec->beep_interface, setDecoderPara, goto error, core_ret,
            beepdec->handle, UNIA_CHANNEL, &parameter);
        if (intvalue > 2) {
          if (alsa_channel_layouts[intvalue]) {
            memcpy (parameter.outputFormat.layout,
                alsa_channel_layouts[intvalue], sizeof (gint32) * intvalue);
          }
          parameter.outputFormat.chan_pos_set = TRUE;
          CORE_API (beepdec->beep_interface, setDecoderPara, goto error,
              core_ret, beepdec->handle, UNIA_OUTPUT_PCM_FORMAT, &parameter);
          beepdec->set_chan_pos = TRUE;
        }
      }
      if (gst_structure_get_int (structure, "depth", &intvalue)) {
        GST_INFO ("Set depth %d", intvalue);
        parameter.depth = intvalue;
        CORE_API (beepdec->beep_interface, setDecoderPara, goto error, core_ret,
            beepdec->handle, UNIA_DEPTH, &parameter);
      }

      if (gst_structure_get_int (structure, "block_align", &intvalue)) {
        GST_INFO ("Set block align %d", intvalue);
        parameter.blockalign = intvalue;
        CORE_API (beepdec->beep_interface, setDecoderPara, goto error, core_ret,
            beepdec->handle, UNIA_WMA_BlOCKALIGN, &parameter);
      }
      if (gst_structure_get_int (structure, "wmaversion", &intvalue)) {
        GST_INFO ("Set wma version %d", intvalue);
        parameter.version = intvalue;
        CORE_API (beepdec->beep_interface, setDecoderPara, goto error, core_ret,
            beepdec->handle, UNIA_WMA_VERSION, &parameter);
      }
      if (value = gst_structure_get_value (structure, "codec_data")) {
        GstBuffer *codec_data = gst_value_get_buffer (value);
        if ((codec_data) && GST_BUFFER_SIZE (codec_data)) {
          GST_INFO ("Set codec_data %" GST_PTR_FORMAT, codec_data);
          parameter.codecData.size = GST_BUFFER_SIZE (codec_data);
          parameter.codecData.buf = GST_BUFFER_DATA (codec_data);
          CORE_API (beepdec->beep_interface, setDecoderPara, goto error,
              core_ret, beepdec->handle, UNIA_CODEC_DATA, &parameter);
        }
      }
	  if ( stream_format = gst_structure_get_string(structure, "stream-format")) {
        GST_INFO ("Set stream_type %s", stream_format);
        if(g_strcmp0(stream_format, "adts") == 0) {
            parameter.stream_type = STREAM_ADTS;
        } 
        else if(g_strcmp0(stream_format, "adif") == 0) {
            parameter.stream_type = STREAM_ADIF;
        }
        else if(g_strcmp0(stream_format, "raw") == 0) {
            parameter.stream_type = STREAM_RAW;
        }
        else {
            parameter.stream_type = STREAM_UNKNOW;
        }
          CORE_API (beepdec->beep_interface, setDecoderPara, goto error,
              core_ret, beepdec->handle, UNIA_STREAM_TYPE, &parameter);
	  }
      if (value = gst_structure_get_value (structure, "framed")) {
        if (g_value_get_boolean (value)) {
          parameter.framed = TRUE;
          beepdec->framed = TRUE;
        } else {
          parameter.framed = FALSE;
          beepdec->framed = FALSE;
        }
        GST_INFO ("Set framed %s", ((parameter.framed) ? "true" : "false"));
        CORE_API (beepdec->beep_interface, setDecoderPara, goto error, core_ret,
            beepdec->handle, UNIA_FRAMED, &parameter);
      }

    } else {
      GST_ERROR ("Create core codec failed!!");
      goto error;
    }
  } else {
    GST_ERROR ("Create interface failed!!");
  }

  return ret;

error:
  if (beepdec->handle) {
    beepdec->beep_interface->deleteDecoder (beepdec->handle);
    beepdec->handle = NULL;
  }
  return FALSE;
}


static GstFlowReturn
gst_beepdec_chain (GstPad * pad, GstBuffer * buffer)
{
  GstBeepDec *beepdec;
  GstFlowReturn ret = GST_FLOW_UNEXPECTED;
  uint32 core_ret;
  uint8 *inbuf = NULL;
  uint32 inbuf_size = 0, offset = 0;
  uint32 status;

  beepdec = GST_BEEPDEC (GST_PAD_PARENT (pad));

  g_return_val_if_fail (beepdec->handle, GST_FLOW_WRONG_STATE);

  if (buffer) {
    inbuf = GST_BUFFER_DATA (buffer);
    inbuf_size = GST_BUFFER_SIZE (buffer);
  }

  beepdec->decoder_stat.compressed_bytes += inbuf_size;

  if (G_UNLIKELY ((beepdec->new_segment))) {
    if ((buffer) && (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
      beepdec->time_offset = GST_BUFFER_TIMESTAMP (buffer);
    } else {
      beepdec->time_offset = beepdec->segment_start;
    }
    beepdec->new_segment = FALSE;
  } else {

    if ((beepdec->framed)
        && (buffer)
        && (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
      if (beepdec->options.resync_threshold > 0) {
        GstClockTimeDiff diff;
        if ((diff =
                GST_CLOCK_ABS_DIFF (GST_BUFFER_TIMESTAMP (buffer),
                    beepdec->time_offset)) >
            beepdec->options.resync_threshold) {
          GST_WARNING ("Timestamp diff exceed %" GST_TIME_FORMAT
              ", Maybe a bug!", GST_TIME_ARGS (diff));
          if (beepdec->options.reset_when_resync) {
            CORE_API (beepdec->beep_interface, resetDecoder,, core_ret,
                beepdec->handle);
          }
          beepdec->time_offset = GST_BUFFER_TIMESTAMP (buffer);
        }
      } else if (beepdec->options.resync_threshold == 0) {
        beepdec->time_offset = GST_BUFFER_TIMESTAMP (buffer);
      }
    }
  }

  if (buffer) {
     GST_LOG ("chain in %"GST_TIME_FORMAT" size %d", GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)), GST_BUFFER_SIZE(buffer));
  }

  do {
    uint32 osize = 0;
    uint8 *obuf = NULL;
    status = 0;


#ifdef MFW_TIME_PROFILE
    gint64 time_diff;
    TIME_PROFILE (CORE_API (beepdec->beep_interface, decode,
            if (obuf) beepdec_core_mem_free (obuf);
            core_ret, beepdec->handle, inbuf, inbuf_size, &offset,
            &obuf, &osize), time_diff);
    beepdec->tp_stat.decodetime += time_diff;
#else
    CORE_API (beepdec->beep_interface, decode,
        if (obuf) beepdec_core_mem_free (obuf), core_ret, beepdec->handle,
        inbuf, inbuf_size, &offset, &obuf, &osize);
#endif

    if (ACODEC_ERROR_STREAM == core_ret) {
      beepdec->err_cnt++;
      if (beepdec->err_cnt < 50) {
        CORE_API (beepdec->beep_interface, resetDecoder, goto bail, core_ret,
            beepdec->handle);
        ret = GST_FLOW_OK;
      }
      goto bail;
    } else {
      beepdec->err_cnt = 0;
    }

    GST_LOG ("Decode return 0x%x", core_ret);
    if (core_ret & CORE_FATAL_ERROR_MASK) {
      GST_ERROR ("error = %x\n", core_ret);
      MFW_WEAK_ASSERT ((obuf == NULL) && (osize == 0));
      goto bail;
    }
    status = core_ret & CORE_STATUS_MASK;

    if (status == ACODEC_CAPIBILITY_CHANGE) {
      UniACodecParameter parameter = { 0 };
      GstTagList *list = gst_tag_list_new ();

      CORE_API (beepdec->beep_interface, getDecoderPara, goto bail, core_ret,
          beepdec->handle, UNIA_OUTPUT_PCM_FORMAT, &parameter);
      if (memcmp (&parameter.outputFormat, &beepdec->outputformat,
              sizeof (UniAcodecOutputPCMFormat))) {
        beepdec->outputformat = parameter.outputFormat;
        GstCaps *caps = beepdec_outputformat_to_caps (beepdec);
        if (caps) {
          GST_INFO ("Set new srcpad caps %" GST_PTR_FORMAT, caps);
          gst_pad_set_caps (beepdec->srcpad, caps);
          gst_caps_unref (caps);
        } else {
          goto bail;
        }
      }

      if (beepdec->set_chan_pos == FALSE) {
        gint32 channels = parameter.outputFormat.channels;
        if (channels > 2) {
          if (alsa_channel_layouts[channels]) {
            memcpy (parameter.outputFormat.layout,
                alsa_channel_layouts[channels], sizeof (gint32) * channels);
          }
          parameter.outputFormat.chan_pos_set = TRUE;
          beepdec->set_chan_pos = TRUE;
        }
        CORE_API (beepdec->beep_interface, setDecoderPara, goto bail, core_ret,
            beepdec->handle, UNIA_OUTPUT_PCM_FORMAT, &parameter);

        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BEEP_CHANNELS,
            parameter.outputFormat.channels, NULL);

        CORE_API (beepdec->beep_interface, getDecoderPara, goto bail, core_ret,
            beepdec->handle, UNIA_BITRATE, &parameter);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
            parameter.bitrate, NULL);

        CORE_API (beepdec->beep_interface, getDecoderPara, goto bail, core_ret,
            beepdec->handle, UNIA_SAMPLERATE, &parameter);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
            GST_TAG_BEEP_SAMPLING_RATE, parameter.samplerate, NULL);

        CORE_API (beepdec->beep_interface, getDecoderPara, goto bail, core_ret,
            beepdec->handle, UNIA_CODEC_DESCRIPTION, &parameter);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
            *(parameter.codecDesc), NULL);

        gst_element_found_tags (GST_ELEMENT (beepdec), list);

      }
    }

    if (obuf) {
      GstBuffer *gstbuf = gst_buffer_new ();
      gint samples;
      GstClockTime duration;
      samples =
          AUDIO_BYTES2SAMPLE (osize, beepdec->outputformat.width,
          beepdec->outputformat.channels);

      beepdec->decoder_stat.uncompressed_samples += samples;
      duration =
          gst_util_uint64_scale (GST_SECOND, (guint64) samples,
          (guint64) beepdec->outputformat.samplerate);

      GST_BUFFER_MALLOCDATA (gstbuf) = GST_BUFFER_DATA (gstbuf) = obuf;
      GST_BUFFER_SIZE (gstbuf) = osize;
      GST_BUFFER_DURATION (gstbuf) = duration;
      GST_BUFFER_FREE_FUNC (gstbuf) = beepdec_core_mem_free;
      if (beepdec->new_buffer_timestamp) {
        GST_BUFFER_TIMESTAMP (gstbuf) = beepdec->time_offset;
        beepdec->new_buffer_timestamp = FALSE;
      }
      if (beepdec->options.resync_threshold >= 0) {
        GST_BUFFER_TIMESTAMP (gstbuf) = beepdec->time_offset;
      }
      gst_buffer_set_caps (gstbuf, GST_PAD_CAPS (beepdec->srcpad));

      GST_LOG("push sample %"GST_TIME_FORMAT" size %d", GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(gstbuf)), GST_BUFFER_SIZE(gstbuf));

      if ((ret = gst_pad_push (beepdec->srcpad, gstbuf)) != GST_FLOW_OK) {
        GST_DEBUG ("Pad push failed, error = %d", ret);
        if (ret != GST_FLOW_NOT_LINKED) {
          goto bail;
        }
      }
      beepdec->time_offset += duration;

    }
  } while (((status != ACODEC_NOT_ENOUGH_DATA)
          && (status != ACODEC_END_OF_STREAM) && (((inbuf_size)
                  && (offset < inbuf_size)) || (inbuf_size == 0))));

  ret = GST_FLOW_OK;
bail:
  {
    if (buffer) {
      gst_buffer_unref (buffer);
    }
    return ret;
  }
}



static GstStateChangeReturn
gst_beepdec_state_change (GstElement * element, GstStateChange transition)
{
  GstBeepDec *beepdec;
  GstStateChangeReturn ret;

  beepdec = GST_BEEPDEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      MM_INIT_DBG_MEM ("beepdec");

      gst_tag_register (GST_TAG_BEEP_CHANNELS, GST_TAG_FLAG_DECODED,
          G_TYPE_UINT, "number of channels", "number of channels", NULL);
      gst_tag_register (GST_TAG_BEEP_SAMPLING_RATE, GST_TAG_FLAG_DECODED,
          G_TYPE_UINT, "sampling frequency (Hz)", "sampling frequency (Hz)",
          NULL);

      beepdec->beep_interface = NULL;
      beepdec->handle = NULL;
      beepdec->byte_duration = -1;
      memset (&beepdec->outputformat, 0, sizeof (UniAcodecOutputPCMFormat));
      beepdec->new_segment = beepdec->new_buffer_timestamp = TRUE;
      beepdec->segment_start = beepdec->time_offset = 0;
      beepdec->framed = FALSE;
      beepdec->set_chan_pos = FALSE;
      memset (&beepdec->decoder_stat, 0, sizeof (BeepDecStat));
#ifdef MFW_TIME_PROFILE
      memset (&beepdec->tp_stat, 0, sizeof (BeepTimeProfileStat));
#endif
      beepdec->err_cnt = 0;
      if (gst_pad_check_pull_range (beepdec->sinkpad)) {
        GstPad *peer_pad = NULL;
        GstFormat fmt = GST_FORMAT_BYTES;
        peer_pad = gst_pad_get_peer (beepdec->sinkpad);
        if (gst_pad_query_duration (peer_pad, &fmt, &beepdec->byte_duration)) {

        }
        gst_object_unref (GST_OBJECT (peer_pad));
        gst_pad_set_query_function (beepdec->srcpad,
            GST_DEBUG_FUNCPTR (gst_beepdec_src_query));
      }
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      beepdec_core_close (beepdec);
      MM_DEINIT_DBG_MEM ();
#ifdef MFW_TIME_PROFILE
      g_print ("total decoding time %" GST_TIME_FORMAT "\n",
          GST_TIME_ARGS (beepdec->tp_stat.decodetime));
#endif
      break;
    default:
      break;
  }
  return ret;
}


static gboolean
gst_beepdec_sink_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = TRUE;
  GstBeepDec *beepdec;
  beepdec = GST_BEEPDEC (GST_PAD_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format = GST_FORMAT_TIME;
      event = gst_beepdec_convert_segment (beepdec, event, &format);
      if (format == GST_FORMAT_TIME) {
        gint64 start, stop, position;
        gst_event_parse_new_segment (event, NULL, NULL, &format, &start,
            &stop, &position);
        beepdec->new_segment = TRUE;
        beepdec->new_buffer_timestamp = TRUE;
        beepdec->segment_start = start;
        GST_INFO ("Get newsegment event from %" GST_TIME_FORMAT "to %"
            GST_TIME_FORMAT " pos %" GST_TIME_FORMAT, GST_TIME_ARGS (start),
            GST_TIME_ARGS (stop), GST_TIME_ARGS (position));
      }
      ret = gst_pad_event_default (pad, event);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      uint32 core_ret;
      CORE_API (beepdec->beep_interface, resetDecoder, goto bail, core_ret,
          beepdec->handle);
      ret = gst_pad_event_default (pad, event);
      break;
    }
    case GST_EVENT_EOS:
    {
      GST_INFO ("EOS received");
      gst_beepdec_chain (pad, NULL);
      ret = gst_pad_event_default (pad, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

  return ret;
bail:
  if (event) {
    gst_event_unref (event);
  }
  return FALSE;
}


static const GstQueryType *
gst_beepdec_query_types (GstPad * pad)
{
  static const GstQueryType src_query_types[] = {
    GST_QUERY_DURATION,
    0
  };

  return src_query_types;
}

static gboolean
gst_beepdec_src_query (GstPad * pad, GstQuery * query)
{

  gboolean ret = FALSE;
  GstBeepDec *beepdec;
  beepdec = GST_BEEPDEC (GST_PAD_PARENT (pad));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      GstFormat fmt;
      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {

        if (beepdec->handle) {
          uint32 core_ret;
          UniACodecParameter parameter = { 0 };
          CORE_API (beepdec->beep_interface, getDecoderPara, goto bail,
              core_ret, beepdec->handle, UNIA_BITRATE, &parameter);

          if ((beepdec->byte_duration > 0) && (parameter.bitrate > 0)) {
            beepdec->byte_avg_rate = parameter.bitrate;
            beepdec->time_duration =
                beepdec->byte_duration * 8 / beepdec->byte_avg_rate *
                GST_SECOND;
          }
        }
        if (beepdec->time_duration < beepdec->time_offset)
          beepdec->time_duration = beepdec->time_offset;
        gst_query_set_duration (query, fmt, beepdec->time_duration);
        ret = TRUE;
      } else {
        goto bail;
      }
      break;
    }


    default:
      goto bail;
      break;
  }
bail:
  return ret;
}


void __attribute__ ((destructor)) beepdec_c_destructor (void);

void
beepdec_c_destructor ()
{
  gint i = 0;
  BeepSinkPadTemplateMap *map = beepdecsubtemplate_map;

  while ((map->pad_template) && (i < GST_BEEP_SUBELEMENT_MAX)) {
    gst_object_unref (GST_OBJECT ((map->pad_template)));
    i++;
    map++;
  }
}

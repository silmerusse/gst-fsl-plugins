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
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. (contact <sario.hu@freescale.com>).

 * Based on qtdemux.c by
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David A. Schleef <ds@schleef.org>
 * Copyright (C) <2006> Wim Taymans <wim@fluendo.com>
 * Copyright (C) <2007> Julien Moutte <julien@fluendo.com>
 * Copyright (C) <2009> Tim-Philipp M¡§1ller <tim@centricular.net>
 */



/*
 * Module Name:    aiurdemux.c
 *
 * Description:    Implementation of unified parser gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */

//#define GST_BUFFER_DEBUG
//#define MEMORY_DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aiurdemux.h"
#include "gstsutils/gstsutils.h"

#define AIURDEMUX_INIT_BLOCK_SIZE (4)

#define AIUR_CORETS_2_GSTTS(ts) (((ts)==PARSER_UNKNOWN_TIME_STAMP)? GST_CLOCK_TIME_NONE : (ts*1000))
#define AIUR_GSTTS_2_CORETS(ts) ((ts)/1000)

#define AIUR_COREDURATION_2_GSTDURATION(ts) (((ts)==PARSER_UNKNOWN_DURATION)? 0 : (ts*1000))
#define AIUR_READMODE_NULL (0xffffffff)

#define AC3_BIG_STARTCODE 0x00000b77
#define AC3_LIT_STARTCODE 0x0000770b

#define AIURDEMUX_FRAME_N_DEFAULT 30
#define AIURDEMUX_FRAME_D_DEFAULT 1

#define AIUR_ENV "aiurenv"

//#define AIUR_SUB_TEXT_SUPPORT

#define AIURDEMUX_IDX_FILE_SUBFIX ".aidx"

#define AIURDEMUX_MIN_OUTPUT_BUFFER_SIZE 8


#define AIUR_LOCACHE_MAX_WAYS  6
#define AIUR_LOCACHE_LINESIZE_SHIFT 12

#define AIUR_MEDIATYPE2STR(media) \
    (((media)==MEDIA_VIDEO)?"video":(((media)==MEDIA_AUDIO)?"audio":"subtitle"))

#define CORE_API(inf, name, elseroutine, err, ...)\
        do{\
            if (inf->name){\
                err = (inf->name)( __VA_ARGS__ );\
                if ((err!=PARSER_SUCCESS) && (err!=PARSER_EOS) && (err!=PARSER_BOS) && (err!=PARSER_ERR_INVALID_MEDIA)){\
                    GST_WARNING("API[" _STR(name) "] failed, ret=%d", err);\
                }\
            }else{\
                GST_WARNING("Warning: API[" _STR(name) "] not implement!");\
                elseroutine;\
            }\
        }while(0)

#define CORE_API_EXIST(inf, name)\
        (inf->name)

#define CORE_API_FAILED(err)\
        (err!=PARSER_SUCCESS)

#define AIUR_RESET_SAMPLE_STAT(stat)\
    do {\
        (stat).start = GST_CLOCK_TIME_NONE;\
        (stat).duration = 0;\
        (stat).flag = 0;\
    }while(0)

#define AIUR_UPDATE_SAMPLE_STAT(stat,timestamp, dura, sflag)\
    do {\
        if (((stat).start==GST_CLOCK_TIME_NONE) && \
            ((timestamp)!=GST_CLOCK_TIME_NONE))\
            (stat).start = (timestamp);\
        (stat).duration += dura;\
        (stat).flag |= sflag;\
    }while(0)

#define AIUR_PROTOCOL_IS_LIVE(uri) \
    (aiurdemux_is_protocol((uri), aiur_liveprotocol_table))

#define AIUR_PROTOCOL_IS_LOCAL(uri) \
    (aiurdemux_is_protocol((uri), aiur_localprotocol_table))

#define AIUR_STREAM_FLUSH_INTERLEAVE_QUEUE(stream) \
    do { \
      GstBuffer * gbuf; \
      while ((gbuf=g_queue_pop_head ((stream)->buf_queue))) { \
        gst_buffer_unref (gbuf); \
      }; \
    } while(0)

#define IS_PID_VALID(pid) ((pid)>=0)


GST_DEBUG_CATEGORY (aiurdemux_debug);


typedef struct
{
  gchar *name;
  ConfigValueType type;
  int offset;
  char *default_value;
} AiurDemuxConfigEntry;

typedef struct
{
  gchar *protocol;
} AiurDemuxProtocolEntry;

static AiurDemuxProtocolEntry aiur_liveprotocol_table[] = {
  {"mms"},
  {"rtmp"},
  {NULL},
};

static AiurDemuxProtocolEntry aiur_localprotocol_table[] = {
  {"file"},
  {NULL},
};


typedef struct
{
  uint32 width;
  uint32 height;
  uint32 fps_n;
  uint32 fps_d;
} AiurDemuxVideoInfo;

typedef struct
{
  uint32 rate;
  uint32 n_channels;
  uint32 sample_width;
  uint32 block_align;
} AiurDemuxAudioInfo;

typedef struct
{
  uint32 width;
  uint32 height;
} AiurDemuxSubtitleInfo;

typedef struct
{
  uint8 *codec_data;
  uint32 length;
  gboolean pushed;
} AiurDemuxCodecData;

typedef struct _AiurDemuxStreamPostProcessor
{
  GstFlowReturn (*process) (GstAiurDemux *, AiurDemuxStream *, GstBuffer **);
  void (*flush) (GstAiurDemux *, AiurDemuxStream *);
  void (*finalize) (GstAiurDemux *, AiurDemuxStream *);
  void *priv;
} AiurDemuxStreamPostProcessor;

typedef struct
{
  gint core_tag;
  gint format;
  const gchar *gst_tag_name;
  const gchar *print_string;
} AiurDemuxTagEntry;

typedef struct
{
  gint64 start;
  gint64 duration;
  uint32 flag;
} AiurSampleStat;

struct _AiurDemuxStream
{
  guint32 track_idx;
  gint32 pid;
  gint32 ppid;
  uint32 type;
  uint32 codec_type;
  uint32 codec_sub_type;

  guint64 duration;
  gchar lang[4];
  uint32 bitrate;

  guint32 mask;

  union
  {
    AiurDemuxVideoInfo video;
    AiurDemuxAudioInfo audio;
    AiurDemuxSubtitleInfo subtitle;
  } info;

  AiurDemuxCodecData codec_data;

  gboolean new_segment;
  gboolean partial_sample;
  gboolean valid;

  gboolean send_codec_data;
  gboolean block;
  gint32 preroll_size;

  guint64 time_position;
  gint64 last_stop;
  gint64 last_start;

  AiurSampleStat sample_stat;

  GstFlowReturn last_ret;

  GstTagList *pending_tags;

  GstBuffer *buffer;

  gboolean pending_eos;

  AiurDemuxStreamPostProcessor post_processor;

  GstCaps *caps;
  GstPad *pad;
  AiurDemuxProgram *program;

  GQueue *buf_queue;
  guint buf_queue_size;
  guint buf_queue_size_max;

  GstAdapter *adapter;
  gint num_of_samples;
};

typedef struct
{
  guint32 id;
  gint32 pid;
  gchar lang[4];
} AiurDemuxProgramTrack;

struct _AiurDemuxProgram
{
  gboolean enabled;
  gint32 program_number;
  gint32 pid;
  uint32 track_num;
  AiurDemuxProgramTrack tracks[0];
};


enum
{
  AIURDEMUX_STATE_PROBE,        /* Wait for mime set and select right core */
  AIURDEMUX_STATE_INITIAL,      /* Initial state, initial core interfaces  */
  AIURDEMUX_STATE_HEADER,       /* Parsing the header */
  AIURDEMUX_STATE_MOVIE,        /* Parsing/Playing the media data */
};
enum
{
  PROP_0,
  PROP_PROGRAM_NUMBER,
  PROP_AUDIO_MAX,
  PROP_VIDEO_MAX,
  PROP_SUBTITLE_MAX,
  PROP_MULTIPROGRAM_ENABLED,
  PROP_STREAM_MASK,
  PROP_PROGRAM_MASK,
  PROP_INTERLEAVE_QUEUE_SIZE,
};



static GstStaticPadTemplate gst_aiurdemux_videosrc_template =
GST_STATIC_PAD_TEMPLATE ("video_%06x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_aiurdemux_audiosrc_template =
GST_STATIC_PAD_TEMPLATE ("audio_%06x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_aiurdemux_subsrc_template =
GST_STATIC_PAD_TEMPLATE ("subtitle_%06x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstsutilsOptionEntry g_aiurdemux_option_table[] = {
  {PROP_PROGRAM_NUMBER, "program-number",
        "program-number",
        "Program number to demux for (-1 to ignore), valid only when multiprogram-enabled=false",
        G_TYPE_INT,
        G_STRUCT_OFFSET (AiurDemuxOption, program_number),
      "-1", "-1", G_MAXINT_STR},
  {PROP_AUDIO_MAX, "audio-max", "audio max",
        "set max number of audio (0 for disable, -1 for no limit", G_TYPE_INT,
      G_STRUCT_OFFSET (AiurDemuxOption, audio_max), "-1", "-1", G_MAXINT_STR},
  {PROP_VIDEO_MAX, "video-max", "video max",
        "set max number of video (0 for disable, -1 for no limit", G_TYPE_INT,
      G_STRUCT_OFFSET (AiurDemuxOption, video_max), "-1", "-1", G_MAXINT_STR},
  {PROP_SUBTITLE_MAX, "subtitle-max", "subtitle max",
        "set max number of subtitle (0 for disable, -1 for no limit",
        G_TYPE_INT, G_STRUCT_OFFSET (AiurDemuxOption, subtitle_max), "-1", "-1",
      G_MAXINT_STR},
  {PROP_MULTIPROGRAM_ENABLED, "multiprogram-enabled", "multiprogram enabled",
        "enable/disable multiprogram",
        G_TYPE_BOOLEAN, G_STRUCT_OFFSET (AiurDemuxOption, multiprogram_enabled),
      "false"},
  {PROP_STREAM_MASK, "stream-mask", "stream mask",
        "set stream bit mask (valid for first 64 audios, 0 for  enable all",
        G_TYPE_UINT64,
        G_STRUCT_OFFSET (AiurDemuxOption, stream_mask),
      "0", "0", G_MAXUINT64_STR},
  {PROP_PROGRAM_MASK, "program-mask", "program mask",
        "set program track bit mask (valid for first 32 programs, 0 for  enable all)",
        G_TYPE_UINT,
        G_STRUCT_OFFSET (AiurDemuxOption, program_mask),
      "0x0", "0", "0xffffffff"},
  {PROP_INTERLEAVE_QUEUE_SIZE, "interleave-queue-size", "interleave queue size",
        "set length of interleave queue in bytes for file read mode only",
        G_TYPE_UINT,
        G_STRUCT_OFFSET (AiurDemuxOption, interleave_queue_size),
      "10240000", "0", G_MAXUINT_STR},
  {-1, NULL, NULL, NULL, 0, 0, NULL}    /* terminator */
};


/* FIXME: merge to g_aiurdemux_option_table */
static AiurDemuxConfigEntry aiur_config_table[] = {
  {"aiur_import_index", TYPE_BOOLEAN, G_STRUCT_OFFSET (AiurDemuxConfig,
            import_index),
      "true"},
  {"aiur_export_index", TYPE_BOOLEAN, G_STRUCT_OFFSET (AiurDemuxConfig,
            export_index),
      "true"},
  {"aiur_index_dir", TYPE_STRING, G_STRUCT_OFFSET (AiurDemuxConfig, index_file_prefix), NULL},  /* default $HOME/.aiur */

  {"aiur_retimestamp_delay_ms", TYPE_INT, G_STRUCT_OFFSET (AiurDemuxConfig, retimestamp_delay_ms), "500"},      /* 500ms */
  {"aiur_retimestamp_threashold_ms", TYPE_INT, G_STRUCT_OFFSET (AiurDemuxConfig, retimestamp_threashold_ms), "2000"},   /* 2 second */

  {"aiur_cache_stream_preserve_size", TYPE_INT,
        G_STRUCT_OFFSET (AiurDemuxConfig,
          cache_stream_preserve_size), "200000"},
  {"aiur_cache_stream_max_size", TYPE_INT, G_STRUCT_OFFSET (AiurDemuxConfig,
          cache_stream_max_size), "10"},

  {"aiur_cache_local_ways", TYPE_INT, G_STRUCT_OFFSET (AiurDemuxConfig,
          cache_local_ways), "0"},
  {"aiur_cache_local_linesize_shift", TYPE_INT,
        G_STRUCT_OFFSET (AiurDemuxConfig,
          cache_local_linesize_shift), "12"},
  {"aiur_max_normal_rate", TYPE_DOUBLE, G_STRUCT_OFFSET (AiurDemuxConfig,
          max_normal_rate), "2.0"},

  {"aiur_max_interleave_second", TYPE_INT, G_STRUCT_OFFSET (AiurDemuxConfig, max_interleave_second), "60"},     /* 60 seconds interleave check */
  {"aiur_max_interleave_byte", TYPE_INT, G_STRUCT_OFFSET (AiurDemuxConfig, max_interleave_bytes), "2097152"},   /* playbin2 preroll 2M bytes */
  {"aiur_num_of_samples", TYPE_INT, G_STRUCT_OFFSET (AiurDemuxConfig,
          num_of_samples), "-1"},

  {"aiur_drop_prerollsample", TYPE_BOOLEAN, G_STRUCT_OFFSET (AiurDemuxConfig,
            drop_sample),
      "true"},

  {NULL}                        /* terminator */
};

static AiurDemuxTagEntry g_user_data_entry[] = {
  {USER_DATA_TITLE, USER_DATA_FORMAT_UTF8, GST_TAG_TITLE,
      "Title                  : %s\n"},
  {USER_DATA_LANGUAGE, USER_DATA_FORMAT_UTF8, GST_TAG_LANGUAGE_CODE,
      "Langurage              : %s\n"},
  {USER_DATA_GENRE, USER_DATA_FORMAT_UTF8, GST_TAG_GENRE,
      "Genre                  : %s\n"},
  {USER_DATA_ARTIST, USER_DATA_FORMAT_UTF8, GST_TAG_ARTIST,
      "Artist                 : %s\n"},
  {USER_DATA_COPYRIGHT, USER_DATA_FORMAT_UTF8, GST_TAG_COPYRIGHT,
      "Copy Right             : %s\n"},
  {USER_DATA_COMMENTS, USER_DATA_FORMAT_UTF8, GST_TAG_COMMENT,
      "Comments               : %s\n"},
  {USER_DATA_CREATION_DATE, USER_DATA_FORMAT_UTF8, GST_TAG_DATE,
      "Creation Date          : %s\n"},
  //{USER_DATA_RATING,          USER_DATA_FORMAT_UTF8, GST_TAG_USER_RATING,     "Album : %s\n"}, /* tag was defined since 0.10.29 */
  {USER_DATA_ALBUM, USER_DATA_FORMAT_UTF8, GST_TAG_ALBUM,
      "Album                  : %s\n"},
  {USER_DATA_VCODECNAME, USER_DATA_FORMAT_UTF8, GST_TAG_VIDEO_CODEC,
      "Video Codec Name       : %s\n"},
  {USER_DATA_ACODECNAME, USER_DATA_FORMAT_UTF8, GST_TAG_AUDIO_CODEC,
      "Audio Codec Name       : %s\n"},
  {USER_DATA_ARTWORK, USER_DATA_FORMAT_JPEG, GST_TAG_IMAGE,
      "Found Artwork          : %" GST_PTR_FORMAT ", %d bytes\n"},
  {USER_DATA_COMPOSER, USER_DATA_FORMAT_UTF8, GST_TAG_COMPOSER,
      "Composer               : %s\n"},
  //{USER_DATA_DIRECTOR,        USER_DATA_FORMAT_UTF8, ?,                       "Director : %s\n"}, /* tag is not defined */
  //{USER_DATA_INFORMATION,     USER_DATA_FORMAT_UTF8, ?,                       "Information : %s\n"}, /* tag is not defined */
  //{USER_DATA_CREATOR,         USER_DATA_FORMAT_UTF8, ?,                       "Creator : %s\n"}, /* tag is not defined */
  //{USER_DATA_PRODUCER,        USER_DATA_FORMAT_UTF8, ?,                       "Producer : %s\n"}, /* tag is not defined */
  {USER_DATA_PERFORMER, USER_DATA_FORMAT_UTF8, GST_TAG_PERFORMER,
      "Performer              : %s\n"},
  //{USER_DATA_REQUIREMENTS,    USER_DATA_FORMAT_UTF8, ?,                       "Requirements : %s\n"}, /* tag is not defined */
  //{USER_DATA_SONGWRITER,      USER_DATA_FORMAT_UTF8, ?,                       "Song Writer : %s\n"}, /* tag is not defined */
  //{USER_DATA_MOVIEWRITER,     USER_DATA_FORMAT_UTF8, ?,                       "Movie Writer : %s\n"}, /* tag is not defined */
  {USER_DATA_TOOL, USER_DATA_FORMAT_UTF8, GST_TAG_ENCODER,
      "Writing Application    : %s\n"},
  {USER_DATA_DESCRIPTION, USER_DATA_FORMAT_UTF8, GST_TAG_DESCRIPTION,
      "Description            : %s\n"},
};

static GstElementClass *parent_class = NULL;

gpointer aiurdemux_loop_push (gpointer * data);

static void gst_aiurdemux_class_init (GstAiurDemuxClass * klass);
static void gst_aiurdemux_base_init (GstAiurDemuxClass * klass);
static void gst_aiurdemux_init (GstAiurDemux * quicktime_demux);
static void gst_aiurdemux_finalize (GObject * object);

static GstStateChangeReturn gst_aiurdemux_change_state (GstElement * element,
    GstStateChange transition);
static gboolean aiurdemux_sink_activate (GstPad * sinkpad);
static gboolean aiurdemux_sink_activate_pull (GstPad * sinkpad,
    gboolean active);
static gboolean aiurdemux_sink_activate_push (GstPad * sinkpad,
    gboolean active);

static void aiurdemux_pull_task (GstPad * pad);
static void aiurdemux_push_task (GstAiurDemux * aiurdemux);
static GstFlowReturn gst_aiurdemux_chain (GstPad * sinkpad, GstBuffer * inbuf);
static gboolean gst_aiurdemux_handle_sink_event (GstPad * pad,
    GstEvent * event);

static void aiurdemux_pretty_print_info (gchar * title, gchar * data,
    int max_raw);
static gchar *aiurdemux_generate_idx_file_location (GstAiurDemux * demux,
    char *prefix);

static AiurDemuxStream *aiurdemux_trackidx_to_stream (GstAiurDemux * demux,
    gint32 track_idx);
static GstFlowReturn
aiurdemux_push_pad_buffer (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstBuffer * buffer);


/* memory callbacks */
void *
aiurdemux_callback_malloc (uint32 size)
{
  void *memory;

  if (size == 0) {
    size = 4;
    GST_WARNING ("Try mallo 0 size buffer, maybe a core parser bug!");
  }

  memory = MM_MALLOC (size);
  return memory;
}


void *
aiurdemux_callback_calloc (uint32 numElements, uint32 size)
{
  void *memory;

  if (size == 0) {
    size = 4;
    GST_WARNING ("Try callo 0 size buffer, maybe a core parser bug!");
  }

  memory = MM_MALLOC (numElements * size);

  if (memory) {
    memset (memory, 0, numElements * size);
  }

  return memory;
}


void *
aiurdemux_callback_realloc (void *ptr, uint32 size)
{
  void *memory;

  if (size == 0) {
    size = 4;
    GST_WARNING ("Try realloc 0 size buffer, maybe a core parser bug!");
  }

  memory = MM_REALLOC (ptr, size);

  return memory;
}


void
aiurdemux_callback_free (void *ptr)
{
  if (ptr) {
    MM_FREE (ptr);
  } else {
    GST_WARNING ("Try free NULL buffer, maybe a core parser bug!");
  }
}

/* pull mode stream callbacks */
FslFileHandle
aiurdemux_callback_open_pull (const uint8 * fileName, const uint8 * mode,
    void *context)
{
  GstAiurDemux *demux = (GstAiurDemux *) context;
  AiurDemuxContentDesc *content;

  content = g_new0 (AiurDemuxContentDesc, 1);
  if (content) {

    content->length = demux->content_info.length;
    content->seekable = demux->content_info.seekable;

    if (demux->config.cache_local_ways) {
      content->cache =
          gst_aiur_local_cache_new (demux->sinkpad,
          demux->config.cache_local_ways,
          demux->config.cache_local_linesize_shift);
    }

    MM_REGRES (content, RES_FILE_DEVICE);
  }

  return content;
}


int32
aiurdemux_callback_close_pull (FslFileHandle handle, void *context)
{
  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    if (content->cache) {
      gst_aiur_local_cache_free (content->cache);
    }
    MM_UNREGRES (handle, RES_FILE_DEVICE);
    g_free (handle);
  }
  return 0;
}


uint32
aiurdemux_callback_read_pull (FslFileHandle handle, void *buffer, uint32 size,
    void *context)
{
  GstBuffer *gstbuffer;
  AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
  GstAiurDemux *demux = (GstAiurDemux *) context;
  GstFlowReturn ret;
  gint32 read_size = 0;
  if ((content == NULL) || (size == 0))
    return 0;

  if (content->cache) {
    read_size =
        gst_aiur_local_cache_read (content->cache, content->offset, size,
        buffer);
    if (read_size >= 0)
      content->offset += read_size;


  } else {

    ret = gst_pad_pull_range (demux->sinkpad, content->offset,
        size, &gstbuffer);

    if (ret == GST_FLOW_OK) {
      read_size = GST_BUFFER_SIZE (gstbuffer);
      content->offset += read_size;
      memcpy (buffer, GST_BUFFER_DATA (gstbuffer), read_size);
      gst_buffer_unref (gstbuffer);
    } else {
      GST_WARNING ("gst_pad_pull_range failed ret = %d", ret);
    }

  }

  return read_size;
}


int32
aiurdemux_callback_seek_pull (FslFileHandle handle, int64 offset,
    int32 whence, void *context)
{
  AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
  int64 newoffset = content->offset;
  int32 ret = 0;

  if (content == NULL)
    return -1;

  switch (whence) {
    case SEEK_SET:
      newoffset = offset;
      break;

    case SEEK_CUR:
      newoffset += offset;
      break;

    case SEEK_END:
      newoffset = content->length + offset;
      break;

    default:
      return -1;
      break;
  }

  if ((newoffset < 0) || ((content->length > 0)
          && (newoffset > content->length))) {
    GST_ERROR ("Failed to seek. Target (%lld) exceeds the file range (%lld)",
        newoffset, content->length);
    ret = -1;
  } else {
    content->offset = newoffset;
  }

  return ret;
}


int64
aiurdemux_callback_tell_pull (FslFileHandle handle, void *context)
{
  AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;

  if (content == NULL)
    return 0;
  return content->offset;
}


int64
aiurdemux_callback_availiable_bytes_pull (FslFileHandle handle,
    int64 bytesRequested, void *context)
{
  return bytesRequested;
}


int64
aiurdemux_callback_size_pull (FslFileHandle handle, void *context)
{
  AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;

  if (content == NULL)
    return 0;

  return content->length;
}


/* push mode stream callbacks */
FslFileHandle
aiurdemux_callback_open_push (const uint8 * fileName, const uint8 * mode,
    void *context)
{
  GstAiurDemux *demux = (GstAiurDemux *) context;
  AiurDemuxContentDesc *content;

  content = g_new0 (AiurDemuxContentDesc, 1);
  if (content) {
    content->cache =
        gst_mini_object_ref (GST_MINI_OBJECT_CAST (demux->stream_cache));
    content->length = demux->content_info.length;
    content->seekable = demux->content_info.seekable;

    MM_REGRES (content, RES_FILE_DEVICE);
  }

  return content;
}


int32
aiurdemux_callback_close_push (FslFileHandle handle, void *context)
{
  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    if (content->cache) {
      gst_mini_object_unref (GST_MINI_OBJECT_CAST (content->cache));
      content->cache = NULL;
    }
    MM_UNREGRES (handle, RES_FILE_DEVICE);
    g_free (handle);
  }
  return 0;
}


uint32
aiurdemux_callback_read_push (FslFileHandle handle, void *buffer, uint32 size,
    void *context)
{

  uint32 ret = 0;

  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    if (size == 0)
      return ret;

    if (content->offset != gst_aiur_stream_cache_get_position (content->cache)) {
      gst_aiur_stream_cache_seek (content->cache, content->offset);
    }
    gint64 readsize =
        gst_aiur_stream_cache_read (content->cache, (guint64) size, buffer);
    if (readsize >= 0) {
      ret = readsize;
      content->offset += readsize;
    }

  }

  return ret;
}


int32
aiurdemux_callback_seek_push (FslFileHandle handle, int64 offset,
    int32 whence, void *context)
{

  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    int64 newoffset = content->offset;
    switch (whence) {
      case SEEK_SET:
        newoffset = offset;
        break;

      case SEEK_CUR:
        newoffset += offset;
        break;

      case SEEK_END:
        newoffset = content->length + offset;
        break;

      default:
        return -1;
        break;
    }

    if ((newoffset < 0) || ((content->length > 0)
            && (newoffset > content->length))) {
      GST_ERROR ("Failed to seek. Target (%lld) exceeds the file range (%lld)",
          newoffset, content->length);
      return -1;
    } else {
      content->offset = newoffset;
    }
  }



  return 0;
}


int64
aiurdemux_callback_size_push (FslFileHandle handle, void *context)
{

  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    return content->length;
  }

  return -1;


}


int64
aiurdemux_callback_tell_push (FslFileHandle handle, void *context)
{
  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    return content->offset;
  }

  return -1;
}


int64
aiurdemux_callback_availiable_bytes_push (FslFileHandle handle,
    int64 bytesRequested, void *context)
{
  return bytesRequested;
}


/* buffer callbacks */
uint8 *
aiurdemux_callback_request_buffer (uint32 stream_idx, uint32 * size,
    void **bufContext, void *parserContext)
{
  uint8 *buffer = NULL;
  GstBuffer *gstbuf = NULL;

  GstAiurDemux *demux = (GstAiurDemux *) parserContext;
  AiurDemuxStream *stream = aiurdemux_trackidx_to_stream (demux, stream_idx);

  MFW_WEAK_ASSERT ((*size) < 100000000);

  if (*size == 0) {
    GST_WARNING
        ("Stream[%02d] request zero size buffer, maybe a core parser bug!",
        stream_idx);
    *size = AIURDEMUX_MIN_OUTPUT_BUFFER_SIZE;
  }

  if (stream) {
    gstbuf = gst_buffer_new_and_alloc (*size);
    *bufContext = gstbuf;
  } else {
    GST_ERROR ("Unknown stream number %d.", stream_idx);
  }

  if (gstbuf) {
    buffer = GST_BUFFER_DATA (gstbuf);
    *bufContext = gstbuf;
    MM_REGRES (gstbuf, RES_GSTBUFFER);
  }

  return buffer;
}


void
aiurdemux_callback_release_buffer (uint32 stream_idx, uint8 * pBuffer,
    void *bufContext, void *parserContext)
{
  GstBuffer *gstbuf = (GstBuffer *) bufContext;
  if (gstbuf) {
    MM_UNREGRES (gstbuf, RES_GSTBUFFER);
    gst_buffer_unref (gstbuf);
  }
}


GType
gst_aiurdemux_get_type (void)
{
  static GType aiurdemux_type = 0;

  if (G_UNLIKELY (!aiurdemux_type)) {
    static const GTypeInfo aiurdemux_info = {
      sizeof (GstAiurDemuxClass),
      (GBaseInitFunc) gst_aiurdemux_base_init, NULL,
      (GClassInitFunc) gst_aiurdemux_class_init,
      NULL, NULL, sizeof (GstAiurDemux), 0,
      (GInstanceInitFunc) gst_aiurdemux_init,
    };

    aiurdemux_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstAiurDemux",
        &aiurdemux_info, 0);
  }
  return aiurdemux_type;
}


static GstPadTemplate *
gst_aiurdemux_sink_pad_template (void)
{
  static GstPadTemplate *templ = NULL;

  if (!templ) {
    GstCaps *caps = aiur_core_get_caps ();

    if (caps) {
      templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    }
  }
  return templ;
}


static gboolean
gst_aiurdemux_setcaps (GstPad * pad, GstCaps * caps)
{
  GstAiurDemux *demux = GST_AIURDEMUX (GST_PAD_PARENT (pad));


  if (!demux->pullbased)
    gst_aiur_stream_cache_attach_pad (demux->stream_cache, pad);

  demux->core_interface = aiur_core_create_interface_from_caps (caps);
  if ((demux->core_interface) && (demux->core_interface->dlentry)
      && (demux->core_interface->dlentry->name)) {
    gst_tag_list_add (demux->tag_list, GST_TAG_MERGE_REPLACE,
        GST_TAG_CONTAINER_FORMAT, (demux->core_interface->dlentry->name), NULL);
  }

  if (demux->core_interface) {
    demux->state = AIURDEMUX_STATE_INITIAL;

    return TRUE;
  } else
    return FALSE;
}


static void
gst_aiurdemux_base_init (GstAiurDemuxClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_aiurdemux_sink_pad_template ());
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_aiurdemux_videosrc_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_aiurdemux_audiosrc_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_aiurdemux_subsrc_template));

  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE (element_class, "aiur universal demuxer",
      "Codec/Demuxer", "demux container file to video, audio, and subtitle");

  GST_DEBUG_CATEGORY_INIT (aiurdemux_debug, "aiurdemux", 0, "aiurdemux plugin");
}


static void
gst_aiurdemux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAiurDemux *self = GST_AIURDEMUX (object);

  if (gstsutils_options_set_option (g_aiurdemux_option_table,
          (gchar *) & self->option, prop_id, value) == FALSE) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
gst_aiurdemux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAiurDemux *self = GST_AIURDEMUX (object);

  if (gstsutils_options_get_option (g_aiurdemux_option_table,
          (gchar *) & self->option, prop_id, value) == FALSE) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
gst_aiurdemux_class_init (GstAiurDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_aiurdemux_finalize;
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_aiurdemux_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_aiurdemux_get_property);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_aiurdemux_change_state);

  gstsutils_options_install_properties_by_options (g_aiurdemux_option_table,
      gobject_class);
}


static void
aiurdemux_load_config (AiurDemuxConfig * aiurconfig)
{
  gboolean loadenv = FALSE;
  AiurDemuxConfigEntry *entry = aiur_config_table;
  char *value;
  char *config = (char *) aiurconfig;

  if (getenv (AIUR_ENV)) {
    loadenv = TRUE;
  }
  while ((entry) && (entry->name)) {
    if ((loadenv == FALSE) || ((value = getenv (entry->name)) == NULL)) {
      value = entry->default_value;
    }
    if (value) {
      switch (entry->type) {
        case TYPE_STRING:
          *(char **) (config + entry->offset) = value;
          break;
        case TYPE_INT:
          *((int *) (config + entry->offset)) =
              g_ascii_strtoll (value, NULL, 0);
          break;
        case TYPE_BOOLEAN:
          if (strcmp (value, "true") == 0) {
            *((gboolean *) (config + entry->offset)) = TRUE;
          } else {
            *((gboolean *) (config + entry->offset)) = FALSE;
          }
          break;
        case TYPE_DOUBLE:
          *((gdouble *) (config + entry->offset)) = g_strtod (value, NULL);
          break;
      }
    }
    entry++;
  }
}

static void
gst_aiurdemux_init (GstAiurDemux * aiurdemux)
{
  char *env;

  aiurdemux->sinkpad =
      gst_pad_new_from_template (gst_aiurdemux_sink_pad_template (), "sink");
  gst_pad_set_activate_function (aiurdemux->sinkpad, aiurdemux_sink_activate);
  gst_pad_set_activatepull_function (aiurdemux->sinkpad,
      aiurdemux_sink_activate_pull);
  gst_pad_set_activatepush_function (aiurdemux->sinkpad,
      aiurdemux_sink_activate_push);
  gst_pad_set_chain_function (aiurdemux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiurdemux_chain));
  gst_pad_set_event_function (aiurdemux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiurdemux_handle_sink_event));
  gst_element_add_pad (GST_ELEMENT_CAST (aiurdemux), aiurdemux->sinkpad);

  gst_pad_set_setcaps_function (aiurdemux->sinkpad, gst_aiurdemux_setcaps);

  gstsutils_options_load_default (g_aiurdemux_option_table,
      (gchar *) & aiurdemux->option);
  gstsutils_options_load_from_keyfile (g_aiurdemux_option_table,
      (gchar *) & aiurdemux->option, FSL_GST_CONF_DEFAULT_FILENAME,
      "aiurdemux");

  aiurdemux->state = AIURDEMUX_STATE_PROBE;
  aiurdemux->pullbased = FALSE;
  aiurdemux_load_config (&aiurdemux->config);
  aiurdemux->stream_cache =
      gst_aiur_stream_cache_new ((guint64) aiurdemux->
      config.cache_stream_preserve_size,
      (guint64) aiurdemux->config.cache_stream_preserve_size +
      aiurdemux->config.cache_stream_max_size, aiurdemux);
  aiurdemux->runmutex = g_mutex_new ();
  aiurdemux->play_mode = AIUR_PLAY_MODE_NORMAL;

  aiurdemux->clip_info.auto_retimestamp = FALSE;
  aiurdemux->clip_info.suggest_read_mode = AIUR_READMODE_NULL;

  gst_segment_init (&aiurdemux->segment, GST_FORMAT_TIME);
}


static void
gst_aiurdemux_finalize (GObject * object)
{
  GstAiurDemux *aiurdemux = GST_AIURDEMUX (object);

  if (aiurdemux->stream_cache) {
    gst_mini_object_unref (GST_MINI_OBJECT_CAST (aiurdemux->stream_cache));
    aiurdemux->stream_cache = NULL;
  }

  if (aiurdemux->runmutex) {
    g_mutex_free (aiurdemux->runmutex);
  }

  PRINT_FINALIZE ("aiurdemux");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_aiurdemux_post_no_playable_stream_error (GstAiurDemux * aiurdemux)
{
}


static const GstQueryType *
gst_aiurdemux_get_src_query_types (GstPad * pad)
{
  static const GstQueryType src_types[] = {
    GST_QUERY_DURATION,
    GST_QUERY_SEEKING,
    0
  };
  return src_types;
}


static gboolean
gst_aiurdemux_get_duration (GstAiurDemux * aiurdemux, gint64 * duration)
{
  gboolean res = TRUE;

  *duration = GST_CLOCK_TIME_NONE;

  if (aiurdemux->clip_info.duration != 0) {
    if (aiurdemux->clip_info.duration != G_MAXINT64) {
      *duration = aiurdemux->clip_info.duration;
    }
  }
  return res;
}


static gboolean
gst_aiurdemux_handle_src_query (GstPad * pad, GstQuery * query)
{
  gboolean res = FALSE;
  GstAiurDemux *aiurdemux = GST_AIURDEMUX (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (pad, "%s query", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {

    case GST_QUERY_DURATION:{
      GstFormat fmt;

      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gint64 duration = -1;

        gst_aiurdemux_get_duration (aiurdemux, &duration);
        if (duration > 0) {
          gst_query_set_duration (query, GST_FORMAT_TIME, duration);
          res = TRUE;
        }
      }
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gboolean seekable = FALSE;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gint64 duration = -1;

        gst_aiurdemux_get_duration (aiurdemux, &duration);

        if ((aiurdemux->content_info.seekable)
            && (aiurdemux->clip_info.seekable)) {
          seekable = TRUE;
        }

        gst_query_set_seeking (query, GST_FORMAT_TIME, seekable, 0, duration);
        res = TRUE;
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, query);
      break;
  }

  gst_object_unref (aiurdemux);

  return res;
}

static void
gst_aiurdemux_push_tags (GstAiurDemux * aiurdemux, AiurDemuxStream * stream)
{
  if (G_LIKELY (stream->pad)) {
    GST_DEBUG_OBJECT (aiurdemux, "Checking pad %s:%s for tags",
        GST_DEBUG_PAD_NAME (stream->pad));

    if (G_UNLIKELY (stream->pending_tags)) {

      gst_element_found_tags_for_pad (GST_ELEMENT_CAST (aiurdemux),
          stream->pad, stream->pending_tags);
      stream->pending_tags = NULL;
    }

    if (aiurdemux->tag_list) {
      GST_DEBUG_OBJECT (aiurdemux,
          "Sending global tags %" GST_PTR_FORMAT, aiurdemux->tag_list);
      gst_element_found_tags (GST_ELEMENT (aiurdemux), aiurdemux->tag_list);

      aiurdemux->tag_list = NULL;
    }
  }
}

/* push event on all source pads; takes ownership of the event */
static void
gst_aiurdemux_push_event (GstAiurDemux * aiurdemux, GstEvent * event)
{
  gint n;
  gboolean pushed_sucessfully = FALSE;
  GstEventType etype = GST_EVENT_TYPE (event);

  for (n = 0; n < aiurdemux->n_streams; n++) {
    GstPad *pad;

    if (pad = aiurdemux->streams[n]->pad) {
      if (gst_pad_push_event (pad, gst_event_ref (event))) {
        pushed_sucessfully = TRUE;
      }
    }

  }
  gst_event_unref (event);
}


static void
aiurdemux_send_stream_newsegment (GstAiurDemux * demux,
    AiurDemuxStream * stream)
{

  if (demux->segment.rate >= 0) {

    if (stream->buffer) {

      if ((GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (stream->buffer)))
          && (GST_BUFFER_TIMESTAMP (stream->buffer) > stream->time_position)) {
        GST_WARNING ("Timestamp unexpect, maybe a core parser bug!");
        if (demux->n_video_streams == 0) {
          stream->time_position = GST_BUFFER_TIMESTAMP (stream->buffer);
        }
      } else {
        //GST_BUFFER_TIMESTAMP(stream->buffer) = stream->time_position;
      }

      GST_WARNING ("Pad %s: Send newseg %" GST_TIME_FORMAT " first buffer %"
          GST_TIME_FORMAT " ", AIUR_MEDIATYPE2STR (stream->type),
          GST_TIME_ARGS (stream->time_position),
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (stream->buffer)));

    }
    gst_pad_push_event (stream->pad,
        gst_event_new_new_segment (FALSE, demux->segment.rate,
            GST_FORMAT_TIME,
            stream->time_position, GST_CLOCK_TIME_NONE, stream->time_position));
  } else {
    if (stream->buffer) {
      GST_WARNING ("Pad %s: Send newseg %" GST_TIME_FORMAT " first buffer %"
          GST_TIME_FORMAT " ", AIUR_MEDIATYPE2STR (stream->type),
          GST_TIME_ARGS (stream->time_position),
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (stream->buffer)));
    }
    gst_pad_push_event (stream->pad,
        gst_event_new_new_segment (FALSE, demux->segment.rate,
            GST_FORMAT_TIME, (gint64) 0, stream->time_position, (gint64) 0));
  }
  stream->new_segment = FALSE;
  demux->new_segment_mask &= (~(stream->mask));

}

static GstFlowReturn
aiurdemux_send_stream_eos (GstAiurDemux * demux, AiurDemuxStream * stream)
{
  GstFlowReturn ret = GST_FLOW_OK;

  if (stream) {
    if (stream->new_segment) {
      aiurdemux_send_stream_newsegment (demux, stream);
    }

    ret = gst_pad_push_event (stream->pad, gst_event_new_eos ());

    stream->valid = FALSE;
    demux->valid_mask &= (~stream->mask);

    GST_WARNING ("Pad %s: Send eos. ", AIUR_MEDIATYPE2STR (stream->type));
  }

  return ret;
}


static void
aiurdemux_send_pending_events (GstAiurDemux * demux)
{
  guint n;

  for (n = 0; n < demux->n_streams; n++) {
    AiurDemuxStream *stream = demux->streams[n];

    if (stream->pending_eos) {
      aiurdemux_send_stream_eos (demux, stream);
    }
  }
}


static gboolean
gst_aiurdemux_convert_seek (GstPad * pad, GstFormat * format,
    GstSeekType cur_type, gint64 * cur, GstSeekType stop_type, gint64 * stop)
{
  gboolean res;
  GstFormat fmt;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (cur != NULL, FALSE);
  g_return_val_if_fail (stop != NULL, FALSE);

  if (*format == GST_FORMAT_TIME)
    return TRUE;

  fmt = GST_FORMAT_TIME;
  res = TRUE;
  if (cur_type != GST_SEEK_TYPE_NONE)
    res = gst_pad_query_convert (pad, *format, *cur, &fmt, cur);
  if (res && stop_type != GST_SEEK_TYPE_NONE)
    res = gst_pad_query_convert (pad, *format, *stop, &fmt, stop);

  if (res)
    *format = GST_FORMAT_TIME;

  return res;
}


static void
aiurdemux_reset_stream (GstAiurDemux * demux, AiurDemuxStream * stream)
{
  stream->valid = TRUE;
  stream->new_segment = TRUE;
  stream->last_ret = GST_FLOW_OK;
  stream->last_stop = 0;
  stream->last_start = GST_CLOCK_TIME_NONE;
  stream->preroll_size = 0;
  stream->pending_eos = FALSE;

  if (stream->buffer) {
    MM_UNREGRES (stream->buffer, RES_GSTBUFFER);
    gst_buffer_unref (stream->buffer);
    stream->buffer = NULL;
  }

  if (stream->post_processor.flush) {
    stream->post_processor.flush (demux, stream);
  }

  if (stream->adapter) {
    gst_adapter_clear (stream->adapter);
  }

  if (stream->buf_queue) {
    AIUR_STREAM_FLUSH_INTERLEAVE_QUEUE (stream);
  }
  stream->buf_queue_size = 0;

  AIUR_RESET_SAMPLE_STAT (stream->sample_stat);

  demux->new_segment_mask |= stream->mask;
  demux->valid_mask |= stream->mask;

  stream->num_of_samples = demux->config.num_of_samples;
}


/* perform the seek.
 * Called with STREAM_LOCK
 */
#define MARK_STREAM_EOS(demux, stream)\
  do {\
    (demux)->new_segment_mask &= (~((stream)->mask));\
    (stream)->valid = FALSE;\
    (stream)->pending_eos = TRUE;\
    (demux)->pending_event = TRUE;\
  }while(0)

static gboolean
gst_aiurdemux_perform_seek (GstAiurDemux * demux, GstSegment * segment,
    gint accurate)
{
  gint64 desired_offset;
  gint n;
  int32 core_ret = 0;
  gdouble rate = segment->rate;

  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  if (rate >= 0) {

    demux->play_mode = AIUR_PLAY_MODE_NORMAL;
    if ((rate > demux->config.max_normal_rate)
        && (((demux->clip_info.read_mode == PARSER_READ_MODE_FILE_BASED)
                && (CORE_API_EXIST (inf, getFileNextSyncSample)))
            || ((demux->clip_info.read_mode == PARSER_READ_MODE_TRACK_BASED)
                && (CORE_API_EXIST (inf, getNextSyncSample))))) {
      demux->play_mode = AIUR_PLAY_MODE_TRICK_FORWARD;
    }
    desired_offset = segment->start;

  } else if (rate < 0) {
    if (((demux->clip_info.read_mode == PARSER_READ_MODE_FILE_BASED)
            && (CORE_API_EXIST (inf, getFileNextSyncSample)))
        || ((demux->clip_info.read_mode == PARSER_READ_MODE_TRACK_BASED)
            && (CORE_API_EXIST (inf, getNextSyncSample)))) {
      demux->play_mode = AIUR_PLAY_MODE_TRICK_BACKWARD;
      desired_offset = segment->stop;
    } else {
      return FALSE;
    }
  }

  GST_WARNING ("Seek to %" GST_TIME_FORMAT ".", GST_TIME_ARGS (desired_offset));

  demux->pending_event = FALSE;

  demux->new_segment_mask = 0;
  demux->valid_mask = 0;

  if ((accurate) || (demux->n_video_streams > 1)
      || (demux->n_video_streams == 0)) {
    /* and set all streams to the final position */
    for (n = 0; n < demux->n_streams; n++) {
      AiurDemuxStream *stream = demux->streams[n];
      guint64 usSeekTime = AIUR_GSTTS_2_CORETS (desired_offset);

      aiurdemux_reset_stream (demux, stream);

      CORE_API (inf, seek,, core_ret, handle, stream->track_idx,
          &usSeekTime, SEEK_FLAG_NO_LATER);



      stream->time_position = desired_offset;

      if ((rate >= 0) && (stream->type == MEDIA_AUDIO)
          && (demux->n_video_streams))
        stream->block = TRUE;
      else
        stream->block = FALSE;

      if ((core_ret != PARSER_SUCCESS)
          || ((demux->play_mode != AIUR_PLAY_MODE_NORMAL)
              && (stream->type == MEDIA_AUDIO))) {
        MARK_STREAM_EOS (demux, stream);
      }
    }

  } else {
    guint64 usSeekTime = AIUR_GSTTS_2_CORETS (desired_offset);
    core_ret = PARSER_SUCCESS;
    AiurDemuxStream *stream = NULL;
    for (n = 0; n < demux->n_streams; n++) {
      if (demux->streams[n]->type == MEDIA_VIDEO) {
        stream = demux->streams[n];
        break;
      }
    }

    if (stream) {
      aiurdemux_reset_stream (demux, stream);

      CORE_API (inf, seek,, core_ret, handle, stream->track_idx,
          &usSeekTime, SEEK_FLAG_NO_LATER);
      GST_INFO ("Video seek return %d time %" GST_TIME_FORMAT, core_ret,
          GST_TIME_ARGS (AIUR_CORETS_2_GSTTS (usSeekTime)));
    }
    if (core_ret != PARSER_SUCCESS) {
      MARK_STREAM_EOS (demux, stream);
    } else {
      desired_offset = AIUR_CORETS_2_GSTTS (usSeekTime);
    }
    stream->time_position = desired_offset;

    for (n = 0; n < demux->n_streams; n++) {
      stream = demux->streams[n];
      usSeekTime = AIUR_GSTTS_2_CORETS (desired_offset);


      if (stream->type != MEDIA_VIDEO) {
        aiurdemux_reset_stream (demux, stream);
        if (core_ret == PARSER_SUCCESS) {
          CORE_API (inf, seek,, core_ret, handle, stream->track_idx,
              &usSeekTime, SEEK_FLAG_NO_LATER);
          if (stream->post_processor.flush) {
            stream->post_processor.flush (demux, stream);
          }

          stream->time_position = desired_offset;

          if ((rate >= 0) && (stream->type == MEDIA_AUDIO)
              && (demux->n_video_streams))
            stream->block = TRUE;
          else
            stream->block = FALSE;

          if ((core_ret != PARSER_SUCCESS)
              || ((demux->play_mode != AIUR_PLAY_MODE_NORMAL)
                  && (stream->type == MEDIA_AUDIO))) {
            MARK_STREAM_EOS (demux, stream);
          }

          core_ret = PARSER_SUCCESS;
        } else {
          MARK_STREAM_EOS (demux, stream);
        }

      }


    }
  }
bail:
  segment->last_stop = desired_offset;
  segment->time = desired_offset;

  return TRUE;
}


/* do a seek in push based mode */
static gboolean
aiurdemux_do_push_seek (GstAiurDemux * aiurdemux, GstPad * pad,
    GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  gboolean flush;
  gboolean update;
  GstSegment seeksegment;
  int i;
  gboolean ret = FALSE;


  if (event) {
    GST_DEBUG_OBJECT (aiurdemux, "doing seek with event");

    gst_event_parse_seek (event, &rate, &format, &flags,
        &cur_type, &cur, &stop_type, &stop);

    /* we have to have a format as the segment format. Try to convert
     * if not. */
    if (!gst_aiurdemux_convert_seek (pad, &format, cur_type, &cur,
            stop_type, &stop)) {
      goto no_format;
    }
    if (stop == (gint64) 0) {
      stop = (gint64) - 1;
    }
  } else {
    GST_DEBUG_OBJECT (aiurdemux, "doing seek without event");
    flags = 0;
  }

  flush = flags & GST_SEEK_FLAG_FLUSH;

  /* stop streaming, either by flushing or by pausing the task */
  if (flush) {
    gst_aiurdemux_push_event (aiurdemux, gst_event_new_flush_start ());
  }

  aiurdemux->running = FALSE;
  gst_aiur_stream_cache_close (aiurdemux->stream_cache);

  /* wait for streaming to finish */
  g_mutex_lock (aiurdemux->runmutex);

  gst_aiur_stream_cache_open (aiurdemux->stream_cache);

  memcpy (&seeksegment, &aiurdemux->segment, sizeof (GstSegment));

  if (event) {
    gst_segment_set_seek (&seeksegment, rate, format, flags,
        cur_type, cur, stop_type, stop, &update);
  }

  /* now do the seek, this actually never returns FALSE */
  ret =
      gst_aiurdemux_perform_seek (aiurdemux, &seeksegment,
      (flags & GST_SEEK_FLAG_ACCURATE));

  /* prepare for streaming again */
  if (flush) {
    gst_aiurdemux_push_event (aiurdemux, gst_event_new_flush_stop ());
  }

  /* commit the new segment */
  memcpy (&aiurdemux->segment, &seeksegment, sizeof (GstSegment));


  aiurdemux->running = TRUE;
  gst_aiur_stream_cache_open (aiurdemux->stream_cache);
  for (i = 0; i < aiurdemux->n_streams; i++)
    aiurdemux->streams[i]->last_ret = GST_FLOW_OK;


  g_thread_create ((GThreadFunc) aiurdemux_loop_push, (gpointer) aiurdemux,
      FALSE, NULL);
  g_mutex_unlock (aiurdemux->runmutex);

  return ret;

  /* ERRORS */
no_format:
  {
    GST_DEBUG_OBJECT (aiurdemux, "unsupported format given, seek aborted.");
    return ret;
  }
}


/* do a seek in pull based mode */
static gboolean
aiurdemux_do_seek (GstAiurDemux * aiurdemux, GstPad * pad, GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  gboolean flush;
  gboolean update;
  GstSegment seeksegment;
  int i;
  gboolean ret = FALSE;

  if (event) {
    GST_DEBUG_OBJECT (aiurdemux, "doing seek with event");

    gst_event_parse_seek (event, &rate, &format, &flags,
        &cur_type, &cur, &stop_type, &stop);

    if (stop == (gint64) 0) {
      stop = (gint64) - 1;
    }

    /* we have to have a format as the segment format. Try to convert
     * if not. */
    if (!gst_aiurdemux_convert_seek (pad, &format, cur_type, &cur,
            stop_type, &stop))
      goto no_format;

    GST_DEBUG_OBJECT (aiurdemux, "seek format %s",
        gst_format_get_name (format));
  } else {
    GST_DEBUG_OBJECT (aiurdemux, "doing seek without event");
    flags = 0;
  }

  flush = flags & GST_SEEK_FLAG_FLUSH;

  /* stop streaming by pausing the task */
  if (flush) {
    gst_aiurdemux_push_event (aiurdemux, gst_event_new_flush_start ());
  }

  gst_pad_pause_task (aiurdemux->sinkpad);

  /* wait for streaming to finish */
  GST_PAD_STREAM_LOCK (aiurdemux->sinkpad);

  /* copy segment, we need this because we still need the old
   * segment when we close the current segment. */
  memcpy (&seeksegment, &aiurdemux->segment, sizeof (GstSegment));

  if (event) {
    gst_segment_set_seek (&seeksegment, rate, format, flags,
        cur_type, cur, stop_type, stop, &update);
  }

  if (flush) {
    gst_aiurdemux_push_event (aiurdemux, gst_event_new_flush_stop ());
  }

  /* now do the seek, this actually never returns FALSE */
  ret =
      gst_aiurdemux_perform_seek (aiurdemux, &seeksegment,
      (flags & GST_SEEK_FLAG_ACCURATE));

  /* commit the new segment */
  memcpy (&aiurdemux->segment, &seeksegment, sizeof (GstSegment));


  /* restart streaming, NEWSEGMENT will be sent from the streaming
   * thread. */
  for (i = 0; i < aiurdemux->n_streams; i++)
    aiurdemux->streams[i]->last_ret = GST_FLOW_OK;

  gst_pad_start_task (aiurdemux->sinkpad,
      (GstTaskFunction) aiurdemux_pull_task, aiurdemux->sinkpad);

  GST_PAD_STREAM_UNLOCK (aiurdemux->sinkpad);

  return ret;

  /* ERRORS */
no_format:
  {
    GST_DEBUG_OBJECT (aiurdemux, "unsupported format given, seek aborted.");
    return ret;
  }
}

static gboolean
gst_aiurdemux_handle_src_event (GstPad * pad, GstEvent * event)
{
  gboolean res = TRUE;
  GstAiurDemux *aiurdemux = GST_AIURDEMUX (gst_pad_get_parent (pad));
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if ((aiurdemux->clip_info.live) || (!aiurdemux->clip_info.seekable)) {
        goto not_support;
      }

      if ((aiurdemux->state == AIURDEMUX_STATE_MOVIE) && aiurdemux->n_streams) {
        if (aiurdemux->pullbased) {
          res = aiurdemux_do_seek (aiurdemux, pad, event);
        } else {
          res = aiurdemux_do_push_seek (aiurdemux, pad, event);
        }
      } else {
        GST_DEBUG_OBJECT (aiurdemux,
            "ignoring seek in push mode in current state");
        res = FALSE;
      }
      gst_event_unref (event);
      break;

    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
      res = FALSE;
      gst_event_unref (event);
      break;
    default:
      res = gst_pad_event_default (pad, event);
      break;
  }
  gst_object_unref (aiurdemux);

  return res;

  /* ERRORS */
not_support:
  {
    gst_object_unref (aiurdemux);
    GST_WARNING ("Unsupport source event %s. ", GST_EVENT_TYPE_NAME (event));
    gst_event_unref (event);
    return FALSE;
  }
}


static gboolean
gst_aiurdemux_handle_sink_event (GstPad * sinkpad, GstEvent * event)
{
  GstAiurDemux *demux = GST_AIURDEMUX (GST_PAD_PARENT (sinkpad));
  gboolean res;


  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gdouble rate, arate;
      gint64 start, stop, time, offset = 0;
      AiurDemuxStream *stream;
      gint idx;
      gboolean update;
      GstSegment segment;

      /* some debug output */
      gst_segment_init (&segment, GST_FORMAT_UNDEFINED);
      gst_event_parse_new_segment_full (event, &update, &rate, &arate,
          &format, &start, &stop, &time);
      gst_segment_set_newsegment_full (&segment, update, rate, arate,
          format, start, stop, time);


      /* we only expect a BYTE segment, e.g. following a seek */
      if (format == GST_FORMAT_BYTES) {
        if (demux->pullbased == FALSE) {
          gst_aiur_stream_cache_set_segment (demux->stream_cache, start, stop);
        }
      } else if (format == GST_FORMAT_TIME) {
        GST_LOG_OBJECT (demux, "handling new segment from%lld", start);
        goto exit;
      } else {
        GST_DEBUG_OBJECT (demux, "unsupported segment format, ignoring");
        goto exit;
      }

      GST_DEBUG_OBJECT (demux,
          "Pushing newseg update %d, rate %g, "
          "applied rate %g, format %d, start %"
          GST_TIME_FORMAT ", " "stop %" GST_TIME_FORMAT,
          update, rate, arate, GST_FORMAT_TIME,
          GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

      if (stop < start) {
        gst_event_unref (event);
        return FALSE;
      }

      /* clear leftover in current segment, if any */
    exit:
      gst_event_unref (event);
      res = TRUE;
      goto drop;
      break;
    }

    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    {
      gint i;

      /* reset flow return, e.g. following seek */
      for (i = 0; i < demux->n_streams; i++) {
        demux->streams[i]->last_ret = GST_FLOW_OK;
      }

      gst_event_unref (event);
      res = TRUE;
      goto drop;
      break;
      break;
    }
    case GST_EVENT_EOS:
      /* If we are in push mode, and get an EOS before we've seen any streams,
       * then error out - we have nowhere to send the EOS */
      if (demux->pullbased) {
        gint i;
        gboolean has_valid_stream = FALSE;
        for (i = 0; i < demux->n_streams; i++) {
          if (demux->streams[i]->pad != NULL) {
            has_valid_stream = TRUE;
            break;
          }
        }
        if (!has_valid_stream)
          gst_aiurdemux_post_no_playable_stream_error (demux);
      } else {
        gst_aiur_stream_cache_seteos (demux->stream_cache, TRUE);
        gst_event_unref (event);
        goto drop;
      }
      break;
    default:
      break;
  }

  res = gst_pad_event_default (demux->sinkpad, event);

drop:
  return res;
}



static GstFlowReturn
gst_aiurdemux_close_core (GstAiurDemux * demux)
{
  int32 core_ret = PARSER_SUCCESS;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  if (inf) {
    if (handle) {

      if ((demux->config.export_index) && (demux->content_info.index_file)
          && (inf->coreid) && (strlen (inf->coreid))) {
        uint32 size = 0;
        AiurIndexTable *itab;

        if (CORE_API_EXIST (inf, exportIndex)) {
          CORE_API (inf, exportIndex,, core_ret, handle, NULL, &size);

          if ((core_ret != PARSER_SUCCESS) && (size < 0)
              || (size > AIUR_IDX_TABLE_MAX_SIZE)) {
            size = 0;
          }
        }
        itab = aiurdemux_create_idx_table (size, inf->coreid);
        if (itab) {
          itab->info.readmode = demux->clip_info.suggest_read_mode;
          if (size) {
            CORE_API (inf, exportIndex,, core_ret, handle, itab->idx, &size);
            if (core_ret != PARSER_SUCCESS) {
              size = 0;
            }
          }
          core_ret =
              aiurdemux_export_idx_table (demux->content_info.index_file, itab);
          if (core_ret == 0)
            GST_INFO ("Index table %s[size:%d] exported.",
                demux->content_info.index_file, size);
          aiurdemux_destroy_idx_table (itab);
        }

      }


      CORE_API (inf, deleteParser,, core_ret, handle);
    }
    aiur_core_destroy_interface (inf);
    demux->core_interface = NULL;
  }
  if (demux->content_info.uri) {
    g_free (demux->content_info.uri);
    demux->content_info.uri = NULL;
  }
  if (demux->content_info.index_file) {
    g_free (demux->content_info.index_file);
    demux->content_info.index_file = NULL;
  }
  MM_DEINIT_DBG_MEM ();

}

static void
aiurdemux_release_resource (GstAiurDemux * demux)
{
  int n;

  if (demux->streams) {
    for (n = 0; n < demux->n_streams; n++) {
      AiurDemuxStream *stream = demux->streams[n];

      if (stream->pad) {
        gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);
        stream->pad = NULL;
      }
      if (stream->caps) {
        gst_caps_unref (stream->caps);
        stream->caps = NULL;
      }
      if (stream->pending_tags) {
        gst_tag_list_free (stream->pending_tags);
        stream->pending_tags = NULL;
      }
      if (stream->buffer) {
        MM_UNREGRES (stream->buffer, RES_GSTBUFFER);
        gst_buffer_unref (stream->buffer);
        stream->buffer = NULL;
      }

      if (stream->adapter) {
        gst_adapter_clear (stream->adapter);
        g_object_unref (stream->adapter);
        stream->adapter = NULL;
      }

      if (stream->buf_queue) {
        GST_INFO ("stream %s[%d] interleave queue max size %d",
            AIUR_MEDIATYPE2STR (stream->type), stream->track_idx,
            stream->buf_queue_size_max);
        AIUR_STREAM_FLUSH_INTERLEAVE_QUEUE (stream);
        g_queue_free (stream->buf_queue);
        stream->buf_queue = NULL;
      }

      if (stream->post_processor.finalize) {
        stream->post_processor.finalize (demux, stream);
      }

      g_free (stream);
    }
    g_free (demux->streams);
    demux->streams = NULL;
  }
  if (demux->programs) {
    for (n = 0; n < demux->clip_info.program_num; n++) {
      AiurDemuxProgram *program = demux->programs[n];
      if (program) {
        g_free (program);
      }
    }
    g_free (demux->programs);
    demux->programs = NULL;
  }

}


static GstStateChangeReturn
gst_aiurdemux_change_state (GstElement * element, GstStateChange transition)
{
  GstAiurDemux *demux = GST_AIURDEMUX (element);
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      MM_INIT_DBG_MEM ("aiurdemux");
      demux->discont_check_track = -1;
      demux->tag_list = gst_tag_list_new ();
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      gint n;

      demux->state = AIURDEMUX_STATE_PROBE;
      demux->pullbased = FALSE;
      if (demux->tag_list)
        gst_tag_list_free (demux->tag_list);

      aiurdemux_release_resource (demux);

      gst_aiurdemux_close_core (demux);

      demux->new_segment_mask = 0;
      demux->valid_mask = 0;
      demux->n_streams = 0;
      demux->n_video_streams = 0;
      demux->n_audio_streams = 0;
      demux->n_sub_streams = 0;
      demux->play_mode = AIUR_PLAY_MODE_NORMAL;
      demux->clip_info.live = FALSE;
      demux->clip_info.suggest_read_mode = AIUR_READMODE_NULL;
      memset (&demux->clip_info, 0, sizeof (AiurDemuxClipInfo));

      gst_segment_init (&demux->segment, GST_FORMAT_TIME);
      break;
    }

    default:
      break;
  }

  return result;
}

static GstFlowReturn
aiurdemux_loop_state_probe (GstAiurDemux * aiurdemux)
{
  GstBuffer *buffer;
  GstFlowReturn ret = GST_FLOW_OK;

  if (aiurdemux->pullbased) {

    gst_pad_pull_range (aiurdemux->sinkpad, (guint64) 0,
        AIURDEMUX_INIT_BLOCK_SIZE, &buffer);
    gst_buffer_unref (buffer);
  }

  return ret;
}



static gboolean
aiurdemux_is_protocol (gchar * uri, AiurDemuxProtocolEntry * entry)
{
  gchar *protocol;
  gboolean ret = FALSE;
  if ((uri) && (protocol = gst_uri_get_protocol (uri))) {
    AiurDemuxProtocolEntry *live_protocol = entry;
    while (live_protocol->protocol) {
      if (strcmp (protocol, live_protocol->protocol) == 0) {
        ret = TRUE;
        break;
      }
      live_protocol++;
    }
    g_free (protocol);
  }
  return ret;
}


static void
aiurdemux_print_content_info (GstAiurDemux * demux)
{
  AiurDemuxContentInfo *content_info = &demux->content_info;
  AiurDemuxStream *stream;
  int i;

  g_print (BLUE_STR ("Content Info:\n"));

  if (content_info->uri) {
    aiurdemux_pretty_print_info ("URI", content_info->uri, 80);
  }
  if (content_info->index_file)
    aiurdemux_pretty_print_info ("Idx File", content_info->index_file, 80);
  g_print (BLUE_STR ("\tSeekable  : %s\n",
          (content_info->seekable ? "Yes" : "No")));
  g_print (BLUE_STR ("\tSize(byte): %lld\n\n", content_info->length));

}


static void
aiurdemux_query_content_info (GstAiurDemux * demux)
{
  gchar *prefix;
  GstPad *pad = demux->sinkpad;

  gst_object_ref (GST_OBJECT_CAST (pad));

  GstQuery *q;
  GstFormat fmt;

  q = gst_query_new_uri ();
  if (gst_pad_peer_query (demux->sinkpad, q)) {
    gchar *uri;
    gst_query_parse_uri (q, &uri);
    if (uri) {
      demux->content_info.uri = g_uri_unescape_string (uri, NULL);
      g_free (uri);
    }
  }
  gst_query_unref (q);

  q = gst_query_new_seeking (GST_FORMAT_BYTES);
  if (gst_pad_peer_query (pad, q)) {
    gst_query_parse_seeking (q, &fmt, &(demux->content_info.seekable), NULL,
        NULL);
  }
  gst_query_unref (q);

  demux->content_info.length = -1;
  q = gst_query_new_duration (GST_FORMAT_BYTES);
  if (gst_pad_peer_query (pad, q)) {
    gint64 duration;
    gst_query_parse_duration (q, &fmt, &(demux->content_info.length));
  }
  gst_query_unref (q);


  if ((prefix = demux->config.index_file_prefix) == NULL) {
    if ((prefix = getenv ("HOME")) == NULL)
      prefix = "";

    prefix = g_strdup_printf ("%s/.aiur", prefix);
  } else {
    prefix = g_strdup (demux->config.index_file_prefix);
  }


  demux->content_info.index_file =
      aiurdemux_generate_idx_file_location (demux, prefix);

  if (demux->content_info.index_file) {
    umask (0);
    mkdir (prefix, 0777);
  }
  g_free (prefix);

  aiurdemux_print_content_info (demux);
}


static GstFlowReturn
aiurdemux_loop_state_init (GstAiurDemux * demux)
{
  GstBuffer *buffer;
  GstFlowReturn ret = GST_FLOW_ERROR;
  int32 core_ret = 0;


  AiurCoreInterface *inf = demux->core_interface;

  if (inf == NULL)
    return GST_FLOW_OK;

  FslParserHandle handle = NULL;
  FslFileStream *file_cbks = g_new0 (FslFileStream, 1);
  ParserMemoryOps *mem_cbks = g_new0 (ParserMemoryOps, 1);
  ParserOutputBufferOps *buf_cbks = g_new0 (ParserOutputBufferOps, 1);

  if ((!file_cbks) || (!mem_cbks) || (!buf_cbks))
    goto fail;

  if (demux->pullbased) {

    file_cbks->Open = aiurdemux_callback_open_pull;
    file_cbks->Read = aiurdemux_callback_read_pull;
    file_cbks->Seek = aiurdemux_callback_seek_pull;
    file_cbks->Tell = aiurdemux_callback_tell_pull;
    file_cbks->Size = aiurdemux_callback_size_pull;
    file_cbks->Close = aiurdemux_callback_close_pull;

    file_cbks->CheckAvailableBytes = aiurdemux_callback_availiable_bytes_pull;

  } else {
    file_cbks->Open = aiurdemux_callback_open_push;
    file_cbks->Read = aiurdemux_callback_read_push;
    file_cbks->Seek = aiurdemux_callback_seek_push;
    file_cbks->Tell = aiurdemux_callback_tell_push;
    file_cbks->Size = aiurdemux_callback_size_push;
    file_cbks->Close = aiurdemux_callback_close_push;

    file_cbks->CheckAvailableBytes = aiurdemux_callback_availiable_bytes_push;
  }

  mem_cbks->Calloc = aiurdemux_callback_calloc;
  mem_cbks->Malloc = aiurdemux_callback_malloc;
  mem_cbks->Free = aiurdemux_callback_free;
  mem_cbks->ReAlloc = aiurdemux_callback_realloc;


  buf_cbks->RequestBuffer = aiurdemux_callback_request_buffer;
  buf_cbks->ReleaseBuffer = aiurdemux_callback_release_buffer;


  aiurdemux_query_content_info (demux);

  demux->clip_info.live = ((AIUR_PROTOCOL_IS_LIVE (demux->content_info.uri))
      || (!demux->content_info.seekable));

  if ((AIUR_PROTOCOL_IS_LIVE (demux->content_info.uri) || demux->clip_info.live)
      && (demux->config.retimestamp_threashold_ms)) {
    demux->clip_info.auto_retimestamp = TRUE;
  } else {
    demux->clip_info.auto_retimestamp = FALSE;
  }

  CORE_API (inf, createParser, goto fail, core_ret,
      (bool) (demux->clip_info.live), file_cbks, mem_cbks, buf_cbks,
      (void *) demux, &handle);

  if (CORE_API_FAILED (core_ret)) {
    goto fail;
  }


  demux->core_handle = handle;
  demux->state = AIURDEMUX_STATE_HEADER;
  ret = GST_FLOW_OK;

  g_free (file_cbks);
  g_free (mem_cbks);
  g_free (buf_cbks);

  return ret;
fail:

  if (file_cbks) {
    g_free (file_cbks);
  }
  if (mem_cbks) {
    g_free (mem_cbks);
  }
  if (buf_cbks) {
    g_free (buf_cbks);
  }
  if (handle) {
    CORE_API (inf, deleteParser,, core_ret, handle);
  }
  return ret;
}


static GstTagList *
aiurdemux_add_user_tags (GstAiurDemux * demux)
{
  int32 core_ret = 0;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  GstTagList *list = demux->tag_list;

  int i;

  if (list == NULL)
    return list;

  if (CORE_API_EXIST (inf, getMetaData)) {
    UserDataID id;
    UserDataFormat format;

    for (i = 0; i < G_N_ELEMENTS (g_user_data_entry); i++) {
      uint8 *userData = NULL;
      uint32 userDataSize = 0;

      id = g_user_data_entry[i].core_tag;
      format = g_user_data_entry[i].format;
      CORE_API (inf, getMetaData, break, core_ret, handle,
          id, &format, &userData, &userDataSize);
      if ((core_ret == PARSER_SUCCESS) &&
          (userData != NULL) && (userDataSize > 0)) {
        if (USER_DATA_FORMAT_UTF8 == format) {
          GString *string = g_string_new_len (userData, userDataSize);
          if (string) {
            /* FIXME : create GDate object for GST_TAG_DATA */
            if (USER_DATA_CREATION_DATE == id) {
              g_string_free (string, TRUE);
              continue;
            }

            gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
                g_user_data_entry[i].gst_tag_name, string->str, NULL);
            GST_INFO (g_user_data_entry[i].print_string, string->str);
            g_string_free (string, TRUE);
          }
        } else if ((USER_DATA_FORMAT_JPEG == format) ||
            (USER_DATA_FORMAT_PNG == format) ||
            (USER_DATA_FORMAT_BMP == format) ||
            (USER_DATA_FORMAT_GIF == format)) {
          GstBuffer *buffer = gst_tag_image_data_to_image_buffer (userData,
              userDataSize, GST_TAG_IMAGE_TYPE_UNDEFINED);
          if (buffer) {
            GST_INFO (g_user_data_entry[i].print_string,
                GST_BUFFER_CAPS (buffer), userDataSize);

            gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
                g_user_data_entry[i].gst_tag_name, buffer, NULL);
            gst_buffer_unref (buffer);
          }
        }
      }
    }
  } else if (CORE_API_EXIST (inf, getUserData)) {

    for (i = 0; i < G_N_ELEMENTS (g_user_data_entry); i++) {
      uint16 *userData = NULL;
      uint32 userDataSize = 0;

      CORE_API (inf, getUserData, break, core_ret, handle,
          g_user_data_entry[i].core_tag, &userData, &userDataSize);
      if (core_ret == PARSER_SUCCESS) {
        if ((userData) && (userDataSize)) {
          gsize in, out;
          gchar *value_utf8;
          value_utf8 =
              g_convert ((uint8 *) userData, userDataSize * 2, "UTF-8",
              "UTF-16LE", &in, &out, NULL);
          if (value_utf8) {
            gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
                g_user_data_entry[i].gst_tag_name, value_utf8, NULL);
            g_free (value_utf8);
          }
        }
      }
    }
  }


  if (gst_tag_list_is_empty (list)) {
    gst_tag_list_free (list);
    list = NULL;
  }

  return list;
}


static void
aiurdemux_make_discont (GstAiurDemux * demux, GstClockTime ts)
{
  int i;
  AiurDemuxStream *stream;

#if 0
  for (i = 0; i < demux->n_streams; i++) {
    stream = demux->streams[i];
    if (stream) {
      stream->time_position = ts;
      stream->new_segment = TRUE;

    }

  }
#else

  demux->base_offset += (ts);


#endif


}

static void
aiurdemux_check_discont (GstAiurDemux * demux, AiurDemuxStream * stream)
{
  if ((demux->clip_info.auto_retimestamp)
      && (GST_CLOCK_TIME_IS_VALID (stream->sample_stat.start))
      && (1) /*(stream->new_segment == FALSE) */ &&(1)  /*(stream->type == MEDIA_AUDIO) */
      &&((demux->discont_check_track < 0)
          || (demux->discont_check_track == stream->track_idx))
      && (stream->sample_stat.start > stream->last_stop)
      && (stream->sample_stat.start - stream->last_stop >
          (GST_MSECOND * demux->config.retimestamp_threashold_ms))) {
    //GST_BUFFER_FLAG_SET(gstbuf, GST_BUFFER_FLAG_DISCONT);

    g_print (RED_STR ("Discont detected from %" GST_TIME_FORMAT " to %"
            GST_TIME_FORMAT "\n", GST_TIME_ARGS (stream->last_stop),
            GST_TIME_ARGS (stream->sample_stat.start)));
    demux->discont_check_track = stream->track_idx;
    aiurdemux_make_discont (demux,
        stream->sample_stat.start - stream->last_stop -
        (GST_MSECOND * demux->config.retimestamp_delay_ms));
  }
}

static void
aiurdemux_adjust_timestamp (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstBuffer * buffer)
{

  //g_print("adjust orig %"GST_TIME_FORMAT" base %"GST_TIME_FORMAT" pos %"GST_TIME_FORMAT"\n",
  //     GST_TIME_ARGS(stream->sample_stat.start), GST_TIME_ARGS(demux->base_offset), GST_TIME_ARGS(stream->time_position));

  if ((demux->base_offset == 0)
      || (!GST_CLOCK_TIME_IS_VALID (stream->sample_stat.start))) {
    GST_BUFFER_TIMESTAMP (buffer) = stream->sample_stat.start;
  } else {
    GST_BUFFER_TIMESTAMP (buffer) =
        ((stream->sample_stat.start >=
            demux->base_offset) ? (stream->sample_stat.start -
            demux->base_offset) : 0);

    if ((GST_BUFFER_TIMESTAMP (buffer) < (stream->time_position))) {
      GST_BUFFER_TIMESTAMP (buffer) = stream->time_position;
    }
  }



  GST_BUFFER_DURATION (buffer) = stream->sample_stat.duration;


}


static void
aiurdemux_update_stream_position (GstAiurDemux * demux,
    AiurDemuxStream * stream, GstBuffer * buffer)
{

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer))) {

    stream->last_stop = stream->last_start = GST_BUFFER_TIMESTAMP (buffer);
    stream->last_stop += GST_BUFFER_DURATION (buffer);
    /* sample duration is wrong sometimes, so using the last_start here to
     * compare with clip duration */
    if (demux->n_video_streams > 0) {
      if ((MEDIA_VIDEO == stream->type) &&
          (GST_BUFFER_TIMESTAMP (buffer) > demux->clip_info.duration)) {
        demux->clip_info.duration = GST_BUFFER_TIMESTAMP (buffer);
      }
    } else {
      if (GST_BUFFER_TIMESTAMP (buffer) > demux->clip_info.duration) {
        demux->clip_info.duration = GST_BUFFER_TIMESTAMP (buffer);
      }
    }
  }
}


GstFlowReturn
aiurdemux_plaintext_check (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstBuffer ** gstbuf)
{
  GstBuffer *newbuf;
  if (!g_utf8_validate (GST_BUFFER_DATA (*gstbuf), GST_BUFFER_SIZE (*gstbuf),
          NULL)) {
    gsize in, out;
    gchar *utf8_text =
        g_convert (GST_BUFFER_DATA (*gstbuf), GST_BUFFER_SIZE (*gstbuf),
        "UTF-8", "UTF-16LE", &in, &out, NULL);
    if (utf8_text) {

      newbuf = gst_buffer_new ();
      GST_BUFFER_MALLOCDATA (newbuf) = (guint8 *) utf8_text;
      GST_BUFFER_DATA (newbuf) = (guint8 *) utf8_text;
      GST_BUFFER_SIZE (newbuf) = strlen (utf8_text);
      gst_buffer_copy_metadata (newbuf, *gstbuf,
          GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_FLAGS);
      gst_buffer_unref (*gstbuf);
      *gstbuf = newbuf;
    }
  }
}

static void
aiurdemux_init_post_processor (GstAiurDemux * demux, AiurDemuxStream * stream)
{
#ifdef AIUR_SUB_TEXT_SUPPORT
  if ((stream->type == MEDIA_TEXT) && (stream->codec_type == TXT_TYPE_UNKNOWN)) {
    stream->post_processor.priv = NULL;
    stream->post_processor.process = aiurdemux_plaintext_check;
    stream->post_processor.flush = NULL;
    stream->post_processor.finalize = NULL;
  }
#endif
}

static void
aiurdemux_print_track_info (AiurDemuxStream * stream)
{

  if ((stream) && (stream->pad) && (stream->caps)) {
    gchar *mime = gst_caps_to_string (stream->caps);
    gchar *padname = gst_pad_get_name (stream->pad);
    g_print (BLUE_STR ("Track %02d [%s] Enabled\n", stream->track_idx,
            padname ? padname : ""));
    if (stream->program) {
      g_print (BLUE_STR ("\tppid: %d, ppid %d\n", stream->ppid, stream->pid));
    }
    g_print (BLUE_STR ("\tDuration: %" GST_TIME_FORMAT "\n",
            GST_TIME_ARGS (stream->duration)));
    g_print (BLUE_STR ("\tLanguage: %s\n", stream->lang));
    if (mime) {
      aiurdemux_pretty_print_info ("Mime", mime, 80);
      g_free (mime);
    }
    if (padname) {
      g_free (padname);
    }
  } else {
    g_print (BLUE_STR ("Track %02d [%s]: Disabled\n", stream->track_idx,
            AIUR_MEDIATYPE2STR (stream->type)));
    g_print (BLUE_STR ("\tCodec: %ld, SubCodec: %ld\n",
            stream->codec_type, stream->codec_sub_type));
  }
}

static void
aiurdemux_parse_video (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_num)
{
  gchar *mime = NULL, *codec;
  gchar *padname;

  int32 core_ret = PARSER_SUCCESS;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  CORE_API (inf, getVideoFrameWidth,, core_ret, handle, track_num,
      &stream->info.video.width);
  CORE_API (inf, getVideoFrameHeight,, core_ret, handle, track_num,
      &stream->info.video.height);

  stream->info.video.fps_n = AIURDEMUX_FRAME_N_DEFAULT;
  stream->info.video.fps_d = AIURDEMUX_FRAME_D_DEFAULT;

  CORE_API (inf, getVideoFrameRate, {
      stream->info.video.fps_n = AIURDEMUX_FRAME_N_DEFAULT;
        stream->info.video.fps_d = AIURDEMUX_FRAME_D_DEFAULT;},
      core_ret, handle, track_num,
      &stream->info.video.fps_n, &stream->info.video.fps_d);

  if ((stream->info.video.fps_n == 0)
      || (stream->info.video.fps_d == 0)) {
    stream->info.video.fps_n = AIURDEMUX_FRAME_N_DEFAULT;
    stream->info.video.fps_d = AIURDEMUX_FRAME_D_DEFAULT;
  }
  stream->send_codec_data = TRUE;

  switch (stream->codec_type) {
    case VIDEO_H263:
      mime = "video/x-h263";
      codec = "H.263";
      break;
    case VIDEO_H264:
      mime = "video/x-h264, parsed = (boolean)true";
      codec = "H.264/AVC";
      break;
    case VIDEO_MPEG2:
      //mime = "video/mp2v";
      mime =
          "video/mpeg, systemstream = (boolean)false, parsed = (boolean)true, mpegversion=(int)2";
      codec = "MPEG2";
      break;
    case VIDEO_MPEG4:
      mime = "video/mpeg, mpegversion=(int)4, parsed = (boolean)true";
      codec = "MPEG4";
      break;
    case VIDEO_JPEG:
      mime = "image/jpeg";
      codec = "JPEG";
      break;
    case VIDEO_MJPG:
      switch (stream->codec_sub_type) {
        case VIDEO_MJPEG_FORMAT_A:
          mime = "image/jpeg";
          codec = "Motion JPEG format A";
          break;
        case VIDEO_MJPEG_FORMAT_B:
          mime = "image/jpeg";
          codec = "Motion JPEG format B";
          break;
        case VIDEO_MJPEG_2000:
          mime = "image/x-j2c";
          codec = "Motion JPEG 2000";
          break;
        default:
          mime = "image/jpeg";
          codec = "Motion JPEG format unknow";
          break;
      }
      break;
    case VIDEO_DIVX:
      switch (stream->codec_sub_type) {
        case VIDEO_DIVX3:
          mime = "video/x-divx, divxversion=(int)3";
          codec = "Divx3";
          break;
        case VIDEO_DIVX4:
          mime = "video/x-divx, divxversion=(int)4";
          codec = "Divx4";
        case VIDEO_DIVX5_6:
        default:
          mime = "video/x-divx, divxversion=(int)5";
          codec = "Divx";
          break;
          break;
      }
      break;
    case VIDEO_XVID:
      mime = "video/x-xvid";
      codec = "Xvid";
      break;
    case VIDEO_WMV:
      switch (stream->codec_sub_type) {
        case VIDEO_WMV7:
          mime = "video/x-wmv, wmvversion=(int)1, format=(fourcc)WMV1";
          codec = "WMV7";
          break;
        case VIDEO_WMV8:
          mime = "video/x-wmv, wmvversion=(int)2, format=(fourcc)WMV2";
          codec = "WMV8";
          break;
        case VIDEO_WMV9:
          mime = "video/x-wmv, wmvversion=(int)3, format=(fourcc)WMV3";
          codec = "WMV9";
          break;
        case VIDEO_WVC1:
          mime = "video/x-wmv, wmvversion=(int)3, format=(fourcc)WVC1";
          codec = "VC1";
          break;
        default:
          goto fail;
          break;
      }
      break;
    case VIDEO_REAL:
      mime = "video/x-pn-realvideo";
      codec = "RealVideo";
      break;
    case VIDEO_SORENSON_H263:
      mime = "video/x-flash-video";
      codec = "Sorenson H.263";
      break;
    case VIDEO_FLV_SCREEN:
      mime = "video/x-flash-video";
      codec = "Flash Screen";
      break;
    case VIDEO_ON2_VP:
      switch (stream->codec_sub_type) {
        case VIDEO_VP6A:
          mime = "video/x-vp6-alpha";
          codec = "VP6 Alpha";
          break;
        case VIDEO_VP6:
          mime = "video/x-vp6-flash";
          codec = "VP6 Flash";
          break;
        case VIDEO_VP8:
          mime = "video/x-vp8";
          codec = "VP8";
          break;
        default:
          goto fail;
          break;
      }
      break;
    default:
      goto fail;
      break;
  }

  mime =
      g_strdup_printf
      ("%s, width=(int)%ld, height=(int)%ld, framerate=(fraction)%ld/%ld",
      mime, stream->info.video.width, stream->info.video.height,
      stream->info.video.fps_n, stream->info.video.fps_d);
  stream->caps = gst_caps_from_string (mime);
  g_free (mime);

  if (stream->pid < 0) {
    stream->pid = demux->n_video_streams;
  }

  padname = g_strdup_printf ("video_%03x%03x", stream->ppid, stream->pid);
  GST_INFO ("Create video pad %s", padname);
  stream->pad =
      gst_pad_new_from_static_template (&gst_aiurdemux_videosrc_template,
      padname);
  g_free (padname);

  demux->n_video_streams++;

  stream->pending_tags = gst_tag_list_new ();
  gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
      GST_TAG_CODEC, codec, NULL);
  if (stream->lang[0] != '\0') {
    gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_LANGUAGE_CODE, stream->lang, NULL);
  }

  if (stream->bitrate) {
    gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_BITRATE, stream->bitrate, NULL);
  }
  return;
fail:
  GST_WARNING ("Unknown Video code-type=%d, sub-type=%d",
      stream->codec_type, stream->codec_sub_type);
  return;
}

static void
aiurdemux_parse_audio (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_num)
{
  gchar *mime = NULL, *codec;
  gchar *padname;

  int32 core_ret = PARSER_SUCCESS;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  CORE_API (inf, getAudioNumChannels,, core_ret, handle, track_num,
      &stream->info.audio.n_channels);
  CORE_API (inf, getAudioSampleRate,, core_ret, handle, track_num,
      &stream->info.audio.rate);
  CORE_API (inf, getAudioBitsPerSample,, core_ret, handle, track_num,
      &stream->info.audio.sample_width);

  if (stream->info.audio.n_channels == 0) {
    stream->info.audio.n_channels = 2;
  }
  if (stream->info.audio.rate == 0) {
    stream->info.audio.rate = 44100;
  }
  if (stream->info.audio.sample_width == 0) {
    stream->info.audio.sample_width = 16;
  }

  switch (stream->codec_type) {
    case AUDIO_AAC:
      mime = "audio/mpeg, mpegversion=(int)4";
      codec = "AAC";
      gchar *stream_type;
      stream->send_codec_data = TRUE;
      switch (stream->codec_sub_type) {
        case AUDIO_AAC_ADTS:
          stream_type = "adts";
          break;
        case AUDIO_AAC_ADIF:
          stream_type = "adif";
          break;
        case AUDIO_AAC_RAW:
          stream_type = "raw";
          break;
        default:
          stream_type = NULL;
          break;
      }
      if (stream_type) {
        mime =
            g_strdup_printf
            ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true, stream-format=%s",
            mime, stream->info.audio.n_channels,
            stream->info.audio.rate, stream->bitrate, stream_type);
      } else {
        mime =
            g_strdup_printf
            ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true",
            mime, stream->info.audio.n_channels,
            stream->info.audio.rate, stream->bitrate);
      }
      break;
    case AUDIO_MPEG2_AAC:
      mime = "audio/mpeg, mpegversion=(int)2, framed=(boolean)true";
      codec = "AAC";
      mime =
          g_strdup_printf
          ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld", mime,
          stream->info.audio.n_channels, stream->info.audio.rate,
          stream->bitrate);
      break;
    case AUDIO_MP3:
      mime =
          "audio/mpeg, mpegversion=(int)1, framed=(boolean)true";
      codec = "MP3";
      mime =
          g_strdup_printf
          ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld", mime,
          stream->info.audio.n_channels, stream->info.audio.rate,
          stream->bitrate);
      break;
    case AUDIO_AC3:
      mime = "audio/x-ac3";
      codec = "AC3";
      mime =
          g_strdup_printf
          ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true",
          mime, stream->info.audio.n_channels, stream->info.audio.rate,
          stream->bitrate);
      break;
    case AUDIO_WMA:
      CORE_API (inf, getAudioBlockAlign,, core_ret, handle, track_num,
          &stream->info.audio.block_align);
      switch (stream->codec_sub_type) {
        case AUDIO_WMA1:
          mime = "audio/x-wma, wmaversion=(int)1";
          codec = "WMA7";
          break;
        case AUDIO_WMA2:
          mime = "audio/x-wma, wmaversion=(int)2";
          codec = "WMA8";
          break;
        case AUDIO_WMA3:
          mime = "audio/x-wma, wmaversion=(int)3";
          codec = "WMA9";
          break;
        default:
          goto fail;
          break;
      }
      stream->send_codec_data = TRUE;
      mime =
          g_strdup_printf
          ("%s, channels=(int)%ld, rate=(int)%ld, block_align=(int)%ld, depth=(int)%ld, bitrate=(int)%ld",
          mime, stream->info.audio.n_channels, stream->info.audio.rate,
          stream->info.audio.block_align, stream->info.audio.sample_width,
          stream->bitrate);
      break;
    case AUDIO_PCM:
    {
      int width, depth, endian;
      gboolean sign = TRUE;
      switch (stream->codec_sub_type) {
        case AUDIO_PCM_U8:
          width = depth = 8;
          endian = G_BYTE_ORDER;
          sign = FALSE;
          break;
        case AUDIO_PCM_S16LE:
          width = depth = 16;
          endian = G_LITTLE_ENDIAN;
          break;
        case AUDIO_PCM_S24LE:
          width = depth = 24;
          endian = G_LITTLE_ENDIAN;
          break;
        case AUDIO_PCM_S32LE:
          width = depth = 32;
          endian = G_LITTLE_ENDIAN;
          break;
        case AUDIO_PCM_S16BE:
          width = depth = 16;
          endian = G_BIG_ENDIAN;
          break;
        case AUDIO_PCM_S24BE:
          width = depth = 24;
          endian = G_BIG_ENDIAN;
          break;
        case AUDIO_PCM_S32BE:
          width = depth = 32;
          endian = G_BIG_ENDIAN;
          break;
        default:
          goto fail;
          break;
      }
      codec = "PCM";
      mime =
          g_strdup_printf
          ("audio/x-raw-int, channels=(int)%ld, rate=(int)%ld, width=(int)%d, depth=(int)%d, endianness=(int)%d, signed=%s",
          stream->info.audio.n_channels, stream->info.audio.rate, width,
          depth, endian, (sign ? "true" : "false"));
    }
      break;
    case AUDIO_REAL:
      switch (stream->codec_sub_type) {
        case REAL_AUDIO_RAAC:
          mime =
              g_strdup_printf
              ("audio/mpeg, mpegversion=(int)4, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
              stream->info.audio.n_channels, stream->info.audio.rate,
              stream->bitrate);
          codec = "AAC";
          break;
        default:
        {
          uint32 frame_bit;
          CORE_API (inf, getAudioBitsPerFrame,, core_ret, handle, track_num,
              &frame_bit);
          mime =
              g_strdup_printf
              ("audio/x-pn-realaudio, channels=(int)%ld, rate=(int)%ld, frame_bit=(int)%ld",
              stream->info.audio.n_channels, stream->info.audio.rate,
              frame_bit);
          codec = "RealAudio";
          stream->send_codec_data = TRUE;
        }
          break;
      }

      break;
    case AUDIO_VORBIS:
      codec = "VORBIS";
      mime =
          g_strdup_printf
          ("audio/x-vorbis, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true",
          stream->info.audio.n_channels, stream->info.audio.rate,
          stream->bitrate);
      stream->send_codec_data = TRUE;
      break;
    case AUDIO_FLAC:
      codec = "FLAC";
      mime =
          g_strdup_printf
          ("audio/x-flac, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
          stream->info.audio.n_channels, stream->info.audio.rate,
          stream->bitrate);
      break;
    case AUDIO_DTS:
      codec = "DTS";
      mime =
          g_strdup_printf
          ("audio/x-dts, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
          stream->info.audio.n_channels, stream->info.audio.rate,
          stream->bitrate);
      break;
    case AUIDO_SPEEX:
      codec = "SPEEX";
      mime =
          g_strdup_printf
          ("audio/x-speex, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
          stream->info.audio.n_channels, stream->info.audio.rate,
          stream->bitrate);
      break;
    case AUDIO_AMR:
      switch (stream->codec_sub_type) {
        case AUDIO_AMR_NB:
          mime = "audio/AMR";
          codec = "AMR-NB";
          if (stream->info.audio.rate != 8000) {
            GST_WARNING
                ("amr-nb should using 8k rate, this maybe a core parser BUG!");
            stream->info.audio.rate = 8000;
          }
          break;
        case AUDIO_AMR_WB:
          mime = "audio/AMR-WB";
          codec = "AMR-WB";
          break;
        default:
          goto fail;
          break;
      }
      stream->send_codec_data = TRUE;
      mime =
          g_strdup_printf
          ("%s, channels=(int)%ld, rate=(int)%ld, depth=(int)%ld, bitrate=(int)%ld",
          mime, stream->info.audio.n_channels, stream->info.audio.rate,
          stream->info.audio.sample_width, stream->bitrate);
      break;
    default:
      goto fail;
  }

  stream->caps = gst_caps_from_string (mime);
  g_free (mime);

  if (stream->pid < 0) {
    stream->pid = demux->n_audio_streams;
  }

  padname = g_strdup_printf ("audio_%03x%03x", stream->ppid, stream->pid);

  GST_INFO ("Create audio pad %s", padname);

  stream->pad =
      gst_pad_new_from_static_template (&gst_aiurdemux_audiosrc_template,
      padname);
  g_free (padname);

  demux->n_audio_streams++;

  stream->pending_tags = gst_tag_list_new ();
  gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
      GST_TAG_CODEC, codec, NULL);
  if (stream->lang[0] != '\0') {
    gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_LANGUAGE_CODE, stream->lang, NULL);
  }

  if (stream->bitrate) {
    gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_BITRATE, stream->bitrate, NULL);
  }

  return;

fail:
  GST_WARNING ("Unknown Audio code-type=%d, sub-type=%d",
      stream->codec_type, stream->codec_sub_type);
  return;
}


static void
aiurdemux_parse_text (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_num)
{
  gchar *mime = NULL;
  gchar *padname;
  int32 core_ret = PARSER_SUCCESS;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  CORE_API (inf, getTextTrackWidth,, core_ret, handle, track_num,
      &stream->info.subtitle.width);
  CORE_API (inf, getTextTrackHeight,, core_ret, handle, track_num,
      &stream->info.subtitle.height);

  switch (stream->codec_type) {
    case TXT_DIVX_FEATURE_SUBTITLE:
      mime = "video/x-avi-unknown, fourcc=(fourcc)DXSB";
      mime =
          g_strdup_printf ("%s, width=(int)%ld, height=(int)%ld", mime,
          stream->info.subtitle.width, stream->info.subtitle.height);
      break;
#ifdef AIUR_SUB_TEXT_SUPPORT
    case TXT_TYPE_UNKNOWN:
      mime = "text/plain";
      mime = g_strdup_printf ("%s", mime);
      break;
#endif
    default:
      goto fail;
  }

  stream->caps = gst_caps_from_string (mime);
  g_free (mime);

  if (stream->pid < 0) {
    stream->pid = demux->n_sub_streams;
  }

  padname = g_strdup_printf ("subtitle_%03x%03x", stream->ppid, stream->pid);

  GST_INFO ("Create video pad %s", padname);

  stream->pad =
      gst_pad_new_from_static_template (&gst_aiurdemux_subsrc_template,
      padname);
  g_free (padname);

  stream->pending_tags = gst_tag_list_new ();
  if (stream->lang[0] != '\0') {
    gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_LANGUAGE_CODE, stream->lang, NULL);
  }

  demux->n_sub_streams++;

fail:

  GST_WARNING ("Unknown Text code-type=%d, sub-type=%d",
      stream->codec_type, stream->codec_sub_type);
  return;
}


static AiurDemuxProgram *
aiurdemux_stream_attach_program (GstAiurDemux * demux, AiurDemuxStream * stream)
{
  AiurDemuxProgram *program = NULL;
  int i, n;
  for (i = 0; i < demux->clip_info.program_num; i++) {
    AiurDemuxProgram *p;
    if (p = demux->programs[i]) {
      for (n = 0; n < p->track_num; n++) {
        if (p->tracks[n].id == stream->track_idx) {
          program = p;
          stream->ppid =
              (IS_PID_VALID (p->program_number) ? p->
              program_number : (IS_PID_VALID (p->pid) ? p->pid : i));
          stream->pid =
              (IS_PID_VALID (p->tracks[n].pid) ? p->tracks[n].pid : n);
          if (p->tracks[n].lang[0] != '\0') {
            memcpy (stream->lang, p->tracks[n].lang, 3);
          }
          goto bail;
        }
      }
    }
  }
bail:
  return program;
}


static int
aiurdemux_parse_streams (GstAiurDemux * demux)
{
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  int total = demux->clip_info.track_num;
  int i;
  char *stream_mime = NULL;
  char *pad_name = NULL;
  int32 core_ret = PARSER_SUCCESS;
  AiurDemuxStream *stream;
  bool enable;

  for (i = 0; i < total; i++) {
    uint64 duration = 0;

    if (demux->n_streams >= GST_AIURDEMUX_MAX_STREAMS)
      goto bail;

    stream = g_new0 (AiurDemuxStream, 1);
    if (stream == NULL)
      goto bail;

    stream->track_idx = i;
    stream->pid = -1;
    stream->ppid = 0;

    if (demux->clip_info.program_num) {
      stream->program = aiurdemux_stream_attach_program (demux, stream);
    }

    CORE_API (inf, getTrackType,, core_ret, handle, i, &stream->type,
        &stream->codec_type, &stream->codec_sub_type);
    CORE_API (inf, getTrackDuration,, core_ret, handle, i, &duration);

    stream->duration = AIUR_CORETS_2_GSTTS (duration);

    CORE_API (inf, getLanguage,, core_ret, handle, i, stream->lang);
    if (core_ret != PARSER_SUCCESS) {
      stream->lang[0] = '\0';
    }

    CORE_API (inf, getBitRate,, core_ret, handle, i, &stream->bitrate);
    if (core_ret != PARSER_SUCCESS) {
      stream->bitrate = 0;
    }

    CORE_API (inf, getDecoderSpecificInfo,, core_ret, handle, i,
        &stream->codec_data.codec_data, &stream->codec_data.length);

    switch (stream->type) {
      case MEDIA_VIDEO:
        if (((demux->option.video_max < 0)
                || (demux->n_video_streams < demux->option.video_max))
            && ((demux->option.stream_mask == 0)
                || (demux->option.stream_mask & ((guint64) 1 << i)))
            && ((stream->program == NULL) || (stream->program->enabled))) {
          aiurdemux_parse_video (demux, stream, i);
          if (demux->tag_list) {
            gchar *codec = NULL;
            gboolean ret = gst_tag_list_get_string (demux->tag_list,
                GST_TAG_VIDEO_CODEC, &codec);
            if (ret)
              gst_tag_list_add (stream->pending_tags,
                  GST_TAG_MERGE_REPLACE, GST_TAG_CODEC, codec, NULL);
            if (codec)
              g_free (codec);
          }
        }
        break;
      case MEDIA_AUDIO:
        if (((demux->option.audio_max < 0)
                || (demux->n_audio_streams < demux->option.audio_max))
            && ((demux->option.stream_mask == 0)
                || (demux->option.stream_mask & ((guint64) 1 << i)))
            && ((stream->program == NULL) || (stream->program->enabled))) {
          aiurdemux_parse_audio (demux, stream, i);
          if (demux->tag_list) {
            gchar *codec = NULL;
            gboolean ret = gst_tag_list_get_string (demux->tag_list,
                GST_TAG_AUDIO_CODEC, &codec);
            if (ret)
              gst_tag_list_add (stream->pending_tags,
                  GST_TAG_MERGE_REPLACE, GST_TAG_CODEC, codec, NULL);
            if (codec)
              g_free (codec);
          }
        }
        break;
      case MEDIA_TEXT:
        if (((demux->option.subtitle_max < 0)
                || (demux->n_sub_streams < demux->option.subtitle_max))
            && ((demux->option.stream_mask == 0)
                || (demux->option.stream_mask & ((guint64) 1 << i)))
            && ((stream->program == NULL) || (stream->program->enabled))) {
          aiurdemux_parse_text (demux, stream, i);
        }
        break;
    }

    if ((stream->send_codec_data) && (stream->codec_data.length)) {
      GstBuffer *gstbuf;
      gstbuf = gst_buffer_new_and_alloc (stream->codec_data.length);
      memcpy (GST_BUFFER_DATA (gstbuf),
          stream->codec_data.codec_data, stream->codec_data.length);
      gst_caps_set_simple (stream->caps, "codec_data",
          GST_TYPE_BUFFER, gstbuf, NULL);
      gst_buffer_unref (gstbuf);
    }

    aiurdemux_print_track_info (stream);

    if (stream->pad) {
      GST_PAD_ELEMENT_PRIVATE (stream->pad) = stream;
      gst_pad_use_fixed_caps (stream->pad);
      gst_pad_set_event_function (stream->pad, gst_aiurdemux_handle_src_event);
      gst_pad_set_query_type_function (stream->pad,
          gst_aiurdemux_get_src_query_types);
      gst_pad_set_query_function (stream->pad, gst_aiurdemux_handle_src_query);
      enable = TRUE;

      CORE_API (inf, enableTrack,, core_ret, handle, i, enable);
      MFW_WEAK_ASSERT (stream->codec_data.length < 10000000);

      aiurdemux_init_post_processor (demux, stream);
      gst_pad_set_caps (stream->pad, stream->caps);
      gst_pad_set_active (stream->pad, TRUE);
      gst_element_add_pad (GST_ELEMENT_CAST (demux), stream->pad);
      stream->mask = (1 << demux->n_streams);
      stream->adapter = gst_adapter_new ();
      aiurdemux_reset_stream (demux, stream);
      if (demux->interleave_queue_size) {
        stream->buf_queue = g_queue_new ();
      }

      demux->streams[demux->n_streams] = stream;
      demux->n_streams++;
      continue;
    }

  next:
    enable = FALSE;
    CORE_API (inf, enableTrack,, core_ret, handle, i, enable);
    if (stream) {
      if (stream->caps) {
        gst_caps_unref (stream->caps);
      }
      if (stream->pending_tags) {
        gst_tag_list_free (stream->pending_tags);
      }
      g_free (stream);
    }

  }

  return demux->n_streams;

bail:
  enable = FALSE;
  for (; i < total; i++) {
    CORE_API (inf, enableTrack,, core_ret, handle, i, enable);
  }

  return demux->n_streams;
}

static gboolean
aiurdemux_set_readmode (GstAiurDemux * demux)
{
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  uint32 readmode = demux->clip_info.suggest_read_mode;
  gboolean force = FALSE;
  int32 core_ret;

  if ((!(CORE_API_EXIST (inf, getFileNextSample)))
      && (!(CORE_API_EXIST (inf, getNextSample)))) {
    g_print (RED_STR
        ("Err: core implement neither getFileNextSample nor getNextSample\n"));
    goto fail;
  }

  if (readmode == AIUR_READMODE_NULL) {
    readmode = PARSER_READ_MODE_FILE_BASED;
    CORE_API (inf, getReadMode, readmode =
        PARSER_READ_MODE_FILE_BASED, core_ret, handle, &readmode);
    if (core_ret != PARSER_SUCCESS) {
      readmode = PARSER_READ_MODE_FILE_BASED;
    } else if ((readmode != PARSER_READ_MODE_FILE_BASED)
        && (readmode != PARSER_READ_MODE_TRACK_BASED))
      readmode = PARSER_READ_MODE_FILE_BASED;
  }

  if (!(CORE_API_EXIST (inf, getFileNextSample))) {
    readmode = PARSER_READ_MODE_TRACK_BASED;
    force = TRUE;
  }

  if (!(CORE_API_EXIST (inf, getNextSample))) {
    readmode = PARSER_READ_MODE_FILE_BASED;
    force = TRUE;
  }

  if (demux->clip_info.live) {
    if ((readmode == PARSER_READ_MODE_TRACK_BASED) && (force))
      goto fail;
    readmode = PARSER_READ_MODE_FILE_BASED;
  }

  CORE_API (inf, setReadMode,, core_ret, handle, readmode);
  if (core_ret != PARSER_SUCCESS) {
    if (force) {
      goto fail;
    }
    readmode =
        ((readmode ==
            PARSER_READ_MODE_FILE_BASED) ? PARSER_READ_MODE_TRACK_BASED
        : PARSER_READ_MODE_FILE_BASED);
    CORE_API (inf, setReadMode,, core_ret, handle, readmode);
    if (core_ret != PARSER_SUCCESS)
      goto fail;
  }

  demux->clip_info.read_mode = demux->clip_info.suggest_read_mode = readmode;
  if ((readmode == PARSER_READ_MODE_FILE_BASED)
      && (demux->option.interleave_queue_size)) {
    demux->interleave_queue_size = demux->option.interleave_queue_size;
  } else {
    demux->interleave_queue_size = 0;
  }

  return TRUE;
fail:
  return FALSE;
}


static void
aiurdemux_pretty_print_info (gchar * title, gchar * data, int max_raw)
{
  int len = strlen (data);
  gchar *tmp = data;
  g_print (BLUE_STR ("\t%s:\n", title));
  while (len > 0) {
    gchar c;
    if (len > max_raw) {
      c = tmp[max_raw];
      tmp[max_raw] = '\0';
      g_print (BLUE_STR ("\t      %s\n", tmp));
      tmp[max_raw] = c;
    } else {
      g_print (BLUE_STR ("\t      %s\n", tmp));
    }
    len -= max_raw;
    tmp += max_raw;
  }
}


static gchar *
aiurdemux_generate_idx_file_location (GstAiurDemux * demux, char *prefix)
{
  gchar *location = NULL, *buf = NULL;

  if (demux->content_info.uri == NULL) {
    goto bail;
  }

  location = g_strdup (demux->content_info.uri);
  if (!AIUR_PROTOCOL_IS_LOCAL (location)) {
    goto bail;
  }

  if (buf = gst_uri_get_location (location)) {
    g_free (location);
    location = buf;
    while (*buf != '\0') {
      if (*buf == '/') {
        *buf = '.';
      }
      buf++;
    }

    buf = g_strdup_printf ("%s/%s.%s", prefix, location, "aidx");
  }

bail:
  if (location)
    g_free (location);
  return buf;
}


static void
aiurdemux_print_clip_info (GstAiurDemux * demux)
{
  AiurDemuxClipInfo *clip_info = &demux->clip_info;
  AiurDemuxStream *stream;
  int i;
  g_print (BLUE_STR ("Movie Info:\n"));
  g_print (BLUE_STR ("\tSeekable  : %s\n",
          (clip_info->seekable ? "Yes" : "No")));
  g_print (BLUE_STR ("\tLive      : %s\n", (clip_info->live ? "Yes" : "No")));
  g_print (BLUE_STR ("\tDuration  : %" GST_TIME_FORMAT "\n",
          GST_TIME_ARGS (clip_info->duration)));
  g_print (BLUE_STR ("\tReadMode  : %s\n",
          ((clip_info->read_mode ==
                  PARSER_READ_MODE_FILE_BASED) ? "File" : "Track")));
  if (clip_info->auto_retimestamp) {
    g_print (BLUE_STR ("\tAutoRetimestamp: %dms\n",
            demux->config.retimestamp_threashold_ms));
  }

  if (clip_info->program_num) {
    g_print (BLUE_STR ("\tprograms    : %ld\n", clip_info->program_num));
  }
  g_print (BLUE_STR ("\tTrack     : %ld\n\n", clip_info->track_num));
}


static gboolean
aiurdemux_parse_pmt (GstAiurDemux * demux)
{
  gboolean ret = FALSE;
  int32 core_ret = PARSER_SUCCESS;
  AiurDemuxClipInfo *clip_info = &demux->clip_info;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  UserDataID id = USER_DATA_PMT;
  UserDataFormat format = USER_DATA_FORMAT_PMT_INFO;
  PMTInfoList *pmt = NULL;
  uint32 userDataSize = 0;

  CORE_API (inf, getMetaData,, core_ret, handle,
      id, &format, (uint8 **) & pmt, &userDataSize);
  if ((core_ret == PARSER_SUCCESS) && (pmt != NULL)
      && (userDataSize > 0)) {
    int n;
    for (n = 0;
        ((n < clip_info->program_num) && (n < pmt->m_dwProgramNum)); n++) {
      AiurDemuxProgram *program;
      PMTInfo *pmtinfo = &pmt->m_ptPMTInfo[n];
      if (program =
          g_malloc (sizeof (AiurDemuxProgram) +
              sizeof (AiurDemuxProgramTrack) * pmtinfo->m_dwTrackNum)) {
        int m;
        program->program_number = pmtinfo->m_dwChannel;
        program->enabled = FALSE;
        program->pid = pmtinfo->m_dwPID;
        program->track_num = pmtinfo->m_dwTrackNum;
        for (m = 0; m < program->track_num; m++) {
          program->tracks[m].id = pmtinfo->m_ptTrackInfo[m].m_dwTrackNo;
          program->tracks[m].pid = pmtinfo->m_ptTrackInfo[m].m_dwPID;
          memcpy (program->tracks[m].lang,
              pmtinfo->m_ptTrackInfo[m].m_byLan, 3);
        }
        demux->programs[n] = program;
      }
    }
    ret = TRUE;
  }

fail:
  return ret;
}


static void
aiurdemux_select_programs (GstAiurDemux * demux)
{
  int n;
  for (n = 0; n < demux->clip_info.program_num; n++) {
    AiurDemuxProgram *program;
    if (program = demux->programs[n]) {
      if (demux->option.multiprogram_enabled) {
        if (((demux->option.program_mask == 0))
            || ((1 << n) & demux->option.program_mask)) {
          program->enabled = TRUE;
        }
      } else {
        if (demux->option.program_number >= 0) {
          if (demux->option.program_number == program->program_number) {
            program->enabled = TRUE;
            break;
          }
        } else {
          program->enabled = TRUE;
          break;
        }
      }
    }
  }
}


gboolean
aiurdemux_parse_program_info (GstAiurDemux * demux)
{
  gboolean ret = FALSE;
  int32 core_ret = PARSER_SUCCESS;
  AiurDemuxClipInfo *clip_info = &demux->clip_info;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  UserDataID id = USER_DATA_PROGRAMINFO;
  UserDataFormat format = USER_DATA_FORMAT_PROGRAM_INFO;
  ProgramInfoMenu *pinfo = NULL;
  uint32 userDataSize = 0;
  int n;

  CORE_API (inf, getMetaData,, core_ret, handle,
      id, &format, (uint8 **) & pinfo, &userDataSize);
  if ((core_ret != PARSER_SUCCESS) || (userDataSize == 0)) {
    pinfo = NULL;
  }

  for (n = 0; n < clip_info->program_num; n++) {
    AiurDemuxProgram *program;
    uint32 track_num, *tracklist;
    CORE_API (inf, getProgramTracks,, core_ret, handle, n,
        &track_num, &tracklist);
    if (core_ret == PARSER_SUCCESS) {
      if (program =
          g_malloc (sizeof (AiurDemuxProgram) +
              sizeof (AiurDemuxProgramTrack) * track_num)) {
        int m;
        program->enabled = FALSE;
        if ((pinfo) && (n < pinfo->m_dwProgramNum)) {
          program->program_number = pinfo->m_atProgramInfo[n].m_dwChannel;
          program->pid = pinfo->m_atProgramInfo[n].m_dwPID;
        } else {
          program->program_number = -1;
          program->pid = -1;
        }
        program->track_num = track_num;
        for (m = 0; m < track_num; m++) {
          program->tracks[m].pid = -1;
          program->tracks[m].id = tracklist[m];
          program->tracks[m].lang[0] = '\0';
        }
        demux->programs[n] = program;
      }
    }
  }
  ret = TRUE;
fail:
  return ret;
}


static gboolean
aiurdemux_parse_programs (GstAiurDemux * demux)
{
  gboolean ret = FALSE;
  if ((aiurdemux_parse_pmt (demux))
      || (aiurdemux_parse_program_info (demux))) {
    aiurdemux_select_programs (demux);
    ret = TRUE;
  }
  return ret;
}


static GstFlowReturn
aiurdemux_loop_state_header (GstAiurDemux * demux)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  int32 core_ret = PARSER_SUCCESS;
  AiurDemuxClipInfo *clip_info = &demux->clip_info;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  int n;
  gboolean need_init_index = TRUE;
  int tracks;
  int readmode = PARSER_READ_MODE_FILE_BASED;
  guint64 duration = 0;
  clip_info->seekable = FALSE;

  if ((demux->config.import_index)
      && (demux->content_info.index_file)) {
    AiurIndexTable *idxtable =
        aiurdemux_import_idx_table (demux->content_info.index_file);
    if (idxtable) {

      if ((idxtable->coreid) && (inf->coreid)
          && (strlen (inf->coreid) == idxtable->coreid_len)
          && (!memcmp (idxtable->coreid, inf->coreid, idxtable->coreid_len))) {

        demux->clip_info.suggest_read_mode = idxtable->info.readmode;
        if (CORE_API_EXIST (inf, initializeIndex)
            && CORE_API_EXIST (inf, importIndex)
            && (idxtable->info.size)) {

          CORE_API (inf, importIndex,, core_ret, handle, idxtable->idx,
              idxtable->info.size);
          if (core_ret == PARSER_SUCCESS) {
            GST_INFO ("Index table %s[size %d] imported.",
                demux->content_info.index_file, idxtable->info.size);
            need_init_index = FALSE;
          }
        }
      }
      aiurdemux_destroy_idx_table (idxtable);
    }
  }

  if (need_init_index) {
    CORE_API (inf, initializeIndex,, core_ret, handle);
  }

  CORE_API (inf, isSeekable,, core_ret, handle, &clip_info->seekable);
  CORE_API (inf, getMovieDuration,, core_ret, handle, &duration);
  clip_info->duration = AIUR_CORETS_2_GSTTS (duration);
  if (aiurdemux_set_readmode (demux) == FALSE) {
    ret = GST_FLOW_ERROR;
    goto bail;
  }

  CORE_API (inf, getNumPrograms,, core_ret, handle, &clip_info->program_num);
  CORE_API (inf, getNumTracks, goto bail, core_ret, handle,
      &clip_info->track_num);
  if (clip_info->track_num == 0) {
    goto bail;
  }

  if ((demux->streams =
          g_new0 (AiurDemuxStream *, clip_info->track_num)) == NULL) {
    goto bail;
  }

  if (clip_info->program_num) {
    if ((demux->programs =
            g_new0 (AiurDemuxProgram *, clip_info->program_num)) == NULL) {
      goto bail;
    }
    if (aiurdemux_parse_programs (demux) == FALSE) {
      goto bail;
    }
  }

  demux->tag_list = aiurdemux_add_user_tags (demux);
  aiurdemux_print_clip_info (demux);
  tracks = aiurdemux_parse_streams (demux);
  for (n = 0; n < clip_info->track_num; n++) {
    guint64 usSeekTime = 0;
    CORE_API (inf, seek,, core_ret, handle, n, &usSeekTime, SEEK_FLAG_NO_LATER);
  }

  if (tracks) {
    demux->state = AIURDEMUX_STATE_MOVIE;
    gst_element_no_more_pads (GST_ELEMENT_CAST (demux));
    ret = GST_FLOW_OK;
  }

  return ret;
bail:
  aiurdemux_release_resource (demux);
  return ret;
}


static AiurDemuxStream *
aiurdemux_trackidx_to_stream (GstAiurDemux * demux, gint32 stream_idx)
{
  AiurDemuxStream *stream = NULL;
  int i;
  for (i = 0; i < demux->n_streams; i++) {
    if (demux->streams[i]->track_idx == stream_idx) {
      stream = demux->streams[i];
      break;
    }
  }
  return stream;
}


static GstFlowReturn
aiurdemux_combine_flows (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstFlowReturn ret)
{
  gint i;
  gboolean unexpected = FALSE, not_linked = TRUE;
  stream->last_ret = ret;
  for (i = 0; i < demux->n_streams; i++) {
    AiurDemuxStream *ostream = demux->streams[i];
    ret = ostream->last_ret;
    if (G_LIKELY (ret != GST_FLOW_UNEXPECTED && ret != GST_FLOW_NOT_LINKED))
      goto done;
    unexpected |= (ret == GST_FLOW_UNEXPECTED);
    not_linked &= (ret == GST_FLOW_NOT_LINKED);
  }

  if (not_linked)
    ret = GST_FLOW_NOT_LINKED;
  else if (unexpected)
    ret = GST_FLOW_UNEXPECTED;
done:
  return ret;
}


static void
aiurdemux_check_long_interleave (GstAiurDemux * demux, AiurDemuxStream * stream)
{

  stream->preroll_size += GST_BUFFER_SIZE (stream->buffer);
  if ((((demux->config.max_interleave_bytes)
              && (stream->preroll_size > demux->config.max_interleave_bytes))
      )) {

    if (demux->config.drop_sample) {

      gst_buffer_unref (stream->buffer);
      MM_UNREGRES (stream->buffer, RES_GSTBUFFER);
      stream->buffer = NULL;
    } else {

      AiurDemuxStream *cstream;
      int n = 0;
      int mask = demux->new_segment_mask;
      mask &= (~(stream->mask));
      while (mask) {
        if (mask & (1 << n)) {
          cstream = demux->streams[n];
          aiurdemux_send_stream_eos (demux, cstream);
          mask &= (~(cstream->mask));
        }
        n++;
      }

    }
    GST_WARNING ("Preroll fail detected, change suggest mode to track based.");
    demux->clip_info.suggest_read_mode = PARSER_READ_MODE_TRACK_BASED;
  }
}


static gint
aiurdemux_choose_next_stream (GstAiurDemux * demux)
{
  int n;
  gint track_num = 0;
  gint64 min_time = -1;
  AiurDemuxStream *stream;
  for (n = 0; n < demux->n_streams; n++) {
    stream = demux->streams[n];
    if (!stream->valid) {
      continue;
    }
    if ((demux->clip_info.read_mode == PARSER_READ_MODE_TRACK_BASED)
        && (stream->partial_sample)) {
      track_num = stream->track_idx;
      break;
    }

    if ((demux->interleave_queue_size)
        && (stream->buf_queue_size > demux->interleave_queue_size)) {
      track_num = stream->track_idx;
      break;
    }

    if (min_time >= 0) {
      if (stream->last_stop < min_time) {
        min_time = stream->last_stop;
        track_num = stream->track_idx;
      }
    } else {
      min_time = stream->last_stop;
      track_num = stream->track_idx;
    }

  }

  return track_num;
}


static GstFlowReturn
aiurdemux_send_stream_eos_all (GstAiurDemux * demux)
{
  GstFlowReturn ret = GST_FLOW_OK;
  AiurDemuxStream *stream;
  gint n;
  if (demux->interleave_queue_size) {
    while (demux->valid_mask) {
      gint track = aiurdemux_choose_next_stream (demux);
      stream = aiurdemux_trackidx_to_stream (demux, track);
      if (stream) {
        GstBuffer *gstbuf;
        GstFlowReturn ret;
        if (gstbuf = g_queue_pop_head (stream->buf_queue)) {
          stream->buf_queue_size -= GST_BUFFER_SIZE (gstbuf);
          ret = aiurdemux_push_pad_buffer (demux, stream, gstbuf);
        } else {
          aiurdemux_send_stream_eos (demux, stream);
        }
      } else {
        break;
      }
    }
  } else {

    for (n = 0; n < demux->n_streams; n++) {
      stream = demux->streams[n];

      if ((stream->valid) && (stream->type == MEDIA_AUDIO)) {
        aiurdemux_send_stream_eos (demux, stream);
      }

    }

    for (n = 0; n < demux->n_streams; n++) {
      stream = demux->streams[n];

      if (stream->valid) {
        aiurdemux_send_stream_eos (demux, stream);
      }

    }
  }

  return ret;
}


static GstFlowReturn
aiurdemux_push_pad_buffer (GstAiurDemux * demux,
    AiurDemuxStream * stream, GstBuffer * buffer)
{
  GstFlowReturn ret;
  static int bb = 0;
  GST_LOG ("%s push sample %" GST_TIME_FORMAT " size %d",
      AIUR_MEDIATYPE2STR (stream->type),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)), GST_BUFFER_SIZE (buffer));

  aiurdemux_update_stream_position (demux, stream, buffer);
  ret = gst_pad_push (stream->pad, buffer);

  MM_UNREGRES (buffer, RES_GSTBUFFER);

  if ((ret != GST_FLOW_OK)) {
    GST_WARNING ("Pad %s push error type %d",
        AIUR_MEDIATYPE2STR (stream->type), ret);
    if (ret == GST_FLOW_UNEXPECTED) {
      aiurdemux_send_stream_eos (demux, stream);
      if (demux->valid_mask == 0) {
        ret = GST_FLOW_UNEXPECTED;
        goto bail;
      }

    }
  }

  ret = GST_FLOW_OK;
bail:
  return ret;
}


static GstFlowReturn
aiurdemux_loop_state_movie (GstAiurDemux * demux)
{
  GstFlowReturn ret = GST_FLOW_OK;
  AiurCoreInterface *inf = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  int32 core_ret;
  AiurDemuxStream *stream = NULL;

  int32 track_idx;
  GstBuffer *gstbuf;
  uint8 *buffer;
  int32 buffer_size;
  uint64 usStartTime;
  uint64 usDuration;
  uint32 sampleFlags;

  gint64 timestamp;
  gint64 duration;

  if (demux->pending_event) {
    aiurdemux_send_pending_events (demux);
    demux->pending_event = FALSE;
    if (demux->valid_mask == 0) {
      return GST_FLOW_UNEXPECTED;
    }
  }

  do {
    core_ret = PARSER_ERR_UNKNOWN;
    buffer = NULL;
    gstbuf = NULL;
    buffer_size = 0;
    usStartTime = 0;
    usDuration = 0;
    sampleFlags = 0;
    track_idx = -1;
    if (demux->clip_info.read_mode == PARSER_READ_MODE_FILE_BASED) {
      if (demux->interleave_queue_size) {
        track_idx = aiurdemux_choose_next_stream (demux);
        stream = aiurdemux_trackidx_to_stream (demux, track_idx);
        if ((stream->buf_queue)
            && (!g_queue_is_empty (stream->buf_queue))) {
          gstbuf = g_queue_pop_head (stream->buf_queue);
          stream->buf_queue_size -= GST_BUFFER_SIZE (gstbuf);
          ret = aiurdemux_push_pad_buffer (demux, stream, gstbuf);
          goto bail;
        }

      }
      if (demux->play_mode == AIUR_PLAY_MODE_NORMAL) {
        CORE_API (inf, getFileNextSample,, core_ret, handle,
            &track_idx, &buffer, (void *) (&gstbuf), &buffer_size,
            &usStartTime, &usDuration, &sampleFlags);
      } else {
        uint32 direction;

        if (demux->play_mode == AIUR_PLAY_MODE_TRICK_FORWARD) {
          direction = FLAG_FORWARD;
        } else {
          direction = FLAG_BACKWARD;
        }
        CORE_API (inf, getFileNextSyncSample,, core_ret, handle,
            direction, &track_idx, &buffer, (void *) (&gstbuf),
            &buffer_size, &usStartTime, &usDuration, &sampleFlags);
      }

    } else {
      track_idx = aiurdemux_choose_next_stream (demux);
      if (demux->play_mode == AIUR_PLAY_MODE_NORMAL) {
        CORE_API (inf, getNextSample,, core_ret, handle,
            track_idx, &buffer, (void *) (&gstbuf), &buffer_size,
            &usStartTime, &usDuration, &sampleFlags);
      } else {
        uint32 direction;
        if (demux->play_mode == AIUR_PLAY_MODE_TRICK_FORWARD) {
          direction = FLAG_FORWARD;
        } else {
          direction = FLAG_BACKWARD;
        }

        CORE_API (inf, getNextSyncSample,, core_ret, handle,
            direction, track_idx, &buffer, (void *) (&gstbuf),
            &buffer_size, &usStartTime, &usDuration, &sampleFlags);
      }
    }

    stream = aiurdemux_trackidx_to_stream (demux, track_idx);
    if ((stream) && (stream->num_of_samples >= 0)) {
      if (stream->num_of_samples == 0) {
        core_ret = PARSER_EOS;
        if (gstbuf) {
          MM_UNREGRES (gstbuf, RES_GSTBUFFER);
          gst_buffer_unref (gstbuf);
          buffer = NULL;
          gstbuf = NULL;
        }
      } else {
        stream->num_of_samples--;
      }
    }

    if ((PARSER_EOS == core_ret) || (PARSER_BOS == core_ret)
        || (PARSER_READ_ERROR == core_ret)) {
#if 0
      MFW_WEAK_ASSERT ((gstbuf == NULL) && (buffer == NULL));
      if (gstbuf) {
        gst_buffer_unref (gstbuf);
        gstbuf = NULL;
      }
#endif
      if (demux->clip_info.read_mode == PARSER_READ_MODE_FILE_BASED) {
        aiurdemux_send_stream_eos_all (demux);
        ret = GST_FLOW_UNEXPECTED;
        goto beach;
      } else {
        aiurdemux_send_stream_eos (demux, stream);
        if (demux->valid_mask == 0) {
          ret = GST_FLOW_UNEXPECTED;
          goto beach;
        }
      }

    } else if (PARSER_ERR_INVALID_MEDIA == core_ret) {
      GST_WARNING ("Movie parser interrupt, track_idx %d, error = %d",
          track_idx, core_ret);
      if (stream) {
        aiurdemux_send_stream_eos (demux, stream);
        if (demux->valid_mask == 0) {
          ret = GST_FLOW_UNEXPECTED;
        }
      } else {
        aiurdemux_send_stream_eos_all (demux);
        ret = GST_FLOW_UNEXPECTED;
      }
      goto beach;
    } else if (PARSER_SUCCESS != core_ret) {
      GST_ERROR ("Movie parser failed, error = %d", core_ret);
      aiurdemux_send_stream_eos_all (demux);
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    if ((!gstbuf) || (buffer_size == 0)) {
      if (gstbuf) {
        MM_UNREGRES (gstbuf, RES_GSTBUFFER);
        gst_buffer_unref (gstbuf);
        gstbuf = NULL;
      }

    }

    if (stream) {
      if (gstbuf) {
        GST_BUFFER_SIZE (gstbuf) = buffer_size;
        if (stream->buffer) {
          MM_UNREGRES (gstbuf, RES_GSTBUFFER);
          gst_adapter_push (stream->adapter, gstbuf);
        } else {
          stream->buffer = gstbuf;
        }
      }
      AIUR_UPDATE_SAMPLE_STAT (stream->sample_stat,
          AIUR_CORETS_2_GSTTS (usStartTime),
          AIUR_COREDURATION_2_GSTDURATION (usDuration), sampleFlags);
      if (sampleFlags & FLAG_SAMPLE_NOT_FINISHED) {
        stream->partial_sample = TRUE;
      } else {
        gint sample_size;
        if (sample_size = gst_adapter_available (stream->adapter)) {
          GstBuffer *newbuf =
              gst_buffer_new_and_alloc (sample_size +
              GST_BUFFER_SIZE (stream->buffer));
          memcpy (GST_BUFFER_DATA (newbuf),
              GST_BUFFER_DATA (stream->buffer),
              GST_BUFFER_SIZE (stream->buffer));
          gst_adapter_copy (stream->adapter,
              GST_BUFFER_DATA (newbuf) + GST_BUFFER_SIZE (stream->buffer),
              0, sample_size);
          gst_buffer_unref (stream->buffer);
          MM_UNREGRES (stream->buffer, RES_GSTBUFFER);
          gst_adapter_clear (stream->adapter);
          stream->buffer = newbuf;
          MM_REGRES (stream->buffer, RES_GSTBUFFER);
        }
        stream->partial_sample = FALSE;
        if (stream->sample_stat.start == stream->last_start) {
          stream->sample_stat.start = GST_CLOCK_TIME_NONE;
        }

        if (stream->buffer) {
          if (!(stream->sample_stat.flag & FLAG_SYNC_SAMPLE)) {
            GST_BUFFER_FLAG_SET (stream->buffer, GST_BUFFER_FLAG_DELTA_UNIT);
          }
        }
      }

    } else {                    /* no stream found */
      MM_UNREGRES (gstbuf, RES_GSTBUFFER);
      gst_buffer_unref (gstbuf);
    }

  } while (sampleFlags & FLAG_SAMPLE_NOT_FINISHED);
  if (demux->play_mode != AIUR_PLAY_MODE_NORMAL) {
    GST_BUFFER_FLAG_SET (stream->buffer, GST_BUFFER_FLAG_DISCONT);
  }

  if ((stream) && (stream->buffer)) {
    aiurdemux_check_discont (demux, stream);
    aiurdemux_adjust_timestamp (demux, stream, stream->buffer);
    if (stream->new_segment) {
      /* FIX ME: clear delta flag for first buffer after newsegment */
      GST_BUFFER_FLAG_UNSET (stream->buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      aiurdemux_send_stream_newsegment (demux, stream);
      gst_aiurdemux_push_tags (demux, stream);
    }


    if (stream->post_processor.process) {
      ret = stream->post_processor.process (demux, stream, &stream->buffer);
    }

    if (stream->buffer) {
      if (stream->valid) {
        if (stream->block) {
          if ((GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (stream->buffer)))
              && (GST_BUFFER_TIMESTAMP (stream->buffer) +
                  GST_BUFFER_DURATION (stream->buffer) <
                  stream->time_position)) {
            goto beach;
          }
          stream->block = FALSE;
        }


        if (G_UNLIKELY (demux->new_segment_mask)) {
          aiurdemux_check_long_interleave (demux, stream);
        }

        if (stream->buffer) {
          gst_buffer_set_caps (stream->buffer, GST_PAD_CAPS (stream->pad));
          if (demux->interleave_queue_size) {
            stream->buf_queue_size += GST_BUFFER_SIZE (stream->buffer);
            if (stream->buf_queue_size > stream->buf_queue_size_max) {
              stream->buf_queue_size_max = stream->buf_queue_size;
            }

            g_queue_push_tail (stream->buf_queue, stream->buffer);
            stream->buffer = NULL;
          } else {
            ret = aiurdemux_push_pad_buffer (demux, stream, stream->buffer);
            stream->buffer = NULL;
          }
        }
      } else {
        MM_UNREGRES (stream->buffer, RES_GSTBUFFER);
        gst_buffer_unref (stream->buffer);
      }
      stream->buffer = NULL;
    }

    AIUR_RESET_SAMPLE_STAT (stream->sample_stat);
#if 0
    if ((demux->eos_cnt > 1) && (stream->duration)
        && (stream->last_stop >= stream->duration)
        && (demux->read_mode == PARSER_READ_MODE_FILE_BASED)) {
      aiurdemux_set_stream_eos (demux, stream);
    }
#endif
    ret = aiurdemux_combine_flows (demux, stream, ret);
  }

bail:
  return ret;

beach:

  if (stream) {
    if (stream->buffer) {
      MM_UNREGRES (stream->buffer, RES_GSTBUFFER);
      gst_buffer_unref (stream->buffer);
      stream->buffer = NULL;
    }

    gst_adapter_clear (stream->adapter);

    AIUR_RESET_SAMPLE_STAT (stream->sample_stat);
  }

  return ret;
}


static void
aiurdemux_push_task (GstAiurDemux * aiurdemux)
{
  GstFlowReturn ret;

  switch (aiurdemux->state) {
    case AIURDEMUX_STATE_PROBE:
      ret = aiurdemux_loop_state_probe (aiurdemux);
      break;
    case AIURDEMUX_STATE_INITIAL:
      ret = aiurdemux_loop_state_init (aiurdemux);
      break;
    case AIURDEMUX_STATE_HEADER:
      ret = aiurdemux_loop_state_header (aiurdemux);
      break;
    case AIURDEMUX_STATE_MOVIE:
      ret = aiurdemux_loop_state_movie (aiurdemux);
      break;
    default:
      /* ouch */
      goto invalid_state;
  }

  /* if something went wrong, pause */
  if (ret != GST_FLOW_OK)
    goto pause;

done:
  return;

invalid_state:
  aiurdemux_send_stream_eos_all (aiurdemux);

pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_LOG_OBJECT (aiurdemux, "pausing task, reason %s", reason);

    aiurdemux->running = FALSE;

    /* fatal errors need special actions */
    if (GST_FLOW_IS_FATAL (ret) || ret == GST_FLOW_NOT_LINKED) {
      /* check EOS */
      if (ret == GST_FLOW_UNEXPECTED) {
        {
          GST_LOG_OBJECT (aiurdemux, "Sending EOS at end of segment");
          ///gst_aiurdemux_push_event (aiurdemux,
          //                          gst_event_new_eos ());
        }
      } else if (ret == GST_FLOW_ERROR) {
        GST_ELEMENT_ERROR (aiurdemux, STREAM, FAILED,
            (NULL), ("streaming stopped, reason %s", reason));
        //gst_aiurdemux_push_event (aiurdemux, gst_event_new_eos ());
      }
    }
    goto done;
  }
}


static void
aiurdemux_pull_task (GstPad * pad)
{
  GstAiurDemux *aiurdemux;
  GstFlowReturn ret;

  aiurdemux = GST_AIURDEMUX (gst_pad_get_parent (pad));

  switch (aiurdemux->state) {
    case AIURDEMUX_STATE_PROBE:
      ret = aiurdemux_loop_state_probe (aiurdemux);
      break;
    case AIURDEMUX_STATE_INITIAL:
      ret = aiurdemux_loop_state_init (aiurdemux);
      break;
    case AIURDEMUX_STATE_HEADER:
      ret = aiurdemux_loop_state_header (aiurdemux);
      break;
    case AIURDEMUX_STATE_MOVIE:
      ret = aiurdemux_loop_state_movie (aiurdemux);
      break;
    default:
      /* ouch */
      goto invalid_state;
  }

  /* if something went wrong, pause */
  if (ret != GST_FLOW_OK)
    goto pause;

done:
  gst_object_unref (aiurdemux);
  return;

invalid_state:
  aiurdemux_send_stream_eos_all (aiurdemux);

pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_WARNING ("pausing task, reason %s", reason);

    gst_pad_pause_task (pad);

    /* fatal errors need special actions */
    if (GST_FLOW_IS_FATAL (ret) || ret == GST_FLOW_NOT_LINKED) {
      /* check EOS */
      if (ret == GST_FLOW_UNEXPECTED) {
        if (aiurdemux->n_streams == 0) {
          /* we have no streams, post an error */
          gst_aiurdemux_post_no_playable_stream_error (aiurdemux);
        }
        {
          GST_LOG_OBJECT (aiurdemux, "Sending EOS at end of segment");
          //gst_aiurdemux_push_event (aiurdemux,
          //                          gst_event_new_eos ());
        }
      } else if (ret == GST_FLOW_ERROR) {
        GST_ELEMENT_ERROR (aiurdemux, STREAM, FAILED,
            (NULL), ("streaming stopped, reason %s, state %d",
                reason, aiurdemux->state));
        //gst_aiurdemux_push_event (aiurdemux, gst_event_new_eos ());
      }
    }
    goto done;
  }
}



static GstFlowReturn
gst_aiurdemux_chain (GstPad * sinkpad, GstBuffer * inbuf)
{
  GstAiurDemux *demux;

  demux = GST_AIURDEMUX (gst_pad_get_parent (sinkpad));
  if (inbuf) {
    gst_aiur_stream_cache_add_buffer (demux->stream_cache, inbuf);
  }

  gst_object_unref (demux);
  return GST_FLOW_OK;
}

static gboolean
aiurdemux_sink_activate (GstPad * sinkpad)
{
  if (gst_pad_check_pull_range (sinkpad)) {
    return gst_pad_activate_pull (sinkpad, TRUE);
  } else
    return gst_pad_activate_push (sinkpad, TRUE);
}

static gboolean
aiurdemux_sink_activate_pull (GstPad * sinkpad, gboolean active)
{
  GstAiurDemux *demux = GST_AIURDEMUX (GST_PAD_PARENT (sinkpad));

  if (active) {
    demux->pullbased = TRUE;
    return gst_pad_start_task (sinkpad,
        (GstTaskFunction) aiurdemux_pull_task, sinkpad);
  } else {
    return gst_pad_stop_task (sinkpad);
  }
}

gpointer
aiurdemux_loop_push (gpointer * data)
{
  GstAiurDemux *demux = (GstAiurDemux *) data;

  g_mutex_lock (demux->runmutex);

  while (demux->running) {
    aiurdemux_push_task (demux);
  }

  g_mutex_unlock (demux->runmutex);
}


static gboolean
aiurdemux_sink_activate_push (GstPad * sinkpad, gboolean active)
{
  GstAiurDemux *demux = GST_AIURDEMUX (GST_PAD_PARENT (sinkpad));

  demux->pullbased = FALSE;

  if (active) {
    demux->running = TRUE;
    g_thread_create ((GThreadFunc) aiurdemux_loop_push,
        (gpointer) demux, FALSE, NULL);
  } else {
    demux->running = FALSE;
    gst_aiur_stream_cache_close (demux->stream_cache);
    /* make sure task is closed */
    g_mutex_lock (demux->runmutex);
    g_mutex_unlock (demux->runmutex);
    return gst_pad_stop_task (sinkpad);
  }

  return TRUE;
}

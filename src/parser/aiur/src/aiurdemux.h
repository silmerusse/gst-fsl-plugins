/*
 * Copyright (c) 2010-2012, Freescale Semiconductor, Inc. All rights reserved.
 *
 */

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
 * Module Name:    aiurdemux.h
 *
 * Description:    Head file of unified parser gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __AIURDEMUX_H__
#define __AIURDEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/tag/tag.h>

#include "mfw_gst_utils.h"

#include "fsl_parser.h"

#include "aiurregistry.h"
#include "aiurstreamcache.h"
#include "aiuridxtab.h"


G_BEGIN_DECLS GST_DEBUG_CATEGORY_EXTERN (aiurdemux_debug);

#define GST_CAT_DEFAULT aiurdemux_debug

#define GST_TYPE_AIURDEMUX \
  (gst_aiurdemux_get_type())
#define GST_AIURDEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AIURDEMUX,GstAiurDemux))
#define GST_AIURDEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AIURDEMUX,GstAiurDemuxClass))
#define GST_IS_AIURDEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AIURDEMUX))
#define GST_IS_AIURDEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AIURDEMUX))

#define GST_AIURDEMUX_CAST(obj) ((GstAiurDemux *)(obj))

#define GST_AIURDEMUX_MAX_STREAMS         32

typedef struct _GstAiurDemux GstAiurDemux;
typedef struct _GstAiurDemuxClass GstAiurDemuxClass;
typedef struct _AiurDemuxStream AiurDemuxStream;
typedef struct _AiurDemuxProgram AiurDemuxProgram;
typedef struct _AiurDemuxOption
{
  gint audio_max;
  gint video_max;
  gint subtitle_max;
  gboolean multiprogram_enabled;
  guint64 stream_mask;
  guint program_mask;
  gint program_number;
  guint interleave_queue_size;
} AiurDemuxOption;

typedef struct
{
  gint64 length;
  gint64 offset;
  gboolean seekable;
  void *cache;
} AiurDemuxContentDesc;


typedef struct
{
  guint64 duration;
  gboolean seekable;
  uint32 track_num;
  uint32 program_num;
  gboolean live;
  gint read_mode;
  uint32 suggest_read_mode;
  gboolean auto_retimestamp;
} AiurDemuxClipInfo;


typedef struct
{
  gchar *uri;
  gchar *index_file;
  gint64 length;
  gboolean seekable;
} AiurDemuxContentInfo;


typedef enum
{
  AIUR_PLAY_MODE_NORMAL,
  AIUR_PLAY_MODE_TRICK_FORWARD,
  AIUR_PLAY_MODE_TRICK_BACKWARD
} AiurDemuxPlayMode;
typedef enum
{
  TYPE_STRING,
  TYPE_INT,
  TYPE_BOOLEAN,
  TYPE_DOUBLE,
} ConfigValueType;

typedef struct
{
  gboolean import_index;
  gboolean export_index;
  char *index_file_prefix;

  gint retimestamp_threashold_ms;
  gint retimestamp_delay_ms;

  gint cache_stream_preserve_size;
  gint cache_stream_max_size;

  gint cache_local_ways;
  gint cache_local_linesize_shift;

  gdouble max_normal_rate;

  gint max_interleave_second;
  gint max_interleave_bytes;

  gint num_of_samples;

  gboolean drop_sample;
} AiurDemuxConfig;

struct _GstAiurDemux
{
  GstElement element;
  GstPad *sinkpad;

  gint state;
  gint save_state;
  gboolean pullbased;

  GMutex *runmutex;
  gboolean running;

  gint n_streams;
  gint n_video_streams;
  gint n_audio_streams;
  gint n_sub_streams;

  AiurDemuxStream **streams;
  AiurDemuxProgram **programs;

  GstSegment segment;

  gboolean send_global_tags;
  GstTagList *tag_list;

  gboolean pending_event;

  AiurDemuxContentInfo content_info;
  AiurDemuxClipInfo clip_info;

  /* core interface */
  AiurCoreInterface *core_interface;
  FslParserHandle *core_handle;

  GstAiurStreamCache *stream_cache;

  AiurDemuxPlayMode play_mode;

  GstClockTime start_time;
  gint32 discont_check_track;
  guint32 new_segment_mask;
  guint32 valid_mask;
  gboolean check_discont;
  GstClockTime base_offset;

  guint interleave_queue_size;

  AiurDemuxConfig config;
  AiurDemuxOption option;
};

struct _GstAiurDemuxClass
{
  GstElementClass parent_class;
};

GType gst_aiurdemux_get_type (void);


G_END_DECLS
#endif /* __AIURDEMUX_H__ */

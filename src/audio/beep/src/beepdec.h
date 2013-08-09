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
 * Module Name:    beepdec.h
 *
 * Description:    Head file of unified audio decoder gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __BEEPDEC_H__
#define __BEEPDEC_H__

#include <gst/gst.h>
#include <gst/tag/tag.h>

#include "fsl_types.h"
#include "fsl_unia.h"

#include "beepregistry.h"

#include "mfw_gst_utils.h"

G_BEGIN_DECLS GST_DEBUG_CATEGORY_EXTERN (gst_beepdec_debug);
#define GST_CAT_DEFAULT gst_beepdec_debug

#define GST_TYPE_BEEPDEC \
  (gst_beepdec_get_type())
#define GST_BEEPDEC_CAST(obj) ((GstBeepDec *)(obj))
#define GST_BEEPDEC(obj) \
  (GST_BEEPDEC_CAST(obj))

#define GST_BEEP_SUBELEMENT_MAX   (20)


typedef struct
{
  gint64 resync_threshold;
  gboolean reset_when_resync;
  gboolean set_layout;
} BeepDecOption;

typedef struct
{
  guint64 compressed_bytes;
  guint64 uncompressed_samples;
} BeepDecStat;

#ifdef MFW_TIME_PROFILE
typedef struct
{
  GstClockTime decodetime;
} BeepTimeProfileStat;
#endif


typedef struct _GstBeepDec GstBeepDec;
typedef struct _GstBeepDecClass GstBeepDecClass;

struct _GstBeepDec
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  gboolean trydownmix;
  gboolean set_chan_pos;
  UniAcodecOutputPCMFormat outputformat;

  gboolean new_segment;
  gboolean new_buffer_timestamp;
  gint64 segment_start;
  gint64 time_offset;
  gboolean framed;

  gint64 byte_duration;
  gint64 byte_avg_rate;
  gint64 time_duration;

  BeepCoreDlEntry *entry;
  BeepCoreInterface *beep_interface;
  UniACodec_Handle handle;
  gint err_cnt;

  BeepDecStat decoder_stat;
  BeepDecOption options;
#ifdef MFW_TIME_PROFILE
  BeepTimeProfileStat tp_stat;
#endif
};

struct _GstBeepDecClass
{
  GstElementClass parent_class;
  BeepCoreDlEntry *entry;
};

static gint32 alsa_1channel_layout[] = {
  /* FC */
  UA_CHANNEL_FRONT_CENTER,
};

static gint32 alsa_2channel_layout[] = {
  /* FL,FR */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
};

static gint32 alsa_3channel_layout[] = {
  /* FL,FR,LFE */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_LFE
};

static gint32 alsa_4channel_layout[] = {
  /* FL,FR,BL,BR */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
};

static gint32 alsa_5channel_layout[] = {
/* FL,FR,BL,BR,FC*/
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
};

static gint32 alsa_6channel_layout[] = {
/* FL,FR,BL,BR,FC,LFE */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
  UA_CHANNEL_LFE,
};

static gint32 alsa_8channel_layout[] = {
/* FL,FR,BL,BR,FC,LFE,SL,SR */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
  UA_CHANNEL_LFE,
  UA_CHANNEL_SIDE_LEFT,
  UA_CHANNEL_SIDE_RIGHT,
};


static gint32 *alsa_channel_layouts[] = {
  NULL,
  alsa_1channel_layout,         // 1
  alsa_2channel_layout,         // 2
  alsa_3channel_layout,
  alsa_4channel_layout,
  alsa_5channel_layout,
  alsa_6channel_layout,
  NULL,
  alsa_8channel_layout,
};

#endif /* __BEEPDEC_H__ */

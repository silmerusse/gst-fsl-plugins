

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
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
 * Module Name:    mfw_gst_streaming_cache.c
 *
 * Description:    Implementation for streamed based demuxer srcpad cache.
 *
 * Portability:    This code is written for Linux OS.
 */

/*
 * Changelog: 
 *
 */

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "aiurdemux.h"

#define WAIT_COND_TIMEOUT(cond, mutex, timeout) \
    do{\
        GTimeVal now;\
        g_get_current_time(&now);\
        g_time_val_add(&now, (glong)(timeout));\
        g_cond_timed_wait((cond),(mutex),&now);\
    }while(0)


#define READ_ADDR(cache)\
    ((cache)->start+(cache)->offset)

#define AVAIL_BYTES(cache)\
    (gst_adapter_available((cache)->adapter)-(cache)->offset)

#define CHECK_PRESERVE(cache)\
    do {\
        if ((cache)->offset>(cache)->threshold_pre){\
            guint64 flush = ((cache)->offset-(cache)->threshold_pre);\
            gst_adapter_flush((cache)->adapter, flush);\
            (cache)->offset =(cache)->threshold_pre;\
            (cache)->start+=flush;\
            g_cond_signal((cache)->consume_cond);\
        }\
    }while(0)

#define READ_BYTES(cache, buffer, readbytes)\
    do {\
        if (buffer){\
            gst_adapter_copy((cache)->adapter, (buffer), (cache)->offset, (readbytes));\
        }\
        (cache)->offset+=(readbytes);\
        CHECK_PRESERVE(cache);\
    }while(0)



static GstAiurStreamCacheClass *aiur_stream_cache_parent_class = NULL;

static GTimeVal timeout = { 1, 0 };

GType gst_aiur_stream_cache_get_type (void);

void
gst_aiur_stream_cache_finalize (GstAiurStreamCache * cache)
{
  if (cache->pad) {
    gst_object_unref (GST_OBJECT_CAST (cache->pad));
    cache->pad = NULL;
  }

  if (cache->adapter) {
    gst_adapter_clear (cache->adapter);
    gst_object_unref (cache->adapter);
    cache->adapter = NULL;
  }

  if (cache->produce_cond) {
    g_cond_free (cache->produce_cond);
    cache->produce_cond = NULL;
  }

  if (cache->consume_cond) {
    g_cond_free (cache->consume_cond);
    cache->consume_cond = NULL;
  }

  if (cache->mutex) {
    g_mutex_free (cache->mutex);
    cache->mutex = NULL;
  }
}

void
gst_aiur_stream_cache_close (GstAiurStreamCache * cache)
{

  if (cache) {
    cache->closed = TRUE;
  }
}

void
gst_aiur_stream_cache_open (GstAiurStreamCache * cache)
{
  if (cache) {
    cache->closed = FALSE;
  }
}



GstAiurStreamCache *
gst_aiur_stream_cache_new (guint64 threshold_max, guint64 threshold_pre,
    void *context)
{
  GstAiurStreamCache *cache =
      (GstAiurStreamCache *) gst_mini_object_new (GST_TYPE_AIURSTREAMCACHE);

  cache->pad = NULL;

  cache->adapter = gst_adapter_new ();
  cache->mutex = g_mutex_new ();
  cache->consume_cond = g_cond_new ();
  cache->produce_cond = g_cond_new ();

  cache->threshold_max = threshold_max;
  cache->threshold_pre = threshold_pre;

  cache->start = 0;
  cache->offset = 0;
  cache->ignore_size = 0;

  cache->eos = FALSE;
  cache->seeking = FALSE;
  cache->closed = FALSE;

  cache->context = context;

  return cache;
}

void
gst_aiur_stream_cache_attach_pad (GstAiurStreamCache * cache, GstPad * pad)
{
  if (cache) {
    g_mutex_lock (cache->mutex);
    if (cache->pad) {
      gst_object_unref (GST_OBJECT_CAST (cache->pad));
      cache->pad = NULL;
    }

    if (pad) {
      cache->pad = gst_object_ref (GST_OBJECT_CAST (pad));

    }
    g_mutex_unlock (cache->mutex);
  }
}

gint64
gst_aiur_stream_cache_availiable_bytes (GstAiurStreamCache * cache)
{
  gint64 avail = -1;

  if (cache) {
    g_mutex_lock (cache->mutex);
    avail = AVAIL_BYTES (cache);
    g_mutex_unlock (cache->mutex);
  }

  return avail;
}


void
gst_aiur_stream_cache_set_segment (GstAiurStreamCache * cache, guint64 start,
    guint64 stop)
{
  if (cache) {
    g_mutex_lock (cache->mutex);

    cache->seeking = FALSE;
    cache->start = start;
    cache->offset = 0;
    cache->ignore_size = 0;
    gst_adapter_clear (cache->adapter);
    cache->eos = FALSE;

    g_cond_signal (cache->consume_cond);

    g_mutex_unlock (cache->mutex);
  }
}


void
gst_aiur_stream_cache_add_buffer (GstAiurStreamCache * cache,
    GstBuffer * buffer)
{
  guint64 size;
  gint trycnt = 0;
  if ((cache == NULL) || (buffer == NULL))
    goto bail;

  g_mutex_lock (cache->mutex);

  size = GST_BUFFER_SIZE (buffer);

  if ((cache->seeking) || (size == 0)) {
    g_mutex_unlock (cache->mutex);
    goto bail;
  }

  if (cache->ignore_size) {
    /* drop part or total buffer */
    if (cache->ignore_size >= size) {
      cache->ignore_size -= size;
      g_mutex_unlock (cache->mutex);
      goto bail;
    } else {
      GST_BUFFER_DATA (buffer) += (cache->ignore_size);
      GST_BUFFER_SIZE (buffer) -= (cache->ignore_size);
      size = GST_BUFFER_SIZE (buffer);
      cache->ignore_size = 0;
    }
    //g_print("cache offset %lld\n", cache->offset);
  }

  gst_adapter_push (cache->adapter, buffer);
  g_cond_signal (cache->produce_cond);

  buffer = NULL;

  if (cache->threshold_max) {
#if 0
    if (cache->threshold_max < size + cache->threshold_pre) {
      cache->threshold_max = size + cache->threshold_pre;
    }
#endif

    while ((gst_adapter_available (cache->adapter) > cache->threshold_max)
        && (cache->closed == FALSE)) {
      if (((++trycnt) & 0x1f) == 0x0) {
        GST_WARNING ("wait push try %d SIZE %d %lld", trycnt,
            gst_adapter_available (cache->adapter), cache->threshold_max);
      }
      WAIT_COND_TIMEOUT (cache->consume_cond, cache->mutex, 1000000);
    }

    if (cache->seeking) {
      g_mutex_unlock (cache->mutex);
      goto bail;
    }
  }


  g_mutex_unlock (cache->mutex);

  return;

bail:
  if (buffer) {
    gst_buffer_unref (buffer);
  }
}

void
gst_aiur_stream_cache_seteos (GstAiurStreamCache * cache, gboolean eos)
{
  if (cache) {
    g_mutex_lock (cache->mutex);
    cache->eos = eos;
    g_cond_signal (cache->produce_cond);
    g_mutex_unlock (cache->mutex);
  }
}


gint64
gst_aiur_stream_cache_get_position (GstAiurStreamCache * cache)
{

  gint64 pos = -1;
  if (cache) {

    g_mutex_lock (cache->mutex);
    pos = READ_ADDR (cache);
    g_mutex_unlock (cache->mutex);
  }
  return pos;
}


gint
gst_aiur_stream_cache_seek (GstAiurStreamCache * cache, guint64 addr)
{
  gboolean ret;

  gint r = 0;


  int isfail = 0;
  if (cache == NULL) {
    return -1;
  }

tryseek:
  g_mutex_lock (cache->mutex);


  if (addr < cache->start) {    /* left */
    GST_ERROR ("Unexpect backward seek addr %lld, cachestart %lld, offset %lld",
        addr, cache->start, cache->offset);
    isfail = 1;
    goto trysendseek;
  } else if (addr <= cache->start + gst_adapter_available (cache->adapter)) {
    if (addr != READ_ADDR (cache)) {
      cache->offset = addr - cache->start;
      CHECK_PRESERVE (cache);
    }

  } else if ((addr > (cache->start + gst_adapter_available (cache->adapter))) && ((addr < cache->start + 2000000) || (isfail))) {       /* right */
    cache->ignore_size =
        addr - cache->start - gst_adapter_available (cache->adapter);

    cache->start = addr;
    cache->offset = 0;
    gst_adapter_clear (cache->adapter);
    g_cond_signal ((cache)->consume_cond);
  } else {
    goto trysendseek;
  }
  g_mutex_unlock (cache->mutex);
  return 0;
#if 1
trysendseek:

  GST_INFO ("stream cache try seek to %lld", addr);

  gst_adapter_clear (cache->adapter);

  cache->offset = 0;
  cache->start = addr;
  cache->ignore_size = 0;


  cache->seeking = TRUE;
  cache->eos = FALSE;
  g_mutex_unlock (cache->mutex);
  ret =
      gst_pad_push_event (cache->pad, gst_event_new_seek ((gdouble) 1,
          GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
          (gint64) addr, GST_SEEK_TYPE_NONE, (gint64) (-1)));
  g_cond_signal (cache->consume_cond);


  if (ret == FALSE) {
    if (isfail == 0) {
      isfail = 1;
      goto tryseek;
    }
    r = -1;
  }
  return r;
#endif
fail:
  g_mutex_unlock (cache->mutex);
  return -1;
}




gint64
gst_aiur_stream_cache_read (GstAiurStreamCache * cache, guint64 size,
    char *buffer)
{
  gint64 readsize = -1;
  gint retrycnt = 0;
  if (cache == NULL) {
    return readsize;
  }

try_read:

  if (cache->closed == TRUE) {
    return readsize;
  }

  g_mutex_lock (cache->mutex);

  if (cache->seeking == TRUE)
    goto not_enough_bytes;

  if ((cache->threshold_max)
      && (cache->threshold_max < size + cache->threshold_pre)) {
    cache->threshold_max = size + cache->threshold_pre;
    /* enlarge maxsize means consumed */
    g_cond_signal (cache->consume_cond);
  }

  if (size > AVAIL_BYTES (cache)) {
    if (cache->eos) {           /* not enough bytes when eos */
      readsize = AVAIL_BYTES (cache);
      if (readsize) {
        READ_BYTES (cache, buffer, readsize);
      }
      goto beach;
    }
    goto not_enough_bytes;
  }

  readsize = size;
  READ_BYTES (cache, buffer, readsize);

  goto beach;


not_enough_bytes:
  //g_print("not enough %lld, try %d\n", size, retrycnt++);
  WAIT_COND_TIMEOUT (cache->produce_cond, cache->mutex, 1000000);
  g_mutex_unlock (cache->mutex);

  goto try_read;


beach:
  g_mutex_unlock (cache->mutex);
  return readsize;
}



static void
gst_aiur_stream_cache_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  aiur_stream_cache_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_aiur_stream_cache_finalize;
}


GType
gst_aiur_stream_cache_get_type (void)
{

  static GType aiur_stream_cache_type = 0;
  if (G_UNLIKELY (aiur_stream_cache_type == 0)) {
    static const GTypeInfo aiurdemux_stream_cache_info = {
      sizeof (GstAiurStreamCacheClass),
      NULL,
      NULL,
      gst_aiur_stream_cache_class_init,
      NULL,
      NULL,
      sizeof (GstAiurStreamCache),
      0,
      NULL,
      NULL
    };
    aiur_stream_cache_type =
        g_type_register_static (GST_TYPE_MINI_OBJECT, "AiurStreamCache",
        &aiurdemux_stream_cache_info, 0);
  }

  return aiur_stream_cache_type;
}



AiurLocalCacheLine *
gst_aiur_local_cacheline_new ()
{
  AiurLocalCacheLine *line = g_new0 (AiurLocalCacheLine, 1);
  return line;
}

void
gst_aiur_local_cacheline_free (AiurLocalCacheLine * line)
{
  if (line) {
    if (line->gstbuf) {
      gst_buffer_unref (line->gstbuf);
      line->gstbuf = NULL;
    }
    g_free (line);
  }
}

AiurLocalCacheLine *
gst_aiur_local_cache_pick_cacheline (AiurLocalCache * cache)
{
  AiurLocalCacheLine *line;
  if (cache->ways >= cache->max_ways) {
    line = cache->head;
    if (line->next) {
      line->next->prev = NULL;

    }

    cache->head = line->next;
    cache->ways--;
    if (line->gstbuf) {
      gst_buffer_unref (line->gstbuf);
      line->gstbuf = NULL;
    }
    //    printf("switch out %lld\n", line->read_through);
    line->prev = line->next = NULL;
    line->read_through = 0;
    line->eos = FALSE;
  } else {
    line = gst_aiur_local_cacheline_new ();
  }
  return line;
}


AiurLocalCacheLine *
gst_aiur_local_cacheline_fill (AiurLocalCache * cache,
    AiurLocalCacheLine * line, guint64 address)
{
  GstFlowReturn ret;

  address &= cache->address_mask;
  ret =
      gst_pad_pull_range (cache->pad, address, cache->cacheline_size,
      &line->gstbuf);
  if (ret == GST_FLOW_OK) {
    line->address = address;
    line->size = GST_BUFFER_SIZE (line->gstbuf);
    if (line->size < cache->cacheline_size)
      line->eos = TRUE;

    if (cache->head) {
      cache->tail->next = line;
      line->prev = cache->tail;
      cache->tail = line;
    } else {
      cache->head = cache->tail = line;
    }
    cache->ways++;

  } else {
    gst_aiur_local_cacheline_free (line);
    line = NULL;
  }
  return line;
}

AiurLocalCacheLine *
gst_aiur_local_cacheline_find (AiurLocalCache * cache, guint64 address)
{
  AiurLocalCacheLine *line = cache->tail;
  while (line) {
    if (line->address == address) {
      return line;
    }
    line = line->prev;
  }
  return line;
}


gint
gst_aiur_local_cache_read (AiurLocalCache * cache, guint64 address, gint size,
    char *buf)
{
  gint read_size = 0, len, offset;
  AiurLocalCacheLine *line;

  if (size == 0)
    return read_size;

  do {
    if ((line =
            gst_aiur_local_cacheline_find (cache,
                (address & cache->address_mask))) == NULL) {
      line = gst_aiur_local_cache_pick_cacheline (cache);
      if (line) {
        line = gst_aiur_local_cacheline_fill (cache, line, address);

      }

    }

    if (line) {
      offset = (address & cache->offset_mask);
      len = line->size - offset;
      if (len > size)
        len = size;
      memcpy (buf, GST_BUFFER_DATA (line->gstbuf) + offset, len);
      address += len;
      size -= len;
      read_size += len;
      buf += len;
      line->read_through += len;
    } else {
      read_size = 0;
      break;
    }
  } while ((size) && (line->eos == FALSE));

  return read_size;
}




AiurLocalCache *
gst_aiur_local_cache_new (GstPad * pad, gint max_ways, gint cachesize_shift)
{
  AiurLocalCache *cache;
  cache = g_new0 (AiurLocalCache, 1);
  if (cache) {
    cache->offset_mask = ((guint64) 1 << cachesize_shift) - 1;
    cache->cacheline_size = (1 << cachesize_shift);
    cache->address_mask = cache->offset_mask ^ G_MAXUINT64;
    cache->max_ways = max_ways;
    cache->pad = pad;
  }
  return cache;
}

void
gst_aiur_local_cache_free (AiurLocalCache * cache)
{
  if (cache) {
    AiurLocalCacheLine *linenext, *line = cache->head;
    while (line) {
      linenext = line->next;
      gst_aiur_local_cacheline_free (line);
      line = linenext;
    }

    g_free (cache);
  }
}

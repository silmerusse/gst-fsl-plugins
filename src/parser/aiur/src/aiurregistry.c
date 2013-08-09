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
 * Module Name:    aiurregistry.c
 *
 * Description:    Implementation of utils functions for registry for
 *                 unified parser core functions
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "aiurdemux.h"


#define CORE_QUERY_INTERFACE_API_NAME "FslParserQueryInterface"

#ifdef _ARM11
#define AIUR_REGISTRY_FILE_DEFAULT "/usr/share/aiur_registry.arm11.cf"
#endif

#ifdef _ARM9
#define AIUR_REGISTRY_FILE_DEFAULT "/usr/share/aiur_registry.arm9.cf"
#endif
#define AIUR_REGISTRY_FILE_ENV_NAME "AIUR_REGISTRY"

#define KEY_LIB "library"
#define KEY_MIME "mime"



static AiurCoreDlEntry *g_aiur_core_entry = NULL;

/* id table for all core apis, the same order with AiurCoreInterface */
uint32 aiur_core_interface_id_table[] = {
  PARSER_API_GET_VERSION_INFO,
  PARSER_API_CREATE_PARSER,
  PARSER_API_DELETE_PARSER,

  PARSER_API_INITIALIZE_INDEX,
  PARSER_API_IMPORT_INDEX,
  PARSER_API_EXPORT_INDEX,

  PARSER_API_IS_MOVIE_SEEKABLE,
  PARSER_API_GET_MOVIE_DURATION,
  PARSER_API_GET_USER_DATA,
  PARSER_API_GET_META_DATA,

  PARSER_API_GET_NUM_TRACKS,

  PARSER_API_GET_NUM_PROGRAMS,
  PARSER_API_GET_PROGRAM_TRACKS,

  PARSER_API_GET_TRACK_TYPE,
  PARSER_API_GET_DECODER_SPECIFIC_INFO,
  PARSER_API_GET_TRACK_DURATION,
  PARSER_API_GET_LANGUAGE,
  PARSER_API_GET_BITRATE,

  PARSER_API_GET_VIDEO_FRAME_WIDTH,
  PARSER_API_GET_VIDEO_FRAME_HEIGHT,
  PARSER_API_GET_VIDEO_FRAME_RATE,

  PARSER_API_GET_AUDIO_NUM_CHANNELS,
  PARSER_API_GET_AUDIO_SAMPLE_RATE,
  PARSER_API_GET_AUDIO_BITS_PER_SAMPLE,

  PARSER_API_GET_AUDIO_BLOCK_ALIGN,
  PARSER_API_GET_AUDIO_CHANNEL_MASK,
  PARSER_API_GET_AUDIO_BITS_PER_FRAME,

  PARSER_API_GET_TEXT_TRACK_WIDTH,
  PARSER_API_GET_TEXT_TRACK_HEIGHT,

  PARSER_API_GET_READ_MODE,
  PARSER_API_SET_READ_MODE,

  PARSER_API_ENABLE_TRACK,

  PARSER_API_GET_NEXT_SAMPLE,
  PARSER_API_GET_NEXT_SYNC_SAMPLE,

  PARSER_API_GET_FILE_NEXT_SAMPLE,
  PARSER_API_GET_FILE_NEXT_SYNC_SAMPLE,

  PARSER_API_SEEK,
};


AiurCoreDlEntry *
aiur_config_to_dllentry (GKeyFile * keyfile, gchar * group)
{
  AiurCoreDlEntry *entry = MM_MALLOC (sizeof (AiurCoreDlEntry));

  if (entry) {
    char *value;
    entry->name = g_strdup (group);
    entry->dl_name = g_key_file_get_string (keyfile, group, KEY_LIB, NULL);
    entry->mime = g_key_file_get_string (keyfile, group, KEY_MIME, NULL);
    entry->next = NULL;


    if ((!entry->name) || (!entry->dl_name) || (!entry->mime)) {
      g_print ("goto failed\n");
      goto fail;
    }

  }
  return entry;

fail:
  if (entry) {
    if (entry->dl_name)
      g_free (entry->dl_name);
    if (entry->mime)
      g_free (entry->mime);
    if (entry->name)
      g_free (entry->name);
    MM_FREE (entry);
    entry = NULL;
  }
  return entry;

}


AiurCoreDlEntry *
aiur_get_dll_entry_from_file (char *filename)
{
  AiurCoreDlEntry *dlentry = NULL, *entry;

  GKeyFile *keyfile = g_key_file_new ();
  gchar **groups, **group;

  if ((keyfile == NULL)
      || (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE,
              NULL)))
    goto fail;

  group = groups = g_key_file_get_groups (keyfile, NULL);


  while (*group) {

    entry = aiur_config_to_dllentry (keyfile, *group);

    if (entry) {
      entry->next = dlentry;
      dlentry = entry;
    }

    group++;
  };

  g_strfreev (groups);


fail:
  if (keyfile) {
    g_key_file_free (keyfile);
  }
  return dlentry;

}

void
_free_dll_entry (AiurCoreDlEntry * entry)
{
  AiurCoreDlEntry *next;
  while (entry) {
    next = entry->next;
    if (entry->dl_name)
      g_free (entry->dl_name);
    if (entry->mime)
      g_free (entry->mime);
    if (entry->name)
      g_free (entry->name);
    MM_FREE (entry);
    entry = next;
  }
}

static AiurCoreDlEntry *
aiur_get_core_entry ()
{
  if (g_aiur_core_entry == NULL) {
    char *aiurenv = getenv (AIUR_REGISTRY_FILE_ENV_NAME);
    if (aiurenv == NULL) {
      aiurenv = AIUR_REGISTRY_FILE_DEFAULT;
    }
    g_aiur_core_entry = aiur_get_dll_entry_from_file (aiurenv);
  }
  return g_aiur_core_entry;
}


AiurCoreInterface *
_aiur_core_create_interface_from_entry (AiurCoreDlEntry * dlentry)
{
  AiurCoreInterface *inf = NULL;
  void *dl_handle = NULL;
  int i;
  int32 err;
  int total = G_N_ELEMENTS (aiur_core_interface_id_table);
  void **papi;
  tFslParserQueryInterface query_interface;

  dl_handle = dlopen (dlentry->dl_name, RTLD_LAZY);

  if (!dl_handle) {
    g_print (RED_STR
        ("Demux core %s error or missed! \n(Err: %s)\n",
            dlentry->dl_name, dlerror ()));
    goto fail;
  }

  query_interface = dlsym (dl_handle, CORE_QUERY_INTERFACE_API_NAME);

  if (query_interface == NULL) {
    g_print ("can not find symbol %s\n", CORE_QUERY_INTERFACE_API_NAME);
    goto fail;
  }
  inf = g_new0 (AiurCoreInterface, 1);

  if (inf == NULL)
    goto fail;

  papi = (void **) inf;

  for (i = 0; i < total; i++) {
    err = query_interface (aiur_core_interface_id_table[i], papi);
    if (err) {
      *papi = NULL;
    }
    papi++;
  }

  inf->dl_handle = dl_handle;

  if (inf->getVersionInfo) {
    inf->coreid = (inf->getVersionInfo) ();
    if (inf->coreid) {
      g_print (BLUE_STR
          ("Aiur: %s \nCore: %s\n  mime: %s\n  file: %s\n",
              VERSION, inf->coreid, dlentry->mime, dlentry->dl_name));
    }
  }
  inf->dlentry = dlentry;

  return inf;
fail:
  if (inf) {
    g_free (inf);
    inf = NULL;
  }

  if (dl_handle) {
    dlclose (dl_handle);
  }
  return inf;
}


AiurCoreDlEntry *
_aiur_core_find_match_dlentry (GstCaps * caps)
{
  AiurCoreDlEntry *dlentry = NULL;
  GstCaps *super_caps = NULL;
  AiurCoreDlEntry *pentry = aiur_get_core_entry ();

  while (pentry) {
    super_caps = gst_caps_from_string (pentry->mime);

    if ((super_caps) && (gst_caps_is_subset (caps, super_caps))) {
      dlentry = pentry;
      gst_caps_unref (super_caps);
      break;

    }
    gst_caps_unref (super_caps);
    pentry = pentry->next;
  }
  return dlentry;
}


GstCaps *
aiur_core_get_caps ()
{
  GstCaps *caps = NULL;
  void *dlhandle;
  int i;
  AiurCoreDlEntry *pentry = aiur_get_core_entry ();

  while (pentry) {
    dlhandle = dlopen (pentry->dl_name, RTLD_LAZY);

    if (!dlhandle) {
      GST_WARNING ("Demux core %s missed! (%s)\n", pentry->dl_name, dlerror ());
      pentry = pentry->next;
      continue;
    }

    if (caps) {
      GstCaps *newcaps = gst_caps_from_string (pentry->mime);
      if (newcaps) {
        if (!gst_caps_is_subset (newcaps, caps)) {
          gst_caps_append (caps, newcaps);
        } else {
          gst_caps_unref (newcaps);
        }
      }
    } else {
      caps = gst_caps_from_string (pentry->mime);
    }

    dlclose (dlhandle);
    pentry = pentry->next;
  }
  return caps;
}


AiurCoreInterface *
aiur_core_create_interface_from_caps (GstCaps * caps)
{
  AiurCoreInterface *inf = NULL;
  AiurCoreDlEntry *pentry = _aiur_core_find_match_dlentry (caps);

  if (pentry) {
    inf = _aiur_core_create_interface_from_entry (pentry);
  }
  return inf;
}


void
aiur_core_destroy_interface (AiurCoreInterface * inf)
{
  if (inf == NULL)
    return;

  if (inf->dl_handle) {
    dlclose (inf->dl_handle);
  }

  g_free (inf);
}


void __attribute__ ((destructor)) aiur_free_dll_entry (void);


void
aiur_free_dll_entry ()
{
  if (g_aiur_core_entry) {
    _free_dll_entry (g_aiur_core_entry);
    g_aiur_core_entry = NULL;
  }

  MM_DEINIT_DBG_MEM ();
}

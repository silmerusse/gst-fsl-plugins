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
 * Module Name:    beepregistry.c
 *
 * Description:    Implementation of utils functions for registry for
 *                 unified audio decoder core functions
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

#include "beepdec.h"


#define CORE_QUERY_INTERFACE_API_NAME "UniACodecQueryInterface"

#ifdef _ARM11
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry.arm11.cf"
#elif defined (_ARM9)
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry.arm9.cf"
#else
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry.arm12.cf"
#endif



#define BEEP_REGISTRY_FILE_ENV_NAME "BEEP_REGISTRY"

#define KEY_LIB "library"
#define KEY_MIME "mime"
#define KEY_RANK "rank"
#define KEY_LONGNAME "longname"
#define KEY_DESCRIPTION "description"


#define BEEP_DEFAULT_RANK FSL_GST_DEFAULT_DECODER_RANK


static BeepCoreDlEntry *g_beep_core_entry = NULL;

/* id table for all core apis, the same order with BeepCoreInterface */
uint32 beep_core_interface_id_table[] = {
  ACODEC_API_GET_VERSION_INFO,
  ACODEC_API_CREATE_CODEC,
  ACODEC_API_DELETE_CODEC,
  ACODEC_API_RESET_CODEC,
  ACODEC_API_SET_PARAMETER,
  ACODEC_API_GET_PARAMETER,
  ACODEC_API_DEC_FRAME,
};

static gchar *
beep_strip_blank (gchar * str)
{
  gchar *ret = NULL;
  if (str) {
    while ((*str == ' ') && (*str != '\0')) {
      str++;
    }
    if (*str != '\0') {
      ret = str;
    }
  }
  return ret;
}

BeepCoreDlEntry *
beep_config_to_dllentry (GKeyFile * keyfile, gchar * group)
{
  BeepCoreDlEntry *entry = MM_MALLOC (sizeof (BeepCoreDlEntry));

  if (entry) {
    char *value;
    gint num;
    memset (entry, 0, sizeof (BeepCoreDlEntry));
    entry->name = g_strdup (group);
    entry->dl_names =
        g_key_file_get_string_list (keyfile, group, KEY_LIB, &num, NULL);
    entry->mime = g_key_file_get_string (keyfile, group, KEY_MIME, NULL);

    if (g_key_file_has_key (keyfile, group, KEY_LONGNAME, NULL)) {
      entry->longname =
          g_key_file_get_string (keyfile, group, KEY_LONGNAME, NULL);
    }
    if (g_key_file_has_key (keyfile, group, KEY_DESCRIPTION, NULL)) {
      entry->description =
          g_key_file_get_string (keyfile, group, KEY_DESCRIPTION, NULL);
    }
    if (g_key_file_has_key (keyfile, group, KEY_RANK, NULL)) {
      entry->rank = g_key_file_get_integer (keyfile, group, KEY_RANK, NULL);
    } else {
      entry->rank = BEEP_DEFAULT_RANK;
    }
    entry->next = NULL;


    if ((!entry->name) || (!entry->dl_names) || (!entry->mime)) {
      GST_ERROR ("beep config file corrupt, please check %s",
          BEEP_REGISTRY_FILE_DEFAULT);
      goto fail;
    }

  }
  return entry;

fail:
  if (entry) {
    if (entry->dl_names)
      g_strfreev (entry->dl_names);
    if (entry->mime)
      g_free (entry->mime);
    if (entry->name)
      g_free (entry->name);
    if (entry->longname)
      g_free (entry->longname);
    if (entry->description)
      g_free (entry->description);
    MM_FREE (entry);
    entry = NULL;
  }
  return entry;

}


BeepCoreDlEntry *
beep_get_dll_entry_from_file (char *filename)
{
  BeepCoreDlEntry *dlentry = NULL, *entry;

  GKeyFile *keyfile = g_key_file_new ();
  gchar **groups, **group;

  if ((keyfile == NULL)
      || (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE,
              NULL)))
    goto fail;

  group = groups = g_key_file_get_groups (keyfile, NULL);


  while (*group) {

    entry = beep_config_to_dllentry (keyfile, *group);

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
_free_dll_entry (BeepCoreDlEntry * entry)
{
  BeepCoreDlEntry *next;
  while (entry) {
    next = entry->next;
    if (entry->dl_names)
      g_strfreev (entry->dl_names);
    if (entry->mime)
      g_free (entry->mime);
    if (entry->name)
      g_free (entry->name);
    if (entry->longname)
      g_free (entry->longname);
    if (entry->description)
      g_free (entry->description);
    MM_FREE (entry);
    entry = next;
  }
}

BeepCoreDlEntry *
beep_get_core_entry ()
{
  if (g_beep_core_entry == NULL) {
    char *beepenv = getenv (BEEP_REGISTRY_FILE_ENV_NAME);
    if (beepenv == NULL) {
      beepenv = BEEP_REGISTRY_FILE_DEFAULT;
    }
    g_beep_core_entry = beep_get_dll_entry_from_file (beepenv);
  }
  return g_beep_core_entry;
}


BeepCoreInterface *
_beep_core_create_interface_from_entry (BeepCoreDlEntry * dlentry)
{
  BeepCoreInterface *inf = NULL;
  void *dl_handle = NULL;
  int i;
  int32 err;
  int total = G_N_ELEMENTS (beep_core_interface_id_table);
  void **papi;
  tUniACodecQueryInterface query_interface;

  i = 0;
  while ((dl_handle == NULL)
      && (dlentry->dl_name = beep_strip_blank (dlentry->dl_names[i++]))) {
    dl_handle = dlopen (dlentry->dl_name, RTLD_LAZY);
  };

  if (!dl_handle) {
    GST_ERROR ("Demux core %s error or missed! \n(Err: %s)\n",
        dlentry->dl_name, dlerror ());
    goto fail;
  }

  query_interface = dlsym (dl_handle, CORE_QUERY_INTERFACE_API_NAME);

  if (query_interface == NULL) {
    GST_ERROR ("can not find symbol %s\n", CORE_QUERY_INTERFACE_API_NAME);
    goto fail;
  }
  inf = g_new0 (BeepCoreInterface, 1);

  if (inf == NULL)
    goto fail;

  papi = (void **) inf;

  for (i = 0; i < total; i++) {
    err = query_interface (beep_core_interface_id_table[i], papi);
    if (err) {
      *papi = NULL;
    }
    papi++;
  }

  inf->dl_handle = dl_handle;

  if (inf->getVersionInfo) {
    const char *version = (inf->getVersionInfo) ();
    if (version) {
      g_print (BLUE_STR
          ("Beep: %s \nCore: %s\n  mime: %s\n  file: %s\n",
              VERSION, version, dlentry->mime, dlentry->dl_name));
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


BeepCoreDlEntry *
_beep_core_find_match_dlentry (GstCaps * caps)
{
  BeepCoreDlEntry *dlentry = NULL;
  GstCaps *super_caps = NULL;
  BeepCoreDlEntry *pentry = beep_get_core_entry ();

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
beep_core_get_cap (BeepCoreDlEntry * entry)
{
  GstCaps *caps = NULL;
  void *dlhandle;
  int i;

  if (entry) {
    dlhandle = NULL;
    i = 0;
    while ((dlhandle == NULL)
        && (entry->dl_name = beep_strip_blank (entry->dl_names[i++]))) {
      dlhandle = dlopen (entry->dl_name, RTLD_LAZY);
    };

    if (dlhandle) {
      caps = gst_caps_from_string (entry->mime);
      dlclose (dlhandle);
    }
  }
  return caps;
}



GstCaps *
beep_core_get_caps ()
{
  GstCaps *caps = NULL;
  void *dlhandle;
  int i;
  BeepCoreDlEntry *pentry = beep_get_core_entry ();

  while (pentry) {

    if (caps) {
      GstCaps *newcaps = beep_core_get_cap (pentry);
      if (newcaps) {
        if (!gst_caps_is_subset (newcaps, caps)) {
          gst_caps_append (caps, newcaps);
        } else {
          gst_caps_unref (newcaps);
        }
      }
    } else {
      caps = beep_core_get_cap (pentry);
    }

    pentry = pentry->next;
  }
  return caps;
}


BeepCoreInterface *
beep_core_create_interface_from_caps (GstCaps * caps)
{
  BeepCoreInterface *inf = NULL;
  BeepCoreDlEntry *pentry = _beep_core_find_match_dlentry (caps);

  if (pentry) {
    inf = _beep_core_create_interface_from_entry (pentry);
    if ((inf) && (!inf->createDecoder)) {
      beep_core_destroy_interface (inf);
      inf = NULL;
    }
  }
  return inf;
}


void
beep_core_destroy_interface (BeepCoreInterface * inf)
{
  if (inf == NULL)
    return;

  if (inf->dl_handle) {
    dlclose (inf->dl_handle);
  }

  g_free (inf);
}


void __attribute__ ((destructor)) beepregistry_c_destructor (void);


void
beepregistry_c_destructor ()
{
  if (g_beep_core_entry) {
    _free_dll_entry (g_beep_core_entry);
    g_beep_core_entry = NULL;
  }

  MM_DEINIT_DBG_MEM ();
}

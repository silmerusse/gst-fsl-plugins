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
* Module Name:    gstsutils.c
*
* Description:    simple utils for gst plugins
*
* Portability:    This code is written for Linux OS and Gstreamer
*/

/*
* Changelog:
*
*/

#include "gstsutils.h"

static gboolean
g_string_to_boolean (const gchar * str)
{
  if ((str) && ((strcmp (str, "true") == 0) || (strcmp (str, "TRUE") == 0))) {
    return TRUE;
  }
  return FALSE;
}


void
gstsutils_options_install_properties_by_options (GstsutilsOptionEntry * table,
    GObjectClass * oclass)
{
#define GSTSUTILS_INSTALL_PROPERTY(basetype) \
    do { \
        g_object_class_install_property (oclass, p->id, \
            g_param_spec_##basetype (p->name, p->nickname, \
               p->desc, \
               (g##basetype) g_ascii_strtoll (p->min, NULL, 0), \
               (g##basetype) g_ascii_strtoll (p->max, NULL, 0), \
               (g##basetype) g_ascii_strtoll (p->def, NULL, 0), G_PARAM_READWRITE)); \
    }while(0)

  GstsutilsOptionEntry *p = table;
  while (p->id >= 0) {
    switch (p->gtype) {
      case (G_TYPE_BOOLEAN):
        g_object_class_install_property (oclass, p->id,
            g_param_spec_boolean (p->name, p->nickname,
                p->desc, g_string_to_boolean (p->def), G_PARAM_READWRITE));
        break;
      case (G_TYPE_INT):
        GSTSUTILS_INSTALL_PROPERTY (int);
        break;
      case (G_TYPE_UINT):
        GSTSUTILS_INSTALL_PROPERTY (uint);
        break;
      case (G_TYPE_LONG):
        GSTSUTILS_INSTALL_PROPERTY (long);
        break;
      case (G_TYPE_ULONG):
        GSTSUTILS_INSTALL_PROPERTY (ulong);
        break;
      case (G_TYPE_INT64):
        GSTSUTILS_INSTALL_PROPERTY (int64);
        break;
      case (G_TYPE_UINT64):
        GSTSUTILS_INSTALL_PROPERTY (uint64);
        break;
      case (G_TYPE_DOUBLE):
        g_object_class_install_property (oclass, p->id,
            g_param_spec_double (p->name, p->nickname,
                p->desc,
                g_strtod (p->min, NULL),
                g_strtod (p->max, NULL),
                g_strtod (p->def, NULL), G_PARAM_READWRITE));
        break;

      case (G_TYPE_STRING):
        g_object_class_install_property (oclass, p->id,
            g_param_spec_string (p->name, p->nickname, p->desc, p->def,
                G_PARAM_READWRITE));
        break;
      case (G_TYPE_ENUM):
      {
        g_object_class_install_property (oclass, p->id,
            g_param_spec_enum (p->name, p->nickname, p->desc, p->typefunc (),
                (gint) (g_ascii_strtoll (p->def, NULL, 0)), G_PARAM_READWRITE));

        break;
      }
      default:
        break;
    };
    p++;
  };
}


static GstsutilsOptionEntry *
gstsutils_options_search_option_by_id (GstsutilsOptionEntry * table, gint id)
{
  if (table) {
    while (table->id != -1) {
      if (table->id == id) {
        return table;
      }
      table++;
    }
  }
  return NULL;
}


void
gstsutils_set_value (gchar * target, GstsutilsOptionEntry * p,
    const gchar * svalue)
{
#define GSTSUTILS_CHECK_AND_SET_VALUE(basetype) \
      do { \
        if (svalue){ \
          g##basetype value = (g##basetype) g_ascii_strtoll(svalue, NULL, 0); \
          if ((value>=(g##basetype) g_ascii_strtoll(p->min, NULL, 0)) \
              && (value<=(g##basetype) g_ascii_strtoll(p->max, NULL, 0))){ \
              *(g##basetype *)(target+p->offset) = value; \
          } \
        } \
      }while(0)
  switch (p->gtype) {
    case (G_TYPE_BOOLEAN):
      if (svalue) {
        *(gboolean *) (target + p->offset) = g_string_to_boolean (svalue);
      }
      break;
    case (G_TYPE_INT):
      GSTSUTILS_CHECK_AND_SET_VALUE (int);
      break;
    case (G_TYPE_UINT):
      GSTSUTILS_CHECK_AND_SET_VALUE (uint);
      break;
    case (G_TYPE_LONG):
      GSTSUTILS_CHECK_AND_SET_VALUE (long);
      break;
    case (G_TYPE_ULONG):
      GSTSUTILS_CHECK_AND_SET_VALUE (ulong);
      break;
    case (G_TYPE_INT64):
      GSTSUTILS_CHECK_AND_SET_VALUE (int64);
      break;
    case (G_TYPE_UINT64):
      GSTSUTILS_CHECK_AND_SET_VALUE (uint64);
      break;
    case (G_TYPE_DOUBLE):
    {
      if (svalue) {
        gdouble value = g_strtod (svalue, NULL);
        if ((value >= g_strtod (p->min, NULL))
            && (value <= g_strtod (p->max, NULL))) {
          *(gdouble *) (target + p->offset) = value;
        }
      }
      break;
    }
    case (G_TYPE_STRING):
    {
      gchar *value = *(gchar **) (target + p->offset);
      if (value) {
        g_free (value);
        *(gchar **) (target + p->offset) = NULL;
      }
      if (svalue) {
        value = g_strdup (svalue);
        if (value) {
          *(gchar **) (target + p->offset) = value;
        }
      }
      break;
    }
    case (G_TYPE_ENUM):
    {
      gint value = (gint) g_ascii_strtoll (svalue, NULL, 0);
      *(gint *) (target + p->offset) = value;
      break;
    }
    default:
      break;
  }
}


gboolean
gstsutils_options_get_option (GstsutilsOptionEntry * table, gchar * option,
    guint id, GValue * value)
{
#define GSTSUTILS_GETVALUE(basetype) \
    do { \
        g_value_set_##basetype (value, *(g##basetype *) (option + p->offset)); \
    }while(0)

  gboolean ret = TRUE;
  if ((value) && (option)) {
    GstsutilsOptionEntry *p = gstsutils_options_search_option_by_id (table, id);
    if (p) {
      switch (p->gtype) {
        case (G_TYPE_BOOLEAN):
          GSTSUTILS_GETVALUE (boolean);
          break;
        case (G_TYPE_INT):
          GSTSUTILS_GETVALUE (int);
          break;
        case (G_TYPE_UINT):
          GSTSUTILS_GETVALUE (uint);
          break;
        case (G_TYPE_LONG):
          GSTSUTILS_GETVALUE (ulong);
          break;
        case (G_TYPE_ULONG):
          GSTSUTILS_GETVALUE (ulong);
          break;
        case (G_TYPE_INT64):
          GSTSUTILS_GETVALUE (int64);
          break;
        case (G_TYPE_UINT64):
          GSTSUTILS_GETVALUE (uint64);
          break;
        case (G_TYPE_DOUBLE):
          GSTSUTILS_GETVALUE (double);
          break;
        case (G_TYPE_STRING):
          g_value_set_string (value, *(gchar **) (option + p->offset));
          break;
        case (G_TYPE_ENUM):
          g_value_set_enum (value, *(gint *) (option + p->offset));
          break;
        default:
          ret = FALSE;
          break;
      };
    }
  } else {
    ret = FALSE;
  }
  return ret;
}


gboolean
gstsutils_options_set_option (GstsutilsOptionEntry * table, gchar * option,
    guint id, const GValue * value)
{
#define GSTSUTILS_SETVALUE(basetype)\
    do {\
        *(g##basetype *)(option + p->offset) = g_value_get_##basetype (value); \
    }while(0)
  gboolean ret = TRUE;
  if ((value) && (option)) {
    GstsutilsOptionEntry *p = gstsutils_options_search_option_by_id (table, id);
    if (p) {
      switch (p->gtype) {
        case (G_TYPE_BOOLEAN):
          GSTSUTILS_SETVALUE (boolean);
          break;
        case (G_TYPE_INT):
          GSTSUTILS_SETVALUE (int);
          break;
        case (G_TYPE_UINT):
          GSTSUTILS_SETVALUE (uint);
          break;
        case (G_TYPE_LONG):
          GSTSUTILS_SETVALUE (long);
          break;
        case (G_TYPE_ULONG):
          GSTSUTILS_SETVALUE (ulong);
          break;
        case (G_TYPE_INT64):
          GSTSUTILS_SETVALUE (int64);
          break;
        case (G_TYPE_UINT64):
          GSTSUTILS_SETVALUE (uint64);
          break;
        case (G_TYPE_DOUBLE):
          GSTSUTILS_SETVALUE (double);
          break;
        case (G_TYPE_STRING):
          gstsutils_set_value (option, p, g_value_get_string (value));
          break;
        case (G_TYPE_ENUM):
          *(gint *) (option + p->offset) = g_value_get_enum (value);
          break;
        default:
          ret = FALSE;
          break;
      };
    }
  } else {
    ret = FALSE;
  }
  return ret;
}


void
gstsutils_options_load_default (GstsutilsOptionEntry * table, gchar * option)
{
  if ((table) && (option)) {
    GstsutilsOptionEntry *p = table;
    while (p->id != -1) {
      gstsutils_set_value (option, p, p->def);
      p++;
    }
  }
}

gboolean
gstsutils_options_load_from_keyfile (GstsutilsOptionEntry * table,
    gchar * option, gchar * filename, gchar * group)
{
  GKeyFile *keyfile = NULL;
  gboolean ret = FALSE;

  if (filename == NULL)
    goto bail;

  if ((option) && (keyfile = g_key_file_new ())
      && (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL))) {
    GstsutilsOptionEntry *p = table;

    while (p->id != -1) {
      if (g_key_file_has_key (keyfile, group, p->name, NULL)) {
        gchar *svalue = g_key_file_get_value (keyfile, group, p->name, NULL);
        gstsutils_set_value (option, p, svalue);
        if (svalue) {
          g_free (svalue);
        }
      }
      p++;
    };
    ret = TRUE;
  }

bail:
  if (keyfile) {
    g_key_file_free (keyfile);
  }
  return ret;
}


gboolean
gstsutils_elementutil_get_int (gchar * filename, gchar * group,
    gchar * field, gint * value)
{
  gboolean ret = FALSE;
  GKeyFile *keyfile = NULL;

  if ((filename) && (field) && (value)) {
    if ((keyfile = g_key_file_new ())
        && (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE,
                NULL))) {
      if (g_key_file_has_key (keyfile, group, field, NULL)) {
        *value = g_key_file_get_integer (keyfile, group, field, NULL);
        ret = TRUE;
      }
    }

  }

  if (keyfile) {
    g_key_file_free (keyfile);
  }
  return ret;
}

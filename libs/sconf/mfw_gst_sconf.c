/*
 * Copyright (c) 2010-2012, Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    sconf.c
 *
 * Description:    Implementation for simple config load.
 *
 * Portability:    This code is written for Linux OS.
 */

/*
 * Changelog: 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mfw_gst_sconf.h"


typedef struct
{
  FILE *fp;
  int cache_valid;
  char cache[200];
} ConfigDesc;


#define GO_NEXTLINE \
     cd->cache_valid = 0;\
     goto nextline;

char *
_adjust_str (char *str)
{
  char *newstr = NULL;
  char *buf;
  while ((*str != '\0') && (*str != '\n') && ((*str == ' ') || *str == '\t')) {
    str++;
  }
  if (*str == '\0')
    goto bail;

  if (*str == '\"') {
    str++;
    buf = str;
    while ((*str != '\0') && (*str != '\n') && (*str != '\"')) {
      str++;
    }
    if ((*str == '\0') || (*str == '\n'))
      goto bail;

    *str = '\0';
  } else {
    buf = str;
    while ((*str != '\0') && (*str != '\n') && (*str != ' ') && (*str != '#'))
      str++;
    *str = '\0';
  }

  if (strlen (buf)) {
    newstr = malloc (strlen (buf) + 1);
    if (newstr) {
      strcpy (newstr, buf);
    }
  }

bail:
  return newstr;
}



ConfigSection *
_sconf_get_next_section (ConfigDesc * cd)
{
  int finished = 0;
  ConfigSection *cs = malloc (sizeof (ConfigSection));
  ConfigProperty *last_cd = NULL;
  char *buf = NULL;
  char *name = NULL;
  char *property = NULL;
  char *value = NULL;

  if (cs == NULL)
    goto bail;

  cs->name = NULL;
  cs->property = NULL;
  cs->next = NULL;

  while (finished == 0) {
  nextline:
    if (cd->cache_valid == 0) {
      buf = fgets (cd->cache, 200, cd->fp);

    } else
      buf = cd->cache;
    if (!buf) {
      break;
    }
    cd->cache_valid = 1;
    while ((*buf != '\0') && (*buf != '\n')) {
      switch (*buf) {
        case '#':
          GO_NEXTLINE;
          break;
        case '[':
          if ((cs->name == NULL)) {
            name = buf + 1;
            buf++;
            while ((*buf != '\0') && (*buf != ']'))
              buf++;
            if (*buf == ']') {
              *buf = '\0';
              cs->name = _adjust_str (name);
            }
            GO_NEXTLINE;
          } else {
            goto bail;
          }
          break;
        case '=':
          if ((cs->name) && (property == NULL)) {
            property = cd->cache;
            *buf = '\0';
            value = buf + 1;
            property = _adjust_str (property);
            value = _adjust_str (value);
            if ((property) && (value)) {
              ConfigProperty *cp = malloc (sizeof (ConfigProperty));
              cp->name = property;
              cp->value = value;
              cp->next = NULL;
              if (last_cd == NULL) {
                cs->property = cp;
              } else {
                last_cd->next = cp;
              }
              last_cd = cp;

            } else {
              if (property)
                free (property);
              if (value)
                free (value);
            }
            property = NULL;
            value = NULL;
            GO_NEXTLINE;
          } else {
            GO_NEXTLINE;
          }
          break;
      }
      buf++;
    }
    GO_NEXTLINE;
  }

bail:
  if ((cs->name == NULL) || (cs->property == NULL)) {
    sconf_free_cs (cs);
    cs = NULL;

    if (buf) {
      goto nextline;
    }

  }
  return cs;
}

char *
sconf_cs_get_field (ConfigSection * cs, const char *field, int copy)
{
  char *value = NULL;
  ConfigProperty *cp = cs->property;
  while (cp) {
    if (strcmp (cp->name, field) == 0) {
      if (copy) {
        value = malloc (strlen (cp->value) + 1);
        strcpy (value, cp->value);
      } else {
        value = cp->value;
      }
      break;

    }
    cp = cp->next;
  }
  return value;

}


char *
sconf_cs_get_name (ConfigSection * cs, int copy)
{
  char *value = NULL;
  if (copy) {
    value = malloc (strlen (cs->name) + 1);
    strcpy (value, cs->name);
  } else {
    value = cs->name;
  }
  return value;
}


ConfigSection *
sconf_get_css_from_file (const char *filename)
{
  ConfigDesc cd;
  cd.cache_valid = 0;
  ConfigSection *cs = NULL, *tail, *tmp;

  cd.fp = fopen (filename, "r");
  if (cd.fp == NULL)
    goto fail;

  while ((tmp = _sconf_get_next_section (&cd)) != NULL) {
    if (cs == NULL) {
      cs = tmp;

    } else {
      tail->next = tmp;
    }
    tail = tmp;

  };

  fclose (cd.fp);

fail:
  return cs;

}



void
sconf_free_cs (ConfigSection * cs)
{
  ConfigProperty *cp, *cpnext;
  ConfigSection *csnext;

  while (cs) {
    csnext = cs->next;
    if (cs->name)
      free (cs->name);

    cp = cs->property;
    while (cp) {
      cpnext = cp->next;
      if (cp->name)
        free (cp->name);
      if (cp->value)
        free (cp->value);
      free (cp);
      cp = cpnext;
    }
    free (cs);
    cs = csnext;
  }
}

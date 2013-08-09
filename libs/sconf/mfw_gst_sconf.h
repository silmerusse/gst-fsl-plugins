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
 * Module Name:    mfw_gst_sconf.h
 *
 * Description:    Head file for simple config load.
 *
 * Portability:    This code is written for Linux OS.
 */

/*
 * Changelog: 
 *
 */

#ifndef __SCONF_H__
#define __SCONF_H__


typedef struct _ConfigProperty
{
  char *name;
  char *value;
  struct _ConfigProperty *next;
} ConfigProperty;

typedef struct _ConfigSection
{
  char *name;
  ConfigProperty *property;
  struct _ConfigSection *next;
} ConfigSection;

ConfigSection *sconf_get_css_from_file (const char *filename);
char *sconf_cs_get_name (ConfigSection * cs, int copy);
char *sconf_cs_get_field (ConfigSection * cs, const char *field, int copy);
void sconf_free_cs (ConfigSection * cs);


#endif

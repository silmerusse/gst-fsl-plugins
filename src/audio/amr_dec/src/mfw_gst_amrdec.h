/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_amrdec.h
 *
 * Description:    Gstreamer plugin for AMR-NB AMR-WB and AMR-WB+ decoder
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*============================================================================
                            INCLUDE FILES
=============================================================================*/

#include <gst/gst.h>

#ifndef __MFW_GST_AMRDEC_H__
#define __MFW_GST_AMRDEC_H__

/*=============================================================================
                                           CONSTANTS
=============================================================================*/

/* None. */

/*=============================================================================
                                             ENUMS
=============================================================================*/

/* None. */

/*=============================================================================
                                            MACROS
=============================================================================*/
G_BEGIN_DECLS
#define MFW_GST_TYPE_AMRDEC \
  (mfw_gst_amrdec_get_type())
#define MFW_GST_AMRDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_AMRDEC,\
   MfwGstAmrdec))
#define MFW_GST_AMRDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_AMRDEC,MfwGstAmrdecClass))
#define MFW_GST_IS_AMRDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_AMRDEC))
#define MFW_GST_IS_AMRDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_AMRDEC))
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
typedef struct _MfwGstAmrdecPrivate MfwGstAmrdecPrivate;

typedef struct _MfwGstAmrdec {
    GstElement element;
    /*< private >*/
    MfwGstAmrdecPrivate *priv;
} MfwGstAmrdec;


typedef struct _MfwGstAmrdecClass {
    GstElementClass parent_class;
} MfwGstAmrdecClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_amrdec_get_type(void);

G_END_DECLS
/*===========================================================================*/
#endif				/* __MFW_GST_AMRDEC_H__ */

/*
 * Copyright (c) 2009-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_downmix.h
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Jan 20 2009 Sario HU <b01138@freescale.com>
 * - Initial version
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#ifndef __MFW_GST_DOWNMIX_H__
#define __MFW_GST_DOWNMIX_H__

#include <gst/gst.h>

/*=============================================================================
                                           CONSTANTS
=============================================================================*/
/* None. */

/*=============================================================================
                                             ENUMS
=============================================================================*/
/* plugin property ID */
enum{
    PROPER_ID_OUTPUT_CHANNEL_NUMBER = 1,
};

/*=============================================================================
                                            MACROS
=============================================================================*/
G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_DOWNMIX \
    (mfw_gst_downmix_get_type())
#define MFW_GST_DOWNMIX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_DOWNMIX, MfwGstDownMix))
#define MFW_GST_DOWNMIX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_DOWNMIX, MfwGstDownMixClass))
#define MFW_GST_IS_DOWNMIX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_DOWNMIX))
#define MFW_GST_IS_DOWNMIX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_DOWNMIX))

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
typedef struct _MfwGstDownMix
{
    GstElement element;
    GstPad *sinkpad, *srcpad;
    gboolean capsSet;
    gboolean init;
    DM_Decode_Config * dmConfig;
    PPP_INPUTPARA *pppInput;
    PPP_INFO *pppInfo;
    guint disiredOutChannels;
}MfwGstDownMix;

typedef struct _MfwGstDownMixClass 
{
    GstElementClass parent_class;
}MfwGstDownMixClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/
/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/
GType mfw_gst_downmix_get_type (void);

G_END_DECLS

/*===========================================================================*/

#endif /* __MFW_GST_DOWNMIX_H__ */

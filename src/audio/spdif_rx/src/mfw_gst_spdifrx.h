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
 * Module Name:    mfw_gst_spdifrx.h
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Feb 16 2009 Dexter Ji <b01140@freescale.com>
 * - Initial version
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#ifndef __MFW_GST_SPDIFRX_H__
#define __MFW_GST_SPDIFRX_H__

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
#define MFW_GST_TYPE_SPDIFRX \
    (mfw_gst_spdifrx_get_type())
#define MFW_GST_SPDIFRX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_SPDIFRX, MfwGstSpdifRX))
#define MFW_GST_SPDIFRX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_SPDIFRX, MfwGstSpdifRXClass))
#define MFW_GST_IS_SPDIFRX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_SPDIFRX))
#define MFW_GST_IS_SPDIFRX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_SPDIFRX))


#define BURST_NULL_DATA     0
#define BURST_AC_3_DATA     1
#define BURST_PAUSE         3
#define BURST_MPEG1_1       4
#define BURST_MPEG1_2_3     5
#define BURST_MPEG2_WO_EXT  5
#define BURST_MPEG2_WI_EXT  6

#define BURST_DEFALUT_VAL   0x00

enum {
    SPDIF_STATE_NULL,
    SPDIF_STATE_SYNC1,
    SPDIF_STATE_SYNC2,
    SPDIF_STATE_TYPE,
    SPDIF_STATE_DATA,
};
typedef enum {
    MEDIA_TYPE_AC3,
}MEDIA_TYPE;

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
typedef struct _burst_struct
{
   guint16 pa;
   guint16 pb;
   guint16 pc;
   guint16 pd;
} burst_struct;



typedef struct _MfwGstSpdifRX
{
    GstElement element;
    GstPad *sinkpad, *srcpad;
    gboolean capsSet;
    gboolean init;
    GstAdapter * pInAdapt;
    GstAdapter * pOutAdapt;
    gint sync1,sync2;
    GstCaps *src_caps;
    gint status;
    MEDIA_TYPE media_type;
    burst_struct *def_preamble;
    gint format;
    gboolean format_detected;
    gint width;
    
}MfwGstSpdifRX;

typedef struct _MfwGstSpdifRXClass 
{
    GstElementClass parent_class;
}MfwGstSpdifRXClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/
/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/
GType mfw_gst_spdifrx_get_type (void);

G_END_DECLS

/*===========================================================================*/

#endif /* __MFW_GST_SPDIFRX_H__ */

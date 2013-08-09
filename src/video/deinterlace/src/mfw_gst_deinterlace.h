/*
 *  Copyright (c) 2008-2012, Freescale Semiconductor, Inc.
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
 * Module Name:    mfw_gst_deinterlace.h
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Mar 12 2008 Dexter JI <b01140@freescale.com>
 * - Initial version
 */



/*=============================================================================
                            INCLUDE FILES
=============================================================================*/

#ifndef __MFW_GST_DEINTERLACE_H__
#define __MFW_GST_DEINTERLACE_H__

#include <gst/gst.h>

typedef unsigned char BYTE;
typedef int            BOOL;


#include "deinterlace_api.h"

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

#define DEFALUT_MAX_COUNT 0

/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_DEINTERLACE \
  (mfw_gst_deinterlace_get_type())
#define MFW_GST_DEINTERLACE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_DEINTERLACE, MfwGstDeinterlace))
#define MFW_GST_DEINTERLACE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_DEINTERLACE, MfwGstDeinterlaceClass))
#define MFW_GST_IS_DEINTERLACE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_DEINTERLACE))
#define MFW_GST_IS_DEINTERLACE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_DEINTERLACE))


/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

/* GstBuffer Deinterlace management entity */
typedef struct _DeIntBufMgt
{
    GSList *head;
    int   flag;
    int   count;
    int   max_count;
}DEINTBUFMGT;

typedef struct _MfwGstDeinterlace
{
    GstElement element;
    GstPad *sinkpad, *srcpad;
    gboolean caps_set;

    DEINTER deint_info; /* The FSL deinterlace main structure */
    DEINTMETHOD deint_method; /* The FSL deinterlace method structure */

    /* Buffer management strategy */
    struct _DeIntBufMgt deint_buf_mgt;
    /* First or new segment flag */
    gboolean is_newsegment;
    /* The odd/even flag of frame for pass through mode */
    gboolean odd_frame;

    gint cr_left_bypixel;       /* crop left offset set by decoder in caps */
    gint cr_right_bypixel;      /* crop right offset set by decoder in caps */
    gint cr_top_bypixel;        /* crop top offset set by decoder in caps */
    gint cr_bottom_bypixel;     /* crop bottom offset set by decoder in caps */

    gint y_crop_width;          /* cropped y width */
    gint uv_crop_width;         /* cropped uv width */

    gint y_crop_height;         /* cropped y height */
    gint uv_crop_height;        /* cropped uv height */

    gint y_width;               /* y width */
    gint uv_width;              /* uv width */
    
    gint y_height;              /* y height */
    gint uv_height;             /* uv height */

    gint required_buf_num;        /* Required buffer number */

    gint fourcc;
    /* The pass through mode flag
     * The pass through mode will deinterlace the buffer to SINK element
     *   without considering the reference frame case.
     * Non-passthrough mode will check the buffer flag to decide reuse
     *   it or copy it to one new buffer.
     */
    gboolean pass_through;

    // Research usage, 
    // a way to let outside control deinterlacing process. Set NULL
    void *dynamic_params; 

    gboolean silent;
}MfwGstDeinterlace;

typedef struct _MfwGstDeinterlaceClass 
{
     GstElementClass parent_class;
}MfwGstDeinterlaceClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_deinterlace_get_type (void);

G_END_DECLS

/*===========================================================================*/

#endif /* __MFW_GST_DEINTERLACE_H__ */

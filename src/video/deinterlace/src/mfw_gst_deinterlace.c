/*
 * Copyright (c) 2008-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_deinterlace.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>
#include "mfw_gst_utils.h"
#include "mfw_gst_deinterlace.h"

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* Filter signals and args */
enum {
  /* FILL ME */
  LAST_SIGNAL
};

enum {
    ARG_0,
    ARG_CHROM_FMT, /* Chroma format. 4:2:0 (0), 4:2:2 (1), 4:4:4 (2) */
    ARG_TOP_FIRST, /* Top filed first or not */
    ARG_MAX_BUF_COUNT, /* Maximum buffer count */
    /* The pass through mode flag
     * The pass through mode will deinterlace the buffer to SINK element
     *   without considering the reference frame case.
     * Non-passthrough mode will check the buffer flag to decide reuse
     *   it or copy it to one new buffer.
     */
    ARG_PASSTHROUGH, 
    ARG_METHOD
};

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/


/*
static GstStaticPadTemplate mfw_deinterlace_src_factory = 
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("I420"))
    );

static GstStaticPadTemplate mfw_deinterlace_sink_factory = 
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("I420"))
    );
*/

#define MFW_GST_DEINTERLACE_CAPS    \
    "video/x-raw-yuv"                                          

static GstStaticPadTemplate mfw_deinterlace_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(MFW_GST_DEINTERLACE_CAPS)
);

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/

/* None. */

/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/

/* None. */

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/

static void	mfw_gst_deinterlace_class_init	 (gpointer klass);
static void	mfw_gst_deinterlace_base_init	 (gpointer klass);
static void	mfw_gst_deinterlace_init	 (MfwGstDeinterlace *filter,
                                                  gpointer gclass);

static void	mfw_gst_deinterlace_set_property (GObject *object, guint prop_id,
                                                  const GValue *value,
					          GParamSpec *pspec);
static void	mfw_gst_deinterlace_get_property (GObject *object, guint prop_id,
                                                  GValue *value,
						  GParamSpec *pspec);

static gboolean mfw_gst_deinterlace_set_caps (GstPad *pad, GstCaps *caps);
static gboolean mfw_gst_deinterlace_sink_event(GstPad * pad,
					      GstEvent * event);
static GstFlowReturn mfw_gst_deinterlace_chain (GstPad *pad, GstBuffer *buf);
static GstPadLinkReturn mfw_gst_deinterlace_link( GstPad * pad, 
           const GstCaps * caps);

/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/
static GstElementClass *parent_class = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
FUNCTION:    src_templ
        
DESCRIPTION: Generate the source pad template.    

IMPORTANT NOTES:
   	    None
=============================================================================*/

static GstPadTemplate *src_templ(void)
{
    static GstPadTemplate *templ = NULL;

    if (!templ) {
        GstCaps *caps;
        GstStructure *structure;
        GValue list = { 0 }
            , fps = {
            0}
            , fmt = {
            0};
        char *fmts[] = { "YV12", "I420",  NULL };
        guint n;

        caps = gst_caps_new_simple("video/x-raw-yuv",
            "format", GST_TYPE_FOURCC,
            GST_MAKE_FOURCC('I', '4', '2', '0'),
            "width", GST_TYPE_INT_RANGE, 16, 4096,
            "height", GST_TYPE_INT_RANGE, 16, 4096,
            NULL);

        structure = gst_caps_get_structure(caps, 0);


        g_value_init(&list, GST_TYPE_LIST);
        g_value_init(&fmt, GST_TYPE_FOURCC);
        for (n = 0; fmts[n] != NULL; n++) {
        gst_value_set_fourcc(&fmt, GST_STR_FOURCC(fmts[n]));
        gst_value_list_append_value(&list, &fmt);
        }
        gst_structure_set_value(structure, "format", &list);
        g_value_unset(&list);
        g_value_unset(&fmt);

        templ =
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
    }
    return templ;
}

/*=============================================================================
FUNCTION:    mfw_deinterlace_passthrough
        
DESCRIPTION: De-interlace the buffer without check the buffer flag and pass it.    
ARGUMENTS PASSED:
		filter   -    pointer to MfwGstDeinterlace structure.
		buf      -    pointer to gst buffer.
RETURN VALUE:
        GST_FLOW_OK - Success.
        Others      - Failed.
IMPORTANT NOTES:
   	    None
=============================================================================*/

static GstFlowReturn 
mfw_deinterlace_passthrough(MfwGstDeinterlace *filter,GstBuffer *buf)
{
    DEINTER * pdeint_info;
    PICTURE *pfrm;
    GstFlowReturn ret = GST_FLOW_OK;
    GSList *head;
    char * tmp;


    DEINTBUFMGT *deint_buf_mgt;
    
    pdeint_info = &filter->deint_info;
    deint_buf_mgt = &filter->deint_buf_mgt;


    
    /* De-interlace the frame buffer when get two buffers
     *   the previous gst buffer is kept in DEINTBUFMGT structure.
     */

    if (filter->odd_frame == FALSE) {

        pdeint_info->method = DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_4TAP;

        pfrm = &pdeint_info->frame[1];
        tmp = (char *) GST_BUFFER_DATA((GstBuffer *)deint_buf_mgt->head->data);

        pfrm->y = tmp
            + filter->y_crop_width * filter->cr_top_bypixel
            + filter->cr_left_bypixel;
        pfrm->cb = tmp 
            + filter->y_crop_width*filter->y_crop_height
            + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
            + (filter->cr_left_bypixel>>1);
        pfrm->cr = (filter->fourcc==GST_STR_FOURCC("NV12")?NULL:(tmp 
            + filter->y_crop_width*filter->y_crop_height
            + filter->uv_crop_height*filter->uv_crop_width
            + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
            + (filter->cr_left_bypixel>>1)));

        GST_DEBUG("%p, %d, %d, %d, %d",pfrm->cr, 
            filter->y_crop_width*filter->y_crop_height, 
            filter->uv_crop_height*filter->uv_crop_width,
            filter->uv_crop_width * filter->cr_top_bypixel,
            filter->cr_left_bypixel);

        pfrm = &pdeint_info->frame[2];
        tmp = (char *) GST_BUFFER_DATA(buf);
        
        pfrm->y = tmp
            + filter->y_crop_width * filter->cr_top_bypixel
            + filter->cr_left_bypixel;
        pfrm->cb = tmp 
            + filter->y_crop_width*filter->y_crop_height
            + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
            + (filter->cr_left_bypixel>>1);
        pfrm->cr = (filter->fourcc==GST_STR_FOURCC("NV12")?NULL:(tmp 
            + filter->y_crop_width*filter->y_crop_height
            + filter->uv_crop_height*filter->uv_crop_width
            + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
            + (filter->cr_left_bypixel>>1)));

        GST_DEBUG("%p, %d, %d, %d, %d",pfrm->cr, 
            filter->y_crop_width*filter->y_crop_height, 
            filter->uv_crop_height*filter->uv_crop_width,
            filter->uv_crop_width * filter->cr_top_bypixel,
            filter->cr_left_bypixel);

        DeinterlaceSafe(pdeint_info);

        ret = gst_pad_push (filter->srcpad, 
            (GstBuffer *)deint_buf_mgt->head->data);     
        deint_buf_mgt->head->data = buf;
    }
    else { // Even frame coming.

        ret = gst_pad_push (filter->srcpad, 
            (GstBuffer *)deint_buf_mgt->head->data);
        deint_buf_mgt->head->data = buf;
    }
    
    if (filter->odd_frame)
        filter->odd_frame = FALSE;
    else
        filter->odd_frame = TRUE;

    return ret;

}

/*=============================================================================
FUNCTION:  mfw_deinterlace_simple
        
DESCRIPTION: De-interlace the buffer with BOB method and pass it.    
ARGUMENTS PASSED:
		filter   -    pointer to MfwGstDeinterlace structure.
		buf      -    pointer to gst buffer.
RETURN VALUE:
        GST_FLOW_OK - Success.
        Others      - Failed.
IMPORTANT NOTES:
   	    None
=============================================================================*/

static GstFlowReturn 
mfw_deinterlace_simple(MfwGstDeinterlace *filter,GstBuffer *buf)
{
    DEINTER * pdeint_info;
    PICTURE *pfrm;
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;
    char * tmp;

    GstFlowReturn ret = GST_FLOW_OK;

    pdeint_info = &filter->deint_info;


    pdeint_info->method = DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_BOB;
    pfrm = &pdeint_info->frame[1];
    
    tmp = (char *) GST_BUFFER_DATA(buf);

    pfrm->y = tmp
        + filter->y_crop_width * filter->cr_top_bypixel
        + filter->cr_left_bypixel;
    pfrm->cb = tmp 
        + filter->y_crop_width*filter->y_crop_height
        + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
        + (filter->cr_left_bypixel>>1);
    pfrm->cr = (filter->fourcc==GST_STR_FOURCC("NV12")?NULL:(tmp 
        + filter->y_crop_width*filter->y_crop_height
        + filter->uv_crop_height*filter->uv_crop_width
        + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
        + (filter->cr_left_bypixel>>1)));


    DeinterlaceSafe(pdeint_info);
    ret = gst_pad_push (filter->srcpad, buf);
    if (ret != GST_FLOW_OK)
        GST_ERROR("Push data failed.");

    return ret;
}

/*=============================================================================
FUNCTION:  mfw_deinterlace
        
DESCRIPTION: De-interlace the buffer with 4TAP method and pass it.    
ARGUMENTS PASSED:
		filter   -    pointer to MfwGstDeinterlace structure.
		buf1      -    pointer to gst buffer.
		buf2      -    pointer to gst buffer.
RETURN VALUE:
        GST_FLOW_OK - Success.
        Others      - Failed.
IMPORTANT NOTES:
   	    None
=============================================================================*/

static GstFlowReturn 
mfw_deinterlace(MfwGstDeinterlace *filter,GstBuffer *buf1, GstBuffer *buf2)
{
    DEINTER * pdeint_info;
    PICTURE *pfrm;
    GstFlowReturn ret1 = GST_FLOW_OK,ret2 = GST_FLOW_OK;
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;
    char * tmp;

    pdeint_info = &filter->deint_info;


    pdeint_info->method = DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_4TAP;
    
    pfrm = &pdeint_info->frame[1];

    tmp = (char *) GST_BUFFER_DATA(buf1);

    pfrm->y = tmp
        + filter->y_crop_width * filter->cr_top_bypixel
        + filter->cr_left_bypixel;
    pfrm->cb = tmp 
        + filter->y_crop_width*filter->y_crop_height
        + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
        + (filter->cr_left_bypixel>>1);
    pfrm->cr = (filter->fourcc==GST_STR_FOURCC("NV12")?NULL:(tmp 
        + filter->y_crop_width*filter->y_crop_height
        + filter->uv_crop_height*filter->uv_crop_width
        + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
        + (filter->cr_left_bypixel>>1)));

    
    pfrm = &pdeint_info->frame[2];

    tmp = (char *) GST_BUFFER_DATA(buf2);

    pfrm->y = tmp
        + filter->y_crop_width * filter->cr_top_bypixel
        + filter->cr_left_bypixel;
    pfrm->cb = tmp 
        + filter->y_crop_width*filter->y_crop_height
        + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
        + (filter->cr_left_bypixel>>1);
    pfrm->cr = (filter->fourcc==GST_STR_FOURCC("NV12")?NULL:(tmp 
        + filter->y_crop_width*filter->y_crop_height
        + filter->uv_crop_height*filter->uv_crop_width
        + filter->uv_crop_width * (filter->cr_top_bypixel>>1)
        + (filter->cr_left_bypixel>>1)));

    DeinterlaceSafe(pdeint_info);


    ret1 = gst_pad_push (filter->srcpad, buf1);
    ret2 = gst_pad_push (filter->srcpad, buf2);


    if ( (ret1 == GST_FLOW_OK) && (ret2 == GST_FLOW_OK))
        return GST_FLOW_OK;
    else {
        GST_ERROR("Push data failed.");
        return GST_FLOW_ERROR;
    }
}


/*=============================================================================
FUNCTION:  mfw_deinterlace
        
DESCRIPTION: De-interlace the gst buffer if it is possible.    
ARGUMENTS PASSED:
		filter   -    pointer to MfwGstDeinterlace structure.
RETURN VALUE:
        GST_FLOW_OK - Success.
        Others      - Failed.
IMPORTANT NOTES:
   	    None
=============================================================================*/
static GstFlowReturn 
mfw_try_to_deinterlace(MfwGstDeinterlace *filter)
{
    DEINTER * pdeint_info;
    PICTURE *pfrm;
    GstFlowReturn ret = GST_FLOW_OK;
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;
    GstBuffer * buf1, * buf2, *buf3, *buf4;

    if (deint_buf_mgt->count < 2)
        return GST_FLOW_OK;
    
    
    buf1 = (GstBuffer *)(deint_buf_mgt->head->data);
    buf2 = (GstBuffer *)(deint_buf_mgt->head->next->data);

    while((gst_buffer_is_metadata_writable(buf1)) &&
        gst_buffer_is_metadata_writable(buf2))
    {   

        deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head, buf1);
        deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head, buf2);

        deint_buf_mgt->count -= 2;

try_nocopy: 

        ret = mfw_deinterlace(filter, buf1, buf2);
        if (ret != GST_FLOW_OK)
            return ret;
        
        if (deint_buf_mgt->count < 2) 
            return GST_FLOW_OK;

        buf1 = (GstBuffer *)(deint_buf_mgt->head->data);
        buf2 = (GstBuffer *)(deint_buf_mgt->head->next->data);

    }
    
    if (deint_buf_mgt->count > deint_buf_mgt->max_count){


        buf1 = (GstBuffer *)(deint_buf_mgt->head->data);
        buf2 = (GstBuffer *)(deint_buf_mgt->head->next->data);


        if (!gst_buffer_is_metadata_writable(buf1)) {

            ret = gst_pad_alloc_buffer_and_set_caps(filter->srcpad, 0, 
               GST_BUFFER_SIZE(buf1), gst_buffer_get_caps(buf1), 
               &buf3);

            if (ret != GST_FLOW_OK) {
                GST_DEBUG("No buffer to get from source pad.");
                return ret;
            }

            GST_BUFFER_TIMESTAMP(buf3) = GST_BUFFER_TIMESTAMP(buf1);
            memcpy(GST_BUFFER_DATA(buf3),GST_BUFFER_DATA(buf1),
                GST_BUFFER_SIZE(buf1));

            deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head, buf1);

            gst_buffer_unref(buf1);
            buf1 = buf3;

        }
        else {
            deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head, buf1);
        }
        if (!gst_buffer_is_metadata_writable(buf2)) {

            ret = gst_pad_alloc_buffer_and_set_caps(filter->srcpad, 0, 
               GST_BUFFER_SIZE(buf2), gst_buffer_get_caps(buf2), 
               &buf4);

            if (ret != GST_FLOW_OK) {
                GST_DEBUG("No buffer to get from source pad.");
                return ret;
            }
            GST_BUFFER_TIMESTAMP(buf4) = GST_BUFFER_TIMESTAMP(buf2);
            memcpy(GST_BUFFER_DATA(buf4),GST_BUFFER_DATA(buf2),
                GST_BUFFER_SIZE(buf2));

            deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head, buf2);

            gst_buffer_unref(buf2);
            buf2 = buf4;


        }
        else {
            deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head, buf2);
        }
        /* try to deinterlace */
        deint_buf_mgt->count -= 2; 
        goto try_nocopy; 
        
    }
    return ret;

}
static void
mfw_gst_deinterlace_set_property (GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    MfwGstDeinterlace *filter = MFW_GST_DEINTERLACE (object);
    DEINTER * pdeint_info = &filter->deint_info;
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;
    switch (prop_id)
    {
    case ARG_CHROM_FMT:
        pdeint_info->chrom_fmt = g_value_get_int (value);
        break;
    case ARG_TOP_FIRST:
        pdeint_info->top_first = g_value_get_boolean(value);
        break;  
    case ARG_MAX_BUF_COUNT:
        deint_buf_mgt->max_count = g_value_get_int (value);
        if (deint_buf_mgt->max_count > filter->required_buf_num)
            deint_buf_mgt->max_count = filter->required_buf_num;
        
        break;
    /* FIXME: The method is currently fixed */
    case ARG_METHOD:
        // pdeint_info->method = g_value_get_int (value);
        break;
    case ARG_PASSTHROUGH:
        filter->pass_through = g_value_get_boolean(value);
        GST_DEBUG(" filter->pass_through = %d",filter->pass_through);
        break;  

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}

static void
mfw_gst_deinterlace_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    MfwGstDeinterlace *filter = MFW_GST_DEINTERLACE (object);
    DEINTER * pdeint_info = &filter->deint_info;
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;

    switch (prop_id) {
    case ARG_CHROM_FMT:
        g_value_set_int (value, pdeint_info->chrom_fmt);
        break;
    case ARG_TOP_FIRST:
        g_value_set_boolean(value, pdeint_info->top_first);
        break;  
    case ARG_MAX_BUF_COUNT:
        g_value_set_int (value, deint_buf_mgt->max_count);
        break;

    case ARG_METHOD:
        g_value_set_int (value, pdeint_info->method);
        break;
    case ARG_PASSTHROUGH:
        g_value_set_boolean(value, filter->pass_through);
        break;          
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
mfw_gst_deinterlace_set_caps (GstPad *pad, GstCaps *caps)
{
    MfwGstDeinterlace *filter;
    GstPad *otherpad;
    DEINTER * pdeint_info;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    DEINTBUFMGT *deint_buf_mgt;


    
    filter = MFW_GST_DEINTERLACE (gst_pad_get_parent (pad));

    pdeint_info = &filter->deint_info;
    deint_buf_mgt = &filter->deint_buf_mgt;
    gst_structure_get_int (structure, "width", &filter->y_crop_width);
    gst_structure_get_int (structure, "height", &filter->y_crop_height);

	gst_structure_get_int(structure, CAPS_FIELD_CROP_LEFT,
			      &filter->cr_left_bypixel);
	gst_structure_get_int(structure, CAPS_FIELD_CROP_TOP,
			      &filter->cr_top_bypixel);
	gst_structure_get_int(structure, CAPS_FIELD_CROP_RIGHT,
			      &filter->cr_right_bypixel);
	gst_structure_get_int(structure, CAPS_FIELD_CROP_BOTTOM,
			      &filter->cr_bottom_bypixel);

    gst_structure_get_fourcc(structure, "format", &filter->fourcc);

    filter->y_width = filter->y_crop_width-filter->cr_left_bypixel-filter->cr_right_bypixel;
    filter->uv_width = filter->y_width>>1;

    filter->y_height = filter->y_crop_height-filter->cr_top_bypixel-filter->cr_bottom_bypixel;
    filter->uv_height = filter->y_height>>1;
    
    filter->uv_crop_height = filter->y_crop_height>>1; // filter->uv_height+filter->cr_top_bypixel+filter->cr_bottom_bypixel;
    filter->uv_crop_width = filter->y_crop_width>>1;   //  filter->uv_width + filter->cr_left_bypixel+filter->cr_right_bypixel;
    
    pdeint_info->width = filter->y_width;
    pdeint_info->height = filter->y_height;

    pdeint_info->y_stride = filter->y_crop_width;

    if (filter->fourcc==GST_STR_FOURCC("NV12")){
        g_print("Deinterlacer input format is NV12\n");
        pdeint_info->uv_stride = filter->uv_crop_width<<1;
    }else{
        g_print("Deinterlacer default input format is I420\n");
        pdeint_info->uv_stride = filter->uv_crop_width;
    }


    
    GST_DEBUG("y (%d x %d).",filter->y_width,filter->y_height);
    GST_DEBUG("uv (%d x %d).",filter->uv_width,filter->uv_height);
    GST_DEBUG("crop y(%d x %d).",filter->y_crop_width,filter->y_crop_height);
    GST_DEBUG("crop uv(%d x %d).",filter->uv_crop_width,filter->uv_crop_height);

    if (filter->caps_set == FALSE) {
        GstCaps * othercaps;

    	gst_structure_get_int(structure, "num-buffers-required",
    			      &filter->required_buf_num);

        GST_DEBUG("Deinterlacer set_caps: Get buffer required: %d",filter->required_buf_num);

        // filter->required_buf_num += 2;

        if ((deint_buf_mgt->max_count > filter->required_buf_num) 
            && (filter->required_buf_num != 0))
            deint_buf_mgt->max_count = filter->required_buf_num;

        othercaps = gst_caps_copy(caps);
        gst_caps_set_simple(othercaps, "num-buffers-required",G_TYPE_INT,
            filter->required_buf_num,NULL);
        
        if (!gst_pad_set_caps (filter->srcpad, othercaps)) {
            GST_ERROR("set caps error");
            gst_object_unref(filter);
            return FALSE;
        }
        gst_caps_unref(othercaps);        
        filter->caps_set = TRUE;

    }else {
        if (!gst_pad_set_caps (filter->srcpad, caps)) {
            GST_ERROR("set caps error");
            gst_object_unref(filter);
            return FALSE;
        }

    }
    

    gst_object_unref(filter);

    return TRUE;
}


/*=============================================================================
FUNCTION:   	mfw_gst_deinterlace_sink_event

DESCRIPTION:	Handles an event on the sink pad.

ARGUMENTS PASSED:
		pad        -    pointer to pad
		event      -    pointer to event
RETURN VALUE:
		TRUE       -	if event is sent to sink properly
		FALSE	   -	if event is not sent to sink properly

PRE-CONDITIONS:    None

POST-CONDITIONS:   None

IMPORTANT NOTES:   None
=============================================================================*/
static gboolean mfw_gst_deinterlace_sink_event(GstPad * pad,
					      GstEvent * event)
{

    MfwGstDeinterlace *filter = MFW_GST_DEINTERLACE (GST_PAD_PARENT(pad));
    PICTURE *pfrm;
    DEINTER * pdeint_info;
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;

    gboolean result = TRUE;
    pdeint_info = &filter->deint_info;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
	{
	    GstFormat format;
	    gst_event_parse_new_segment(event, NULL, NULL, &format, NULL,
					NULL, NULL);
	    if (format == GST_FORMAT_TIME) {
    		GST_DEBUG("Came to the FORMAT_TIME call");
            /* Handling the NEW SEGMENT EVENT */
            filter->is_newsegment = TRUE;

            while (deint_buf_mgt->head != NULL) {
            	gst_buffer_unref((GstBuffer *)deint_buf_mgt->head->data);

                deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head,
                    deint_buf_mgt->head->data);
                
            }
            deint_buf_mgt->count = 0;

           
	    } else {
    		GST_DEBUG("Dropping newsegment event in format %s",
    			  gst_format_get_name(format));
    		result = TRUE;
	    }
        result = gst_pad_event_default(pad, event);
	    break;
	}

    case GST_EVENT_EOS:
	{

	    GST_DEBUG("Deinterlacer: Get EOS event");

        if ( (filter->pass_through == TRUE)&&(filter->odd_frame == TRUE) ) {
            /* Only one frame left, we can only use the Bob method */
            result = mfw_deinterlace_simple(filter, 
                (GstBuffer *)deint_buf_mgt->head->data);
            
            deint_buf_mgt->head = 
                g_slist_remove(deint_buf_mgt->head,deint_buf_mgt->head->data);
            

        } else if (filter->pass_through == FALSE) {
            mfw_try_to_deinterlace(filter);
            if (deint_buf_mgt->count > 1) {
                while (deint_buf_mgt->head != NULL) {
                	gst_buffer_unref((GstBuffer *)deint_buf_mgt->head->data);
                    deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head,
                        deint_buf_mgt->head->data);
                    deint_buf_mgt->count--;    
                    
                }
            }
            /* Deinterlace the last frame with Bob method.*/
            if (deint_buf_mgt->count == 1) {
                mfw_deinterlace_simple(filter, deint_buf_mgt->head->data);
                gst_buffer_unref((GstBuffer *)deint_buf_mgt->head->data);
                deint_buf_mgt->head = 
                    g_slist_remove(deint_buf_mgt->head,deint_buf_mgt->head->data);
                deint_buf_mgt->count--;
                
            }

        }

        result = gst_pad_event_default(pad, event);
	    if (result != TRUE) {
    		GST_ERROR("Error in pushing the event, result is %d",
    			  result);
	    }
eos_exit:
        filter->is_newsegment = TRUE;

	    break;
	}
    case GST_STATE_CHANGE_READY_TO_NULL:
	{
        filter->caps_set = FALSE;
        while (deint_buf_mgt->head != NULL) {
        	gst_buffer_unref((GstBuffer *)deint_buf_mgt->head->data);
            deint_buf_mgt->head = g_slist_remove(deint_buf_mgt->head,
                deint_buf_mgt->head->data);
            deint_buf_mgt->count--;    
            
        }	   
        break;
	}
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    break;
	}

    }

    GST_DEBUG("Out of mfw_gst_deinterlace_sink_event() function");
    return result;
}

/*=============================================================================
FUNCTION:    mfw_gst_deinterlace_bufferalloc
        
DESCRIPTION: Pass the buffer allocation request to the source pad.    

IMPORTANT NOTES:
   	    None
=============================================================================*/
static GstFlowReturn
mfw_gst_deinterlace_bufferalloc (GstPad * pad, guint64 offset, guint size, 
    GstCaps * caps, GstBuffer ** buf)
{
    MfwGstDeinterlace *filter;
    GstFlowReturn result = GST_FLOW_OK;


    GstStructure *structure = gst_caps_get_structure(caps, 0);

    filter = MFW_GST_DEINTERLACE (GST_OBJECT_PARENT (pad));

    if (filter->caps_set == FALSE) {
        GstCaps * othercaps;

        othercaps = gst_caps_copy(caps);

    	gst_structure_get_int(structure, "num-buffers-required",
    			      &filter->required_buf_num);

        GST_DEBUG("Get buffer required: %d",filter->required_buf_num);

        filter->required_buf_num += 3;

        GST_DEBUG("Requested buffer count %d.",filter->required_buf_num);

        gst_caps_set_simple(othercaps, "num-buffers-required",G_TYPE_INT,
            filter->required_buf_num,NULL);

        /* Forward to src pad, without setting caps on the src pad */
        result = gst_pad_alloc_buffer (filter->srcpad, offset, size, othercaps, buf);
        if (result != GST_FLOW_OK)
            GST_DEBUG("Allocated buffer from src pad failed.");

        gst_caps_unref(othercaps);
        filter->caps_set = TRUE;
    }
    else {
        /* Forward to src pad, without setting caps on the src pad */
        result = gst_pad_alloc_buffer (filter->srcpad, offset, size, caps, buf);
        if (result != GST_FLOW_OK)
            GST_DEBUG("Allocated buffer from src pad failed.");

    }

    return result;
}


/* chain function
 * this function does the actual processing
 */

static GstFlowReturn
mfw_gst_deinterlace_chain (GstPad *pad, GstBuffer *buf)
{
    MfwGstDeinterlace *filter;
    DEINTMETHOD *pMethod;
    DEINTER * pdeint_info;
    PICTURE *pfrm;
    GstFlowReturn ret = GST_FLOW_OK;
    int count;
    char version[128];
    int i;
    int value;

    g_return_if_fail (pad != NULL);
    g_return_if_fail (buf != NULL);

    filter = MFW_GST_DEINTERLACE (GST_OBJECT_PARENT (pad));
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;

    pdeint_info = &filter->deint_info;


    if (filter->is_newsegment == TRUE) {

        GST_DEBUG("Freescale Deinterlacer version: %s.",GetDeinterlaceVersionInfo());

        pdeint_info->method = DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_4TAP;

        /* Initialize the buffer mgt structure 
         * In pass-through mode, only use the first element to keep the 
         * previous gst buffer.
         */
        if(filter->pass_through) {
            deint_buf_mgt->head = g_slist_append(deint_buf_mgt->head,buf);
            
            filter->odd_frame = FALSE;

        }
        else {
            deint_buf_mgt->head = NULL;
            deint_buf_mgt->head = g_slist_append(deint_buf_mgt->head,buf);

            deint_buf_mgt->count = 1;

        }
        
        filter->is_newsegment = FALSE;
        return GST_FLOW_OK;

    }

    /* Currently only support DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_4TAP mode */
    if (pdeint_info->method == DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_4TAP)
    {
        
        if (filter->pass_through) {
            ret = mfw_deinterlace_passthrough(filter,buf);
        }
        else {

            deint_buf_mgt->head = g_slist_append(deint_buf_mgt->head,buf);

            deint_buf_mgt->count++;
            ret = mfw_try_to_deinterlace(filter);
            if (ret != GST_FLOW_OK)
            {
                GST_ERROR("Try to deinterlace failed.");
            }
        }
 
    }

    return ret;
}

/*=============================================================================
FUNCTION:    mfw_gst_deinterlace_get_type
        
DESCRIPTION:    

ARGUMENTS PASSED:
        
  
RETURN VALUE:
        

PRE-CONDITIONS:
   	    None
 
POST-CONDITIONS:
   	    None

IMPORTANT NOTES:
   	    None
=============================================================================*/

GType
mfw_gst_deinterlace_get_type(void)
{
    static GType mfw_deinterlace_type = 0;

    if (!mfw_deinterlace_type)
    {
        static const GTypeInfo mfw_deinterlace_info =
        {
            sizeof (MfwGstDeinterlaceClass),
            (GBaseInitFunc) mfw_gst_deinterlace_base_init,
            NULL,
            (GClassInitFunc) mfw_gst_deinterlace_class_init,
            NULL,
            NULL,
            sizeof (MfwGstDeinterlace),
            0,
            (GInstanceInitFunc) mfw_gst_deinterlace_init,
        };
        
        mfw_deinterlace_type = g_type_register_static (GST_TYPE_ELEMENT,
            "MfwGstDeinterlace",
            &mfw_deinterlace_info, 
            0
        );
    }
    return mfw_deinterlace_type;
}

static void
mfw_gst_deinterlace_base_init (gpointer klass)
{

    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    gst_element_class_add_pad_template (
        element_class,
        src_templ()
    );
    
    gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&mfw_deinterlace_sink_factory));
    
    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "audio deinterlacer",
        "Filter/Converter/Video", "Deinterlace video raw data");
    
    return;
}

/* Initialize the plugin's class */
static void
mfw_gst_deinterlace_class_init (gpointer klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class = (GObjectClass*) klass;
    gstelement_class = (GstElementClass*) klass;

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = mfw_gst_deinterlace_set_property;
    gobject_class->get_property = mfw_gst_deinterlace_get_property;

    /* CHECKME */
    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        ARG_CHROM_FMT, 
        g_param_spec_int ("chrom_fmt", "chrom fmt", 
        "Chroma format. 4:2:0 (0), 4:2:2 (1), 4:4:4 (2) support.", 
            G_MININT, G_MAXINT,
            0, G_PARAM_READWRITE)
    );       

    /* FIXME: for future usage */
    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        ARG_TOP_FIRST, 
        g_param_spec_boolean ("top_first", "top_first", 
        "top first or not. 1: top first", 
            TRUE, G_PARAM_READWRITE)
    );       

    /* FIXME: for future usage */
    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        ARG_METHOD, 
        g_param_spec_int ("method", "method", 
        "deinterlace method. only support frame sad 4 tap mode",
         G_MININT, G_MAXINT,
            DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_4TAP, 
            G_PARAM_READWRITE)
    );       

    /* CHECKME */
    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        ARG_PASSTHROUGH, 
        g_param_spec_boolean ("pass_through", "pass through", "pass through", 
            TRUE, G_PARAM_READWRITE)
    ); 

    /* CHECKME */
    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        ARG_MAX_BUF_COUNT, 
        g_param_spec_int ("buf_count", "buffer count", 
        "deinterlace maximum buffer count",
         G_MININT, G_MAXINT,
            DEFALUT_MAX_COUNT, 
            G_PARAM_READWRITE)
    ); 
    return;

  
}

/* Initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void 
mfw_gst_deinterlace_init	 (MfwGstDeinterlace *filter,
    gpointer gclass)

{
    DEINTER * pdeint_info = &filter->deint_info;
    DEINTBUFMGT *deint_buf_mgt = &filter->deint_buf_mgt;

    filter->sinkpad = gst_pad_new_from_template (
    gst_static_pad_template_get (
        &mfw_deinterlace_sink_factory), 
        "sink");
    gst_pad_set_setcaps_function (filter->sinkpad, mfw_gst_deinterlace_set_caps);

    filter->srcpad = gst_pad_new_from_template(src_templ(), "src");

    gst_pad_set_setcaps_function (filter->srcpad, mfw_gst_deinterlace_set_caps);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    gst_pad_set_chain_function (filter->sinkpad, mfw_gst_deinterlace_chain);
    /* Set buffer allocation to pass the buffer allocation 
     *   to the SINK element 
     */
    gst_pad_set_bufferalloc_function(filter->sinkpad,
        mfw_gst_deinterlace_bufferalloc);
    

    gst_pad_set_event_function(filter->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_deinterlace_sink_event));

    // Initialize the FSL deinterlace structure.
    pdeint_info->chrom_fmt = 0;
    pdeint_info->top_first = TRUE;
    pdeint_info->method = DEINTMETHOD_BLOCK_VT_GROUP_FRAMESAD_4TAP;
    filter->pass_through = FALSE;
    filter->silent = FALSE;
    filter->is_newsegment = FALSE;
    filter->odd_frame = TRUE;
    filter->cr_left_bypixel = 0;
    filter->cr_right_bypixel = 0;
    filter->cr_top_bypixel = 0;
    filter->cr_bottom_bypixel = 0;
    filter->y_height = 0;
    filter->y_width = 0;
    filter->uv_height = 0;
    filter->uv_width = 0;
    filter->y_crop_height = 0;
    filter->y_crop_width = 0;
    filter->uv_crop_height = 0;
    filter->uv_crop_width = 0;
    filter->caps_set = FALSE;
    
    deint_buf_mgt->max_count = DEFALUT_MAX_COUNT;
    deint_buf_mgt->count = 0;
    deint_buf_mgt->head = NULL;
    return;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 *
 * exchange the string 'plugin' with your elemnt name
 */
static gboolean
plugin_init (GstPlugin *plugin)
{
    return gst_element_register (plugin, "mfw_deinterlacer",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_DEINTERLACE);
}

FSL_GST_PLUGIN_DEFINE("deinterlace", "video de-interlace post-processor", plugin_init);


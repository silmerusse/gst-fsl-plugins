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
 * Module Name:    mfw_gst_ipu_csc.c
 *
 * Description:
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 * Jan 07 2010 Guo Yue <B13906@freescale.com>
 * - Initial version
 */

/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>
#include "mfw_gst_utils.h"
#ifdef MEMORY_DEBUG
#include "mfw_gst_debug.h"
#endif
//#include "src_ppp_interface.h"  /*fsl src ppp*/
#include <fcntl.h>              /* fcntl */
#include <sys/mman.h>           /* mmap  */
#include <sys/ioctl.h>          /* ioctl */


#include "libs/gstnext/gstnext.h"
#include "libs/gstbufmeta/gstbufmeta.h"

// core ipu library

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;


#include "mfw_gst_ipu_csc.h"

#define IPU_PHYSICAL_OUTPUT_BUFFER
#define WALKAROUND_YUY2_YUYV
#define TIME_PROFILE


#define MEMORY_DEVICE_NAME "/dev/mxc_ipu"

#define DEFAULT_OUTPUT_WIDTH 0//640
#define DEFAULT_OUTPUT_HEIGHT 0//480
//#define DEFAULT_OUTPUT_FORMAT 0x30323449 //"I420"
//#define DEFAULT_OUTPUT_CSTYPE CSTYPE_YUV //YUV
#define DEFAULT_OUTPUT_FORMAT 0x50424752 //"RGBP"
#define DEFAULT_OUTPUT_CSTYPE CSTYPE_RGB //RGB

#define CAPS_CSTYPE_YUV "video/x-raw-yuv"
#define CAPS_CSTYPE_RGB "video/x-raw-rgb"
#define CAPS_CSTYPE_GRAY "video/x-raw-gray"

#define IS_DMABLE_BUFFER(buffer) ( (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1])) \
                                 || ( GST_IS_BUFFER(buffer) \
                                 &&  GST_BUFFER_FLAG_IS_SET((buffer),GST_BUFFER_FLAG_LAST)))
#define DMABLE_BUFFER_PHY_ADDR(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]) ? \
                                        ((GstBufferMeta *)(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))->physical_data : \
                                        GST_BUFFER_OFFSET(buffer))

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None. */

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
/* None. */

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/
#define MFW_GST_IPU_CSC_CAPS    \
    "video/x-raw-yuv"

#ifdef MEMORY_DEBUG
    static Mem_Mgr mem_mgr = {0};

#define IPU_CSC_MALLOC( size)\
        dbg_malloc((&mem_mgr),(size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
#define IPU_CSC_FREE( ptr)\
        dbg_free(&mem_mgr, (ptr), "line" STR(__LINE__) "of" STR(__FUNCTION__) )

#else
#define IPU_CSC_MALLOC(size)\
        g_malloc((size))
#define IPU_CSC_FREE( ptr)\
        g_free((ptr))

#endif

#define IPU_CSC_FATAL_ERROR(...) g_print(RED_STR(__VA_ARGS__))
#define IPU_CSC_FLOW(...) g_print(BLUE_STR(__VA_ARGS__))
#define IPU_CSC_FLOW_DEFAULT IPU_CSC_FLOW("%s:%d\n", __FUNCTION__, __LINE__)

#define	GST_CAT_DEFAULT	mfw_gst_ipucsc_debug

typedef struct {
    GstVideoFormat gst_video_format;
    guint ipu_format;
    gchar * mime;
}IPUFormatMapper;
/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/
static IPUFormatMapper g_support_formats[] = {
    {GST_VIDEO_FORMAT_NV12, IPU_PIX_FMT_NV12, GST_VIDEO_CAPS_YUV("NV12")},
    {GST_VIDEO_FORMAT_I420, IPU_PIX_FMT_YUV420P, GST_VIDEO_CAPS_YUV("I420")},
    {GST_VIDEO_FORMAT_UYVY, IPU_PIX_FMT_UYVY, GST_VIDEO_CAPS_YUV("UYUV")},
    {GST_VIDEO_FORMAT_RGB16,IPU_PIX_FMT_RGB565, GST_VIDEO_CAPS_RGB_16},
    {GST_VIDEO_FORMAT_UNKNOWN, -1, NULL}
};


/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_ipucsc_debug);

static void	mfw_gst_ipu_csc_class_init(MfwGstIPUCSCClass * klass);
static void	mfw_gst_ipu_csc_base_init(MfwGstIPUCSCClass * klass);
static void	mfw_gst_ipu_csc_init(MfwGstIPUCSC *filter);

//static void	mfw_gst_ipu_csc_set_property(GObject *object, guint prop_id,
//                                         const GValue *value, GParamSpec *pspec);
//static void	mfw_gst_ipu_csc_get_property(GObject *object, guint prop_id,
//                                         GValue *value, GParamSpec *pspec);

static gboolean mfw_gst_ipu_csc_set_caps (GstBaseTransform * btrans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean mfw_gst_ipu_csc_get_unit_size (GstBaseTransform * btrans,
    GstCaps * caps, guint * size);
static GstFlowReturn mfw_gst_ipu_csc_transform (GstBaseTransform * btrans,
    GstBuffer * inbuf, GstBuffer * outbuf);
#if 0
static GstFlowReturn mfw_gst_ipu_csc_transform_ip (GstBaseTransform * btrans,
    GstBuffer * inbuf);
#endif

static void mfw_gst_ipu_csc_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/

static GstPadTemplate *sinktempl, *srctempl;
static GstElementClass *parent_class = NULL;


/* copies the given caps */
static GstCaps *
mfw_gst_ipu_csc_caps_remove_format_info (GstPadDirection direction, GstCaps * caps)
{
  int i;
  GstStructure *structure;
  GstCaps *rgbcaps;
  GstCaps *graycaps;

  caps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    structure = gst_caps_get_structure (caps, i);

    gst_structure_set_name (structure, "video/x-raw-yuv");
    gst_structure_remove_field (structure, "format");
    gst_structure_remove_field (structure, "endianness");
    gst_structure_remove_field (structure, "depth");
    gst_structure_remove_field (structure, "bpp");
    gst_structure_remove_field (structure, "red_mask");
    gst_structure_remove_field (structure, "green_mask");
    gst_structure_remove_field (structure, "blue_mask");
    gst_structure_remove_field (structure, "alpha_mask");
    gst_structure_remove_field (structure, "palette_data");
    /* Remove width and height to let ipucsc plugin support resize function */
    gst_structure_remove_field (structure, "width");
    gst_structure_remove_field (structure, "height");
  }

  gst_caps_do_simplify (caps);
  rgbcaps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (rgbcaps); i++) {
    structure = gst_caps_get_structure (rgbcaps, i);

    gst_structure_set_name (structure, "video/x-raw-rgb");
  }
  graycaps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (graycaps); i++) {
    structure = gst_caps_get_structure (graycaps, i);

    gst_structure_set_name (structure, "video/x-raw-gray");
  }

  gst_caps_append (caps, graycaps);
  gst_caps_append (caps, rgbcaps);

  return caps;
}


static GstCaps *
mfw_gst_ipu_get_template_caps(void)
{
    GstCaps * caps = NULL;
    IPUFormatMapper * map = g_support_formats;
    while(map->gst_video_format!=GST_VIDEO_FORMAT_UNKNOWN){
        GstCaps * newcaps = gst_caps_from_string(map->mime);
        if (newcaps){
            if (caps){
                gst_caps_append(caps, newcaps);
            }else{
                caps = newcaps;
            }
        }
        map++;
    };
    return caps;
}


/* The caps can be transformed into any other caps with format info removed.
 * However, we should prefer passthrough, so if passthrough is possible,
 * put it first in the list. */
static GstCaps *
mfw_gst_ipu_csc_transform_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *template;
  GstCaps *result;

  template = mfw_gst_ipu_get_template_caps ();
  result = gst_caps_intersect (caps, template);
  gst_caps_unref (template);

  gst_caps_append (result, mfw_gst_ipu_csc_caps_remove_format_info (direction,caps));

  GST_DEBUG_OBJECT (btrans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

static guint
mfw_gst_ipu_videoformat_to_ipuformat(GstVideoFormat format)
{
    IPUFormatMapper * map = g_support_formats;
    while(map->gst_video_format!=GST_VIDEO_FORMAT_UNKNOWN){
        if (map->gst_video_format==format)
            return map->ipu_format;
        map++;
    };
    return 0;
}

static gboolean 
mfw_gst_ipu_core_init(MfwGstIPUCSC* filter, GstCaps * incap, GstCaps * outcap)
{
    gint intvalue, intvalue0;
    GstVideoFormat format; 
    GstStructure * structure;
    IPUTaskOne * itask = &filter->iputask;

    GST_LOG ("Input caps are %" GST_PTR_FORMAT, incap);
    GST_LOG ("Output caps are %" GST_PTR_FORMAT, outcap);

    if (gst_video_format_parse_caps(incap, &format, &intvalue, &intvalue0)==FALSE){
      goto fail;
    }
    INPUT_WIDTH(itask) = intvalue;
    INPUT_HEIGHT(itask) = intvalue0;
    INPUT_FORMAT(itask) = mfw_gst_ipu_videoformat_to_ipuformat(format);
    filter->input_framesize = gst_video_format_get_size(format, intvalue, intvalue0);

    structure = gst_caps_get_structure (incap, 0);

    if (gst_structure_get_int (structure, CAPS_FIELD_CROP_LEFT, &intvalue)){
       INPUT_CROP_X(itask)=GST_ROUND_UP_8(intvalue);
    }
    intvalue = 0;
    gst_structure_get_int (structure, CAPS_FIELD_CROP_RIGHT, &intvalue);
    intvalue = GST_ROUND_UP_8(intvalue);
       
    INPUT_CROP_WIDTH(itask)=INPUT_WIDTH(itask)-INPUT_CROP_X(itask)-intvalue;

    if (gst_structure_get_int (structure, CAPS_FIELD_CROP_TOP, &intvalue)){
       INPUT_CROP_Y(itask)=GST_ROUND_UP_8(intvalue);
    }
    intvalue = 0;
    gst_structure_get_int (structure, CAPS_FIELD_CROP_BOTTOM, &intvalue);
    intvalue = GST_ROUND_UP_8(intvalue);
       
    INPUT_CROP_HEIGHT(itask)=INPUT_HEIGHT(itask)-INPUT_CROP_Y(itask)-intvalue;

    if (gst_video_format_parse_caps(outcap, &format, &intvalue, &intvalue0)==FALSE){
      goto fail;
    }
    OUTPUT_WIDTH(itask) = intvalue;
    OUTPUT_HEIGHT(itask) = intvalue0;
    OUTPUT_FORMAT(itask) = mfw_gst_ipu_videoformat_to_ipuformat(format);
    filter->output_framesize = gst_video_format_get_size(format, intvalue, intvalue0);

    structure = gst_caps_get_structure(outcap, 0);
 
    if (gst_structure_get_int (structure, CAPS_FIELD_CROP_LEFT, &intvalue)){
       OUTPUT_CROP_X(itask)=GST_ROUND_UP_8(intvalue);
    }
    intvalue = 0;
    gst_structure_get_int (structure, CAPS_FIELD_CROP_RIGHT, &intvalue);
    intvalue = GST_ROUND_UP_8(intvalue);
       
    OUTPUT_CROP_WIDTH(itask)=OUTPUT_WIDTH(itask)-OUTPUT_CROP_X(itask)-intvalue;

    if (gst_structure_get_int (structure, CAPS_FIELD_CROP_TOP, &intvalue)){
       OUTPUT_CROP_Y(itask)=GST_ROUND_UP_8(intvalue);
    }
    intvalue = 0;
    gst_structure_get_int (structure, CAPS_FIELD_CROP_BOTTOM, &intvalue);
    intvalue = GST_ROUND_UP_8(intvalue);
       
    OUTPUT_CROP_HEIGHT(itask)=OUTPUT_HEIGHT(itask)-OUTPUT_CROP_Y(itask)-intvalue;
      
    return TRUE;
fail:
    return FALSE;
}

static gboolean
mfw_gst_ipu_csc_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  MfwGstIPUCSC *space;
  GstStructure *structure;
  const GValue *in_framerate = NULL;
  const GValue *out_framerate = NULL;
  const GValue *in_par = NULL;
  const GValue *out_par = NULL;
  gboolean res;

  
  space = MFW_GST_IPU_CSC (btrans);

  /* parse in and output values */
  structure = gst_caps_get_structure (incaps, 0);


  /* and framerate */
  in_framerate = gst_structure_get_value (structure, "framerate");
  if (in_framerate == NULL || !GST_VALUE_HOLDS_FRACTION (in_framerate))
    goto no_framerate;

  /* this is optional */
  in_par = gst_structure_get_value (structure, "pixel-aspect-ratio");

  structure = gst_caps_get_structure (outcaps, 0);


  /* and framerate */
  out_framerate = gst_structure_get_value (structure, "framerate");
  if (out_framerate == NULL || !GST_VALUE_HOLDS_FRACTION (out_framerate))
    goto no_framerate;

  /* this is optional */
  out_par = gst_structure_get_value (structure, "pixel-aspect-ratio");

  /* these must match */
  if (/*in_width != out_width || in_height != out_height ||*/
      gst_value_compare (in_framerate, out_framerate) != GST_VALUE_EQUAL)
    goto format_mismatch;

  /* if present, these must match too */
  if (in_par && out_par
      && gst_value_compare (in_par, out_par) != GST_VALUE_EQUAL)
    goto format_mismatch;



  space->interlaced = FALSE;
  gst_structure_get_boolean (structure, "interlaced", &space->interlaced);

  // Initial core ipu library;
  res = mfw_gst_ipu_core_init(space, incaps, outcaps);
  if (!res)
    goto core_ipu_library_init_io_parameter_failed;



  return TRUE;

  /* ERRORS */
no_width_height:
  {
    GST_DEBUG_OBJECT (space, "did not specify width or height");
    return FALSE;
  }
no_framerate:
  {
    GST_DEBUG_OBJECT (space, "did not specify framerate");
    return FALSE;
  }
format_mismatch:
  {
    GST_DEBUG_OBJECT (space, "input and output formats do not match");
    return FALSE;
  }
invalid_in_caps:
  {
    GST_DEBUG_OBJECT (space, "could not configure context for input format");
    return FALSE;
  }
invalid_out_caps:
  {
    GST_DEBUG_OBJECT (space, "could not configure context for output format");
    return FALSE;
  }
core_ipu_library_init_io_parameter_failed:
  {
    GST_DEBUG_OBJECT (space, "could not initialize the core ipu library IO parameter");
    return FALSE;
  }
}

GType
mfw_gst_ipu_csc_get_type (void)
{
  static GType mfw_ipu_csc_type = 0;

  if (!mfw_ipu_csc_type) {
    static const GTypeInfo mfw_ipu_csc_info = {
      sizeof (MfwGstIPUCSCClass),
      (GBaseInitFunc) mfw_gst_ipu_csc_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_ipu_csc_class_init,
      NULL,
      NULL,
      sizeof (MfwGstIPUCSC),
      0,
      (GInstanceInitFunc) mfw_gst_ipu_csc_init,
    };

    mfw_ipu_csc_type = g_type_register_static (GST_TYPE_BASE_TRANSFORM,
        "MfwGstIPUCSC", &mfw_ipu_csc_info, 0);
  }

  return mfw_ipu_csc_type;
}

static void
mfw_gst_ipu_csc_base_init (MfwGstIPUCSCClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class, srctempl);
  gst_element_class_add_pad_template (element_class, sinktempl);
  
  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "IPU-based video converter",
      "Filter/Converter/Video", "Convert video raw data between different formats and resolutions by using IPU");
  
}

static void
mfw_gst_ipu_csc_finalize (GObject * obj)
{
  MfwGstIPUCSC *filter = MFW_GST_IPU_CSC (obj);

  if (filter->hbuf_in){
      mfw_free_hw_buffer(filter->hbuf_in);
      filter->hbuf_in_size = 0;
  }
  if (filter->hbuf_out){
      mfw_free_hw_buffer(filter->hbuf_out);
      filter->hbuf_out_size = 0;
  }

  if (filter->ipufd!=-1){
    close(filter->ipufd);
    filter->ipufd = -1;
  }
     
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
mfw_gst_ipu_csc_class_init (MfwGstIPUCSCClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_finalize);

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_transform_caps);
  gstbasetransform_class->set_caps = GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_set_caps);
  gstbasetransform_class->get_unit_size =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_get_unit_size);
  gstbasetransform_class->transform =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_transform);
#if 0
  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_transform_ip);
#endif
  gstbasetransform_class->fixate_caps = GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_fixate_caps);

  gstbasetransform_class->passthrough_on_same_caps = TRUE;

  GST_DEBUG_CATEGORY_INIT(mfw_gst_ipucsc_debug, "mfw_ipucsc",
              0, "FreeScale's IPU Color Space Converter Gst Plugin's Log");
}

static void
mfw_gst_ipu_csc_init (MfwGstIPUCSC * space)
{
  GstBaseTransform * trans = (GstBaseTransform*)space;

  space->ipufd = -1;
  strcpy(space->device_name, "/dev/mxc_ipu");
  gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (space), TRUE);

#define MFW_GST_IPU_CSC_PLUGIN VERSION
      PRINT_CORE_VERSION("IPU_CSC_CORE_LIBRARY_VERSION_INFOR_01.00");
      PRINT_PLUGIN_VERSION(MFW_GST_IPU_CSC_PLUGIN);
}



static gboolean
mfw_gst_ipu_csc_get_unit_size (GstBaseTransform * btrans, GstCaps * caps,
    guint * size)
{
  GstVideoFormat format; 
  gint width, height;
  gst_video_format_parse_caps(caps, &format, &width, &height);
  *size = gst_video_format_get_size(format, width, height);
  return TRUE;
}

#if 0
/* FIXME: Could use transform_ip to implement endianness swap type operations */
static GstFlowReturn
mfw_gst_ipu_csc_transform_ip (GstBaseTransform * btrans, GstBuffer * inbuf)
{
  /* do nothing */
  return GST_FLOW_OK;
}
#endif


static gboolean 
mfw_gst_ipu_core_start_convert (GstBaseTransform * btrans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
    MfwGstIPUCSC * filter = MFW_GST_IPU_CSC (btrans);
    gboolean ret = FALSE;
    IPUTaskOne * itask = &filter->iputask;
    gboolean copy_input = (!IS_DMABLE_BUFFER(inbuf));
    gboolean copy_output = (!IS_DMABLE_BUFFER(outbuf));

    GST_LOG("start convert copy_input(%s), copy_output(%s)", (copy_input?"yes":"no"), (copy_output?"yes":"no"));

    if (copy_input){
      if (filter->input_framesize!=filter->hbuf_in_size){
        if (filter->hbuf_in){
           mfw_free_hw_buffer(filter->hbuf_in);
           filter->hbuf_in_size = 0;
        }
        filter->hbuf_in = mfw_new_hw_buffer(filter->input_framesize, &filter->hbuf_in_paddr, &filter->hbuf_in_vaddr, 0);
        if (filter->hbuf_in==NULL){
           goto fail;
        }
        filter->hbuf_in_size = filter->input_framesize;
      }
      memcpy(filter->hbuf_in_vaddr, GST_BUFFER_DATA(inbuf), filter->input_framesize);
      INPUT_PADDR(itask) = filter->hbuf_in_paddr;
    }else{
      INPUT_PADDR(itask) = DMABLE_BUFFER_PHY_ADDR(inbuf);
    }

    if (copy_output){
      if (filter->output_framesize!=filter->hbuf_out_size){
        if (filter->hbuf_out){
           mfw_free_hw_buffer(filter->hbuf_out);
           filter->hbuf_out_size = 0;
        }
        filter->hbuf_out = mfw_new_hw_buffer(filter->output_framesize, &filter->hbuf_out_paddr, &filter->hbuf_out_vaddr, 0);
        if (filter->hbuf_out==NULL){
           goto fail;
        }
        filter->hbuf_out_size = filter->output_framesize;
      }
     OUTPUT_PADDR(itask) = filter->hbuf_out_paddr;
    }else{
      OUTPUT_PADDR(itask) = DMABLE_BUFFER_PHY_ADDR(outbuf);
    }

   KICK_IPUTASKONE(filter->ipufd, itask);
  if (copy_output){
      memcpy(GST_BUFFER_DATA(outbuf), filter->hbuf_out_vaddr, filter->output_framesize);
   }
 
    ret = TRUE;
    return ret;

fail:
    return ret;    
}



static GstFlowReturn
mfw_gst_ipu_csc_transform (GstBaseTransform * btrans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  MfwGstIPUCSC *filter;
  gint result;

  filter = MFW_GST_IPU_CSC (btrans);
#ifndef IPULIB  
  if (G_UNLIKELY(filter->ipufd==-1)){
      filter->ipufd = open(filter->device_name, O_RDWR, 0);
      if (filter->ipufd<=0){
         filter->ipufd = -1;
         return GST_FLOW_ERROR;
      }
  }
#endif
  if (mfw_gst_ipu_core_start_convert(btrans, inbuf, outbuf)==FALSE){
    goto not_supported;
  }

  return GST_FLOW_OK;

  /* ERRORS */
unknown_format:
  {
    GST_ELEMENT_ERROR (filter, CORE, NOT_IMPLEMENTED, (NULL),
        ("attempting to convert colorspaces between unknown formats"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
not_supported:
  {
    GST_ELEMENT_ERROR (filter, CORE, NOT_IMPLEMENTED, (NULL),
        ("cannot convert between formats"));
    return GST_FLOW_NOT_SUPPORTED;
  }
}

struct _GstBaseTransformPrivate
{
  /* QoS *//* with LOCK */
  gboolean qos_enabled;
  gdouble proportion;
  GstClockTime earliest_time;
  /* previous buffer had a discont */
  gboolean discont;

  GstActivateMode pad_mode;

  gboolean gap_aware;

  /* caps used for allocating buffers */
  gboolean proxy_alloc;
  GstCaps *sink_alloc;
  GstCaps *src_alloc;
  /* upstream caps and size suggestions */
  GstCaps *sink_suggest;
  guint size_suggest;
  gboolean suggest_pending;

  gboolean reconfigure;
};


gboolean
gst_video_calculate_display_ratio_csc (guint * dar_n, guint * dar_d,
    guint video_width, guint video_height,
    guint video_par_n, guint video_par_d,
    guint display_par_n, guint display_par_d)
{
  gint num, den;

  GValue display_ratio = { 0, };
  GValue tmp = { 0, };
  GValue tmp2 = { 0, };

  g_return_val_if_fail (dar_n != NULL, FALSE);
  g_return_val_if_fail (dar_d != NULL, FALSE);

  g_value_init (&display_ratio, GST_TYPE_FRACTION);
  g_value_init (&tmp, GST_TYPE_FRACTION);
  g_value_init (&tmp2, GST_TYPE_FRACTION);

  /* Calculate (video_width * video_par_n * display_par_d) /
   * (video_height * video_par_d * display_par_n) */
  gst_value_set_fraction (&display_ratio, video_width, video_height);
  gst_value_set_fraction (&tmp, video_par_n, video_par_d);

  if (!gst_value_fraction_multiply (&tmp2, &display_ratio, &tmp))
    goto error_overflow;

  gst_value_set_fraction (&tmp, display_par_d, display_par_n);

  if (!gst_value_fraction_multiply (&display_ratio, &tmp2, &tmp))
    goto error_overflow;

  num = gst_value_get_fraction_numerator (&display_ratio);
  den = gst_value_get_fraction_denominator (&display_ratio);

  g_value_unset (&display_ratio);
  g_value_unset (&tmp);
  g_value_unset (&tmp2);

  g_return_val_if_fail (num > 0, FALSE);
  g_return_val_if_fail (den > 0, FALSE);

  *dar_n = num;
  *dar_d = den;

  return TRUE;
error_overflow:
  g_value_unset (&display_ratio);
  g_value_unset (&tmp);
  g_value_unset (&tmp2);
  return FALSE;
}

static void
mfw_gst_ipu_csc_fixate_caps (GstBaseTransform * base, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *ins, *outs;
  const GValue *from_par, *to_par;

  g_return_if_fail (gst_caps_is_fixed (caps));

  GST_DEBUG_OBJECT (base, "trying to fixate othercaps %" GST_PTR_FORMAT
      " based on caps %" GST_PTR_FORMAT, othercaps, caps);

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (othercaps, 0);

  from_par = gst_structure_get_value (ins, "pixel-aspect-ratio");
  to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");

  /* we have both PAR but they might not be fixated */
  if (from_par && to_par) {
    gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;

    gint count = 0, w = 0, h = 0;

    guint num, den;

    /* from_par should be fixed */
    g_return_if_fail (gst_value_is_fixed (from_par));

    from_par_n = gst_value_get_fraction_numerator (from_par);
    from_par_d = gst_value_get_fraction_denominator (from_par);

    /* fixate the out PAR */
    if (!gst_value_is_fixed (to_par)) {
      GST_DEBUG_OBJECT (base, "fixating to_par to %dx%d", from_par_n,
          from_par_d);
      gst_structure_fixate_field_nearest_fraction (outs, "pixel-aspect-ratio",
          from_par_n, from_par_d);
    }

    to_par_n = gst_value_get_fraction_numerator (to_par);
    to_par_d = gst_value_get_fraction_denominator (to_par);

    /* if both width and height are already fixed, we can't do anything
     * about it anymore */
    if (gst_structure_get_int (outs, "width", &w))
      ++count;
    if (gst_structure_get_int (outs, "height", &h))
      ++count;
    if (count == 2) {
      GST_DEBUG_OBJECT (base, "dimensions already set to %dx%d, not fixating",
          w, h);
      return;
    }

    gst_structure_get_int (ins, "width", &from_w);
    gst_structure_get_int (ins, "height", &from_h);

    if (!gst_video_calculate_display_ratio_csc (&num, &den, from_w, from_h,
            from_par_n, from_par_d, to_par_n, to_par_d)) {
      GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
          ("Error calculating the output scaled size - integer overflow"));
      return;
    }

    GST_DEBUG_OBJECT (base,
        "scaling input with %dx%d and PAR %d/%d to output PAR %d/%d",
        from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d);
    GST_DEBUG_OBJECT (base, "resulting output should respect ratio of %d/%d",
        num, den);

    /* now find a width x height that respects this display ratio.
     * prefer those that have one of w/h the same as the incoming video
     * using wd / hd = num / den */

    /* if one of the output width or height is fixed, we work from there */
    if (h) {
      GST_DEBUG_OBJECT (base, "height is fixed,scaling width");
      w = (guint) gst_util_uint64_scale_int (h, num, den);
    } else if (w) {
      GST_DEBUG_OBJECT (base, "width is fixed, scaling height");
      h = (guint) gst_util_uint64_scale_int (w, den, num);
    } else {
      /* none of width or height is fixed, figure out both of them based only on
       * the input width and height */
      /* check hd / den is an integer scale factor, and scale wd with the PAR */
      if (from_h % den == 0) {
        GST_DEBUG_OBJECT (base, "keeping video height");
        h = from_h;
        w = (guint) gst_util_uint64_scale_int (h, num, den);
      } else if (from_w % num == 0) {
        GST_DEBUG_OBJECT (base, "keeping video width");
        w = from_w;
        h = (guint) gst_util_uint64_scale_int (w, den, num);
      } else {
        GST_DEBUG_OBJECT (base, "approximating but keeping video height");
        h = from_h;
        w = (guint) gst_util_uint64_scale_int (h, num, den);
      }
    }
    GST_DEBUG_OBJECT (base, "scaling to %dx%d", w, h);

    /* now fixate */
    gst_structure_fixate_field_nearest_int (outs, "width", w);
    gst_structure_fixate_field_nearest_int (outs, "height", h);
  } else {
    gint width, height;

    if (gst_structure_get_int (ins, "width", &width)) {
      if (gst_structure_has_field (outs, "width")) {
        gst_structure_fixate_field_nearest_int (outs, "width", width);
      }
    }
    if (gst_structure_get_int (ins, "height", &height)) {
      if (gst_structure_has_field (outs, "height")) {
        gst_structure_fixate_field_nearest_int (outs, "height", height);
      }
    }
  }

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);
}

static gboolean
plugin_init (GstPlugin *plugin)
{
    GstCaps *caps;

    /* template caps */
    caps = mfw_gst_ipu_get_template_caps ();

    /* build templates */
    srctempl = gst_pad_template_new ("src",
        GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_copy (caps));

    /* the sink template will do palette handling as well... */
    sinktempl = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);

    return gst_element_register (plugin, "mfw_ipucsc",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_IPU_CSC);
}

FSL_GST_PLUGIN_DEFINE("ipucsc", "IPU-based video converter", plugin_init);



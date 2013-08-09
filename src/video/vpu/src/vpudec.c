/*
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. All rights reserved.
 */

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
 * Module Name:   vpudec.c
 *
 * Description:   Implementation of VPU-based video decoder gstreamer plugin
 *
 * Portability:   This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#include <string.h>

#include "vpudec.h"
#include "gstsutils.h"
#include "gstnext.h"

/* FIX ME */
//#define MX6_CLEARDISPLAY_WORKAROUND

#define DEFAULT_FRAME_BUFFER_ALIGNMENT_H 16
#define DEFAULT_FRAME_BUFFER_ALIGNMENT_V 16

#define VPU_RAW_CAPS \
    VIDEO_RAW_CAPS_NV12 ";"\
    VIDEO_RAW_CAPS_I420 ";"\
    VIDEO_RAW_CAPS_YV12 ";"\
    VIDEO_RAW_CAPS_TILED ";"\
    VIDEO_RAW_CAPS_TILED_FIELD

#define CORE_API(func, elseroutine, err, ...)\
  do{\
      g_mutex_lock(vpudec->lock);\
      if (vpudec->options.profiling){\
        gint64 time_diff;\
        TIME_PROFILE((err) = (func)( __VA_ARGS__ ), time_diff);\
        vpudec->profile_count.decode_time+=time_diff;\
      }else{\
      (err) = (func)( __VA_ARGS__ );\
      }\
      g_mutex_unlock(vpudec->lock);\
      if (VPU_DEC_RET_SUCCESS!=(err)){\
        GST_ERROR("Func %s failed!! with ret %d", STR(func), (err));\
        elseroutine;\
      }\
  }while(0)

#define CORE_API_UNLOCKED(func, elseroutine, err, ...)\
  do{\
      (err) = (func)( __VA_ARGS__ );\
      GST_LOG("Call %s return 0x%x", STR(func), (err));\
      if (VPU_DEC_RET_SUCCESS!=(err)){\
        GST_ERROR("Func %s failed!!", STR(func));\
        elseroutine;\
      }\
  }while(0)

#define Align(ptr,align)	((align) ? ((((guint32)(ptr))+(align)-1)/(align)*(align)) : ((guint32)(ptr)))

#define VPUDEC_CONFIG_DECODERETRYCNT_DEFAULT (50)
#define VPUDEC_TS_BUFFER_LENGTH_DEFAULT (1024)

#define IS_DMABLE_BUFFER(buffer) ( (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1])) \
                                 || ( GST_IS_BUFFER(buffer) \
                                 &&  GST_BUFFER_FLAG_IS_SET((buffer),GST_BUFFER_FLAG_LAST)))
#define DMABLE_BUFFER_PHY_ADDR(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]) ? \
                                        ((GstBufferMeta *)(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))->physical_data : \
                                        (gpointer)(guint32)GST_BUFFER_OFFSET(buffer))

#define ATTACH_MEM2VPUDEC(vpudec, desc)\
  do {\
    (desc)->next = (vpudec)->mems;\
    (vpudec)->mems = (desc);\
  }while(0)

#define SKIP_NUM_MASK (0xff)

typedef struct
{
  void *paddr;
  void *vaddr;
} VpuMemory;

enum
{
  SKIP_NONE = 0,
  SKIP_1OF4 = 0x3,
  SKIP_1OF8 = 0x7,
  SKIP_1OF16 = 0xf,
  SKIP_1OF32 = 0x1f,
  SKIP_1OF64 = 0x3f,
  SKIP_BP = 0x100,
  SKIP_B = 0x200,
};

enum
{
  PROP_0,
  PROP_LOW_LATENCY,
  PROP_FRAMERATE_NU,
  PROP_FRAMERATE_DE,
  PROP_ADAPTIVE_FRAMEDROP,
  PROP_FRAMES_PLUS,
  PROP_OUTPUT_FORMAT,
  PROP_DROP_LEVEL_MASK,
  PROP_EXPERIMENTAL_TSM,
  PROP_PROFILING,
};

static void gst_vpudec_finalize (GObject * object);

static GstFlowReturn gst_vpudec_chain (GstPad * pad, GstBuffer * buffer);
static gboolean gst_vpudec_setcaps (GstPad * pad, GstCaps * caps);
static GstStateChangeReturn gst_vpudec_state_change (GstElement * element,
    GstStateChange transition);
static gboolean gst_vpudec_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_vpudec_src_event (GstPad * pad, GstEvent * event);
static const GstQueryType *gst_vpudec_query_types (GstPad * pad);
static gboolean gst_vpudec_src_query (GstPad * pad, GstQuery * query);
static GType gst_vpudec_get_output_format_type (void);
static void vpudec_init_qos_ctrl(VpuDecQosCtl * qos);

static gint g_fieldmap[] = {
  FIELD_NONE,
  FIELD_TOP,
  FIELD_BOTTOM,
  FIELD_INTERLACED_TB,
  FIELD_INTERLACED_BT,
  FIELD_NONE,
};

static gchar *g_format_caps_map[6][6] = {
  {VIDEO_RAW_CAPS_NV12, VIDEO_RAW_CAPS_I420, VIDEO_RAW_CAPS_YV12, VIDEO_RAW_CAPS_TILED, VIDEO_RAW_CAPS_TILED_FIELD, NULL},      //auto
  {VIDEO_RAW_CAPS_I420, NULL},  //i420
  {VIDEO_RAW_CAPS_YV12, NULL},  //yv12
  {VIDEO_RAW_CAPS_NV12, NULL},  //nv12
  {VIDEO_RAW_CAPS_TILED, NULL}, //tiled
  {VIDEO_RAW_CAPS_TILED_FIELD, NULL}    //tiled field
};


#define STR_MAX_INT "2147483647"

static GstsutilsOptionEntry g_vpudec_option_table[] = {
  {PROP_LOW_LATENCY, "low-latency", "low latency",
        "set low latency mode enable/disable for streaming case",
        G_TYPE_BOOLEAN,
      G_STRUCT_OFFSET (VpuDecOption, low_latency), "false"},
  {PROP_FRAMERATE_NU, "framerate-nu", "framerate numerator",
        "set framerate numerator", G_TYPE_INT,
      G_STRUCT_OFFSET (VpuDecOption, framerate_n), "30", "1", STR_MAX_INT},
  {PROP_FRAMERATE_DE, "framerate-de", "framerate denominator",
        "set framerate denominator", G_TYPE_INT,
      G_STRUCT_OFFSET (VpuDecOption, framerate_d), "1", "1", STR_MAX_INT},
  {PROP_ADAPTIVE_FRAMEDROP, "framedrop", "frame drop",
        "enable adaptive frame drop for smoothly playback", G_TYPE_BOOLEAN,
      G_STRUCT_OFFSET (VpuDecOption, adaptive_drop), "true"},
  {PROP_FRAMES_PLUS, "frame-plus", "addtionlal frames",
        "set number of addtional frames for smoothly playback", G_TYPE_INT,
      G_STRUCT_OFFSET (VpuDecOption, bufferplus), "6", "1", STR_MAX_INT},
  {PROP_OUTPUT_FORMAT, "output-format", "output format",
        "set raw format for output",
        G_TYPE_ENUM,
        G_STRUCT_OFFSET (VpuDecOption, ofmt), "0", NULL, NULL,
      gst_vpudec_get_output_format_type},
  {PROP_DROP_LEVEL_MASK, "framedrop-level-mask", "framedrop level mask",
        "set enable mask for drop policy 0x100: drop B/P frame; 0x200: drop B frame; 0xff mask of frame not display",
        G_TYPE_UINT,
      G_STRUCT_OFFSET (VpuDecOption, drop_level_mask), "0x3ff", "0", "0x3ff"},
  {PROP_EXPERIMENTAL_TSM, "experimental-tsm", "experimental tsm",
        "enable/disable experimental timestamp algorithm",
        G_TYPE_BOOLEAN, G_STRUCT_OFFSET (VpuDecOption, experimental_tsm),
      "true"},
  {PROP_PROFILING, "profile", "profile", "enable profile on vpudec",
      G_TYPE_BOOLEAN, G_STRUCT_OFFSET (VpuDecOption, profiling), "false"},
  /* terminator */
  {-1, NULL, NULL, NULL, 0, 0, NULL},
};



typedef struct
{
  gint std;
  char *mime;
} VPUMapper;

static VPUMapper vpu_mappers[] = {
  {VPU_V_MPEG4, "video/mpeg, mpegversion=(int)4"},
  {VPU_V_AVC, "video/x-h264"},
  {VPU_V_H263, "video/x-h263"},
  {VPU_V_MPEG2,
      "video/mpeg, systemstream=(boolean)false, mpegversion=(int){1,2}"},
  {VPU_V_VC1_AP, "video/x-wmv, wmvversion=(int)3, format=(fourcc)WVC1"},
  {VPU_V_VC1, "video/x-wmv, wmvversion=(int)3"},
  {VPU_V_XVID, "video/x-xvid"},
  {VPU_V_VP8, "video/x-vp8"},
  {VPU_V_MJPG, "image/jpeg"},
  {-1, NULL}
};


static GEnumValue output_format_enum[] = {
  {0, "auto", "auto"},
  {1, "I420", "i420"},
  {2, "YV12", "yv12"},
  {3, "NV12", "nv12"},
  {4, "tiled format", "tile"},
  {5, "tiled field format", "tilefield"},
  {0, NULL, NULL}
};

#define GST_CAT_DEFAULT gst_vpudec_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VPU_RAW_CAPS)
    );

GST_DEBUG_CATEGORY (gst_vpudec_debug);

static GType
gst_vpudec_get_output_format_type (void)
{
  static GType output_format_type = 0;
  if (!output_format_type) {
    output_format_type =
        g_enum_register_static ("vpudec_outputformat", output_format_enum);
  }

  return output_format_type;
}


static GstStructure *
vpudec_find_compatible_structure_for_string (gchar * capstr,
    GstStructure * structurex)
{
  GstStructure *structure, *newstructure = NULL;
  guint i;
  if (structure = gst_structure_from_string (capstr, NULL)) {
    if (structurex) {
      if (gst_structure_is_subset (structurex, structure)) {
        newstructure = gst_structure_copy (structure);
      }
      if (gst_structure_is_subset (structure, structurex)) {
        newstructure = gst_structure_copy (structurex);
      }
      gst_structure_free (structure);
    }
  }

  return newstructure;
}

static GstStructure *
vpudec_find_compatible_structure_for_strings (gchar ** capstrs, GstCaps * caps)
{
  gchar **capstr;

  if (gst_caps_is_any (caps)) {
    return gst_structure_from_string (VIDEO_RAW_CAPS_I420, NULL);
  } else {

    gint i, size = gst_caps_get_size (caps);
    for (i = 0; i < size; i++) {
      capstr = capstrs;
      GstStructure *allow_structure;
      if (allow_structure = gst_caps_get_structure (caps, i)) {
        while ((*capstr) && (allow_structure)) {
          GstStructure *structure;
          if (structure =
              vpudec_find_compatible_structure_for_string (*capstr,
                  allow_structure)) {
            return structure;
          }
          capstr++;
        }
      }
    }
  }
  return NULL;
}

static gint
vpudec_find_std (GstCaps * caps)
{

  VPUMapper *mapper = vpu_mappers;
  while (mapper->mime) {
    GstCaps *scaps = gst_caps_from_string (mapper->mime);
    if (scaps) {
      if (gst_caps_is_subset (caps, scaps)) {
        gst_caps_unref (scaps);
        return mapper->std;
      }
      gst_caps_unref (scaps);
    }
    mapper++;
  }
  return -1;
}


static void
vpudec_core_mem_free_normal_buffer (VpuDecMem * mem)
{
  VpuDecRetCode core_ret;

  if (mem) {
    if (mem->handle) {
      MM_FREE (mem->handle);
    }
    MM_FREE (mem);
  }
}


static VpuDecMem *
vpudec_core_mem_alloc_normal_buffer (gint size, void **vaddr)
{
  VpuDecRetCode core_ret;
  VpuDecMem *mem = MM_MALLOC (sizeof (VpuDecMem));
  *vaddr = MM_MALLOC (size);

  if ((mem == NULL) || (*vaddr == NULL)) {
    goto fail;
  }

  mem->freefunc = vpudec_core_mem_free_normal_buffer;
  mem->handle = *vaddr;
  return mem;
fail:
  if (*vaddr) {
    MM_FREE (*vaddr);
  }
  if (mem) {
    MM_FREE (mem);
    mem = NULL;
  }
  return mem;
}


static void
vpudec_core_mem_free_dma_buffer (VpuDecMem * mem)
{
  VpuDecRetCode core_ret;

  if (mem) {
    if (mem->handle) {
      MM_UNREGRES (mem->handle, RES_FILE_DEVICE);
      CORE_API_UNLOCKED (VPU_DecFreeMem,, core_ret, mem->handle);
      MM_FREE (mem->handle);
    }
    MM_FREE (mem);
  }
}


static VpuDecMem *
vpudec_core_mem_alloc_dma_buffer (gint size, void **paddr, void **vaddr)
{
  VpuDecRetCode core_ret;
  VpuDecMem *mem = MM_MALLOC (sizeof (VpuDecMem));
  VpuMemDesc *vmem = MM_MALLOC (sizeof (VpuMemDesc));

  if ((mem == NULL) || (vmem == NULL)) {
    goto fail;
  }
  vmem->nSize = size;
  CORE_API_UNLOCKED (VPU_DecGetMem, goto fail, core_ret, vmem);
  if (paddr) {
    *paddr = (void *) vmem->nPhyAddr;
  }
  if (vaddr) {
    *vaddr = (void *) vmem->nVirtAddr;
  }
  MM_REGRES (vmem, RES_FILE_DEVICE);
  mem->freefunc = vpudec_core_mem_free_dma_buffer;
  mem->handle = (void *) vmem;
  mem->parent = NULL;
  return mem;
fail:
  if (vmem) {
    MM_FREE (vmem);
  }
  if (mem) {
    MM_FREE (mem);
    mem = NULL;
  }
  return mem;
}

static gboolean
vpudec_prealloc_memories (GstVpuDec * vpudec, VpuMemInfo * mem)
{
  gboolean ret = FALSE;
  int i;
  for (i = 0; i < mem->nSubBlockNum; i++) {
    VpuMemSubBlockInfo *block = &mem->MemSubBlock[i];

    if (block->nSize) {
      VpuDecMem *vblock;
      int size = block->nSize + block->nAlignment - 1;
      if (block->MemType == VPU_MEM_VIRT) {
        void *vaddr;
        if (vblock = vpudec_core_mem_alloc_normal_buffer (size, &vaddr)) {
          block->pVirtAddr = (unsigned char *) Align (vaddr, block->nAlignment);
        } else {
          goto fail;
        }
      } else if (block->MemType == VPU_MEM_PHY) {
        void *vaddr, *paddr;
        if (vblock = vpudec_core_mem_alloc_dma_buffer (size, &paddr, &vaddr)) {
          block->pPhyAddr = (unsigned char *) Align (paddr, block->nAlignment);
          block->pVirtAddr = (unsigned char *) Align (vaddr, block->nAlignment);
        } else {
          goto fail;
        }
      } else {
        goto fail;
      }
      ATTACH_MEM2VPUDEC (vpudec, vblock);

    } else {
      goto fail;
    }
  }
  ret = TRUE;
fail:
  return ret;
}

static void
vpudec_free_memories (GstVpuDec * vpudec)
{
  VpuDecMem *mem = vpudec->mems, *memnext;

  while (mem) {
    memnext = mem->next;
    if ((mem->handle)) {
      if (mem->freefunc) {
        mem->freefunc (mem);
      }
    }
    mem = memnext;
  }
  vpudec->mems = NULL;
}

static void
vpudec_free_frames (GstVpuDec * vpudec)
{
  if (vpudec->frames) {
    int i;
    for (i = 0; i < vpudec->frame_num; i++) {
      if (vpudec->frames[i].gstbuf) {
        gst_buffer_unref (vpudec->frames[i].gstbuf);
      }
    }
    MM_FREE (vpudec->frames);
    vpudec->frames = NULL;
  }
  vpudec->frame_num = 0;
}



static gboolean
vpudec_core_init (GstVpuDec * vpudec)
{
  gboolean ret = FALSE;

  VpuVersionInfo version;
  VpuWrapperVersionInfo w_version;
  VpuDecRetCode core_ret;

  memset (&vpudec->context, 0, sizeof (VpuDecContext));
  vpudec->codec_data = NULL;
  vpudec->frames = NULL;
  vpudec->mems = NULL;
  vpudec->tsm = createTSManager (VPUDEC_TS_BUFFER_LENGTH_DEFAULT);
  vpudec->tsm_mode = MODE_AI;
  vpudec->drop_policy = VPU_DEC_SKIPNONE;
  vpudec->drop_level = SKIP_NONE;
  vpudec->output_size = 0;
  vpudec->prerolling = TRUE;
  vpudec->field_info = VPU_FIELD_NONE;
  vpudec->ospec.width_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_H;
  vpudec->ospec.height_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_V;
  vpudec->ospec.buffer_align = 1;
  vpudec->ospec.ostructure = NULL;
  vpudec->use_new_tsm = FALSE;
  memset (&vpudec->vpu_stat, 0, sizeof (VpuDecStat));
  vpudec_init_qos_ctrl(&vpudec->qosctl);

  CORE_API (VPU_DecGetVersionInfo, goto fail, core_ret, &version);
  CORE_API (VPU_DecGetWrapperVersionInfo, goto fail, core_ret, &w_version);

  g_print (GREEN_STR ("vpudec versions :)\n"));
  g_print (GREEN_STR ("\tplugin: %s\n", VERSION));
  g_print (GREEN_STR ("\twrapper: %d.%d.%d(%s)\n", w_version.nMajor,
          w_version.nMinor, w_version.nRelease,
          (w_version.pBinary ? w_version.pBinary : "unknown")));
  g_print (GREEN_STR ("\tvpulib: %d.%d.%d\n", version.nLibMajor,
          version.nLibMinor, version.nLibRelease));
  g_print (GREEN_STR ("\tfirmware: %d.%d.%d.%d\n", version.nFwMajor,
          version.nFwMinor, version.nFwRelease, version.nFwCode));

  CORE_API (VPU_DecQueryMem, goto fail, core_ret, &vpudec->context.meminfo);

  if (!vpudec_prealloc_memories (vpudec, &vpudec->context.meminfo)) {
    vpudec_free_memories (vpudec);
    goto fail;
  }

  ret = TRUE;
fail:
  return ret;

}

static void
vpudec_core_deinit (GstVpuDec * vpudec)
{
  VpuDecRetCode core_ret;
  if (vpudec->tsm) {
    destroyTSManager (vpudec->tsm);
    vpudec->tsm = NULL;
  }

  CORE_API (VPU_DecFlushAll, {
        if (core_ret == VPU_DEC_RET_FAILURE_TIMEOUT) {
        CORE_API (VPU_DecReset,, core_ret, vpudec->context.handle);
          GST_ELEMENT_ERROR (vpudec, STREAM, FAILED,
              (("VPU decode timeout")), ("VPU decode timeout"));}
      }
      , core_ret, vpudec->context.handle);
  if (vpudec->ospec.ostructure) {
    gst_structure_free (vpudec->ospec.ostructure);
  }
  if (vpudec->codec_data) {
    gst_buffer_unref (vpudec->codec_data);
    vpudec->codec_data = NULL;
  }

  if (vpudec->context.handle) {
    CORE_API (VPU_DecClose,, core_ret, vpudec->context.handle);
    vpudec->context.handle = 0;
  }

  vpudec_free_frames (vpudec);
  vpudec_free_memories (vpudec);

  GST_INFO ("Stat:\n\tin  : %lld\n\tout : %lld\n\tshow: %lld",
      vpudec->vpu_stat.in_cnt, vpudec->vpu_stat.out_cnt,
      vpudec->vpu_stat.show_cnt);

}


static void
_do_init (GType object_type)
{
  GST_DEBUG_CATEGORY_INIT (gst_vpudec_debug, "vpudec", 0, "VPU video ");
}

GST_BOILERPLATE_FULL (GstVpuDec, gst_vpudec, GstElement, GST_TYPE_ELEMENT,
    _do_init);

static void
gst_vpudec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpuDec *self = GST_VPUDEC (object);

  switch (prop_id) {

    default:
      if (gstsutils_options_set_option (g_vpudec_option_table,
              (gchar *) & self->options, prop_id, value) == FALSE) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
  }
  return;
}

static void
gst_vpudec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpuDec *self = GST_VPUDEC (object);

  switch (prop_id) {
    default:
      if (gstsutils_options_get_option (g_vpudec_option_table,
              (gchar *) & self->options, prop_id, value) == FALSE) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
  }
  return;
}


static GstCaps *
vpu_core_get_caps ()
{
  GstCaps *caps = NULL;
  VPUMapper *map = vpu_mappers;
  while ((map) && (map->mime)) {
    if (caps) {
      GstCaps *newcaps = gst_caps_from_string (map->mime);
      if (newcaps) {
        if (!gst_caps_is_subset (newcaps, caps)) {
          gst_caps_append (caps, newcaps);
        } else {
          gst_caps_unref (newcaps);
        }
      }
    } else {
      caps = gst_caps_from_string (map->mime);
    }
    map++;
  }
  return caps;
}

static GstPadTemplate *
gst_vpudec_sink_pad_template (void)
{
  static GstPadTemplate *templ = NULL;

  if (!templ) {
    GstCaps *caps = vpu_core_get_caps ();

    if (caps) {
      templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    }
  }
  return templ;
}


static void
gst_vpudec_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_vpudec_sink_pad_template ());
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE (element_class, "VPU-based video decoder",
      "Codec/Decoder/Video",
      "Decode compressed video to raw data by using VPU");
}

static void
gst_vpudec_class_init (GstVpuDecClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  object_class->set_property = GST_DEBUG_FUNCPTR (gst_vpudec_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_vpudec_get_property);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_vpudec_finalize);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_vpudec_state_change);

  gstsutils_options_install_properties_by_options (g_vpudec_option_table,
      object_class);
}

static void
gst_vpudec_init (GstVpuDec * vpudec, GstVpuDecClass * klass)
{
  vpudec->sinkpad =
      gst_pad_new_from_template (gst_vpudec_sink_pad_template (), "sink");
  gst_pad_set_setcaps_function (vpudec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_vpudec_setcaps));
  gst_pad_set_chain_function (vpudec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_vpudec_chain));
  gst_pad_set_event_function (vpudec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_vpudec_sink_event));
  gst_element_add_pad (GST_ELEMENT (vpudec), vpudec->sinkpad);

  vpudec->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_set_event_function (vpudec->srcpad,
      GST_DEBUG_FUNCPTR (gst_vpudec_src_event));
  gst_element_add_pad (GST_ELEMENT (vpudec), vpudec->srcpad);
  gst_pad_use_fixed_caps (vpudec->srcpad);

  memset (&vpudec->options, 0, sizeof (VpuDecOption));

  vpudec->options.mosaic_threshold = 30;
  vpudec->options.decode_retry_cnt = VPUDEC_CONFIG_DECODERETRYCNT_DEFAULT;

  gstsutils_options_load_default (g_vpudec_option_table,
      (gchar *) & vpudec->options);
  gstsutils_options_load_from_keyfile (g_vpudec_option_table,
      (gchar *) & vpudec->options, FSL_GST_CONF_DEFAULT_FILENAME, "vpudec");

  vpudec->lock = g_mutex_new ();

  VPU_DecLoad ();
}


static void
gst_vpudec_free_internal_frame (gpointer p)
{
  GstBufferMeta *meta = (GstBufferMeta *) p;
  VpuDecMem *frameblock = (VpuDecMem *) meta->priv;
  if (frameblock->parent) {
    gst_object_unref (frameblock->parent);
  }
  if (frameblock->freefunc) {
    frameblock->freefunc (frameblock);
  }
  gst_buffer_meta_free (meta);
}

static void
gst_vpudec_assign_frame_pointers (VpuOutPutSpec * ospec,
    VpuFrameBuffer * coreframe, VpuMemory * frame_memory,
    VpuMemory * extra_memory)
{
  gint stride = ospec->crop_left + ospec->width + ospec->crop_right;
  coreframe->nStrideY = stride;
  coreframe->nStrideC = stride / 2;
  coreframe->pbufY = (unsigned char *) frame_memory->paddr;
  coreframe->pbufVirtY = (unsigned char *) frame_memory->vaddr;
  if (ospec->fourcc == GST_STR_FOURCC ("YV12")) {
    coreframe->pbufCr =
        (unsigned char *) frame_memory->paddr + ospec->plane_size[0];
    coreframe->pbufCb = coreframe->pbufCr + ospec->plane_size[1];
    coreframe->pbufVirtCr =
        (unsigned char *) frame_memory->vaddr + ospec->plane_size[0];
    coreframe->pbufVirtCb = coreframe->pbufVirtCr + ospec->plane_size[1];
  } else if (ospec->fourcc == GST_STR_FOURCC ("TNVF")) {
    coreframe->pbufCb =
        (unsigned char *) frame_memory->paddr + ospec->plane_size[0];
    coreframe->pbufVirtCb =
        (unsigned char *) frame_memory->vaddr + ospec->plane_size[0];
    coreframe->pbufY_tilebot = coreframe->pbufCb + ospec->plane_size[1];
    coreframe->pbufVirtY_tilebot = coreframe->pbufVirtCb + ospec->plane_size[1];
    coreframe->pbufCb_tilebot = coreframe->pbufY_tilebot + ospec->plane_size[2];
    coreframe->pbufVirtCb_tilebot =
        coreframe->pbufVirtY_tilebot + ospec->plane_size[2];
  } else {
    coreframe->pbufCb =
        (unsigned char *) frame_memory->paddr + ospec->plane_size[0];
    coreframe->pbufCr = coreframe->pbufCb + ospec->plane_size[1];
    coreframe->pbufVirtCb =
        (unsigned char *) frame_memory->vaddr + ospec->plane_size[0];
    coreframe->pbufVirtCr = coreframe->pbufVirtCb + ospec->plane_size[1];
  }

  coreframe->pbufMvCol = (unsigned char *) extra_memory->paddr;
  coreframe->pbufVirtMvCol = (unsigned char *) extra_memory->vaddr;

}

static gboolean
gst_vpudec_core_create_and_register_frames (GstVpuDec * vpudec, gint num)
{
  gboolean ret = FALSE;
  int timeout = vpudec->options.allocframetimeout;
  GstFlowReturn retval;
  GstBuffer *gstbuf;
  VpuFrameBuffer *vpucore_frame;
  VpuDecFrame *vpu_frame;
  VpuFrameBuffer *vpuframebuffers = MM_MALLOC (sizeof (VpuFrameBuffer) * num);
  VpuDecMem *mvblock;
  VpuMemory frame_memory, extra_memory;

  vpudec->frames = MM_MALLOC (sizeof (VpuDecFrame) * num);

  if ((vpuframebuffers == NULL) || (vpudec->frames == NULL)) {
    goto fail;
  }

  if (vpudec->ospec.frame_extra_size) {
    if ((mvblock =
            vpudec_core_mem_alloc_dma_buffer (num *
                vpudec->ospec.frame_extra_size, &extra_memory.paddr,
                &extra_memory.vaddr))) {
      ATTACH_MEM2VPUDEC (vpudec, mvblock);
    } else {
      goto fail;
    }
  }

  memset (vpuframebuffers, 0, sizeof (VpuFrameBuffer) * num);
  memset (vpudec->frames, 0, sizeof (VpuDecFrame) * num);

  vpudec->frame_num = 0;

  do {
    gstbuf = NULL;
    retval = gst_pad_alloc_buffer_and_set_caps (vpudec->srcpad, 0,
        vpudec->ospec.frame_size, GST_PAD_CAPS (vpudec->srcpad), &gstbuf);
    if (retval == GST_FLOW_OK) {
      if ((IS_DMABLE_BUFFER (gstbuf))
          && (((guint32) GST_BUFFER_DATA (gstbuf) %
                  vpudec->ospec.buffer_align) == 0)) {
        vpucore_frame = &vpuframebuffers[vpudec->frame_num];
        vpu_frame = &vpudec->frames[vpudec->frame_num];
        frame_memory.vaddr = (gchar *) GST_BUFFER_DATA (gstbuf);
        frame_memory.paddr = (gchar *) DMABLE_BUFFER_PHY_ADDR (gstbuf);
        gst_vpudec_assign_frame_pointers (&vpudec->ospec, vpucore_frame,
            &frame_memory, &extra_memory);
        vpu_frame->key = frame_memory.vaddr;
        vpu_frame->display_handle = NULL;
        vpu_frame->gstbuf = gstbuf;
        vpu_frame->id = vpudec->frame_num;

        extra_memory.paddr += vpudec->ospec.frame_extra_size;
        extra_memory.vaddr += vpudec->ospec.frame_extra_size;
        vpudec->frame_num++;
      } else {
        gst_buffer_unref (gstbuf);
        timeout--;
        usleep (30000);
      }
    } else {
      timeout--;
      usleep (30000);
    }
  } while (((timeout) >= 0) && (vpudec->frame_num < num));

  if (vpudec->frame_num == num) {
    VpuDecRetCode core_ret;
    CORE_API (VPU_DecRegisterFrameBuffer, goto fail, core_ret,
        vpudec->context.handle, vpuframebuffers, num);
    ret = TRUE;
  } else {
    VpuDecMem *frameblock;
    while (vpudec->frame_num < num) {
      gint size = vpudec->ospec.frame_size + vpudec->ospec.buffer_align - 1;
      frameblock =
          vpudec_core_mem_alloc_dma_buffer (size, &frame_memory.paddr,
          &frame_memory.vaddr);
      if (frameblock) {
        GstBufferMeta *bufmeta = gst_buffer_meta_new ();
        frame_memory.paddr =
            (void *) Align (frame_memory.paddr, vpudec->ospec.buffer_align);
        frame_memory.vaddr =
            (void *) Align (frame_memory.vaddr, vpudec->ospec.buffer_align);
        vpucore_frame = &vpuframebuffers[vpudec->frame_num];
        vpu_frame = &vpudec->frames[vpudec->frame_num];
        frameblock->parent = gst_object_ref (vpudec);
        {
          gstbuf = gst_buffer_new ();

          GST_BUFFER_SIZE (gstbuf) = vpudec->ospec.frame_size;
          GST_BUFFER_DATA (gstbuf) = (gchar *) frame_memory.vaddr;
          GST_BUFFER_OFFSET (gstbuf) = 0;
          gst_buffer_set_caps (gstbuf, GST_PAD_CAPS (vpudec->srcpad));

          gint index = G_N_ELEMENTS (gstbuf->_gst_reserved) - 1;
          bufmeta->physical_data = frame_memory.paddr;
          bufmeta->priv = frameblock;
          gstbuf->_gst_reserved[index] = bufmeta;


          GST_BUFFER_MALLOCDATA (gstbuf) = (guint8 *) bufmeta;
          GST_BUFFER_FREE_FUNC (gstbuf) = gst_vpudec_free_internal_frame;
        }

        gst_vpudec_assign_frame_pointers (&vpudec->ospec, vpucore_frame,
            &frame_memory, &extra_memory);
        vpu_frame->key = frame_memory.vaddr;
        vpu_frame->display_handle = NULL;
        vpu_frame->gstbuf = gstbuf;
        vpu_frame->id = vpudec->frame_num;

        extra_memory.paddr += vpudec->ospec.frame_extra_size;
        extra_memory.vaddr += vpudec->ospec.frame_extra_size;
        vpudec->frame_num++;

      } else {
        GST_ERROR ("Can not allocate enough framebuffers for output!!");
        goto fail;
      }
    }
    GST_WARNING ("Allocate Internal framebuffers!!!!");
    VpuDecRetCode core_ret;
    CORE_API (VPU_DecRegisterFrameBuffer, goto fail, core_ret,
        vpudec->context.handle, vpuframebuffers, num);
    ret = TRUE;

  }

fail:
  if (vpuframebuffers) {
    MM_FREE (vpuframebuffers);
  }


  return ret;

}

static gboolean
gst_vpudec_check_and_set_output_spec (GstVpuDec * vpudec, gint width,
    gint height)
{
  gboolean ret = TRUE;
  VpuOutPutSpec *ospec = &vpudec->ospec;

  if (vpudec->context.openparam.CodecFormat == VPU_V_MJPG) {

#define JPEG_SUPPORT_MASK  (0x1b)
    /*
     * 0 C 4:2:0,
     * 1 C 4:2:2 horizontal,
     * 2 C 4:2:2 vertical,
     * 3 C 4:4:4,
     * 4 C 4:0:0
     */
    if (!((1 << vpudec->context.initinfo.
                nMjpgSourceFormat) & JPEG_SUPPORT_MASK)) {
      ret = FALSE;
    } else {
      ospec->plane_size[0] = width * height;
      ospec->frame_extra_size = 0;

      /* FIX ME assume crop_left and crop_top all equal 0 */
      switch (vpudec->context.initinfo.nMjpgSourceFormat) {
        case 0:
          vpudec->ospec.ostructure =
              gst_structure_from_string (VIDEO_RAW_CAPS_I420, NULL);
          ospec->frame_size = ospec->plane_size[0] * 3 / 2;
          ospec->plane_size[1] = ospec->plane_size[0] / 4;
          break;
        case 1:
          vpudec->ospec.ostructure =
              gst_structure_from_string (VIDEO_RAW_CAPS_Y42B, NULL);
          ospec->plane_size[1] = ospec->plane_size[0] / 2;
          ospec->frame_size = ospec->plane_size[0] * 2;
          break;
        case 3:
          vpudec->ospec.ostructure =
              gst_structure_from_string (VIDEO_RAW_CAPS_Y444, NULL);
          ospec->plane_size[1] = ospec->plane_size[0];
          ospec->frame_size = ospec->plane_size[0] * 3;
          break;
        case 4:
          vpudec->ospec.ostructure =
              gst_structure_from_string (VIDEO_RAW_CAPS_Y800, NULL);
          ospec->frame_size = ospec->plane_size[0];
          ospec->plane_size[1] = 0;
          break;
        default:
          break;
      };

      gst_structure_get_fourcc (vpudec->ospec.ostructure, "format",
          &ospec->fourcc);
    }
  } else {
    gst_structure_get_fourcc (vpudec->ospec.ostructure, "format",
        &ospec->fourcc);

    if (ospec->fourcc == GST_STR_FOURCC ("TNVF")) {
      ospec->plane_size[0] = ospec->plane_size[2] =
          Align (width * height / 2, vpudec->ospec.buffer_align);
      ospec->plane_size[1] = ospec->plane_size[3] =
          Align (width * height / 4, vpudec->ospec.buffer_align);
      ospec->frame_size = (ospec->plane_size[0] + ospec->plane_size[1]) * 2;
      ospec->frame_extra_size = width * height / 4;

    } else {
      ospec->plane_size[0] = Align (width * height, vpudec->ospec.buffer_align);
      ospec->plane_size[1] =
          Align (width * height / 4, vpudec->ospec.buffer_align);

      ospec->frame_size = ospec->plane_size[0] + 2 * ospec->plane_size[1];
      ospec->frame_extra_size = width * height / 4;
    }
  }
  return ret;
}



static gboolean
gst_vpudec_set_downstream_pad (GstVpuDec * vpudec, gint num)
{
  gboolean ret = FALSE;
  GstCaps *caps = NULL;
  guint pad_width, pad_height;
  VpuDecInitInfo *initinfo = &vpudec->context.initinfo;
  VpuOutPutSpec *ospec = &vpudec->ospec;


  GST_INFO
      ("Get initinfo padwidth %d, padheight %d, left %d, right %d, top %d, bottom %d, interlace %d",
      initinfo->nPicWidth, initinfo->nPicHeight, initinfo->PicCropRect.nLeft,
      initinfo->PicCropRect.nRight, initinfo->PicCropRect.nTop,
      initinfo->PicCropRect.nBottom, initinfo->nInterlace);

  if (initinfo->nInterlace){
    ospec->height_align <<= 1;
  }

  pad_width = Align (initinfo->nPicWidth, ospec->width_align);
  pad_height = Align (initinfo->nPicHeight, ospec->height_align);

  if (initinfo->nAddressAlignment) {
    ospec->buffer_align = initinfo->nAddressAlignment;
  }

  ospec->width = initinfo->PicCropRect.nRight - initinfo->PicCropRect.nLeft;
  ospec->height = initinfo->PicCropRect.nBottom - initinfo->PicCropRect.nTop;

  ospec->crop_left = initinfo->PicCropRect.nLeft;
  ospec->crop_top = initinfo->PicCropRect.nTop;
  ospec->crop_right = pad_width - ospec->width - ospec->crop_left;
  ospec->crop_bottom = pad_height - ospec->height - ospec->crop_top;

  ospec->width_ratio = initinfo->nQ16ShiftWidthDivHeightRatio;
  ospec->height_ratio = 0x10000;

  if ((ospec->width_ratio % ospec->height_ratio) == 0) {
    ospec->width_ratio = (ospec->width_ratio / ospec->height_ratio);
    ospec->height_ratio = 1;
  }

  if (gst_vpudec_check_and_set_output_spec (vpudec, pad_width,
          pad_height) == FALSE) {
    GST_ERROR ("gst_vpudec_check_and_set_output_spec failed!!");
    goto fail;
  }

  caps =
      gst_caps_new_full (gst_structure_copy (vpudec->ospec.ostructure), NULL);
  gst_caps_set_simple (caps, "width", G_TYPE_INT,
      ospec->width + ospec->crop_left + ospec->crop_right, "height", G_TYPE_INT,
      ospec->height + ospec->crop_top + ospec->crop_bottom,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, ospec->width_ratio,
      ospec->height_ratio, CAPS_FIELD_CROP_TOP, G_TYPE_INT, ospec->crop_top,
      CAPS_FIELD_CROP_LEFT, G_TYPE_INT, ospec->crop_left, CAPS_FIELD_CROP_RIGHT,
      G_TYPE_INT, ospec->crop_right, CAPS_FIELD_CROP_BOTTOM, G_TYPE_INT,
      ospec->crop_bottom, CAPS_FIELD_REQUIRED_BUFFER_NUMBER, G_TYPE_INT, num,
      "framerate", GST_TYPE_FRACTION, vpudec->options.framerate_n,
      vpudec->options.framerate_d, "alignment", G_TYPE_INT, ospec->buffer_align,
      NULL);

  GST_INFO ("Try set downstream caps %" GST_PTR_FORMAT, caps);

  if (!(gst_pad_set_caps (vpudec->srcpad, caps))) {
    GST_ERROR ("Error in set downstream caps");
    goto fail;
  }
  caps = NULL;
  ret = TRUE;
fail:
  if (caps) {
    gst_caps_unref (caps);
  }
  return ret;
}



static void
gst_vpudec_core_check_display_queue (GstVpuDec * vpudec)
{
  int i;
  VpuDecFrame *frame;
  for (i = 0; i < vpudec->frame_num; i++) {
    if ((vpudec->frames[i].display_handle)      //){
        && (vpudec->age != vpudec->frames[i].age)) {
      if (gst_buffer_is_writable (vpudec->frames[i].gstbuf)) {
        VpuDecRetCode core_ret;
        CORE_API (VPU_DecOutFrameDisplayed,, core_ret, vpudec->context.handle,
            (vpudec->frames[i].display_handle));
        vpudec->frames[i].display_handle = NULL;
        vpudec->output_size--;
#ifdef MX6_CLEARDISPLAY_WORKAROUND
        return;
#endif
      }
    } else {
    }
  }
}




static gboolean
gst_vpudec_core_start (GstVpuDec * vpudec)
{
  gboolean ret = FALSE;
  int num =
      vpudec->context.initinfo.nMinFrameBufferCount +
      vpudec->options.bufferplus;
  GST_INFO ("Get min framebuffer count %d",
      vpudec->context.initinfo.nMinFrameBufferCount);

  if (!gst_vpudec_set_downstream_pad (vpudec, num)) {
    goto fail;
  }

  if (vpudec->tsm) {
    setTSManagerFrameRate (vpudec->tsm, vpudec->options.framerate_n,
        vpudec->options.framerate_d);
  }
  if (!gst_vpudec_core_create_and_register_frames (vpudec, num)) {
    goto fail;
  }

  ret = TRUE;
fail:
  return ret;
}


static void
gst_vpudec_finalize (GObject * object)
{
  GstVpuDec *vpudec;

  vpudec = GST_VPUDEC (object);

  VPU_DecUnLoad ();

  g_mutex_free (vpudec->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_vpudec_setconfig (GstVpuDec * vpudec)
{

  VpuDecRetCode core_ret;
  gint32 param;
  GST_INFO ("Set drop policy %d", vpudec->drop_policy);
  CORE_API (VPU_DecConfig,, core_ret, vpudec->context.handle,
      VPU_DEC_CONF_SKIPMODE, &(vpudec->drop_policy));
}


static gboolean
gst_vpudec_setcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret = TRUE;
  GstVpuDec *vpudec;
  GstCaps *src_caps;
  int enabled = 0;

  vpudec = GST_VPUDEC (GST_PAD_PARENT (pad));

  GST_INFO ("Get upstream caps %" GST_PTR_FORMAT, caps);

  if ((vpudec->context.openparam.CodecFormat =
          vpudec_find_std (caps)) >= VPU_V_MPEG4) {
    VpuDecRetCode core_ret;
    GstStructure *structure;
    gint intvalue, intvalue0;
    const GValue *value = NULL;

    GST_INFO ("Get codec std %d", vpudec->context.openparam.CodecFormat);

    structure = gst_caps_get_structure (caps, 0);
    if (gst_structure_get_int (structure, "width", &intvalue)) {
      GST_INFO ("Set Width %d", intvalue);
      vpudec->context.openparam.nPicWidth = intvalue;
    }
    if (gst_structure_get_int (structure, "height", &intvalue)) {
      GST_INFO ("Set Height %d", intvalue);
      vpudec->context.openparam.nPicHeight = intvalue;
    }
    if (value = gst_structure_get_value (structure, "codec_data")) {
      GstBuffer *codec_data = gst_value_get_buffer (value);
      if ((codec_data) && GST_BUFFER_SIZE (codec_data)) {
        GST_INFO ("Set codec_data %" GST_PTR_FORMAT, codec_data);
        vpudec->codec_data = gst_buffer_ref (codec_data);
      }
    }
    if (value = gst_structure_get_value (structure, "framed")) {
      if (g_value_get_boolean (value)) {
        vpudec->framed = TRUE;
      } else {
        vpudec->framed = FALSE;
      }
      GST_INFO ("Set framed %s", ((vpudec->framed) ? "true" : "false"));
    }
    if (gst_structure_get_fraction (structure, "framerate", &intvalue,
            &intvalue0)) {
      if ((intvalue > 0) && (intvalue0 > 0)) {
        vpudec->options.framerate_n = intvalue;
        vpudec->options.framerate_d = intvalue0;
      }
    }
    vpudec->context.openparam.nChromaInterleave = 0;
    vpudec->context.openparam.nMapType = 0;
    vpudec->context.openparam.nTiled2LinearEnable = 0;

    if (vpudec->context.openparam.CodecFormat != VPU_V_MJPG) {
      GstCaps *allow_caps;
      if (allow_caps = gst_pad_get_allowed_caps (vpudec->srcpad)) {
        GST_INFO ("got downstream allow caps %" GST_PTR_FORMAT, allow_caps);
        if ((vpudec->ospec.ostructure =
                vpudec_find_compatible_structure_for_strings (g_format_caps_map
                    [vpudec->options.ofmt], allow_caps)) == NULL) {
          gst_caps_unref (allow_caps);
          goto error;
        } else {
          gint value;
          if (gst_structure_get_int (vpudec->ospec.ostructure, "width_align",
                  &value)) {
            vpudec->ospec.width_align = value;
          }
          if (gst_structure_get_int (vpudec->ospec.ostructure, "height_align",
                  &value)) {
            vpudec->ospec.height_align = value;
          }
        }
        gst_caps_unref (allow_caps);
      } else {
        if ((vpudec->ospec.ostructure =
                gst_structure_from_string (VIDEO_RAW_CAPS_I420,
                    NULL)) == NULL) {
          goto error;
        }
      }
      const gchar *mime = gst_structure_get_name (vpudec->ospec.ostructure);
      if (!strcmp (mime, VIDEO_RAW_CAPS_YUV)) {
        guint32 fourcc = 0;
        gst_structure_get_fourcc (vpudec->ospec.ostructure, "format", &fourcc);
        if (fourcc == GST_STR_FOURCC ("NV12")) {
          vpudec->context.openparam.nChromaInterleave = 1;
        } else if (fourcc == GST_STR_FOURCC ("TNVP")) {
          vpudec->context.openparam.nChromaInterleave = 1;
          vpudec->context.openparam.nMapType = 1;
        } else if (fourcc == GST_STR_FOURCC ("TNVF")) {
          vpudec->context.openparam.nChromaInterleave = 1;
          vpudec->context.openparam.nMapType = 2;
        }

      }
    } else {
      vpudec->context.openparam.nChromaInterleave = 0;
    }

    vpudec->context.openparam.nReorderEnable = 1;
    vpudec->context.openparam.nEnableFileMode = 0;

    CORE_API (VPU_DecOpen, goto error, core_ret, &vpudec->context.handle,
        &vpudec->context.openparam, &vpudec->context.meminfo);

    CORE_API (VPU_DecGetCapability, goto error, core_ret,
        vpudec->context.handle, VPU_DEC_CAP_FRAMESIZE, &enabled);
    if ((enabled) && (vpudec->options.experimental_tsm)) {
      GST_INFO ("Use new tsm scheme");
      vpudec->use_new_tsm = TRUE;
    } else {
      GST_INFO ("Use normal tsm scheme");
    }

    gst_vpudec_setconfig (vpudec);

  } else {
    GST_ERROR ("Can not find proper codec standard");
    goto error;
  }

  return ret;

error:
  return FALSE;
}


static VpuDecFrame *
gst_vpudec_get_frame (GstVpuDec * vpudec, VpuFrameBuffer * cframe)
{
  VpuDecFrame *frame;
  int i;
  for (i = 0; i < vpudec->frame_num; i++) {
    frame = &vpudec->frames[i];
    if (frame->key == cframe->pbufVirtY) {
      return frame;
    }
  }
  return NULL;
}

static void
gst_vpudec_set_field (GstVpuDec * vpudec, GstBuffer * buf)
{
  if (!buf)
    return;

  GstCaps *caps = GST_BUFFER_CAPS (buf);
  GstCaps *newcaps;
  GstStructure *stru;
  gint field = 0;
  stru = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (stru, "field", &field);
  if (field != g_fieldmap[vpudec->field_info]) {
    newcaps = gst_caps_copy (caps);
    gst_caps_set_simple (newcaps, "field", G_TYPE_INT,
        g_fieldmap[vpudec->field_info], NULL);
    gst_buffer_set_caps (buf, newcaps);
    gst_caps_unref (newcaps);
  }

}

static GstFlowReturn
gst_vpudec_show_frame (GstVpuDec * vpudec, VpuDecFrame * frame,
    VpuFrameBuffer * p)
{
  GstFlowReturn ret = GST_FLOW_OK;
  vpudec->vpu_stat.out_cnt++;
  gboolean display = FALSE;
  if ((vpudec->mosaic_cnt == 0)
      || (vpudec->mosaic_cnt > vpudec->options.mosaic_threshold)) {
    gint dropcntmask;
    if (dropcntmask = (vpudec->drop_level & SKIP_NUM_MASK)) {
      if ((vpudec->vpu_stat.out_cnt & dropcntmask)) {
        display = TRUE;
      }
    } else {
      display = TRUE;
    }
  }

  if (display) {
    vpudec->prerolling = FALSE;
    vpudec->vpu_stat.show_cnt++;
    GstBuffer *gstbuf = frame->gstbuf;
    if (vpudec->field_info) {
      gst_vpudec_set_field (vpudec, gstbuf);
    }
    GST_BUFFER_TIMESTAMP (gstbuf) = TSManagerSend2 (vpudec->tsm, p);

    GST_LOG ("Predict time %" GST_TIME_FORMAT " actually time %"
        GST_TIME_FORMAT, GST_TIME_ARGS (vpudec->predict_ts),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (gstbuf)));

    GST_BUFFER_DURATION (gstbuf) = 0;
    gst_buffer_ref (gstbuf);

    GST_LOG ("push sample %" GST_TIME_FORMAT " size %d",
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (gstbuf)),
        GST_BUFFER_SIZE (gstbuf));

    ret = gst_pad_push (vpudec->srcpad, gstbuf);
  } else {
    TSManagerSend (vpudec->tsm);
  }

  return ret;
}


static GstFlowReturn
gst_vpudec_process_error(GstVpuDec * vpudec, VpuDecRetCode vpuretcode)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  VpuDecRetCode core_ret;
  VpuDecErrInfo err;

  CORE_API (VPU_DecGetErrInfo, , core_ret, vpudec->context.handle, &err);

  switch (err){
    case VPU_DEC_ERR_NOT_SUPPORTED:
      GST_ELEMENT_ERROR(vpudec, STREAM, FORMAT, ("format not support"), (NULL));
      break;
    case VPU_DEC_ERR_CORRUPT:
      ret = GST_FLOW_UNEXPECTED;
      GST_ELEMENT_ERROR(vpudec, STREAM, DECODE, ("corrupt stream detect"), (NULL));
      break;
    default:
      GST_ELEMENT_ERROR(vpudec, STREAM, FAILED, ("unknown error detect"), (NULL));
      break;
  };

bail:
  return ret;

}


#define VPU_ASSIGN_OUTPUT(vpudec, frame, buffer)\
  do {\
    (vpudec)->output_size++; \
    (frame)->age = (++((vpudec)->age)); \
    (frame)->display_handle = (buffer); \
  }while(0)

static GstFlowReturn
gst_vpudec_chain (GstPad * pad, GstBuffer * buffer)
{
  GstVpuDec *vpudec;
  GstFlowReturn ret = GST_FLOW_UNEXPECTED;
  VpuBufferNode inbuf = { 0 };
  VpuDecRetCode core_ret;
  int core_buf_ret;
  gint retrycnt;
  gint avail_frame;
  gboolean retry;

  vpudec = GST_VPUDEC (GST_PAD_PARENT (pad));

  retrycnt = 0;

  g_return_val_if_fail (vpudec->context.handle, GST_FLOW_WRONG_STATE);

  if (buffer) {
    vpudec->vpu_stat.in_cnt++;

    GST_LOG ("Chain in with size = %d", GST_BUFFER_SIZE (buffer));

    if (G_UNLIKELY ((vpudec->new_segment))) {
      if ((buffer) && (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
        resyncTSManager (vpudec->tsm, GST_BUFFER_TIMESTAMP (buffer),
            vpudec->tsm_mode);
      }
      vpudec->new_segment = FALSE;
    }

    if (vpudec->use_new_tsm) {
      TSManagerReceive2 (vpudec->tsm, GST_BUFFER_TIMESTAMP (buffer),
          GST_BUFFER_SIZE (buffer));
    } else {
      TSManagerReceive (vpudec->tsm, GST_BUFFER_TIMESTAMP (buffer));
    }
    inbuf.nSize = GST_BUFFER_SIZE (buffer);
    inbuf.pPhyAddr = NULL;
    inbuf.pVirAddr = GST_BUFFER_DATA (buffer);
    if (vpudec->codec_data) {
      inbuf.sCodecData.nSize = GST_BUFFER_SIZE (vpudec->codec_data);
      inbuf.sCodecData.pData = GST_BUFFER_DATA (vpudec->codec_data);
    }
  } else {
    /* eos case, weird vpu wrap */
    inbuf.nSize = 0;
    inbuf.pVirAddr = (unsigned char *) 0x1;
  }

  do {

    retry = FALSE;

    if (vpudec->frame_num) {
#ifdef MX6_CLEARDISPLAY_WORKAROUND
      while (vpudec->output_size > vpudec->options.bufferplus) {
#endif
        gst_vpudec_core_check_display_queue (vpudec);
#ifdef MX6_CLEARDISPLAY_WORKAROUND
        if (vpudec->output_size > vpudec->options.bufferplus)
          usleep (10000);
        else
          break;
      };
#endif
    }
    vpudec->predict_ts = TSManagerQuery2 (vpudec->tsm, NULL);
    core_buf_ret = 0;
    CORE_API (VPU_DecDecodeBuf, {
          if (core_ret == VPU_DEC_RET_FAILURE_TIMEOUT)
        CORE_API (VPU_DecReset,, core_ret, vpudec->context.handle); goto bail;}
        , core_ret, vpudec->context.handle, &inbuf, &core_buf_ret);

    GST_LOG ("buf status 0x%x data %d", core_buf_ret, inbuf.nSize);

    if ((vpudec->use_new_tsm) && (core_buf_ret & VPU_DEC_ONE_FRM_CONSUMED)) {
      VpuDecFrameLengthInfo linfo;
      CORE_API (VPU_DecGetConsumedFrameInfo,, core_ret, vpudec->context.handle,
          &linfo);
      if (core_ret == VPU_DEC_RET_SUCCESS) {
        TSManagerValid2 (vpudec->tsm, linfo.nFrameLength + linfo.nStuffLength,
            linfo.pFrame);
      }
    }

    if (core_buf_ret & VPU_DEC_OUTPUT_DIS) {
      VpuDecOutFrameInfo oinfo;
      VpuDecFrame *oframe;
      vpudec->mosaic_cnt = 0;
      CORE_API (VPU_DecGetOutputFrame, goto bail, core_ret,
          vpudec->context.handle, &oinfo);
      oframe = gst_vpudec_get_frame (vpudec, oinfo.pDisplayFrameBuf);

      if (oframe) {
        retrycnt = 0;
        if (oframe->display_handle == NULL) {
          VPU_ASSIGN_OUTPUT (vpudec, oframe, oinfo.pDisplayFrameBuf);
          vpudec->field_info = oinfo.eFieldType;
          ret = gst_vpudec_show_frame (vpudec, oframe, oinfo.pDisplayFrameBuf);
        } else {
          GST_WARNING ("Frame %d still in displaying queue!!", oframe->id);
          TSManagerSend (vpudec->tsm);
        }
      } else {
        GST_ERROR ("Can not find output frame %p", oinfo.pDisplayFrameBuf);
        goto bail;
      }

    } else if (core_buf_ret & VPU_DEC_INIT_OK) {
      CORE_API (VPU_DecGetInitialInfo, goto bail, core_ret,
          vpudec->context.handle, &vpudec->context.initinfo);
      if (!gst_vpudec_core_start (vpudec)) {
        goto bail;
      }
    } else if (core_buf_ret & VPU_DEC_OUTPUT_REPEAT) {
      GST_INFO ("Got repeat information!!");
      TSManagerSend (vpudec->tsm);
    } else if (core_buf_ret & VPU_DEC_OUTPUT_DROPPED) {
      GST_INFO ("Got drop information!!");
      TSManagerSend (vpudec->tsm);
    } else if (core_buf_ret & VPU_DEC_OUTPUT_NODIS) {
      retrycnt++;
      GST_INFO ("Got no disp information!!");
    } else if (core_buf_ret & VPU_DEC_OUTPUT_MOSAIC_DIS) {
      vpudec->mosaic_cnt++;
      VpuDecOutFrameInfo oinfo;
      VpuDecFrame *oframe;
      CORE_API (VPU_DecGetOutputFrame, goto bail, core_ret,
          vpudec->context.handle, &oinfo);
      oframe = gst_vpudec_get_frame (vpudec, oinfo.pDisplayFrameBuf);

      if (oframe) {
        retrycnt = 0;
        if (oframe->display_handle == NULL) {
          VPU_ASSIGN_OUTPUT (vpudec, oframe, oinfo.pDisplayFrameBuf);
          ret = gst_vpudec_show_frame (vpudec, oframe, oinfo.pDisplayFrameBuf);
        } else {
          GST_WARNING ("Frame %d still in displaying queue!!", oframe->id);
          TSManagerSend (vpudec->tsm);
        }
      } else {
        GST_ERROR ("Can not find output frame %p", oinfo.pDisplayFrameBuf);
        goto bail;
      }
    } else {
      retrycnt++;
    }

    if (core_buf_ret & VPU_DEC_NO_ENOUGH_BUF) {
      GST_WARNING
          ("Got no frame buffer message, return 0x%x, %d frames in displaying queue!!",
          core_buf_ret, vpudec->output_size);
      retrycnt++;
    }

    if (core_buf_ret & VPU_DEC_SKIP) {
      GST_INFO ("Got skip message!!");
      TSManagerSend (vpudec->tsm);
    }

    if (core_buf_ret & VPU_DEC_NO_ENOUGH_INBUF) {
      GST_INFO ("Got not enough input message!!");
    }

    if (core_buf_ret & VPU_DEC_FLUSH) {
      GST_WARNING ("Got need flush message!!");
      CORE_API (VPU_DecFlushAll,, core_ret, vpudec->context.handle);
    }

    if (core_buf_ret & VPU_DEC_OUTPUT_EOS) {
      GST_INFO ("Got EOS message!!");
      if (buffer) {
        GST_WARNING ("Got EOS message with buffer not NULL!!");
      }
      break;
    }
#if 0
    if ((core_buf_ret == VPU_DEC_INPUT_USED) && (retrycnt == 0)) {
      inbuf.nSize = 0;
      inbuf.pVirAddr = NULL;
      retry = TRUE;
    } else {
      retry = FALSE;
    }
#endif

    if (core_buf_ret & VPU_DEC_OUTPUT_EOS) {
      break;
    } else if (buffer == NULL) {
      retrycnt++;
      retry = TRUE;
    }

    if (core_buf_ret & VPU_DEC_NO_ENOUGH_INBUF) {
      break;
    }

    if ((!(core_buf_ret & VPU_DEC_INPUT_USED)) && (inbuf.nSize)) {
      retry = TRUE;
    }


    if (vpudec->prerolling) {
      if ((inbuf.nSize)) {
        if (retry == FALSE) {
          inbuf.nSize = 0;
          inbuf.pVirAddr = 0;
          retry = TRUE;
        }
      }
    }

    if (vpudec->options.low_latency) {
      if ((inbuf.nSize)) {
        if (retry == FALSE) {
          inbuf.nSize = 0;
          inbuf.pVirAddr = 0;
          retry = TRUE;
        }
      } else if (core_buf_ret & VPU_DEC_OUTPUT_DIS) {
        retry = TRUE;
      }
    }


  } while ((retry) && ((retrycnt) < vpudec->options.decode_retry_cnt));

  if (retrycnt >= vpudec->options.decode_retry_cnt) {
    GST_WARNING ("Retry too many times, maybe BUG!!");
  }

  ret = GST_FLOW_OK;

  if (buffer) {
    gst_buffer_unref (buffer);
  }

  return ret;
bail:
  {
    if (buffer) {
      gst_buffer_unref (buffer);
    }

    ret = gst_vpudec_process_error(vpudec, core_ret);
    return ret;
  }
}



static GstStateChangeReturn
gst_vpudec_state_change (GstElement * element, GstStateChange transition)
{
  GstVpuDec *vpudec;
  GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

  vpudec = GST_VPUDEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      MM_INIT_DBG_MEM ("vpudec");
      if (!vpudec_core_init (vpudec)) {
        goto bail;
      }
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      vpudec_core_deinit (vpudec);
      MM_DEINIT_DBG_MEM ();
      if (vpudec->options.profiling) {
        g_print ("total decode time %" GST_TIME_FORMAT "\n",
            GST_TIME_ARGS (vpudec->profile_count.decode_time));
      }
      break;
    }
    default:
      break;
  }
bail:
  return ret;
}


static void
vpudec_init_qos_ctrl(VpuDecQosCtl * qos)
{
  qos->cur_drop_level = 0;
  qos->guard_ms = 5000;
  qos->l1_ms = 2000000;
  qos->l2_ms = 200000;
  qos->l3_ms = 10000;
  qos->l4_ms = 5000;
}


static guint
vpudec_process_qos (GstVpuDec *vpudec, GstClockTimeDiff diff)
{
  guint level;
  VpuDecQosCtl * q = &vpudec->qosctl;
  gint micro_diff = (diff)/1000;
  if ((micro_diff + q->guard_ms) > 0) {
    if (micro_diff>q->l1_ms) {
      level = SKIP_B | SKIP_1OF4;
    } else if (micro_diff>q->l2_ms) {
      level = SKIP_B;
    } else if (micro_diff>q->l3_ms) {
      level = SKIP_1OF16;
    } else if (micro_diff>q->l4_ms) {
      level = SKIP_1OF32;
    } else {
      level = SKIP_1OF64;
    }
  }
  else {
    level = 0;
  }
  if (level!=q->cur_drop_level){
    GST_INFO ("change drop level from %x to %x", q->cur_drop_level, level);
    q->cur_drop_level = level;
  }
  return level;
}


static gboolean
gst_vpudec_src_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = TRUE;

  GstVpuDec *vpudec;
  vpudec = GST_VPUDEC (GST_PAD_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
    {
      GstClockTimeDiff diff;
      GstClockTime timestamp;
      gdouble proportion;
      gint drop_policy;

      gst_event_parse_qos (event, &proportion, &diff, &timestamp);

      if (vpudec->options.adaptive_drop) {
        vpudec->drop_level = vpudec_process_qos(vpudec, diff);
        vpudec->drop_level &= vpudec->options.drop_level_mask;

        drop_policy =
          ((vpudec->drop_level & SKIP_BP) ? VPU_DEC_SKIPPB : ((vpudec->
              drop_level & SKIP_B) ? VPU_DEC_SKIPB : VPU_DEC_SKIPNONE));

        if (drop_policy != vpudec->drop_policy) {
          VpuDecRetCode core_ret;
          GST_INFO ("change vpu config %d to %d", vpudec->drop_policy,
              drop_policy);
         vpudec->drop_policy = drop_policy;
         gst_vpudec_setconfig (vpudec);
        }
      }

      ret = gst_pad_push_event (vpudec->sinkpad, event);
      break;
    }

    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }
  return ret;
}


static gboolean
gst_vpudec_sink_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = TRUE;
  GstVpuDec *vpudec;
  vpudec = GST_VPUDEC (GST_PAD_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gint64 start, stop, position;
      gint64 nstart, nstop;
      GstEvent *nevent;
      gdouble rate;

      gst_event_parse_new_segment (event, NULL, &rate, &format, &start,
          &stop, &position);

      if (format == GST_FORMAT_TIME) {

        if ((rate <= 2.0) && (rate >= 0.0)) {
          vpudec->tsm_mode = MODE_AI;
        } else {
          vpudec->tsm_mode = MODE_FIFO;
        }
        vpudec->new_segment = TRUE;
        vpudec->segment_start = start;
        resyncTSManager (vpudec->tsm, vpudec->segment_start, vpudec->tsm_mode);
        GST_INFO ("Get newsegment event from %" GST_TIME_FORMAT "to %"
            GST_TIME_FORMAT " pos %" GST_TIME_FORMAT, GST_TIME_ARGS (start),
            GST_TIME_ARGS (stop), GST_TIME_ARGS (position));
      } else {
        GST_WARNING ("Unsupport newsegment format %d", format);
        goto bail;
      }
      ret = gst_pad_event_default (pad, event);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      VpuDecRetCode core_ret;
      int i;
      CORE_API (VPU_DecFlushAll,, core_ret, vpudec->context.handle);

      for (i = 0; i < vpudec->frame_num; i++) {
        if (vpudec->frames[i].display_handle) {
          CORE_API (VPU_DecOutFrameDisplayed,, core_ret, vpudec->context.handle,
              (vpudec->frames[i].display_handle));
          vpudec->frames[i].display_handle = NULL;
        }
      };
      vpudec->prerolling = TRUE;
      vpudec->output_size = 0;
      ret = gst_pad_event_default (pad, event);
      break;
    }
    case GST_EVENT_EOS:
    {
      GST_INFO ("EOS received");
      gst_vpudec_chain (pad, NULL);
      ret = gst_pad_event_default (pad, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

  return ret;
bail:
  if (event) {
    gst_event_unref (event);
  }
  return FALSE;
}


static const GstQueryType *
gst_vpudec_query_types (GstPad * pad)
{
  static const GstQueryType src_query_types[] = {
    GST_QUERY_DURATION,
    0
  };

  return src_query_types;
}

static gboolean
gst_vpudec_src_query (GstPad * pad, GstQuery * query)
{

  gboolean ret = FALSE;
  GstVpuDec *vpudec;
  vpudec = GST_VPUDEC (GST_PAD_PARENT (pad));

  switch (GST_QUERY_TYPE (query)) {

    default:
      goto bail;
      break;
  }
bail:
  return ret;
}

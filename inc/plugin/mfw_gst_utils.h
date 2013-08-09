/*
 * Copyright (c) 2008-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_utils.h
 *
 * Description:    Head file of utilities for Gstreamer plugins.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 * Mar, 10 2008 Sario HU<b01138@freescale.com>
 * - Initial version
 * - Add direct render related macros.
 *
 * Jun, 13 2008 Dexter JI<b01140@freescale.com>
 * - Add framedrop algorithm related macros.
 *
 * Aug, 26 2008 Sario HU<b01138@freescale.com>
 * - Add misc macros. Rename to mfw_gst_utils.h.
 */


#ifndef __MFW_GST_UTILS_H__
#define __MFW_GST_UTILS_H__

/*=============================================================================
                                 MACROS
=============================================================================*/

#define _STR(s)     #s
#define STR(s)      _STR(s)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ANSI color print */
#define COLOR_RED       31
#define COLOR_GREEN     32
#define COLOR_YELLOW    33
#define COLOR_BLUE      34
#define COLOR_PURPLE    35
#define COLOR_CYAN      36

#define COLORFUL_STR(color, format, ...)\
    "\33[1;" STR(color) "m" format "\33[0m", ##__VA_ARGS__

#define YELLOW_STR(format,...)      COLORFUL_STR(COLOR_YELLOW,format, ##__VA_ARGS__)
#define RED_STR(format,...)         COLORFUL_STR(COLOR_RED,format, ##__VA_ARGS__)
#define GREEN_STR(format,...)       COLORFUL_STR(COLOR_GREEN,format, ##__VA_ARGS__)
#define BLUE_STR(format,...)        COLORFUL_STR(COLOR_BLUE,format,##__VA_ARGS__)
#define PURPLE_STR(format,...)      COLORFUL_STR(COLOR_PURPLE,format,##__VA_ARGS__)
#define CYAN_STR(format,...)        COLORFUL_STR(COLOR_CYAN,format,##__VA_ARGS__)

#define MFW_WEAK_ASSERT(condition) \
  do {\
    if (!(condition)){\
      g_print(RED_STR("ASSERTION FAILED in %s:%d\n", __FILE__, __LINE__));\
    }\
  }while(0)


/* version info print */
#define PRINT_CORE_VERSION(ver)\
    do{\
        g_print(YELLOW_STR("%s.\n",(ver)));\
    }while(0)

#define PRINT_PLUGIN_VERSION(ver)\
    do {\
        g_print(GREEN_STR("%s %s build on %s %s.\n", #ver,(ver),__DATE__,__TIME__));\
    }while(0)

#define PRINT_FINALIZE(name)\
    do {\
        g_print(GREEN_STR("[--->FINALIZE %s\n", (name)));\
    }while(0)


/* bit mask flag operation */
#define FLAG_SET_BIT(flag, bit)\
    do {\
        (flag) |= (bit);\
    }while(0)

#define FLAG_CLEAR_BIT(flag, bit)\
    do {\
        (flag) &= (~(bit));\
    }while(0)

#define FLAG_TEST_BIT(flag, bit)\
    ((flag) & (bit))


#define ROUNDUP8(data)\
    ((((data)+7)>>3)<<3)

/* resource related debug defines */
typedef enum
{
  RES_MEM = 0,
  RES_FILE_DEVICE = 1,
  RES_GSTBUFFER = 2,
  RES_GSTADAPTER = 3,
  RES_TYPE_MAX,
} Fsl_Resource_Type;


#include <time.h>

#define TIME_PROFILE(func_body, timdiff) \
  do{\
    struct timespec tstart, tstop;\
    clock_gettime(CLOCK_MONOTONIC, &tstart);\
    func_body;\
    clock_gettime(CLOCK_MONOTONIC, &tstop);\
    (timdiff) = GST_SECOND*(tstop.tv_sec-tstart.tv_sec) + (tstop.tv_nsec-tstart.tv_nsec);\
  }while(0)

#ifdef MEMORY_DEBUG
#include "mfw_gst_debug.h"

#if 0
#define MM_MALLOC(size)\
    dbg_mem_alloc((size), "line" STR(__LINE__) " of " STR(__FILE__) )
#define MM_FREE(ptr)\
    dbg_mem_free((ptr), "line " STR(__LINE__) " of " STR(__FILE__) )
#define MM_INIT_DBG_MEM(mname)\
    dbg_mem_init(mname)
#define MM_DEINIT_DBG_MEM()\
    dbg_mem_deinit()
#define MM_CHECK(detail)\
        dbg_mem_print_nonfree((detail))

#else




#define MM_MALLOC(size) fsl_alloc_dbg((size), __FILE__, __LINE__)
#define MM_REALLOC(ptr, size) fsl_realloc_dbg((ptr), (size), __FILE__, __LINE__)

#define MM_FREE(ptr) fsl_dealloc_dbg((ptr), __FILE__, __LINE__)

#define MM_INIT_DBG_MEM(mname) g_mmmodulename = (mname)
#define MM_DEINIT_DBG_MEM() \
    print_non_free_resource();
#define MM_CHECK(detail)

#define MM_REGRES(key, rtype)\
    fsl_reg_res_debug((key),(rtype),__FILE__, __LINE__)

#define MM_UNREGRES(key, rtype)\
    fsl_unreg_res_debug((key),(rtype),__FILE__, __LINE__)




#endif
#else
#define MM_MALLOC(size)\
    g_malloc((size))
#define MM_REALLOC(ptr, size)\
        g_realloc((ptr), (size))
#define MM_FREE(ptr)\
    g_free((ptr))

#define MM_INIT_DBG_MEM(mname)
#define MM_DEINIT_DBG_MEM()
#define MM_CHECK(detail)

#define MM_REGRES(key, rtype)

#define MM_UNREGRES(key, rtype)


#endif



/* ranking of fsl gstreamer plugins */
#define FSL_GST_RANK_HIGH (GST_RANK_PRIMARY+1)

#define FSL_GST_DEFAULT_DECODER_RANK (GST_RANK_SECONDARY+2)
#define FSL_GST_DEFAULT_DECODER_RANK_LEGACY (GST_RANK_SECONDARY+1)


#define FSL_GST_CONF_DEFAULT_FILENAME "/usr/share/gst-fsl-plugins.conf"


/* resource defines */


/* video caps defines */
#define VIDEO_RAW_CAPS_YUV \
    "video/x-raw-yuv"
#define VIDEO_RAW_CAPS_I420 \
    "video/x-raw-yuv, format=(fourcc)I420"
#define VIDEO_RAW_CAPS_NV12 \
    "video/x-raw-yuv, format=(fourcc)NV12"
#define VIDEO_RAW_CAPS_YV12 \
    "video/x-raw-yuv, format=(fourcc)YV12"
#define VIDEO_RAW_CAPS_Y42B \
    "video/x-raw-yuv, format=(fourcc)Y42B"
#define VIDEO_RAW_CAPS_Y444 \
"video/x-raw-yuv, format=(fourcc)Y444"
#define VIDEO_RAW_CAPS_Y800 \
    "video/x-raw-yuv, format=(fourcc)Y800"
#define VIDEO_RAW_CAPS_TILED \
    "video/x-raw-yuv, format=(fourcc)TNVP"
#define VIDEO_RAW_CAPS_TILED_FIELD \
    "video/x-raw-yuv, format=(fourcc)TNVF"


#define CAPS_FIELD_CROP_LEFT "crop-left"
#define CAPS_FIELD_CROP_RIGHT "crop-right"
#define CAPS_FIELD_CROP_TOP "crop-top"
#define CAPS_FIELD_CROP_BOTTOM "crop-bottom"

#define CAPS_FIELD_REQUIRED_BUFFER_NUMBER "num-buffers-required"

#define PARSER_VIDEOPAD_TEMPLATE_NAME "video_%02d"
#define PARSER_AUDIOPAD_TEMPLATE_NAME "audio_%02d"

#define FSL_GST_MM_PLUGIN_AUTHOR "Multimedia Team <shmmmw@freescale.com>"
#define FSL_GST_MM_PLUGIN_PACKAGE_NAME "Freescle Gstreamer Multimedia Plugins"
#define FSL_GST_MM_PLUGIN_PACKAGE_ORIG "http://www.freescale.com"
#define FSL_GST_MM_PLUGIN_LICENSE "LGPL"

#define FSL_GST_MM_PLUGIN_NAME_SUBFIX ".imx"

#define FSL_GST_PLUGIN_DEFINE(name, description, initfunc)\
  GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,\
      GST_VERSION_MINOR,\
      name FSL_GST_MM_PLUGIN_NAME_SUBFIX,\
      description,\
      initfunc,\
      VERSION,\
      FSL_GST_MM_PLUGIN_LICENSE,\
      FSL_GST_MM_PLUGIN_PACKAGE_NAME, FSL_GST_MM_PLUGIN_PACKAGE_ORIG)

#define FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, longname, klass, descrption) \
  do {\
    gst_element_class_set_details_simple ((element_class), (longname),\
      (klass), (descrption),\
      FSL_GST_MM_PLUGIN_AUTHOR);\
  }while(0)


#define FSL_MM_IOCTL(device, request, errorroute, ...)\
    do{\
        int ret;\
        if ((ret = ioctl((device), (request), ##__VA_ARGS__))<0){\
            g_print(RED_STR("%s:%d ioctl error, return %d\n", __FILE__, __LINE__, ret));\
            goto errorroute;\
        }\
    }while(0)

/* common resolution limitation by platform */
#if defined (_MX51)
// 1080p
#define MAX_RESOLUTION_WIDTH    1920
#define MAX_RESOLUTION_HEIGHT   1080
#define MIN_RESOLUTION_WIDTH    64
#define MIN_RESOLUTION_HEIGHT   64

#elif defined(_MX37)
// SVGA
#define MAX_RESOLUTION_WIDTH    800
#define MAX_RESOLUTION_HEIGHT   600
#define MIN_RESOLUTION_WIDTH    64
#define MIN_RESOLUTION_HEIGHT   64

#elif defined(_MX31)|| defined(_MX35)
// D1
#define MAX_RESOLUTION_WIDTH    720
#define MAX_RESOLUTION_HEIGHT   576
#define MIN_RESOLUTION_WIDTH    64
#define MIN_RESOLUTION_HEIGHT   64

#else
#define MAX_RESOLUTION_WIDTH    720
#define MAX_RESOLUTION_HEIGHT   576
#define MIN_RESOLUTION_WIDTH    64
#define MIN_RESOLUTION_HEIGHT   64
#endif

enum field_info
{
  FIELD_NONE,
  FIELD_INTERLACED_TB,
  FIELD_INTERLACED_BT,
  FIELD_TOP,
  FIELD_BOTTOM,
};

/*=============================================================================
                     DIRECT RENDER RELATED MACROS
=============================================================================*/

#if (DIRECT_RENDER_VERSION==2)
/*Direct render v2, support get/release decoder interface only*/
#define EXT_BUFFER_NUM 4

#ifndef BM_FLOW
#define BM_FLOW(...)
#endif

#ifndef BM_TRACE_BUFFER
#define BM_TRACE_BUFFER(...)
#endif

typedef enum
{
  BMDIRECT = 0,
  BMINDIRECT = 1
} BMMode;

#define BM_FLAG_NOT_RENDER (GST_BUFFER_FLAG_LAST<<1)
#define BM_FLAG_REFED (GST_BUFFER_FLAG_LAST<<2)
#define BM_FLAG_ALL (BM_FLAG_NOT_RENDER|BM_FLAG_REFED)


static BMMode bm_mode = BMDIRECT;
static GSList *bm_list = NULL;
static gint bm_buf_num = 0;
static gboolean bm_get_buf_init = FALSE;

#define BM_CLEAN_LIST do{\
        while(bm_list){\
            if (GST_BUFFER_FLAG_IS_SET(bm_list->data, BM_FLAG_NOT_RENDER)){\
                GST_BUFFER_FLAG_UNSET(bm_list->data, BM_FLAG_NOT_RENDER);\
                gst_buffer_unref(bm_list->data);\
            }\
            if (GST_BUFFER_FLAG_IS_SET(bm_list->data, BM_FLAG_REFED)){\
                GST_BUFFER_FLAG_UNSET(bm_list->data, BM_FLAG_REFED);\
                gst_buffer_unref(bm_list->data);\
            }\
            (bm_list) = g_slist_remove((bm_list), (bm_list)->data);\
		};\
	}while(0)

#define BM_INIT(rmdmode, decbufnum, rendbufnum) do{\
        BM_FLOW("BM_INIT\n", 0);\
        bm_buf_num = decbufnum;\
        BM_CLEAN_LIST;\
	}while(0)

#include "gstbufmeta.h"
#define IS_DMABLE_BUFFER(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))

#define BM_GET_BUFFER(tgtpad, size, pdata) do{\
        GstBuffer * buffer = NULL;\
        GstFlowReturn result;\
        GstCaps *src_caps = NULL;\
        src_caps = GST_PAD_CAPS((tgtpad));\
        if(G_UNLIKELY(bm_get_buf_init == FALSE)){ \
            gint limit = 30; \
            int i;\
            result = gst_pad_alloc_buffer_and_set_caps ((tgtpad), 0,(size),(src_caps),&buffer); \
            if (result != GST_FLOW_OK) { \
                GST_ERROR(">>DECODER: Error %d in allocating the Framebuffer[%d]\n",result, i); \
            } \
            while (((limit--) > 0) && ((result != GST_FLOW_OK)|| (!(IS_DMABLE_BUFFER (buffer))))){ \
                usleep (30000); \
                if (result == GST_FLOW_OK) \
                    gst_buffer_unref (buffer); \
                result = gst_pad_alloc_buffer_and_set_caps ((tgtpad), 0,(size),(src_caps),&buffer); \
                if (result != GST_FLOW_OK) { \
                    GST_ERROR(">>DECODER: Error %d in allocating the Framebuffer[%d]\n",result, i); \
                    continue; \
                } \
            } \
            if (buffer) \
                gst_buffer_unref (buffer); \
            else { \
                GST_ERROR ("Could not allocate Framebuffer"); \
                return -1; \
            } \
            bm_get_buf_init = TRUE; \
        }\
        result = gst_pad_alloc_buffer_and_set_caps((tgtpad), 0,(size), src_caps,&buffer);\
        if (result==GST_FLOW_OK){\
            GST_BUFFER_FLAG_SET(buffer, BM_FLAG_ALL);\
            (pdata) = GST_BUFFER_DATA(buffer);\
            gst_buffer_ref(buffer);\
            bm_list=g_slist_append(bm_list, buffer);\
            BM_FLOW("BM_GET_BUFFERv2 %p:d %p\n", buffer, pdata);\
            BM_TRACE_BUFFER("codec request %p:d %p\n", buffer, pdata);\
            break;\
        }\
        if (result!=GST_FLOW_OK){\
            (pdata)=NULL;\
            g_print("BM_GET_BUFFERv2 no buffer, %d in codec\n", g_slist_length(bm_list));\
        }\
    }while(0)

#define BM_QUERY_HWADDR(pdata, hwbuffer) do{\
		GSList * tmp = (bm_list);\
		GstBuffer * buffer;\
		while(tmp){\
			buffer = (GstBuffer *)(tmp->data);\
			if (GST_BUFFER_DATA(buffer)==(pdata)){\
                (hwbuffer) = GST_BUFFER_OFFSET(buffer);\
				BM_FLOW("BM_HWTRANSITION v%p=h%p\n", buffer, (hwbuffer));\
				break;\
			}\
			tmp = tmp->next;\
		}\
		if (tmp==NULL)\
			g_print("BM_HWTRANSITION illegal %p!\n", pdata);\
	}while (0)

#define BM_RELEASE_BUFFER(pdata) do{\
		GSList * tmp = (bm_list);\
		GstBuffer * buffer;\
		while(tmp){\
			buffer = (GstBuffer *)(tmp->data);\
			if (GST_BUFFER_DATA(buffer)==(pdata)){\
				BM_FLOW("BM_RELEASE_BUFFERv2 %p:d %p\n", buffer, pdata);\
					if (GST_BUFFER_FLAG_IS_SET(buffer, BM_FLAG_REFED)){\
                    BM_FLOW("BM_RELEASE_BUFFERv2 unref %p\n", buffer); \
                    GST_BUFFER_FLAG_UNSET(buffer, BM_FLAG_REFED);\
                    gst_buffer_unref(buffer);\
                    if (!GST_BUFFER_FLAG_IS_SET(buffer,BM_FLAG_ALL)){\
                        BM_FLOW("BM_RELEASE_BUFFERv2 remove %p from list\n", buffer); \
                        (bm_list) = g_slist_remove((bm_list), buffer);\
                    }\
                }else{\
                    BM_FLOW("BM_RELEASE_BUFFERv2 unref %p\n", buffer); \
                    GST_BUFFER_FLAG_UNSET(buffer, BM_FLAG_ALL);\
                    gst_buffer_unref(buffer);\
                    BM_FLOW("BM_RELEASE_BUFFERv2 remove %p from list\n", buffer); \
                    (bm_list) = g_slist_remove((bm_list), buffer);\
                    BM_TRACE_BUFFER("release released %p:d %p\n", buffer, pdata);\
                }\
				break;\
			}\
			tmp = tmp->next;\
		}\
		if (tmp==NULL)\
			g_print("BM_RELEASE_BUFFERv2 illegal %p!\n", pdata);\
	}while (0)

#define BM_REJECT_BUFFER(pdata) do{\
		GSList * tmp = (bm_list);\
		GstBuffer * buffer;\
		while(tmp){\
			buffer = (GstBuffer *)(tmp->data);\
			if (GST_BUFFER_DATA(buffer)==(pdata)){\
				BM_FLOW("BM_REJECT_BUFFERv2 %p:d %p\n", buffer, pdata);\
				if (GST_BUFFER_FLAG_IS_SET(buffer, BM_FLAG_NOT_RENDER)){\
                BM_FLOW("BM_REJECT_BUFFERv2 unref %p\n", buffer); \
                GST_BUFFER_FLAG_UNSET(buffer, BM_FLAG_NOT_RENDER);\
                    gst_buffer_unref(buffer);\
                    if (!GST_BUFFER_FLAG_IS_SET(buffer,BM_FLAG_ALL)){\
                        BM_FLOW("BM_REJECT_BUFFERv2 remove %p from list\n", buffer); \
                        (bm_list) = g_slist_remove((bm_list), buffer);\
                    }\
                }else{\
                    BM_TRACE_BUFFER("reject rendered %p:d %p\n", buffer, pdata);\
                }\
				break;\
			}\
			tmp = tmp->next;\
		}\
		if (tmp==NULL)\
			g_print("BM_REJECT_BUFFERv2 illegal %p!\n", pdata);\
	}while (0)

#define BM_RENDER_BUFFER(pdata, tgtpad, status, timestamp, duration) do{\
        GSList * tmp = (bm_list);\
		GstBuffer * buffer;\
		while(tmp){\
			buffer = (GstBuffer *)(tmp->data);\
			if (GST_BUFFER_DATA(buffer)==(pdata)){\
				BM_FLOW("BM_RENDER_BUFFERv2 %p:d %p t %"GST_TIME_FORMAT"\n", buffer, pdata, GST_TIME_ARGS((timestamp)));\
				if (GST_BUFFER_FLAG_IS_SET(buffer, BM_FLAG_NOT_RENDER)){\
                BM_FLOW("BM_RENDER_BUFFERv2 unref %p\n", buffer); \
                GST_BUFFER_FLAG_UNSET(buffer, BM_FLAG_NOT_RENDER);\
                    if (!GST_BUFFER_FLAG_IS_SET(buffer,BM_FLAG_ALL)){\
                        BM_FLOW("BM_RENDER_BUFFERv2 remove %p from list\n", buffer); \
                        (bm_list) = g_slist_remove((bm_list), buffer);\
                    }\
                    GST_BUFFER_TIMESTAMP(buffer) = (timestamp); \
                    GST_BUFFER_DURATION(buffer) = (duration); \
                    status = gst_pad_push((tgtpad), buffer);\
                }else{\
                    BM_TRACE_BUFFER("reder rendered %p:d %p\n", buffer, pdata);\
                }\
				break;\
			}\
			tmp = tmp->next;\
		}\
		if (tmp==NULL)\
			g_print("BM_RENDER_BUFFERv2 illegal %p!\n", pdata);\
	}while (0)

#define BM_GET_MODE bm_mode
#define BM_GET_BUFFERNUM (bm_buf_num+EXT_BUFFER_NUM)

#endif //(DIRECT_RENDER_VERSION==2)


/*=============================================================================
                  FRAME DROPING RELATED MACROS/FUNCTIONS
=============================================================================*/

#ifdef FRAMEDROPING_ENALBED

#define OVERHEAD_TIME 50
#define GST_BUFFER_FLAG_IS_SYNC (GST_BUFFER_FLAG_LAST<<2)
#define KEY_FRAME_SHIFT 3
#define KEY_FRAME_ARRAY (1<<KEY_FRAME_SHIFT)
#define KEY_FRAME_MASK (KEY_FRAME_ARRAY-1)

struct sfd_frames_info
{
  int total_frames;
  int dropped_frames;
  int dropped_iframes;
  int is_dropped_iframes;
  int estimate_decoding_time;
  int decoded_time;
  int curr_nonekey_frames, total_key_frames;
  int key_frames_interval[8];
  struct timeval tv_start, tv_stop;
};

#define INIT_SFD_INFO(x)                \
do {                                    \
    gint i;                             \
    (x)->total_frames = 0;              \
    (x)->dropped_frames = 0;            \
    (x)->dropped_iframes = 0;           \
    (x)->is_dropped_iframes = 0;        \
    (x)->estimate_decoding_time = 0;    \
    (x)->decoded_time = 0;              \
    (x)->curr_nonekey_frames = 0;    	\
    (x)->total_key_frames = 0;     		\
    for(i=0;i<KEY_FRAME_ARRAY;i++) {    \
        (x)->key_frames_interval[i]=0;  \
    }                                   \
} while(0);

#define CALC_SFD_DECODED_TIME(x)                                            \
do {                                                                        \
    int decoded_time;                                                       \
    int decoded_frames = (x)->total_frames-(x)->dropped_frames;             \
    decoded_time = ((x)->tv_stop.tv_sec - (x)->tv_start.tv_sec) * 1000000   \
        + (x)->tv_stop.tv_usec - (x)->tv_start.tv_usec;                     \
    (x)->decoded_time += decoded_time;                                      \
    if (decoded_frames == 0) {                                              \
        (x)->estimate_decoding_time = decoded_time;                         \
    } else {                                                                \
    if ( decoded_time > (x)->estimate_decoding_time)                        \
        (x)->estimate_decoding_time = (x)->decoded_time / decoded_frames ;  \
    }                                                                       \
        GST_DEBUG("SFD info:\ntotal frames : %d,\tdropped frames : %d.\n",  \
            (x)->total_frames,(x)->dropped_frames);                         \
        GST_DEBUG("Decoded time: %d,\tAverage decoding time : %d.\n",       \
            decoded_time, (x)->estimate_decoding_time);                     \
}while(0);

#define GST_ADD_SFD_FIELD(caps)                     \
do {                                                \
    GValue sfd_value = { G_TYPE_INT, 1};            \
    GstStructure *s,*structure;                     \
    structure = gst_caps_get_structure((caps),0);   \
    s = gst_structure_copy(structure);              \
    gst_structure_set_value(s,"sfd",&sfd_value);    \
    gst_caps_remove_structure((caps), 0);           \
    gst_caps_append_structure((caps), s);           \
}while(0);

#define MIN_DELAY_TIME 2000000
#define MAX_DELAY_TIME 3000000

#define GST_QOS_EVENT_HANDLE(pSfd_info,diff,framerate) do {						\
	if  ((pSfd_info)->is_dropped_iframes == 0) {                                \
    	int key_frames_interval,next_key_frame_time;							\
    	int micro_diff = (diff)/1000;                                           \
    	gint i;                                                                 \
    	if (micro_diff>MAX_DELAY_TIME) {                                        \
        	(pSfd_info)->is_dropped_iframes =1;									\
            GST_ERROR ("The time of decoding is far away the system,"           \
                "so should drop some frames\n");								\
            break;                                                              \
        }                                                                       \
    	if((pSfd_info)->total_key_frames >= KEY_FRAME_ARRAY) {					\
            for(i=0;i<KEY_FRAME_ARRAY;i++) {                                    \
                key_frames_interval += (pSfd_info)->key_frames_interval[i];     \
            }                                                                   \
            key_frames_interval >>= KEY_FRAME_SHIFT;                            \
    		next_key_frame_time = (1000000 / (framerate)) * 					\
    			(key_frames_interval - (pSfd_info)->curr_nonekey_frames);		\
    	}																		\
    	else																	\
    		next_key_frame_time = 0;											\
        if ( (micro_diff > MIN_DELAY_TIME) &&                                   \
		  (next_key_frame_time) && (next_key_frame_time < micro_diff) ) {		\
        	(pSfd_info)->is_dropped_iframes =1;									\
            GST_ERROR ("The time of decoding is after the system," 	            \
                "so should drop some frames\n");								\
        	GST_ERROR ("key frame interval: %d,"                                \
        	    "estimate next I frames: %d.\n",key_frames_interval, 	        \
        		key_frames_interval-(pSfd_info)->curr_nonekey_frames);			\
        	GST_ERROR ("diff time: %d, to next I frame time: %d\n",	            \
        		(micro_diff),next_key_frame_time);							    \
	    }																		\
    }                                                                           \
} while(0);
#define GET_TIME(x) do {        \
    gettimeofday((x), 0);       \
} while(0);

/*=============================================================================
FUNCTION:               Strategy_FD

DESCRIPTION:            Strategy of Frame dropping in.

ARGUMENTS PASSED:       None.


RETURN VALUE:           GstFlowReturn
                        GST_FLOW_ERROR: The GST buffer should be dropped.
                        GST_FLOW_OK: original case.

PRE-CONDITIONS:  	    None

POST-CONDITIONS:   	    None

IMPORTANT NOTES:   	    None
=============================================================================*/
static GstFlowReturn
Strategy_FD (int is_keyframes, struct sfd_frames_info *psfd_info)
{
  psfd_info->total_frames++;
  psfd_info->curr_nonekey_frames++;

  if (is_keyframes) {
    (psfd_info)->is_dropped_iframes = 0;
    (psfd_info)->
        key_frames_interval[(psfd_info)->total_key_frames & (KEY_FRAME_MASK)]
        = (psfd_info)->curr_nonekey_frames;
    (psfd_info)->total_key_frames++;
    (psfd_info)->curr_nonekey_frames = 0;
  }
  if ((psfd_info)->is_dropped_iframes) {
    if (!(is_keyframes)) {
      (psfd_info)->dropped_frames++;
      GST_WARNING ("SFD info:\ntotal frames : %d,\tdropped frames : %d.\n",
          (psfd_info)->total_frames, (psfd_info)->dropped_frames);
      return GST_FLOW_ERROR;
    }
  }
  return GST_FLOW_OK;
}

#endif



/*=============================================================================
FUNCTION:               printbuf

DESCRIPTION:            To print buffer data

ARGUMENTS PASSED: buffer , buffer size

=============================================================================*/
//*
static void
printbuf (char *buf, int size)
{
  int i;
  for (i = 0; i < size; i++) {
    if ((i & 0xf) == 0)
      g_print ("%06x: ", i);
    g_print ("%02x ", buf[i]);
    if ((i & 0xf) == 0xf)
      g_print ("\n");
  }
  g_print ("\n");
}


/*=============================================================================
FUNCTION:               get_chipname

DESCRIPTION:            To get chipname from /proc/cpuinfo

ARGUMENTS PASSED: STR of chipname

RETURN VALUE:            chip code
=============================================================================*/
//*

#define CHIPCODE(a,b,c,d)( (((unsigned int)((a)))<<24) | (((unsigned int)((b)))<<16)|(((unsigned int)((c)))<<8)|(((unsigned int)((d)))))
typedef enum
{
  CC_MX23 = CHIPCODE ('M', 'X', '2', '3'),
  CC_MX25 = CHIPCODE ('M', 'X', '2', '5'),
  CC_MX27 = CHIPCODE ('M', 'X', '2', '7'),
  CC_MX28 = CHIPCODE ('M', 'X', '2', '8'),
  CC_MX31 = CHIPCODE ('M', 'X', '3', '1'),
  CC_MX35 = CHIPCODE ('M', 'X', '3', '5'),
  CC_MX37 = CHIPCODE ('M', 'X', '3', '7'),
  CC_MX50 = CHIPCODE ('M', 'X', '5', '0'),
  CC_MX51 = CHIPCODE ('M', 'X', '5', '1'),
  CC_MX53 = CHIPCODE ('M', 'X', '5', '3'),
  CC_MX6Q = CHIPCODE ('M', 'X', '6', 'Q'),
  CC_MX60 = CHIPCODE ('M', 'X', '6', '0'),
  CC_UNKN = CHIPCODE ('U', 'N', 'K', 'N'),

} CHIP_CODE;

#define IS_PXP(ccode) \
    (((ccode)==CC_MX23) \
    ||((ccode)==CC_MX28)\
    ||((ccode)==CC_MX50)\
    ||((ccode)==CC_MX60))

#define IS_IPU(ccode) \
    (((ccode)==CC_MX25) \
    ||((ccode)==CC_MX27)\
    ||((ccode)==CC_MX31)\
    ||((ccode)==CC_MX35)\
    ||((ccode)==CC_MX37)\
    ||((ccode)==CC_MX51)\
    ||((ccode)==CC_MX53)\
    ||((ccode)==CC_MX6Q)\
    ||((ccode)==CC_UNKN))

static CHIP_CODE
getChipCode (void)
{
  FILE *fp = NULL;
  char buf[100], *p, *rev;
  char chip_name[3];
  int len = 0, i;
  int chip_num;
  CHIP_CODE cc = CC_UNKN;
  fp = fopen ("/proc/cpuinfo", "r");
  if (fp == NULL) {
    return cc;
  }
  while (!feof (fp)) {
    p = fgets (buf, 100, fp);
    p = strstr (buf, "Revision");
    if (p != NULL) {
      rev = index (p, ':');
      if (rev != NULL) {
        rev++;
        chip_num = strtoul (rev, NULL, 16);
        chip_num >>= 12;
        break;
      }
    }
  }

  fclose (fp);

  switch (chip_num) {
    case 0x23:
      cc = CC_MX23;
      break;
    case 0x25:
      cc = CC_MX25;
      break;
    case 0x27:
      cc = CC_MX27;
      break;
    case 0x28:
      cc = CC_MX28;
      break;
    case 0x31:
      cc = CC_MX31;
      break;
    case 0x35:
      cc = CC_MX35;
      break;
    case 0x37:
      cc = CC_MX37;
      break;
    case 0x50:
      cc = CC_MX50;
      break;
    case 0x51:
      cc = CC_MX51;
      break;
    case 0x53:
      cc = CC_MX53;
      break;
    case 0x63:
    case 0x61:
      cc = CC_MX6Q;
      break;
    case 0x60:
      cc = CC_MX60;
      break;
    default:
      cc = CC_UNKN;
      break;
  }

  return cc;
}

/*=============================================================================
                      DEMO PROTECTION RELATED MACROS
=============================================================================*/

/* The following is for DEMO protection */
#define DEMO_STR "DEMO"

#define INIT_DEMO_MODE(strVer,demomode)     \
do {                                        \
    if (strstr((strVer), DEMO_STR)>0)       \
        (demomode) = 1;                     \
    else                                    \
        (demomode) = 0;                     \
}while(0);

#define DEMO_LIVE_TIME 120

#define DEMO_LIVE_CHECK(demomode,timestamp,srcpad)              \
do {                                                            \
    if (                                                        \
        ( (demomode) == 1 ) &&                                  \
        ( ((timestamp) / GST_SECOND ) > DEMO_LIVE_TIME)         \
        )                                                       \
    {                                                           \
        GstEvent *event;                                        \
        GST_WARNING("This is a demo version,                    \
        and the time exceed 2 minutes.                          \
            Sending EOS event.\n");                             \
        event = gst_event_new_eos();                            \
        gst_pad_push_event ((srcpad), event);                   \
        (demomode) = 2;                                         \
    }                                                           \
}while(0);

#endif //__MFW_GST_UTILS_H__

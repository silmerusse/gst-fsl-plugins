/*
 *  Copyright (c) 2009-2012, Freescale Semiconductor, Inc.
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
 * Module Name:    mfw_gst_vpu_decoder.h  
 *
 * Description:    Include File for Hardware (VPU) Decoder Plugin 
 *                 for Gstreamer  
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#ifndef __MFW_GST_VPU_DECODER_H__
#define __MFW_GST_VPU_DECODER_H__


#define FRAMEDROPING_ENALBED
#define DIRECT_RENDER_VERSION 2
#include "mfw_gst_utils.h"
#include <sys/time.h>
#include <gst/video/video.h>

#include "mfw_gst_ts.h"
#include "gstbufmeta.h"

/*======================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=======================================================================================*/

G_BEGIN_DECLS GST_DEBUG_CATEGORY_EXTERN (mfw_gst_vpudec_debug);
#define GST_CAT_DEFAULT mfw_gst_vpudec_debug

enum
{
  MFW_GST_VPU_PROP_0,
  MFW_GST_VPU_CODEC_TYPE,
  MFW_GST_VPU_PROF_ENABLE,
  MFW_GST_VPU_DBK_ENABLE,
  MFW_GST_VPU_DBK_OFFSETA,
  MFW_GST_VPU_DBK_OFFSETB,
  MFW_GST_VPU_USE_INTERNAL_BUFFER,
  MFW_GST_VPU_LOOPBACK,
  MFW_GST_VPU_ROTATION,
  MFW_GST_VPU_MIRROR,
  MFW_GST_VPU_LATENCY,
  MFW_GST_VPU_PASSTHRU,
  MFW_GST_VPU_PARSER,           // use for playing from elementary file - no parser
  MFW_GST_VPU_FRAMEDROP,        // disable frame dropping
  MFW_GST_VPU_OUTPUT_FMT,
  MFW_GST_VPU_FRAMERATE_NU,
  MFW_GST_VPU_FRAMERATE_DE,
};

enum
{
  MP4_MPEG4 = 0,
  MP4_DIVX5_HIGHER = 1,
  MP4_XVID = 2,
  MP4_DIVX4 = 5,
};

enum
{
  SKIP_MAX_VAL = 5,
  SKIP_L3 = 4,
  SKIP_L2 = 3,
  SKIP_B = 2,
  SKIP_BP = 1,
  SKIP_NONE = 0,
};                              /* Based on VPU firmware skipframeMode */


//#define GST_LATENCY g_print
#ifndef GST_LATENCY
#define GST_LATENCY
#endif

//#define GST_MUTEX g_print
#ifndef GST_MUTEX
#define GST_MUTEX
#endif

//#define GST_FRAMEDBG g_print
#ifndef GST_FRAMEDBG
#define GST_FRAMEDBG
#endif

//#define GST_FRAMEDROP g_print
#ifndef GST_FRAMEDROP
#define GST_FRAMEDROP
#endif

//#define GST_TIMESTAMP g_print
#ifndef GST_TIMESTAMP
#define GST_TIMESTAMP
#endif

#define GST_MEM_COPY g_print


#define VPU_PARALLELIZATION 1
#define NAL_START_CODE_SIZE 4
#define NAL_START_DEFAULT_LENGTH 100
#define IS_DMABLE_BUFFER(buffer) ( (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1])) \
                                 || ( GST_IS_BUFFER(buffer) \
                                 &&  GST_BUFFER_FLAG_IS_SET((buffer),GST_BUFFER_FLAG_LAST)))
#define DMABLE_BUFFER_PHY_ADDR(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]) ? \
                                        ((GstBufferMeta *)(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))->physical_data : \
                                        GST_BUFFER_OFFSET(buffer))

#define HAS_DIVX_DECODER(chip_code) ((chip_code == CC_MX37) || \
                                     (chip_code == CC_MX51) || \
                                     (chip_code == CC_MX53))
#define HAS_RV_DECODER(chip_code) ((chip_code == CC_MX51) || \
                                   (chip_code == CC_MX53))
#define HAS_MJPEG_DECODER(chip_code) ((chip_code == CC_MX51) || \
                                      (chip_code == CC_MX53))
#define HAS_SORENSON_DECODER(chip_code) (chip_code == CC_MX53)

#define VPU_DELAY_TIME_L1 10000
#define VPU_DELAY_TIME_L2 200000
#define VPU_DELAY_TIME_L3 2000000
/* 
 * Frame dropping strategy will be separated to 3 types.
 * Type 1: dropping B frames & dropping one frame in each eight frames. 
 * Type 2: dropping one frame in each four frames.
 * Type 3: dropping BP frames & dropping one frmaes in each four frames.
 * dropping one frame in each four/eight frames only do not render the dropped frames. 
 * It could be useful for long interval I frames case.
 */
#define GST_VPU_QOS_EVENT_HANDLE(skipmode,diff) do {				            \
  	int micro_diff = (diff)/1000;                                               \
  	if ((micro_diff + VPU_DELAY_TIME_L1) > 0) {                                 \
    	if (micro_diff>VPU_DELAY_TIME_L3) {                                     \
        	skipmode = SKIP_BP;								                    \
            GST_WARNING ("The time of decoding is %d ms away the system time,"     \
                "drop B&P frames.", (micro_diff/1000));     				            \
            break;                                                              \
        }                                                                       \
    	else if (micro_diff>VPU_DELAY_TIME_L2) {                                \
        	skipmode = SKIP_L2;								                    \
            GST_DEBUG ("The time of decoding is %d ms away the system time,"       \
                "Enable L2 strategy", (micro_diff/1000));     				        \
            break;                                                              \
        } else {                                                                \
        	skipmode = SKIP_B;								                    \
            GST_INFO ("The time of decoding is %d ms away the system time,"        \
                "drop B frames", (micro_diff/1000));     				            \
            break;                                                              \
        }                                                                       \
    }                                                                           \
    else {                                                                      \
        GST_INFO("Disable the frame dropping");                               \
        skipmode = SKIP_NONE;                                                   \
    }                                                                           \
} while(0);


struct timeval tv_begin, tv_end;

#define TIME_BEGIN()    \
do {\
        memset(&tv_begin,0,sizeof(struct timeval)); memset(&tv_end,0,sizeof(struct timeval));   \
        gettimeofday (&tv_begin, 0);\
}while(0);
#if 0
#define TIME_END(info)    \
do {\
        gettimeofday (&tv_end, 0);\
        g_print("%s: time interval:" GST_TIME_FORMAT"\n", (char *)info, \
        GST_TIME_ARGS((guint64)(tv_end.tv_sec*1000000+tv_end.tv_usec) - \
        (guint64)(tv_begin.tv_sec*1000000+tv_begin.tv_usec)));\
}while(0);
#else
#define TIME_END(info)    \
do {\
        gettimeofday (&tv_end, 0);\
        g_print("%s: time interval:%lld\n", (char *)info, \
        ((guint64)(tv_end.tv_sec*1000000+tv_end.tv_usec) - \
        (guint64)(tv_begin.tv_sec*1000000+tv_begin.tv_usec)));\
}while(0);

#endif

/*=============================================================================
                                           CONSTANTS
=============================================================================*/
#define STREAM_END_SIZE 0
#define DEFAULT_FRAME_RATE_NUMERATOR (30)
#define DEFAULT_FRAME_RATE_DENOMINATOR (1)

#define DEFAULT_PAR_WIDTH   1
#define DEFAULT_PAR_HEIGHT  1

// used for latency
#define MIN_DATA_IN_VPU_720P            (50*1024)
#define MIN_DATA_IN_VPU_VGA             2048
#define MIN_DATA_IN_VPU_QVGA            1024
#define MIN_DATA_IN_VPU_QCIF            512

#define BUFF_FILL_SIZE_LARGE (1024 * 1024)
#define BUFF_FILL_SIZE_SMALL (512 * 1024)

#define PS_SAVE_SIZE		0x028000
#define SLICE_SAVE_SIZE		0x02D800
#define MIN_WIDTH       16      //64  /* ENGR00109002 bug fix */
#define MIN_HEIGHT      16      //64  /* ENGR00109002 bug fix */

#define PROCESSOR_CLOCK    333

#define DEFAULT_DBK_OFFSET_VALUE    5
#define QT_H264_SYS_HEAD_IDX   6
#define QT_H264_SPS_LEN    2

#define NUM_MAX_VPU_REQUIRED 20
#define NUM_FRAME_BUF	(NUM_MAX_VPU_REQUIRED + EXT_BUFFER_NUM + 2)
#define NUM_READ_BUF    32

//#define MAX_STREAM_BUF  512
//For Composing the RCV format for VC1

//VPU Supports only FOURCC_WMV3_WMV format (i.e. WMV9 only)
#define CODEC_VERSION	(0x5 << 24)     //FOURCC_WMV3_WMV
#define NUM_FRAMES		0xFFFFFF

#define SET_HDR_EXT			0x80000000
#define RCV_VERSION_2		0x40000000



/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_type_vpu_dec_get_type (void);
GType mfw_gst_vpudec_codec_get_type (void);


/* None. */

/*=============================================================================
                                             ENUMS
=============================================================================*/

/* None. */

/*=============================================================================
                                            MACROS
=============================================================================*/

G_BEGIN_DECLS
#define MFW_GST_TYPE_VPU_DEC (mfw_gst_type_vpu_dec_get_type())
#define MFW_GST_VPU_DEC(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_VPU_DEC,MfwGstVPU_Dec))
#define MFW_GST_VPU_DEC_CLASS(klass) \
    G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_VPU_DEC,MfwGstVPU_DecClass))
#define MFW_GST_IS_VPU_DEC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_VPU_DEC))
#define MFW_GST_IS_VPU_DEC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_VPU_DEC))
#define MFW_GST_TYPE_VPU_DEC_CODEC (mfw_gst_vpudec_codec_get_type())
#define MFW_GST_TYPE_VPU_DEC_MIRROR (mfw_gst_vpudec_mirror_get_type())
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef enum
{
  FB_STATE_ILLEGAL,
  FB_STATE_ALLOCATED,           /* buffer is in allocated */
  FB_STATE_DISPLAY,             /* buffer is in display */
  FB_STATE_DECODED,             /* buffer is in decoded */
  FB_STATE_PENDING,             /* buffer is pending release */
  FB_STATE_FREE,                /* buffer need free */
} FB_STATE;

typedef enum
{
  FB_TYPE_GST,                  /* Buffer allocated from Gstreamer */
  FB_TYPE_HW,                   /* Buffer allocated from hardware */
} FB_TYPE;


typedef struct _MfwGstVPU_Thread
{
  GThread *event_thread;
  GMutex *mutex;                // mutex for handling decode states
  GAsyncQueue *async_q;
  gboolean getoutput_pending;
  GstFlowReturn retval;
  GCond *cond_event[5];
} MfwGstVPU_Thread;


typedef struct _MfwGstVPU_Dec
{
  // Gstreamer plugin
  GstElement element;           // instance of base class 
  GstPad *sinkpad;              // sink pad of element receive input buffers
  GstPad *srcpad;               // source of element to push display buffers downstream
  GstBuffer *pushbuff;          // output buffer to push downstream 
  GstBuffer *gst_buffer;        // input buffer 
  // state changes 
  gboolean flushing;            // Flag to indicate the flush event so decoding can exit
  gboolean just_flushed;        // Flag to indicate rest
  gboolean eos;                 // Flag for end of stream
  gboolean firstFrameProcessed; // Flag for inserting headers for first frame
  gboolean is_frame_started;    // flag to signify VPU decode in progress
  gboolean lastframedropped;    // flag to show last frame was dropped
  gboolean passthru;            // flag to not decode - just pass thru
  gboolean check_for_bframe;    // used after seeking to ignore b frames before i or p
  gboolean decoding_completed;  // used to mark if frame can be rendered

  // VPU specific  
  DecHandle *handle;            // handle after opening VPU
  DecOpenParam *decOP;          // parameters for opening VPU
  DecInitialInfo *initialInfo;  // parameters returned by VPU after initialization
  DecParam *decParam;           // parameters passed to VPU for starting decode
  DecOutputInfo *outputInfo;    // parameters returned by VPU after decode complete
  vpu_mem_desc ps_mem_desc;     // ps save buffer for H.264
  vpu_mem_desc slice_mem_desc;  // slice save buffer for H.264
  vpu_mem_desc bit_stream_buf;
  CodStd codec;                 // codec standard to be selected 
  GMutex *vpu_mutex;            // mutex for handling decode states
  guint picWidth;               // Width of the Image obtained through caps negotiation
  guint picHeight;              // Height of the Image obtained through caps negotiation 
  gboolean file_play_mode;      // Flag for file play mode 
  gboolean vpu_init;            // VPU initialisation flag 
  gboolean vpu_opened;          // flag to signify VPU is opened 
  gboolean vpu_chipinit;        // flag to know if vpu_init was called

  // Input buffer 
  guint8 *start_addr;           // start addres of the physical input buffer 
  guint8 *end_addr;             // end addres of the physical input buffer 
  guint8 *base_addr;            // base addres of the physical input buffer 
  PhysicalAddress base_write;   // Base physical address of the input ring buffer
  PhysicalAddress end_write;    // End physical address of the input ring buffer 
  guint buff_consumed;          // size of current sink buffer consumed - 0 if consumed 
  guint data_in_vpu;            // size of data in vpu input buffer not processed


  // Frame buffer members for output
  guint yuv_frame_size;         // size of the output buffer in YUV format
  gboolean framebufinit_done;   // flag to signify frame buffer allocation complete
  guint numframebufs;           // Number of Frame buffers allocated 
  // (including those not registered with VPU)
  FrameBuffer frameBuf[NUM_FRAME_BUF];  // physical memory for frame buffers 
  guint8 *frame_virt[NUM_FRAME_BUF];    // virtual addresses mapped from frameBuf
  FB_STATE fb_state_plugin[NUM_FRAME_BUF];      // states for frame buffer
  FB_TYPE fb_type[NUM_FRAME_BUF];       // type of memory allocation through gst or hardware                                
  gint fb_pic_type[NUM_FRAME_BUF];      // picture type of the frame buffers
  vpu_mem_desc frame_mem[NUM_FRAME_BUF];        // structure for Frame buffer parameters 
  // if not used with V4LSink
  GstBuffer *outbuffers[NUM_FRAME_BUF]; // GST output buffers allocated through V4Lsink
  gboolean direct_render;       // allow VPU buffers to be used for display when using V4L
  gint buf_alignment_h;
  gint buf_alignment_v;

  // First Frame header 
  GstBuffer *codec_data;        // Header data needed for VC-1 and some codecs
  guint codec_data_len;         // Header Extension obtained through caps negotiation

  // Properties values set by input
  gboolean loopback;            // Flag to turn of parallelism 
  gboolean use_internal_buffer; // use internal buffer instead of allocate from downstream
  gboolean parser_input;        // parser input - assumed if height and width set in caps
  gboolean frame_drop_allowed;  // used to disable frame dropping
  guint rotation_angle;         // rotation angle used for VPU to rotate 
  guint rot_buff_idx;           // rotator buf index to toggle during display
  MirrorDirection mirror_dir;   // VPU mirror direction

  gboolean dbk_enabled;         // deblocking parameters 
  gint dbk_offset_a;
  gint dbk_offset_b;
  gboolean min_latency;         // minimize latency in starting
  gint min_data_in_vpu;         // minimum amount of data in VPU before decoding
  gboolean profiling;           // enable profiling 
  gint mp4Class;                // used for mp4class

  void *pTS_Mgr;                //Time Stamp manager
  GstClockTime time_per_frame;

  gfloat frame_rate;            // Frame rate of display 
  guint frame_rate_de;          // frame rate denominator   
  guint frame_rate_nu;          // frame rate denominator
  guint par_width;              // pixel width
  guint par_height;             // pixel height


  gboolean predict_gop;         // if gop sizes are standard use to predict
  gboolean set_ts_manually;     // creates automatic timestamps
  gboolean nal_check;           // no nal checking for raw file input
  gboolean accumulate_hdr;      // accumulate headers in streaming mode
  gboolean hdr_received;        // no decoding of just headers
  gboolean allow_parallelization;       // allow parallelization
  gboolean must_copy_data;      // flag for determining parallelization
  gboolean state_playing;       // playing state
  gboolean in_cleanup;          // state for marking cleanup mode

  // latency
  guint num_gops;               // number of I frames
  guint gop_size;               // size of GOP
  guint64 idx_last_gop;         // last gop to calculate new gop size avg
  gboolean gop_is_next;         // flag if gop is next frame based on gop size

  gboolean check_for_iframe;    // check for midstream if resolution is not provided
  GstClockTime clock_base;      // save clock from play start to drop ts before

  // Performance statistics 
  guint64 decode_wait_time;     // time for decode of one frame
  gfloat avg_fps_decoding;      // average fps of decoding 
  guint64 chain_Time;           // time spent in the chain function 
  guint non_iframes_dropped;    // number of non-I frames dropped
  guint frames_dropped;         // number of frames dropped due to error 
  guint frames_rendered;        // number of the rendered frames 
  guint frames_decoded;         // number of the decoded frames 
  guint num_timeouts;           // number of timeouts to determine if reset is needed
  struct timeval tv_prof, tv_prof1;
  struct timeval tv_prof2, tv_prof3;

  gint field;                   // Frame or Field information. 
  guint codec_subtype;          // to specify different profiles for codec, added in engr115104
  guint NALLengthFieldSize;
  gint fmt;
  GstVideoFormat video_format;

  gboolean trymutex;
  gboolean sfd;
  gint skipmode;
  // needed for VPU thread case
  guint loop_cnt;
  MfwGstVPU_Thread *vpu_thread;
  GstFlowReturn retval;
  gint width, height;
  struct sfd_frames_info sfd_info;
  guint init_fail_cnt;

  CHIP_CODE chip_code;
  gint buffer_fill_size;

  gint new_segment;
  TSMGR_MODE tsm_mode;

  gboolean new_ts;

} MfwGstVPU_Dec;

typedef struct _MfwGstVPU_DecClass
{
  GstElementClass parent_class;

} MfwGstVPU_DecClass;



/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/



/*======================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=======================================================================================*/
void mfw_gst_vpudec_post_fatal_error_msg (MfwGstVPU_Dec * vpu_dec,
    gchar * error_msg);




G_END_DECLS
#endif /* __MFW_GST_VPU_DECODER_H__ */

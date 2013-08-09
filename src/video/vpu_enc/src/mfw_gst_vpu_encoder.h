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
 * Module Name:    mfw_gst_vpu_encoder.h  
 *
 * Description:    Include File for Hardware (VPU) Encoder Plugin 
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
#ifndef __MFW_GST_VPU_ENCODER_H__
#define __MFW_GST_VPU_ENCODER_H__

#include <gst/video/video.h>

/*======================================================================================
                                     LOCAL CONSTANTS
=======================================================================================*/

/*maximum limit of the output buffer */
#define BUFF_FILL_SIZE (1024 * 1024)

/* Maximum width and height */
#define MAX_MJPEG_WIDTH		8192
#define MAX_MJPEG_HEIGHT	8192

/* Minimum width and height */
#define MIN_WIDTH		48
#define MIN_HEIGHT		32

/* invalie width and height */
#define INVALID_WIDTH		0
#define INVALID_HEIGHT		0

/* Rate control releated */
/* Maximum bit rate */
#define MAX_BITRATE		32767   /* 0x7FFF */
/* Invalid bit rate */
#define INVALID_BITRATE	0       /* no bitrate control, VBR */
/* Maximum Gamma */
#define MAX_GAMMA	32768
/* Default Gamma */
#define DEFAULT_GAMMA	(0.75 * 32768)
/* Default RC interval mode */
#define DEFAULT_RC_INTERVAL_MODE	0       /* Normal rate control */
/* Default max and min QP */
#define MPEG4_QP_MAX	31
#define MPEG4_QP_MIN	1
#define H264_QP_MAX	51
#define H264_QP_MIN	0

/* Default height, width - Set to QCIF */
#define DEFAULT_HEIGHT	176
#define DEFAULT_WIDTH	144

#define DEFAULT_FRAME_RATE	30
#define INVALID_FRAME_RATE	0
#define DEFAULT_I_FRAME_INTERVAL 0
#define DEFAULT_GOP_SIZE    30
#define MAX_GOP_SIZE        32767

#define VPU_HUFTABLE_SIZE 432
#define VPU_QMATTABLE_SIZE 192

#define VPU_INVALID_QP -1
#define VPU_DEFAULT_H264_QP 25
#define VPU_DEFAULT_MPEG4_QP 15
#define VPU_MAX_H264_QP 51
#define VPU_MAX_MPEG4_QP 31

#define MAX_TIMESTAMP         32
#define TIMESTAMP_INDEX_MASK (MAX_TIMESTAMP-1)

#define HAS_MJPEG_ENCODER(chip_code) ((chip_code == CC_MX51) || \
                                      (chip_code == CC_MX53))


#define TIMESTAMP_IN(context, timestamp)\
    do{\
        (context)->timestamp_buffer[(context)->ts_rx] = (timestamp);\
        (context)->ts_rx = (((context)->ts_rx+1) & TIMESTAMP_INDEX_MASK);\
    }while(0)

#define TIMESTAMP_OUT(context, timestamp)\
    do{\
        (timestamp) = (context)->timestamp_buffer[(context)->ts_tx];\
        (context)->ts_tx = (((context)->ts_tx+1) & TIMESTAMP_INDEX_MASK);\
    }while(0)

#define DESIRED_FRAME_TIMESTAMP(context, noffset)\
    ((context)->segment_starttime\
                +gst_util_uint64_scale_int(GST_SECOND, ((context)->segment_encoded_frame+(noffset))*(context)->framerate_d,\
                (context)->framerate_n))

/*======================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=======================================================================================*/

/* properties set on the encoder */
enum
{
  MFW_GST_VPU_PROP_0,
  MFW_GST_VPU_CODEC_TYPE,
  MFW_GST_VPU_PROF_ENABLE,
  MFW_GST_VPUENC_WIDTH,
  MFW_GST_VPUENC_HEIGHT,
  MFW_GST_VPUENC_FRAME_RATE,
  MFW_GST_VPUENC_BITRATE,
  MFW_GST_VPUENC_FORCEIINTERVAL,
  MFW_GST_VPUENC_GOP,
  MFW_GST_VPUENC_QP,
  MFW_GST_VPUENC_MAX_QP,
  MFW_GST_VPUENC_MIN_QP,
  MFW_GST_VPUENC_GAMMA,
  MFW_GST_VPUENC_INTRAREFRESH,
  MFW_GST_VPUENC_H263PROFILE0,
  MFW_GST_VPUENC_LOOPBACK,
  MFW_GST_VPUENC_INTRA_QP,
  MFW_GST_VPUENC_CROP_LEFT,
  MFW_GST_VPUENC_CROP_TOP,
  MFW_GST_VPUENC_CROP_RIGHT,
  MFW_GST_VPUENC_CROP_BOTTOM,
  MFW_GST_VPUENC_ROTATION_ANGLE,
  MFW_GST_VPUENC_MIRROR_DIRECTION,
  MFW_GST_VPUENC_AVC_BYTE_STREAM,

};

enum
{
  MFW_GST_VPUENC_YUV_420,
  MFW_GST_VPUENC_YUV_422H,
  MFW_GST_VPUENC_YUV_422V,
  MFW_GST_VPUENC_YUV_444,
  MFW_GST_VPUENC_YUV_400
};

enum
{
  MFW_GST_VPUENC_PLANAR_Y,
  MFW_GST_VPUENC_PLANAR_Y_UV,
  MFW_GST_VPUENC_PLANAR_Y_U_V,
  MFW_GST_VPUENC_PLANAR_Y_V_U,
};

/*======================================================================================
                                        LOCAL MACROS
=======================================================================================*/
#define	GST_CAT_DEFAULT	mfw_gst_vpuenc_debug

/*======================================================================================
                                      STATIC VARIABLES
=======================================================================================*/

/*======================================================================================
                                     GLOBAL VARIABLES
=======================================================================================*/

/*=============================================================================
                                           CONSTANTS
=============================================================================*/
#define NUM_INPUT_BUF   3

/*=============================================================================
                                             ENUMS
=============================================================================*/

/* None. */

/*=============================================================================
                                            MACROS
=============================================================================*/

G_BEGIN_DECLS
#define MFW_GST_TYPE_VPU_ENC (mfw_gst_type_vpu_enc_get_type())
#define MFW_GST_VPU_ENC(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_VPU_ENC,MfwGstVPU_Enc))
#define MFW_GST_VPU_ENC_CLASS(klass) \
    G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_VPU_ENC,MfwGstVPU_EncClass))
#define MFW_GST_IS_VPU_ENC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_VPU_ENC))
#define MFW_GST_IS_VPU_ENC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_VPU_ENC))
// headers for H264
#define SPS_HDR 0
#define PPS_HDR 1
// headers for MPEG4
#define VOS_HDR 0
#define VIS_HDR 1
#define VOL_HDR 2
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef struct _MfwGstVPU_Enc
{
  // Gstreamer plugin members
  GstElement element;           // instance of base class
  GstPad *sinkpad;              // sink pad of element
  GstPad *srcpad;               // source pad of element
  //GstElementClass *parent_class;        // parent class

  // VPU specific members defined in vpu_lib.h
  EncHandle handle;
  EncOpenParam *encOP;
  EncInitialInfo *initialInfo;
  EncOutputInfo *outputInfo;
  EncParam *encParam;


  // Input buffer members
  vpu_mem_desc bit_stream_buf;  // input bit stream allocated memory
  guint8 *start_addr;           // start addres of the Hardware output buffer
  guint8 *curr_addr;            // current addres of the Hardware input buffer
  guint8 *end_addr;             // end address of hardware input buffer
  guint gst_copied;             // amt copied before previous encode
  GstBuffer *gst_buffer;        // buffer for wrap around to copy remainder after encode frame complete


  // State members
  gboolean vpu_init;            // Signifies if VPU is initialized yet
  gboolean is_frame_started;    // Is VPU in encoding state

  // Header members
  gint num_total_frames;        // frame counter of all frames including skipped
  gint num_encoded_frames;      // num_encoded_frames
  gint frames_since_hdr_sent;   // num frames between headers sent
  GstBuffer *hdr_data;
  GstBuffer *codec_data;

  // Frame buffer members
  vpu_mem_desc *vpuRegisteredFramesDesc;
  FrameBuffer *vpuRegisteredFrames;
  vpu_mem_desc vpuInFrameDesc;
  FrameBuffer vpuInFrame;
  gint picSizeY;
  gint picSizeC;
  gint numframebufs;
  guint encWidth;               // width of output
  guint encHeight;              // height of output
  guint yuv_frame_size;         // size of input
  guint bytes_consumed;         // consumed from buffer

  // Properties set by input
  guint width;                  // width of input
  guint height;                 // height of input
  CodStd codec;                 // codec standard to be selected
  gboolean heightProvided;      // Set when the user provides the height on the command line 
  gboolean widthProvided;       // Set when the user provides the width on the command line 
  gboolean codecTypeProvided;   // Set when the user provides the compression format on the command line     
  guint intraRefresh;           // least N MB's in every P-frame will be encoded as intra MB's.
  gboolean h263profile0;        // to encode H.263 Profile 0 instead of default H.263 Profile 3
  guint intra_qp;               // qp to set for all I frames

  // members for bitrate
  guint bitrate;                // bitrate
  gboolean setbitrate;          // True to set new bitrate
  guint max_qp;                 // maximum qp used for CBR only                
  guint min_qp;                 // minimum qp used for CBR only                
  guint max_qp_en;              // enable max_qp                
  guint min_qp_en;              // enable min_qp                
  guint gamma;                  // gamma used for CBR only
  guint bits_per_second;        // calculate using number of frames per second
  guint bits_per_stream;        // keep total per stream to use avg
  guint fs_read;                // idx into array below for reading
  guint fs_write;               // idx into array below for writing
  guint frame_size[MAX_TIMESTAMP];      //subtract oldest frame size to keep running bit

  gboolean profile;             // profiling info 
  guint qp;                     // quantization parameter if bitrate not set
  guint gopsize;                // gop size - interval between I frames
  guint forceIFrameInterval;    // Force i frame interval

  // Frame rate members
  gfloat src_framerate;         // source frame rate set on EncOpen
  gfloat tgt_framerate;         // target frame rate set on first frame with Enc_set
  gint framerate_n;             // frame rate numerator passed from caps
  gint framerate_d;             // frame rate denominator passed from caps
  guint num_in_interval;        //number of frames in interval
  guint num_enc_in_interval;    // number of frames to encode before skipping
  guint idx_interval;           // counter of frames in skipping interval
  gboolean fDropping_till_IFrame;       // flag to drop until I frame    

  // timestamps
  GstClockTime timestamp_buffer[MAX_TIMESTAMP];
  guint ts_rx;                  // received timestamps
  guint ts_tx;                  // output timestamps

  // members for fixed bit rate
  GstSegment segment;
  guint64 segment_encoded_frame;        // Used for generating fixed bit rate
  GstClockTime segment_starttime;       // fixed bit rate timestamp generation
  gboolean forcefixrate;
  GstClockTime time_per_frame;  // save the time for one frame to use later
  GstClockTime total_time;      // time based on number of frames
  GstClockTime time_before_enc; // time saved before encode started

  GstVideoFormat format;        // fourcc format of input
  guint32 yuv_proportion;       // yuv format of input
  guint32 yuv_planar;           // yuv planar format of input
  GstClockTime enc_start;
  GstClockTime enc_stop;
  gboolean loopback;
  gboolean avc_byte_stream;

  guint crop_left;
  guint crop_top;
  guint crop_right;
  guint crop_bottom;

  guint rotation;
  guint mirror;

  gboolean forcekeyframe;

  CHIP_CODE chip_code;
  guint max_width;
  guint max_height;

  void *pTS_Mgr;                //Time Stamp manager

} MfwGstVPU_Enc;

typedef struct _MfwGstVPU_EncClass
{
  GstElementClass parent_class;

} MfwGstVPU_EncClass;

G_END_DECLS
#endif /* __MFW_GST_VPU_ENCODER_H__ */

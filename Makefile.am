ACLOCAL_AMFLAGS = -I m4

LIBSDIRS = libs

IPUDIRS =
if USE_I_CSC
IPUDIRS += src/video/ipu_csc/src
endif

if USE_I_SINK
IPUDIRS += src/misc/i_sink/src
endif

if USE_AIUR
AIURDIRS = src/parser/aiur/src
endif

if USE_BEEP
BEEPDIRS = src/audio/beep/src
endif

if USE_V4L_SINK
V4LSINKDIRS = src/misc/v4l_sink/src
endif

if PLATFORM_IS_MX28
BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif


TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(AACDEC_DIR)  $(MP3DEC_DIR)  $(MP3ENC_DIR) \
  $(H264DEC_DIR) $(MPEG4DEC_DIR)  \
 $(WMA10DEC_DIR) $(TOOLDIRS)

endif

if PLATFORM_IS_MX233
BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/video/mpeg4asp_dec/src

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif


if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif


TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(AACDEC_DIR)  $(MP3DEC_DIR)  $(MP3ENC_DIR) \
  $(H264DEC_DIR) $(MPEG4DEC_DIR)  \
 $(WMA10DEC_DIR) $(TOOLDIRS)
endif

if PLATFORM_IS_MX25
BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif


TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(AACDEC_DIR) $(MP3DEC_DIR) $(MP3ENC_DIR) \
 $(WMA10DEC_DIR) $(TOOLDIRS)

endif

if PLATFORM_IS_MX27
BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/video/vpu_dec.full/src       \
           src/misc/v4l_source/src

TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS)  $(WMA8ENC_DIR) $(AC3DEC_DIR) $(TOOLDIRS)
endif

if PLATFORM_IS_MX31
BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/audio/audio_pp/src      \
           src/misc/v4l_source/src


if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_AACP_DEC
AACPDEC_DIR =    \
		src/audio/aacplus_dec/src
else
AACPDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_DOWNMIX
DOWNMIX_DIR =    \
		  src/audio/down_mix/src
else
DOWNMIX_DIR =
endif

if USE_VORBIS_DEC
VORBISDEC_DIR =    \
		  src/audio/vorbis_dec/src
else
VORBISDEC_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif

if USE_MPEG2_DEC
MPEG2DEC_DIR =    \
		  src/video/mpeg2_dec/src
else
MPEG2DEC_DIR =
endif

if USE_WMV9MP_DEC
WMV9MPDEC_DIR =    \
		  src/video/wmv9mp_dec/src
else
WMV9MPDEC_DIR =
endif

if USE_WMV78_DEC
WMV78DEC_DIR =    \
		  src/video/wmv_dec/src
else
WMV78DEC_DIR =
endif



TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(AACDEC_DIR) $(AACPDEC_DIR) $(MP3DEC_DIR) $(WMA8ENC_DIR) $(MP3ENC_DIR) \
  $(H264DEC_DIR) $(MPEG4DEC_DIR) $(MPEG2DEC_DIR) $(WMV9MPDEC_DIR) $(WMV78DEC_DIR) \
 $(WMA10DEC_DIR) $(VORBISDEC_DIR) $(DOWNMIX_DIR) $(AC3DEC_DIR) $(TOOLDIRS)

endif



if PLATFORM_IS_MX35
BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/audio/audio_pp/src      \
           src/audio/spdif_rx/src      \
           src/audio/spdif_tx/src      \
           src/misc/v4l_source/src

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_AACP_DEC
AACPDEC_DIR =    \
		src/audio/aacplus_dec/src
else
AACPDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_DOWNMIX
DOWNMIX_DIR =    \
		  src/audio/down_mix/src
else
DOWNMIX_DIR =
endif

if USE_VORBIS_DEC
VORBISDEC_DIR =    \
		  src/audio/vorbis_dec/src
else
VORBISDEC_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif

if USE_MPEG2_DEC
MPEG2DEC_DIR =    \
		  src/video/mpeg2_dec/src
else
MPEG2DEC_DIR =
endif

if USE_WMV9MP_DEC
WMV9MPDEC_DIR =    \
		  src/video/wmv9mp_dec/src
else
WMV9MPDEC_DIR =
endif

if USE_WMV78_DEC
WMV78DEC_DIR =    \
		  src/video/wmv_dec/src
else
WMV78DEC_DIR =
endif



TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(AACDEC_DIR) $(AACPDEC_DIR) $(MP3DEC_DIR) $(WMA8ENC_DIR) $(MP3ENC_DIR) \
  $(H264DEC_DIR) $(MPEG4DEC_DIR) $(MPEG2DEC_DIR) $(WMV9MPDEC_DIR) $(WMV78DEC_DIR) \
 $(WMA10DEC_DIR) $(VORBISDEC_DIR) $(DOWNMIX_DIR) $(AC3DEC_DIR) $(TOOLDIRS)



endif

if PLATFORM_IS_MX37
BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/audio/audio_pp/src   \
           src/video/vpu_dec.full/src       \
           src/video/deinterlace/src        \
           src/misc/v4l_source/src

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_AACP_DEC
AACPDEC_DIR =    \
		src/audio/aacplus_dec/src
else
AACPDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_DOWNMIX
DOWNMIX_DIR =    \
		  src/audio/down_mix/src
else
DOWNMIX_DIR =
endif

if USE_VORBIS_DEC
VORBISDEC_DIR =    \
		  src/audio/vorbis_dec/src
else
VORBISDEC_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif


if USE_AC3_DEC
AC3DEC_DIR =    \
		  src/audio/ac3_dec/src
else
AC3DEC_DIR =
endif

TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(AACDEC_DIR) $(AACPDEC_DIR) $(MP3DEC_DIR) $(WMA8ENC_DIR) $(MP3ENC_DIR) \
 $(WMA10DEC_DIR) $(VORBISDEC_DIR) $(DOWNMIX_DIR) $(AC3DEC_DIR) $(TOOLDIRS)

endif

if PLATFORM_IS_MX50

BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/audio/audio_pp/src      \
           src/video/vpu_dec.full/src       \
           src/video/deinterlace/src   \
           src/misc/v4l_source/src

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_AACP_DEC
AACPDEC_DIR =    \
		src/audio/aacplus_dec/src
else
AACPDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_DOWNMIX
DOWNMIX_DIR =    \
		  src/audio/down_mix/src
else
DOWNMIX_DIR =
endif

if USE_VORBIS_DEC
VORBISDEC_DIR =    \
		  src/audio/vorbis_dec/src
else
VORBISDEC_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif


if USE_AC3_DEC
AC3DEC_DIR =    \
		  src/audio/ac3_dec/src
else
AC3DEC_DIR =
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif

if USE_MPEG2_DEC
MPEG2DEC_DIR =    \
		  src/video/mpeg2_dec/src
else
MPEG2DEC_DIR =
endif

if USE_WMV9MP_DEC
WMV9MPDEC_DIR =    \
		  src/video/wmv9mp_dec/src
else
WMV9MPDEC_DIR =
endif

if USE_WMV78_DEC
WMV78DEC_DIR =    \
		  src/video/wmv_dec/src
else
WMV78DEC_DIR =
endif

TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(AACDEC_DIR) $(AACPDEC_DIR) $(MP3DEC_DIR) $(WMA8ENC_DIR) $(MP3ENC_DIR) \
 $(WMA10DEC_DIR) $(VORBISDEC_DIR) $(DOWNMIX_DIR) $(AC3DEC_DIR) $(H264DEC_DIR) $(MPEG4DEC_DIR) $(MPEG2DEC_DIR) \
 $(WMV9MPDEC_DIR) $(WMV78DEC_DIR) $(TOOLDIRS)

endif

if PLATFORM_IS_MX51

BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/audio/amr_dec/src      \
           src/audio/amr_enc/src      \
           src/video/vpu_dec.full/src       \
           src/video/vpu_enc/src       \
           src/video/deinterlace/src   \
           src/misc/v4l_source/src


if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_DOWNMIX
DOWNMIX_DIR =    \
		  src/audio/down_mix/src
else
DOWNMIX_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif

if USE_MPEG2_DEC
MPEG2DEC_DIR =    \
		  src/video/mpeg2_dec/src
else
MPEG2DEC_DIR =
endif

if USE_WMV9MP_DEC
WMV9MPDEC_DIR =    \
		  src/video/wmv9mp_dec/src
else
WMV9MPDEC_DIR =
endif

if USE_WMV78_DEC
WMV78DEC_DIR =    \
		  src/video/wmv_dec/src
else
WMV78DEC_DIR =
endif

TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(IPUDIRS) $(BASEDIRS) $(AACDEC_DIR) $(AACPDEC_DIR) $(MP3DEC_DIR) $(WMA8ENC_DIR) $(MP3ENC_DIR) \
 $(WMA10DEC_DIR) $(VORBISDEC_DIR) $(DOWNMIX_DIR) $(AC3DEC_DIR) $(AMRDEC_DIR)  $(H264DEC_DIR) $(MPEG4DEC_DIR) $(MPEG2DEC_DIR) \
 $(WMV9MPDEC_DIR) $(WMV78DEC_DIR) $(TOOLDIRS)

endif

if PLATFORM_IS_MX53

BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/audio/audio_pp/src      \
           src/audio/amr_dec/src       \
           src/video/vpu_dec.full/src       \
           src/video/deinterlace/src   \
           src/misc/v4l_source/src

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_AACP_DEC
AACPDEC_DIR =    \
		src/audio/aacplus_dec/src
else
AACPDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_DOWNMIX
DOWNMIX_DIR =    \
		  src/audio/down_mix/src
else
DOWNMIX_DIR =
endif

if USE_VORBIS_DEC
VORBISDEC_DIR =    \
		  src/audio/vorbis_dec/src
else
VORBISDEC_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif

if USE_AC3_DEC
AC3DEC_DIR =    \
		  src/audio/ac3_dec/src
else
AC3DEC_DIR =
endif

if USE_AMR_DEC
AMRDEC_DIR =    \
		  src/audio/amr_dec/src
else
AMRDEC_DIR =
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif

if USE_MPEG2_DEC
MPEG2DEC_DIR =    \
		  src/video/mpeg2_dec/src
else
MPEG2DEC_DIR =
endif

if USE_WMV9MP_DEC
WMV9MPDEC_DIR =    \
		  src/video/wmv9mp_dec/src
else
WMV9MPDEC_DIR =
endif

if USE_WMV78_DEC
WMV78DEC_DIR =    \
		  src/video/wmv_dec/src
else
WMV78DEC_DIR =
endif

TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(IPUDIRS) $(BASEDIRS) $(AACDEC_DIR) $(AACPDEC_DIR) $(MP3DEC_DIR) $(WMA8ENC_DIR) $(MP3ENC_DIR) \
 $(WMA10DEC_DIR) $(VORBISDEC_DIR) $(DOWNMIX_DIR) $(AC3DEC_DIR) $(AMRDEC_DIR)  $(H264DEC_DIR) $(MPEG4DEC_DIR) $(MPEG2DEC_DIR) \
 $(WMV9MPDEC_DIR) $(WMV78DEC_DIR) $(TOOLDIRS)

endif

if PLATFORM_IS_MX6X

BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(V4LSINKDIRS)   \
           src/audio/audio_pp/src      \
           src/misc/v4l_source/src

if USE_VPU_WRAP
VPUWRAPDIRS = src/video/vpu/src
endif

if USE_H264_DEC
H264DEC_DIR =    \
		  src/video/h264_dec/src
else
H264DEC_DIR =
endif

if USE_MPEG4_DEC
MPEG4DEC_DIR =    \
		  src/video/mpeg4asp_dec/src
else
MPEG4DEC_DIR =
endif

if USE_MPEG2_DEC
MPEG2DEC_DIR =    \
		  src/video/mpeg2_dec/src
else
MPEG2DEC_DIR =
endif

if USE_WMV9MP_DEC
WMV9MPDEC_DIR =    \
		  src/video/wmv9mp_dec/src
else
WMV9MPDEC_DIR =
endif

if USE_WMV78_DEC
WMV78DEC_DIR =    \
		  src/video/wmv_dec/src
else
WMV78DEC_DIR =
endif

if USE_AAC_DEC
AACDEC_DIR =    \
		  src/audio/aac_dec/src
else
AACDEC_DIR =
endif

if USE_AACP_DEC
AACPDEC_DIR =    \
		src/audio/aacplus_dec/src
else
AACPDEC_DIR =
endif

if USE_MP3_DEC
MP3DEC_DIR =    \
		  src/audio/mp3_dec/src
else
MP3DEC_DIR =
endif

if USE_WMA10_DEC
WMA10DEC_DIR =    \
		  src/audio/wma10_dec/src
else
WMA10DEC_DIR =
endif

if USE_MP3_ENC
MP3ENC_DIR =    \
		  src/audio/mp3_enc/src
else
MP3ENC_DIR =
endif

if USE_DOWNMIX
DOWNMIX_DIR =    \
		  src/audio/down_mix/src
else
DOWNMIX_DIR =
endif

if USE_VORBIS_DEC
VORBISDEC_DIR =    \
		  src/audio/vorbis_dec/src
else
VORBISDEC_DIR =
endif

if USE_WMA8_ENC
WMA8ENC_DIR =    \
		  src/audio/wma8_enc/src
else
WMA8ENC_DIR =
endif

if USE_AC3_DEC
AC3DEC_DIR =    \
		  src/audio/ac3_dec/src
else
AC3DEC_DIR =
endif

if USE_AMR_DEC
AMRDEC_DIR =    \
		  src/audio/amr_dec/src
else
AMRDEC_DIR =
endif


TOOLDIRS =      tools/                      \
		tools/gplay

SUBDIRS =  $(AUDIODIRS)  $(LIBSDIRS) $(BASEDIRS) $(VPUWRAPDIRS) $(AACDEC_DIR) $(AACPDEC_DIR) $(MP3DEC_DIR) $(WMA8ENC_DIR) $(MP3ENC_DIR) \
 $(H264DEC_DIR) $(MPEG2DEC_DIR) $(MPEG4DEC_DIR) $(WMV9MPDEC_DIR) $(WMV78DEC_DIR) \
 $(WMA10DEC_DIR) $(VORBISDEC_DIR) $(DOWNMIX_DIR) $(AC3DEC_DIR) $(AMRDEC_DIR) $(IPUDIRS) $(TOOLDIRS)

endif

noinst_HEADERS = \
    inc/common/fsl_debug.h      \
    inc/plugin/mfw_gst_utils.h  \
    inc/plugin/mfw_gst_debug.h

fslmmdocdir = $(docdir)/imx-mm/gstreamer-0.10
fslmmdoc_DATA = \
    docs/misc/platforms/EULA.txt    \
    docs/misc/platforms/Linux_Multimedia_Framework_User_Guide.pdf

pcverfiles = gstreamer-fsl-@GST_MAJORMINOR@.pc
### how to generate versioned .pc files from .pc files in this dir
%-@GST_MAJORMINOR@.pc: %.pc
	cp $< $@
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcverfiles)

CLEANFILES = $(pcverfiles)
pcinfiles = gstreamer-fsl.pc.in

DISTCLEANFILES = $(pcinfiles:.in=)

EXTRA_DIST = autogen.sh $(fslmmdoc_DATA) $(pcinfiles) COPYING-LGPL-2 COPYING-LGPL-2.1

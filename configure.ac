AC_PREREQ(2.59)

dnl initialize autoconf
dnl releases only do -Wall, git and prerelease does -Werror too
dnl use a three digit version number for releases, and four for git/prerelease
AC_INIT(GStreamer i.MX Plug-ins, 3.0.7, http://www.freescale.com)

dnl initialize automake
AM_INIT_AUTOMAKE(gst-fsl-plugins, 3.0.7)

AC_CONFIG_MACRO_DIR([m4])

AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

AC_DEFUN([FIND_PARSER_SONAME],
[
    AC_MSG_CHECKING([for lib_$1_parser_$2_elinux.3.0.so soname])
    path=`$PKG_CONFIG --variable=libdir libfslparser`
    SONAME=$( $CC -print-file-name=$path/lib_$1_parser_$2_elinux.so | \
	    while read output; do objdump -p $output | \
	    grep "SONAME" | \
	    sed -e 's/ \+SONAME \+//'; done 2> /dev/null )
    if [[ -z "$SONAME" ]]; then
    	AC_MSG_RESULT([no])
	AC_MSG_WARN([Unable to determine soname of lib_$1_parser_$2_elinux.so library, use default])
	SONAME=lib_$1_parser_$2_elinux.so
    else
    	AC_MSG_RESULT([$SONAME])
    fi
    apath=`echo $path | sed -e 's|.*/imx-mm/|/usr/lib/imx-mm/|'`
    $1_$2_SONAME=$apath/$SONAME
    AC_SUBST($1_$2_SONAME)
])

AC_DEFUN([FIND_ACODEC_SONAME],
[
    AC_MSG_CHECKING([for lib_$1_wrap_$2_elinux.so soname])
    path=`$PKG_CONFIG --variable=libdir libfslaudiocodec | sed -e 's|.*/imx-mm/|/usr/lib/imx-mm/|'`
    SONAME=$( $CC -print-file-name=$path/wrap/lib_$1_wrap_$2_elinux.so | \
	    while read output; do objdump -p $output | \
	    grep "SONAME" | \
	    sed -e 's/ \+SONAME \+//'; done 2> /dev/null )
    if [[ -z "$SONAME" ]]; then
    	AC_MSG_RESULT([no])
	AC_MSG_WARN([Unable to determine soname of lib_$1_wrap_$2_elinux.so library, use default])
	SONAME=lib_$1_wrap_$2_elinux.so
    else
    	AC_MSG_RESULT([$SONAME])
    fi

    apath=`echo $path | sed -e 's|.*/imx-mm/|/usr/lib/imx-mm/|'`
    $1_$2_SONAME=$apath/wrap/$SONAME
    AC_SUBST($1_$2_SONAME)
])

AC_DEFUN([CHECK_DISABLE_FEATURE],
[
    AC_ARG_ENABLE($1,
        [AS_HELP_STRING([--disable-[$1]], [$2])],
        [use_[$1]=$enableval],
        [use_[$1]=yes])
    if test "$use_[$1]" = "yes"; then
        SAVE_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS=`$PKG_CONFIG --cflags libfslaudiocodec`
        CPPFLAGS+=`$PKG_CONFIG --cflags libfslvideocodec`
        CPPFLAGS+=`$PKG_CONFIG --cflags libfslparser`
        CPPFLAGS+=`$PKG_CONFIG --cflags libfslvpuwrap`
        AC_CHECK_HEADERS([$4], HAVE_[$3]=yes, HAVE_[$3]=no && break)
        CPPFLAGS="$SAVE_CPPFLAGS"
        if test "x$HAVE_[$3]" = "xyes"; then
            enabled_feature="$enabled_feature\n\t\t[$5]"
        else
            disabled_feature="$disabled_feature\n\t\t[$5]"
        fi
    else
        disabled_feature="$disabled_feature\n\t\t[$5]"
    fi
    AM_CONDITIONAL(USE_[$3], test "x$HAVE_[$3]" = "xyes")
])

dnl AM_MAINTAINER_MODE provides the option to enable maintainer mode
AM_MAINTAINER_MODE

dnl check for tools
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl check target type
if test "$PLATFORM" = "MX28" || test "$PLATFORM" = "MX233" || test "$PLATFORM" = "MX25" || test "$PLATFORM" = "MX27" || \
   test "$PLATFORM" = "MX31" || test "$PLATFORM" = "MX35" || test "$PLATFORM" = "MX37" \
   || test "$PLATFORM" = "MX51" || test "$PLATFORM" = "MX53" || test "$PLATFORM" = "MX50" || test "$PLATFORM" = "MX5X" \
   || test "$PLATFORM" = "MX6" ;
then
  AC_MSG_NOTICE(build for $PLATFORM)
else
  AC_MSG_ERROR(No target platform specified! Use ./configure PLATFORM=(MX28/MX233/MX25/MX27/MX31/MX35/MX37/MX51/MX53/MX50/MX5X/MX6) to retry)
fi

AC_SUBST(PLATFORM)

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer

GST_REQUIRED=0.10

GST_MAJORMINOR=0.10
PKG_CHECK_MODULES(GST, \
  gstreamer-$GST_MAJORMINOR >= $GST_REQUIRED,
  HAVE_GST=yes,HAVE_GST=no)

dnl Give error and exit if we don't have gstreamer
if test "x$HAVE_GST" = "xno"; then
  AC_MSG_ERROR(you need gstreamer development packages installed !)
fi

dnl make GST_CFLAGS and GST_LIBS available
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_BASE, gstreamer-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BASE=yes, HAVE_GST_BASE=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_BASE" = "xno"; then
  AC_MSG_ERROR(no GStreamer base found)
fi

dnl make GST_BASE_CFLAGS and GST_BASE_LIBS available
AC_SUBST(GST_BASE_CFLAGS)
AC_SUBST(GST_BASE_LIBS)

dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_PLUGINS_BASE, gstreamer-plugins-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_PLUGINS_BASE=yes, HAVE_GST_PLUGINS_BASE=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_PLUGINS_BASE" = "xno"; then
  AC_MSG_ERROR(no GStreamer plugin base libs found)
fi

dnl make GST_PLUGINS_BASE_CFLAGS and GST_PLUGINS_BASE_LIBS available
AC_SUBST(GST_PLUGINS_BASE_CFLAGS)
AC_SUBST(GST_PLUGINS_BASE_LIBS)

AC_CHECK_HEADERS([vpu_lib.h], [HAVE_VPU_LIB=yes], [HAVE_VPU_LIB=no])
if test "x$HAVE_VPU_LIB" = "xyes"; then
    VPU_LIBS=`$PKG_CONFIG --libs libfslvpuwrap 2>/dev/null`
    VPU_LIBS+=" -lvpu"
fi
AC_SUBST(VPU_LIBS)

AC_CHECK_HEADERS([linux/ipu.h], [HAVE_IPU_HEAD=yes], [HAVE_IPU_HEAD=no])
if test "x$HAVE_IPU_HEAD" = "xno"; then
    ipuhead=`find $CROSS_ROOT/usr/src/ -name ipu.h | grep "/linux/ipu.h" | head -n 1 | sed -e 's,/linux/ipu.h,,'`
    if test "x$ipuhead" != "x"; then
        IPU_CFLAGS=-I$ipuhead
    fi
fi
AC_CHECK_LIB(ipu, mxc_ipu_lib_task_init, [IPU_LIBS=-lipu], [echo "No libipu around, don't use it"])
AC_SUBST(IPU_CFLAGS)
AC_SUBST(IPU_LIBS)

FSL_MM_CORE_CFLAGS=`$PKG_CONFIG --cflags libfslaudiocodec 2>/dev/null`
FSL_MM_CORE_CFLAGS+=`$PKG_CONFIG --cflags libfslvideocodec 2>/dev/null`
FSL_MM_CORE_CFLAGS+=`$PKG_CONFIG --cflags libfslparser 2>/dev/null`
FSL_MM_CORE_CFLAGS+=`$PKG_CONFIG --cflags libfslvpuwrap 2>/dev/null`
AC_SUBST(FSL_MM_CORE_CFLAGS)

FSL_MM_CORE_LIBS=`$PKG_CONFIG --libs libfslaudiocodec 2>/dev/null`
FSL_MM_CORE_LIBS+=`$PKG_CONFIG --libs libfslvideocodec 2>/dev/null`
FSL_MM_CORE_LIBS+=`$PKG_CONFIG --libs libfslparser 2>/dev/null`
FSL_MM_CORE_LIBS+=`$PKG_CONFIG --libs libfslvpuwrap 2>/dev/null`
AC_SUBST(FSL_MM_CORE_LIBS)

EXTRAPLATFORMCFLAGS="-DARM -D_GNUC_"
XTRAINCPATHS="$FSL_MM_CORE_CFLAGS"
CFLAGS="$CFLAGS $XTRAINCPATHS $EXTRAPLATFORMCFLAGS"

FIND_PARSER_SONAME(asf,arm9)
FIND_PARSER_SONAME(asf,arm11)
FIND_PARSER_SONAME(avi,arm9)
FIND_PARSER_SONAME(avi,arm11)
FIND_PARSER_SONAME(flv,arm9)
FIND_PARSER_SONAME(flv,arm11)
FIND_PARSER_SONAME(mpg2,arm9)
FIND_PARSER_SONAME(mpg2,arm11)
FIND_PARSER_SONAME(mp4,arm9)
FIND_PARSER_SONAME(mp4,arm11)
FIND_PARSER_SONAME(mkv,arm9)
FIND_PARSER_SONAME(mkv,arm11)
FIND_PARSER_SONAME(ogg,arm9)
FIND_PARSER_SONAME(ogg,arm11)

FIND_ACODEC_SONAME(aacplusd, arm9)
FIND_ACODEC_SONAME(aacplusd, arm11)
FIND_ACODEC_SONAME(aacplusd, arm12)
FIND_ACODEC_SONAME(aacd, arm9)
FIND_ACODEC_SONAME(aacd, arm11)
FIND_ACODEC_SONAME(aacd, arm12)
FIND_ACODEC_SONAME(mp3d, arm9)
FIND_ACODEC_SONAME(mp3d, arm11)
FIND_ACODEC_SONAME(mp3d, arm12)
FIND_ACODEC_SONAME(wma10d, arm9)
FIND_ACODEC_SONAME(wma10d, arm11)
FIND_ACODEC_SONAME(wma10d, arm12)
FIND_ACODEC_SONAME(ac3d, arm11)
FIND_ACODEC_SONAME(ac3d, arm12)
FIND_ACODEC_SONAME(vorbisd, arm11)
FIND_ACODEC_SONAME(vorbisd, arm12)

dnl set the plugindir where plugins should be installed
plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AM_CONDITIONAL(PLATFORM_IS_MX2X, test "x$PLATFORM" = "xMX28" || test "x$PLATFORM" = "xMX233" || test "x$PLATFORM" = "xMX25" || test "x$PLATFORM" = "xMX27")
AM_CONDITIONAL(PLATFORM_IS_MX233, test "x$PLATFORM" = "xMX233")
AM_CONDITIONAL(PLATFORM_IS_MX25, test "x$PLATFORM" = "xMX25")
AM_CONDITIONAL(PLATFORM_IS_MX27, test "x$PLATFORM" = "xMX27")
AM_CONDITIONAL(PLATFORM_IS_MX28, test "x$PLATFORM" = "xMX28")

AM_CONDITIONAL(PLATFORM_IS_MX3X, test "x$PLATFORM" = "xMX31" || test "x$PLATFORM" = "xMX35" || test "x$PLATFORM" = "xMX37")
AM_CONDITIONAL(PLATFORM_IS_MX31, test "x$PLATFORM" = "xMX31")
AM_CONDITIONAL(PLATFORM_IS_MX35, test "x$PLATFORM" = "xMX35")
AM_CONDITIONAL(PLATFORM_IS_MX37, test "x$PLATFORM" = "xMX37")


AM_CONDITIONAL(PLATFORM_IS_MX5X, test "x$PLATFORM" = "xMX51" || test "x$PLATFORM" = "xMX53" || test "x$PLATFORM" = "xMX50" || test "x$PLATFORM" = "xMX5X")
AM_CONDITIONAL(PLATFORM_IS_MX51, test "x$PLATFORM" = "xMX51")
AM_CONDITIONAL(PLATFORM_IS_MX53, test "x$PLATFORM" = "xMX53")
AM_CONDITIONAL(PLATFORM_IS_MX50, test "x$PLATFORM" = "xMX50")

AM_CONDITIONAL(PLATFORM_IS_MX6X, test "x$PLATFORM" = "xMX6")


AM_CONDITIONAL(LTIBBUILD, test "x$BUILDER" = "xLTIB")

CHECK_DISABLE_FEATURE(aacdec, [Disable aac decoder plugin], [AAC_DEC], [aacd_dec_interface.h], [plugin: aac_dec])
CHECK_DISABLE_FEATURE(aacpdec, [Disable aac plus decoder plugin], [AACP_DEC], [aacplus_dec_interface.h], [plugin: aacp_dec])
CHECK_DISABLE_FEATURE(mp3dec, [Disable mp3 decoder plugin], [MP3_DEC], [mp3_dec_interface.h], [plugin: mp3_dec])
CHECK_DISABLE_FEATURE(vorbisdec, [Disable vorbis decoder plugin], [VORBIS_DEC], [oggvorbis_dec_api.h], [plugin: vorbis_dec])
CHECK_DISABLE_FEATURE(wma10dec, [Disable wma10 decoder plugin], [WMA10_DEC], [wma10-dec/wma10_dec_interface.h], [plugin: wma10_dec])
CHECK_DISABLE_FEATURE(ac3dec, [Disable ac3 decoder plugin], [AC3_DEC], [ac3d_dec_interface.h], [plugin: ac3_dec])
CHECK_DISABLE_FEATURE(amrdec, [Disable amr decoder plugin], [AMR_DEC], [nb_amr_dec_api.h wbamr_dec_interface.h], [plugin: amr_dec])

CHECK_DISABLE_FEATURE(mp3enc, [Disable mp3 encoder plugin], [MP3_ENC], [mp3_enc_interface.h], [plugin: mp3_enc])
CHECK_DISABLE_FEATURE(wma8enc, [Disable wma8 encoder plugin], [WMA8_ENC], [wma8_enc_interface.h], [plugin: wma8_enc])

CHECK_DISABLE_FEATURE(downmix, [Disable downmix audio process plugin], [DOWNMIX], [downmix_dec_interface.h], [plugin: downmix])

CHECK_DISABLE_FEATURE(h264dec, [Disable h264 decoder plugin], [H264_DEC], [avcd_dec_api.h], [plugin: h264_dec])
CHECK_DISABLE_FEATURE(mpeg4dec, [Disable mpeg4 decoder plugin], [MPEG4_DEC], [mpeg4_asp_api.h], [plugin: mpeg4_dec])
CHECK_DISABLE_FEATURE(mpeg2dec, [Disable mpeg2 decoder plugin], [MPEG2_DEC], [mpeg2_dec_api.h], [plugin: mpeg2_dec])
CHECK_DISABLE_FEATURE(wmv9mpdec, [Disable wmv9mp decoder plugin], [WMV9MP_DEC], [wmv9mp_dec_api.h], [plugin: wmv9mp_dec])
CHECK_DISABLE_FEATURE(wmv78dec, [Disable wmv78 decoder plugin], [WMV78_DEC], [wmv789_dec_api.h], [plugin: wmv78_dec])

CHECK_DISABLE_FEATURE(vpuwrap, [Disable vpu plugin], [VPU_WRAP], [vpu_wrapper.h], [plugin: vpu_wrap])

CHECK_DISABLE_FEATURE(aiur, [Disable aiur demux], [AIUR], [fsl_parser.h], [plugin: aiur])
CHECK_DISABLE_FEATURE(beep, [Disable beep audio decoder], [BEEP], [fsl_unia.h], [plugin: beep])

CHECK_DISABLE_FEATURE(v4lsink, [Disable fsl v4l sink], [V4L_SINK], [linux/videodev2.h], [plugin: v4lsink])

saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $IPU_CFLAGS"
CHECK_DISABLE_FEATURE(isink, [Disable fsl ipu sink], [I_SINK], [linux/mxcfb.h], [plugin: isink])
CHECK_DISABLE_FEATURE(ipucsc, [Disable fsl ipu csc], [I_CSC], [linux/mxcfb.h], [plugin: ipucsc])
CFLAGS=$saved_CFLAGS

CHECK_DISABLE_FEATURE(ipulib, [Disable ipu lib], [IPU_LIB], [mxc_ipu_hl_lib.h], [library: libipu])
CHECK_DISABLE_FEATURE(x11, [Disable x11 lib], [X11], [X11/Xlib.h], [library: libX11])

AC_SUBST(GSTMXLIBS_VERSION)
GSTMXLIBS_VERSION=0:1:0

AC_OUTPUT(
Makefile
gstreamer-fsl.pc
libs/Makefile
src/audio/aac_dec/src/Makefile
src/audio/aacplus_dec/src/Makefile
src/audio/ac3_dec/src/Makefile
src/audio/mp3_dec/src/Makefile
src/audio/mp3_enc/src/Makefile
src/audio/vorbis_dec/src/Makefile
src/audio/wma10_dec/src/Makefile
src/audio/wma8_enc/src/Makefile
src/audio/spdif_rx/src/Makefile
src/audio/spdif_tx/src/Makefile
src/audio/audio_pp/src/Makefile
src/audio/down_mix/src/Makefile
src/audio/amr_dec/src/Makefile
src/audio/amr_enc/src/Makefile
src/audio/beep/src/Makefile
src/audio/beep/src/beep_registry.arm9.cf
src/audio/beep/src/beep_registry.arm11.cf
src/audio/beep/src/beep_registry.arm12.cf
src/video/h264_dec/src/Makefile
src/video/mpeg4asp_dec/src/Makefile
src/video/mpeg2_dec/src/Makefile
src/video/wmv9mp_dec/src/Makefile
src/video/wmv_dec/src/Makefile
src/video/vpu_dec.full/src/Makefile
src/video/vpu_enc/src/Makefile
src/video/vpu/src/Makefile
src/video/ipu_csc/src/Makefile
src/video/deinterlace/src/Makefile
src/parser/aiur/src/Makefile
src/parser/aiur/src/aiur_registry.arm11.cf
src/parser/aiur/src/aiur_registry.arm9.cf
src/misc/v4l_source/src/Makefile
src/misc/v4l_sink/src/Makefile
src/misc/i_sink/src/Makefile
tools/Makefile
tools/gplay/Makefile)

echo -e "Configure result:"
echo -e "\tEnabled features:$enabled_feature"
echo -e "\tDisabled features:$disabled_feature"

# plugindir is set in configure
if PLATFORM_IS_MX2X
CORELIB=_H264_dec_arm9_elinux
endif

if PLATFORM_IS_MX3X
CORELIB=_H264_dec_arm11_elinux
endif

if PLATFORM_IS_MX5X
CORELIB=_H264_dec_arm11_elinux
endif

if PLATFORM_IS_MX6X
CORELIB=_H264_dec_arm11_elinux
endif



# change libgstplugin.la to something more suitable
plugin_LTLIBRARIES = libmfw_gst_h264dec.la

# for the next set of variables, rename the prefix if you renamed the .la

# sources used to compile this plug-in
libmfw_gst_h264dec_la_SOURCES = mfw_gst_h264dec.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
if PLATFORM_IS_MX2X
libmfw_gst_h264dec_la_CFLAGS = $(GST_BASE_CFLAGS) -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -O2 -DDPB_FIX -D_$(PLATFORM) -DDIRECT_RENDER_VERSION=2 -DFRAMEDROPING_ENALBED -march=armv5te -mcpu=arm926ej-s  -I../../../../libs/me -I../../../../libs/gstbufmeta 
endif


if PLATFORM_IS_MX3X
libmfw_gst_h264dec_la_CFLAGS = $(GST_BASE_CFLAGS) -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -O2 -DDPB_FIX -DDIRECT_RENDER_VERSION=2 -DFRAMEDROPING_ENALBED  -I../../../../libs/me -I../../../../libs/gstbufmeta
endif


if PLATFORM_IS_MX5X
libmfw_gst_h264dec_la_CFLAGS = $(GST_BASE_CFLAGS) -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -O2 -DDPB_FIX -DDIRECT_RENDER_VERSION=2 -DFRAMEDROPING_ENALBED  -I../../../../libs/me -I../../../../libs/gstbufmeta
endif

if PLATFORM_IS_MX6X
libmfw_gst_h264dec_la_CFLAGS = $(GST_BASE_CFLAGS) -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -O2 -DDPB_FIX -DDIRECT_RENDER_VERSION=2 -DFRAMEDROPING_ENALBED  -I../../../../libs/me -I../../../../libs/gstbufmeta
endif

libmfw_gst_h264dec_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -l$(CORELIB)
libmfw_gst_h264dec_la_LIBADD += ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_h264dec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_h264dec.h


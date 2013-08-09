if PLATFORM_IS_MX2X
CORELIB=_mp3_enc_arm9_elinux
endif


if PLATFORM_IS_MX3X
CORELIB=_mp3_enc_arm11_elinux
endif
if PLATFORM_IS_MX5X
CORELIB=_mp3_enc_arm12_elinux
endif
if PLATFORM_IS_MX6X
CORELIB=_mp3_enc_arm12_elinux
endif

# plugindir is set in configure

# change libgstplugin.la to something more suitable
plugin_LTLIBRARIES = libmfw_gst_mp3enc.la

# for the next set of variables, rename the prefix if you renamed the .la

# sources used to compile this plug-in
libmfw_gst_mp3enc_la_SOURCES = mfw_gst_mp3enc.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
if PLATFORM_IS_MX2X
libmfw_gst_mp3enc_la_CFLAGS = $(GST_CFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -march=armv5te -mcpu=arm926ej-s
endif


if PLATFORM_IS_MX3X
libmfw_gst_mp3enc_la_CFLAGS = $(GST_CFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
endif

if PLATFORM_IS_MX5X
libmfw_gst_mp3enc_la_CFLAGS = $(GST_CFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
endif
if PLATFORM_IS_MX6X
libmfw_gst_mp3enc_la_CFLAGS = $(GST_CFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
endif


libmfw_gst_mp3enc_la_LIBADD = $(GST_LIBS) -lgstbase-$(GST_MAJORMINOR) -l$(CORELIB)
libmfw_gst_mp3enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_mp3enc.h

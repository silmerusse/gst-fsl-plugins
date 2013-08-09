# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_beep.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_beep_la_SOURCES =  beep.c beepregistry.c beepdec.c beeptypefind.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
if PLATFORM_IS_MX2X
libmfw_gst_beep_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -I../../../../libs -D_ARM9 -march=armv5te -mcpu=arm926ej-s
libmfw_gst_beep_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_MAJORMINOR) -lgstriff-@GST_MAJORMINOR@ -lgsttag-@GST_MAJORMINOR@ -ldl ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
endif


if PLATFORM_IS_MX3X
libmfw_gst_beep_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -I../../../../libs -D_ARM11
libmfw_gst_beep_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_MAJORMINOR) -lgstriff-@GST_MAJORMINOR@ -lgsttag-@GST_MAJORMINOR@ -ldl ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
endif


if PLATFORM_IS_MX5X
libmfw_gst_beep_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -I../../../../libs -D_ARM12
libmfw_gst_beep_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_MAJORMINOR) -lgstriff-@GST_MAJORMINOR@ -lgsttag-@GST_MAJORMINOR@ -ldl ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
endif

if PLATFORM_IS_MX6X
libmfw_gst_beep_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -I../../../../libs -D_ARM12
libmfw_gst_beep_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_MAJORMINOR) -lgstriff-@GST_MAJORMINOR@ -lgsttag-@GST_MAJORMINOR@ -ldl ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
endif

libmfw_gst_beep_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS)

libmfw_gst_beep_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

# headers we need but don't want installed
noinst_HEADERS =  beepregistry.h beepdec.h
data_DATA = beep_registry.arm9.cf beep_registry.arm11.cf beep_registry.arm12.cf

EXTRA_DIST = beep_registry.arm9.cf.in beep_registry.arm11.cf.in beep_registry.arm12.cf.in

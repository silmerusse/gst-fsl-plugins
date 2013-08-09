# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_aiur_demux.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_aiur_demux_la_SOURCES =  aiur.c aiurregistry.c aiurstreamcache.c aiuridxtab.c aiurdemux.c aiurtypefind.c
libmfw_gst_aiur_demux_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer -DFSL_AVI_DRM_SUPPORT $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -I../../../../libs
libmfw_gst_aiur_demux_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS)  -lgstriff-@GST_MAJORMINOR@ -lgsttag-@GST_MAJORMINOR@ -ldl ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_aiur_demux_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) 
libmfw_gst_aiur_demux_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS) 

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
if PLATFORM_IS_MX2X
libmfw_gst_aiur_demux_la_CFLAGS += -D_ARM9 -march=armv5te -mcpu=arm926ej-s
registry_file=aiur_registry.arm9.cf.in
reg_inst_file=aiur_registry.arm9.cf
else
libmfw_gst_aiur_demux_la_CFLAGS += -D_ARM11
registry_file=aiur_registry.arm11.cf.in
reg_inst_file=aiur_registry.arm11.cf
endif

# headers we need but don't want installed
noinst_HEADERS =  aiurregistry.h aiurdemux.h aiurstreamcache.h aiuridxtab.h
data_DATA = $(reg_inst_file)

EXTRA_DIST = $(registry_file)

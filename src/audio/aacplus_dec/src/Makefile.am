if PLATFORM_IS_MX2X
CORELIB=_aac_dec_arm9_elinux
endif
if PLATFORM_IS_MX3X
CORELIB=_aac_dec_arm11_elinux
endif

if PLATFORM_IS_MX5X
CORELIB=_aac_dec_arm12_elinux 
endif
if PLATFORM_IS_MX6X
CORELIB=_aac_dec_arm12_elinux 
endif

if PLATFORM_IS_MX2X
SBRLIB=_aacplus_dec_arm9_elinux
endif
if PLATFORM_IS_MX3X
SBRLIB=_aacplus_dec_arm11_elinux
endif
if PLATFORM_IS_MX5X
SBRLIB=_aacplus_dec_arm11_elinux
endif
if PLATFORM_IS_MX6X
SBRLIB=_aacplus_dec_arm11_elinux
endif

# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_aacplusdec.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_aacplusdec_la_SOURCES =  mfw_gst_aacplusdec.c 

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_aacplusdec_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -DMPEG4 -DARM_OPT_MACROS -DLC -DPUSH_MODE -fno-omit-frame-pointer -fPIC 
libmfw_gst_aacplusdec_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
if PLATFORM_IS_MX2X
libmfw_gst_aacplusdec_la_CFLAGS += -march=armv5te -mcpu=arm926ej-s
libmfw_gst_aacplusdec_la_CPPFLAGS += -march=armv5te -mcpu=arm926ej-s
endif
libmfw_gst_aacplusdec_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -l$(CORELIB) -lgstaudio-$(GST_MAJORMINOR) -l$(SBRLIB)
libmfw_gst_aacplusdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS) -lgstriff-@GST_MAJORMINOR@

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_aacplusdec.h

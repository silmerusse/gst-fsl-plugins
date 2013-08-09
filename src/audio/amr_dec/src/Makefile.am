if PLATFORM_IS_MX3X
CORELIB=-l_nb_amr_dec_arm9_elinux -l_wb_amr_dec_arm9_elinux
endif

if PLATFORM_IS_MX5X
CORELIB=-l_nb_amr_dec_arm9_elinux -l_wb_amr_dec_arm9_elinux
endif
if PLATFORM_IS_MX6X
CORELIB=-l_nb_amr_dec_arm9_elinux -l_wb_amr_dec_arm9_elinux
endif

# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_amrdec.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_amrdec_la_SOURCES =  mfw_gst_amrdec.c 

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_amrdec_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -DMPEG4 -DARM_OPT_MACROS -DLC -fno-omit-frame-pointer -fPIC 
libmfw_gst_amrdec_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
libmfw_gst_amrdec_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) $(CORELIB) -lgstaudio-$(GST_MAJORMINOR)
libmfw_gst_amrdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS) -lgstriff-@GST_MAJORMINOR@

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_amrdec.h

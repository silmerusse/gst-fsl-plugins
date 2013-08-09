# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_vpu.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_vpu_la_SOURCES =  vpu.c vpudec.c vpuenc.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs

libmfw_vpu_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin  -I../../../../libs/me -I../../../../libs/gstbufmeta -I../../../../libs/gstsutils -I../../../../libs/gstnext
libmfw_vpu_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS)  -lgstriff-@GST_MAJORMINOR@ -lgsttag-@GST_MAJORMINOR@  

libmfw_vpu_la_LIBADD += ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la

libmfw_vpu_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) 

libmfw_vpu_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(VPU_LIBS)

# headers we need but don't want installed
noinst_HEADERS =  vpudec.h vpuenc.h


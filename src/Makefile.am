# Copyright (c) 2007 Intel Corporation. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

AM_CPPFLAGS = \
	-DPTHREADS		\
	$(DRM_CFLAGS)		\
	$(LIBVA_DEPS_CFLAGS)	\
	$(NULL)

driver_cflags = \
	-Wall			\
	-fvisibility=hidden	\
	$(NULL)

driver_ldflags = \
	-module -avoid-version	\
	-no-undefined		\
	-Wl,--no-undefined	\
	$(NULL)

driver_libs = \
	-lpthread -ldl		\
	$(DRM_LIBS) -ldrm_intel	\
	$(LIBVA_DEPS_LIBS)	\
	$(NULL)

source_c = \
	wrapper_drv_video.c		\
	$(NULL)

source_h = \
	wrapper_drv_video.h	\
	$(NULL)

wrapper_drv_video_la_LTLIBRARIES	= wrapper_drv_video.la
wrapper_drv_video_ladir		= $(LIBVA_DRIVERS_PATH)
wrapper_drv_video_la_CFLAGS	= $(driver_cflags)
wrapper_drv_video_la_LDFLAGS	= $(driver_ldflags)
wrapper_drv_video_la_LIBADD	= $(driver_libs)
wrapper_drv_video_la_SOURCES	= $(source_c)
noinst_HEADERS			= $(source_h)

#if USE_X11
#source_c			+= i965_output_dri.c
#source_h			+= i965_output_dri.h
#endif

#if USE_WAYLAND
#source_c			+= i965_output_wayland.c
#source_h			+= i965_output_wayland.h
#driver_cflags			+= $(WAYLAND_CFLAGS)
#endif

# Wayland protocol
#i965_output_wayland.c: $(protocol_source_h)
#@wayland_scanner_rules@

#DIST_SUBDIRS = $(SUBDIRS) wayland

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in config.h.in

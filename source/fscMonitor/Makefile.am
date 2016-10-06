##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2015 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
# Firmware Sanity Check Monitor Process
bin_PROGRAMS = fscMonitor
AM_CFLAGS = -D_ANSC_LINUX -D_ANSC_USER -D_ANSC_LITTLE_ENDIAN_
AM_CFLAGS += -I$(top_srcdir)/hal/include -I=/usr/include/ccsp
AM_LDFLAGS = -L$(top_builddir)/../CcspCommonLibrary/lib -L$(top_builddir)/../hal/lib
AM_LDFLAGS += -lhal_wifi -lhal_platform -lsysevent -lsyscfg -lccsp_common

AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4

fscMonitor_SOURCES = fscMonitor.c

fscMonitor_CPPFLAGS = -I$(top_srcdir)/source -I$(top_srcdir)/source/include -I$(top_srcdir)/source/util/utils $(CPPFLAGS) $(DBUS_CFLAGS)
#fscMonitor_CPPFLAGS = -I/var/tmp/pc-rdkb/include/dbus-1.0 -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/custom -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/include -I$(top_srcdir)/../CcspCommonLibrary/source/debug_api/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/include/linux -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/package/slap/include -I$(top_srcdir)/../hal/include -I$(top_srcdir)/../CcspCommonLibrary/source/util_api/http/include -I$(top_srcdir)/../CcspCommonLibrary/source/util_api/ansc/include -I$(top_srcdir)/source/TR-181/ml -I$(top_srcdir)/source/TR-181/sbapi -I$(top_srcdir)/include/TR-181/ml -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/common/MessageBusHelper/include -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/common/PoamIrepFolder -I$(top_srcdir)/source/Custom -I$(top_srcdir)/./include $(CPPFLAGS)

fscMonitor_LDFLAGS = -lhal_platform -lhal_wifi -lsysevent -lsyscfg -lccsp_common -lpthread -lz
#fscMonitor_LDADD = \
#             $(top_builddir)/source/sysevent/lib/libsysevent.la \
#             $(top_builddir)/source/syscfg/lib/libsyscfg.la \
#             $(top_builddir)/source/utctx/lib/libutctx.la

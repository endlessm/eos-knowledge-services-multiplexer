# Copyright (C) 2016-2018 Endless Mobile, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
# All rights reserved.

BUILT_SOURCES =
CLEANFILES =
MAINTAINERCLEANFILES = $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL)
EXTRA_DIST =

# # # SUBSTITUTED FILES # # #
# These files need to be filled in with make variables
do_subst = $(SED) -e 's|%bindir%|$(bindir)|g' -e 's|%arch%|$(UNAME_ARCH)|g'
subst_files = \
	data/com.endlessm.EknServices.SearchProviderV1.service \
	data/com.endlessm.EknServices2.SearchProviderV2.service \
	data/com.endlessm.EknServices3.SearchProviderV3.service \
	$(NULL)

$(subst_files): %: %.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $(@D) && \
	$(do_subst) $< > $@

CLEANFILES += $(subst_files)
EXTRA_DIST += $(patsubst %,%.in,$(subst_files))

bin_PROGRAMS = eks-multi-search-provider-dispatcher
eks_multi_search_provider_dispatcher_SOURCES = \
	multi-services/eks-multi-search-provider-dispatcher.c \
	$(NULL)
eks_multi_search_provider_dispatcher_CFLAGS = \
	$(MULTI_SERVICES_CFLAGS) \
	-I $(builddir)/multi-search-provider \
	$(AM_CFLAGS) \
	$(NULL)
eks_multi_search_provider_dispatcher_LDADD = \
	$(MULTI_SERVICES_LIBS) \
	$(NULL)

servicedir = $(datadir)/dbus-1/services
service_DATA = \
	data/com.endlessm.EknServices.SearchProviderV1.service \
	data/com.endlessm.EknServices2.SearchProviderV2.service \
	data/com.endlessm.EknServices3.SearchProviderV3.service \
	$(NULL)

-include $(top_srcdir)/git.mk


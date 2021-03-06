########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

# Figure out the version of Android we're building against.
#
PLATFORM_VERSION := $(shell \
	if [ -f $(TARGET_ROOT)/product/$(TARGET_PRODUCT)/system/build.prop ]; then \
		cat $(TARGET_ROOT)/product/$(TARGET_PRODUCT)/system/build.prop | \
			grep ^ro.build.version.release | cut -f2 -d'=' | cut -f1 -d'-'; \
	else \
		echo 2.0; \
	fi)

define version-starts-with
$(shell echo $(PLATFORM_VERSION) | grep -q ^$(1); \
	[ "$$?" = "0" ] && echo 1 || echo 0)
endef

# ro.build.version.release contains the version number for release builds, or
# the version codename otherwise. In this case we need to assume that the
# version of Android we're building against has the features that are in the
# final release of that version, so we set PLATFORM_VERSION to the
# corresponding release number.
#
ifeq ($(call version-starts-with,Eclair),1)
PLATFORM_VERSION := 2.0
else ifeq ($(call version-starts-with,Froyo),1)
PLATFORM_VERSION := 2.2
else ifeq ($(call version-starts-with,Gingerbread),1)
PLATFORM_VERSION := 2.3
else ifeq ($(call version-starts-with,Honeycomb),1)
PLATFORM_VERSION := 3.0
else ifeq ($(call version-starts-with,IceCreamSandwichMR),1)
PLATFORM_VERSION := 4.0.3
else ifeq ($(call version-starts-with,IceCreamSandwich),1)
PLATFORM_VERSION := 4.0
else ifeq ($(call version-starts-with,JellyBean),1)
PLATFORM_VERSION := 4.1
else ifeq ($(shell echo $(PLATFORM_VERSION) | grep -qE "[A-Za-z]+"; echo $$?),0)
PLATFORM_VERSION := 5.0
endif

PLATFORM_VERSION_MAJ   := $(shell echo $(PLATFORM_VERSION) | cut -f1 -d'.')
PLATFORM_VERSION_MIN   := $(shell echo $(PLATFORM_VERSION) | cut -f2 -d'.')
PLATFORM_VERSION_PATCH := $(shell echo $(PLATFORM_VERSION) | cut -f3 -d'.')

# Not all versions have a patchlevel; fix that up here
#
ifeq ($(PLATFORM_VERSION_PATCH),)
PLATFORM_VERSION_PATCH := 0
endif

# Macros to help categorize support for features and API_LEVEL for tests.
#
is_at_least_icecream_sandwich := \
	$(shell test $(PLATFORM_VERSION_MAJ) -ge 4 && echo 1 || echo 0)
is_at_least_icecream_sandwich_mr1 := \
	$(shell ( test $(PLATFORM_VERSION_MAJ) -gt 4 || \
				( test $(PLATFORM_VERSION_MAJ) -eq 4 && \
					( test $(PLATFORM_VERSION_MIN) -ge 1 || \
					  test $(PLATFORM_VERSION_PATCH) -ge 3 ) ) ) && echo 1 || echo 0)
is_at_least_jellybean := \
	$(shell ( test $(PLATFORM_VERSION_MAJ) -gt 4 || \
				( test $(PLATFORM_VERSION_MAJ) -eq 4 && \
				  test $(PLATFORM_VERSION_MIN) -ge 1 ) ) && echo 1 || echo 0)

# FIXME: Assume "future versions" are >=5.0, but we don't really know
is_future_version := \
	$(shell ( test $(PLATFORM_VERSION_MAJ) -ge 5 ) && echo 1 || echo 0)

# Picking an exact match of API_LEVEL for the platform we're building
# against can avoid compatibility theming and affords better integration.
#
ifeq ($(is_future_version),1)
API_LEVEL := 17
else ifeq ($(is_at_least_jellybean),1)
API_LEVEL := 16
else ifeq ($(is_at_least_icecream_sandwich),1)
# MR1        15
API_LEVEL := 14
else ifeq ($(is_at_least_honeycomb),1)
# MR2        13
# MR1        12
API_LEVEL := 11
else ifeq ($(is_at_least_gingerbread),1)
# MR1        10
API_LEVEL := 9
else ifeq ($(is_at_least_froyo),1)
API_LEVEL := 8
else ifeq ($(is_at_least_eclair),1)
# MR1        7
# 2.0.1      6
API_LEVEL := 5
else
$(error Must build against Android >= ICS MR1)
endif

# Each DDK is tested against only a single version of the platform.
# Warn if a different platform version is used.
#
ifeq ($(is_future_version),1)
$(info WARNING: Android version is newer than this DDK supports)
else ifneq ($(is_at_least_icecream_sandwich),1)
$(info WARNING: Android version is older than this DDK supports)
endif

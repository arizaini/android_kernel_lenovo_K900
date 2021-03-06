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

# from-one-* recipes make a thing from one source file, so they use $<. Others
# use $(MODULE_something) instead of $^

# We expect that MODULE_*FLAGS contains all the flags we need, including the
# flags for all modules (like $(ALL_CFLAGS) and $(ALL_HOST_CFLAGS)), and
# excluding flags for include search dirs or for linking libraries. The
# exceptions are ALL_EXE_LDFLAGS and ALL_LIB_LDFLAGS, since they depend on the
# type of thing being linked, so they appear in the commands below

define host-o-from-one-c
$(if $(V),,@echo "  HOST_CC " $(call relative-to-top,$<))
$(HOST_CC) -MD -c $(MODULE_HOST_CFLAGS) $(MODULE_INCLUDE_FLAGS) \
	-include $(CONFIG_H) $< -o $@
endef

define target-o-from-one-c
$(if $(V),,@echo "  CC      " $(call relative-to-top,$<))
$(CC) -MD -c $(MODULE_CFLAGS) $(MODULE_INCLUDE_FLAGS) \
	 -include $(CONFIG_H) $< -o $@
endef

# We use $(CC) to compile C++ files, and expect it to detect that it's
# compiling C++
define host-o-from-one-cxx
$(if $(V),,@echo "  HOST_CC " $(call relative-to-top,$<))
$(HOST_CC) -MD -c $(MODULE_HOST_CXXFLAGS) $(MODULE_INCLUDE_FLAGS) \
	 -include $(CONFIG_H) $< -o $@
endef

define target-o-from-one-cxx
$(if $(V),,@echo "  CC      " $(call relative-to-top,$<))
$(CC) -MD -c $(MODULE_CXXFLAGS) $(MODULE_INCLUDE_FLAGS) \
	 -include $(CONFIG_H) $< -o $@
endef

define host-executable-from-o
$(if $(V),,@echo "  HOST_LD " $(call relative-to-top,$@))
$(HOST_CC) $(MODULE_HOST_LDFLAGS) \
	-o $@ $(sort $(MODULE_ALL_OBJECTS)) $(MODULE_LIBRARY_DIR_FLAGS) \
	$(MODULE_LIBRARY_FLAGS)
endef

define host-executable-cxx-from-o
$(if $(V),,@echo "  HOST_LD " $(call relative-to-top,$@))
$(HOST_CXX) $(MODULE_HOST_LDFLAGS) \
	-o $@ $(sort $(MODULE_ALL_OBJECTS)) $(MODULE_LIBRARY_DIR_FLAGS) \
	$(MODULE_LIBRARY_FLAGS)
endef

define target-executable-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(CC) \
	$(SYS_EXE_LDFLAGS) $(MODULE_LDFLAGS) -o $@ \
	$(SYS_EXE_CRTBEGIN) $(sort $(MODULE_ALL_OBJECTS)) $(SYS_EXE_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) $(LIBGCC)
endef

define target-executable-cxx-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(CXX) \
	$(SYS_EXE_LDFLAGS) $(MODULE_LDFLAGS) -o $@ \
	$(SYS_EXE_CRTBEGIN) $(sort $(MODULE_ALL_OBJECTS)) $(SYS_EXE_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) $(LIBGCC)
endef

define target-shared-library-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(CC) -shared -Wl,-Bsymbolic \
	$(SYS_LIB_LDFLAGS) $(MODULE_LDFLAGS) -o $@ \
	$(SYS_LIB_CRTBEGIN) $(sort $(MODULE_ALL_OBJECTS)) $(SYS_LIB_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) $(LIBGCC)
endef

define host-shared-library-from-o
$(if $(V),,@echo "  HOST_LD " $(call relative-to-top,$@))
$(HOST_CC) -shared -Wl,-Bsymbolic \
	$(MODULE_HOST_LDFLAGS) -o $@ \
	$(sort $(MODULE_ALL_OBJECTS)) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS)
endef

# If there were any C++ source files in a shared library, we use this recipe,
# which runs the C++ compiler to link the final library
define target-shared-library-cxx-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(CXX) -shared -Wl,-Bsymbolic \
	$(SYS_LIB_LDFLAGS) $(MODULE_LDFLAGS) -o $@ \
	$(SYS_LIB_CRTBEGIN) $(sort $(MODULE_ALL_OBJECTS)) $(SYS_LIB_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) $(LIBGCC)
endef

define host-copy-debug-information
$(HOST_OBJCOPY) --only-keep-debug $@ $(basename $@).dbg
endef

define host-strip-debug-information
$(HOST_STRIP) --strip-unneeded $@
endef

define host-add-debuglink
$(if $(V),,@echo "  DBGLINK " $(call relative-to-top,$(basename $@).dbg))
$(HOST_OBJCOPY) --add-gnu-debuglink=$(basename $@).dbg $@
endef

define target-copy-debug-information
$(OBJCOPY) --only-keep-debug $@ $(basename $@).dbg
endef

define target-strip-debug-information
$(STRIP) --strip-unneeded $@
endef

define target-add-debuglink
$(if $(V),,@echo "  DBGLINK " $(call relative-to-top,$(basename $@).dbg))
$(OBJCOPY) --add-gnu-debuglink=$(basename $@).dbg $@
endef

define host-static-library-from-o
$(if $(V),,@echo "  HOST_AR " $(call relative-to-top,$@))
$(HOST_AR) cru $@ $(sort $(MODULE_ALL_OBJECTS))
endef

define target-static-library-from-o
$(if $(V),,@echo "  AR      " $(call relative-to-top,$@))
$(AR) cru $@ $(sort $(MODULE_ALL_OBJECTS))
endef

define tab-c-from-y
$(if $(V),,@echo "  BISON   " $(call relative-to-top,$<))
$(BISON) $(MODULE_BISON_FLAGS) -o $@ -d $<
endef

define l-c-from-l
$(if $(V),,@echo "  FLEX    " $(call relative-to-top,$<))
$(FLEX) $(MODULE_FLEX_FLAGS) -o$@ $<
endef

define clean-dirs
$(if $(V),,@echo "  RM      " $(call relative-to-top,$(MODULE_DIRS_TO_REMOVE)))
$(RM) -rf $(MODULE_DIRS_TO_REMOVE)
endef

define make-directory
$(MKDIR) -p $@
endef

define check-exports
endef

# Check a source file with the program specified in $(CHECK).
# If $(CHECK) is empty, don't do anything.
ifeq ($(CHECK),)
check-src :=
else
define check-src-1
$(if $(V),,@echo "  CHECK   " $(call relative-to-top,$<))
$(if $(IGNORE_CHECK_ERRORS),-,)$(CHECK) $(MODULE_INCLUDE_FLAGS) \
	$(if $(CHECK_NO_CONFIG_H),,-include $(CONFIG_H)) \
	$(filter -D%,$(if $(MODULE_HOST_BUILD),$(MODULE_HOST_CFLAGS),$(MODULE_CFLAGS))) \
	$(CHECKFLAGS) $<
endef
# If CHECK_ONLY is set, only check files matching a Make pattern.
# e.g. CHECK_ONLY=opengles1/%.c
define check-src
$(if $(if $(CHECK_ONLY),$(filter $(CHECK_ONLY),$<),true),$(check-src-1),@:)
endef
endif

# Programs used in recipes

BISON ?= bison
CC ?= gcc
CXX ?= g++
HOST_CC ?= gcc
HOST_CXX ?= g++
JAR ?= jar
JAVA ?= java
JAVAC ?= javac
ZIP ?= zip

ifeq ($(USE_CCACHE),1)
CCACHE ?= ccache
endif

override AR				:= $(if $(V),,@)$(CROSS_COMPILE)ar
override BISON			:= $(if $(V),,@)$(BISON)
override BZIP2			:= $(if $(V),,@)bzip2 -9
override CAT			:= $(if $(V),,@)cat
override CC				:= $(if $(V),,@)$(CCACHE) $(CROSS_COMPILE)$(CC)
override CC_CHECK		:= $(if $(V),,@)$(MAKE_TOP)/tools/cc-check.sh
override CXX			:= $(if $(V),,@)$(CCACHE) $(CROSS_COMPILE)$(CXX)
override CHMOD			:= $(if $(V),,@)chmod
override CHECK			:= $(if $(CHECK),$(if $(V),,@)$(CHECK),)
override CP				:= $(if $(V),,@)cp
override ECHO			:= $(if $(V),,@)echo
override FLEX			:= $(if $(V),,@)flex
override GAWK			:= $(if $(V),,@)gawk
override GREP			:= $(if $(V),,@)grep
override HOST_AR		:= $(if $(V),,@)ar
override HOST_CC		:= $(if $(V),,@)$(CCACHE) $(HOST_CC)
override HOST_CXX		:= $(if $(V),,@)$(CCACHE) $(HOST_CXX)
override HOST_OBJCOPY	:= $(if $(V),,@)objcopy
override HOST_STRIP		:= $(if $(V),,@)strip
override INSTALL		:= $(if $(V),,@)install
override JAR			:= $(if $(V),,@)$(JAR)
override JAVA			:= $(if $(V),,@)$(JAVA)
override JAVAC			:= $(if $(V),,@)$(JAVAC)
override M4				:= $(if $(V),,@)m4
override MKDIR			:= $(if $(V),,@)mkdir
override MV				:= $(if $(V),,@)mv
override OBJCOPY		:= $(if $(V),,@)$(CROSS_COMPILE)objcopy
override PERL			:= $(if $(V),,@)perl
override PYTHON			:= $(if $(V),,@)python
override RANLIB			:= $(if $(V),,@)$(CROSS_COMPILE)ranlib
override RM				:= $(if $(V),,@)rm -f
override ROGUEASM		:= $(if $(V),,@)$(HOST_OUT)/rogueasm
override SED			:= $(if $(V),,@)sed
override STRIP			:= $(if $(V),,@)$(CROSS_COMPILE)strip
override TAR			:= $(if $(V),,@)tar
override TOUCH  		:= $(if $(V),,@)touch
override TEST			:= $(if $(V),,@)test
override VHD2INC		:= $(if $(V),,@)$(HOST_OUT)/vhd2inc
override ZIP			:= $(if $(V),,@)$(ZIP)

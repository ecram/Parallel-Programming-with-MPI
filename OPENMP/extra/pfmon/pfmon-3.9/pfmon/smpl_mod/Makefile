#
# Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
# Contributed by Stephane Eranian <eranian@hpl.hp.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA
#

TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)/../..

include $(TOPDIR)/config.mk
include $(TOPDIR)/rules.mk

CFLAGS += -I. -I.. -D_GNU_SOURCE

#
# modules supporting any PMU models
#
SRCS=inst_hist_smpl.c compact_smpl.c raw_smpl.c detailed_smpl.c pfmon_smpl_util.c

ifeq ($(ARCH),ia64)
SRCS  += detailed_itanium.c detailed_itanium2.c detailed_montecito.c \
         dear_hist_ia64.c dear_itanium.c dear_itanium2.c dear_montecito.c \
	 pfmon_smpl_ia64_old.c \
	 inst_hist_ia64_old_smpl.c \
	 detailed_ia64_old_smpl.c \
         dear_hist_ia64_old_smpl.c
endif

ifeq ($(ARCH),ia32)
SRCS   += pebs_smpl.c
endif
ifeq ($(ARCH),x86_64)
SRCS   += pebs_smpl.c
endif

OBJS=$(SRCS:.c=.o)

TARGET=libsmplfmt.a

all: $(TARGET)

$(TARGET): $(OBJS)
	rm -f $@
	$(AR) rv $@ $(OBJS)

$(OBJS) : $(SRCS) $(TOPDIR)/rules.mk $(TOPDIR)/config.mk Makefile

clean: 
	rm -f $(OBJS) $(TARGET) 

distclean: clean

depend:
	$(MKDEP) $(SRCS)

.PHONY: install

#
# Copyright (c) 2001-2006 Hewlett-Packard Development Company, L.P.
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


#
# Look in config.mk for options
#
TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

subdirs=pfmon tests docs

include config.mk

all: 
	@echo compiling pfmon for $(ARCH) architecture
ifeq ($(CONFIG_PFMON_STATIC),y)
	@echo compiling pfmon as a static binary
else
	@echo compiling pfmon as a shared binary
endif
	@set -e ; for d in $(subdirs) ; do $(MAKE) -C $$d $@ ; done

clean: 
	@set -e ; for d in $(subdirs) ; do $(MAKE) -C $$d $@ ; done

distclean: 
	@set -e ; for d in $(subdirs) ; do $(MAKE) -C $$d $@ ; done

depend: 
	@set -e ; for d in $(subdirs) ; do $(MAKE) -C $$d $@ ; done

tar: distclean
	a=`basename $$PWD`; cd ..; tar zcf $$a.tar.gz $$a; echo generated ../$$a.tar.gz;

tarcvs: distclean
	a=`basename $$PWD`; cd ..; tar --exclude=CVS -zcf $$a.tar.gz $$a; echo generated ../$$a.tar.gz;

install: 
	@set -e ; for d in $(subdirs) ; do $(MAKE) -C $$d $@ ; done


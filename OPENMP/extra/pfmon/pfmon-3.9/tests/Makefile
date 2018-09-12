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
TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)/..

include $(TOPDIR)/config.mk
include $(TOPDIR)/rules.mk

LIBS += -L$(PFMLIBDIR) -lpfm
LIBPFM=$(PFMLIBDIR)/libpfm.a
CFLAGS += -I$(TOPDIR)/pfmon -D_GNU_SOURCE

TARGETS= getpid noploop fpops exec saxpy saxpyf  fib  pong \
	linked_list forktest forkexectest dlopentest1 dlopentest2 dlopentest3

DLLIBS=libabc.so libxyz.so libfib1.so libfib2.so

XTRA_TARGETS=pfdbg pthreadtest exectest forktest forkexectest unknown

ifeq ($(ARCH),ia64)
TARGETS +=una spectest specatest unasig triggers itlb_test
endif

ALL_TARGETS=$(TARGETS) $(XTRA_TARGETS) $(DLLIBS)

all: $(TARGETS) $(XTRA_TARGETS) $(DLLIBS)

pthreadtest: pthreadtest.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ pthreadtest.o -lpthread

pfdbg: pfdbg.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) pfdbg.o $(LIBS)

libfib1.so : libfib1.o
	$(CC) libfib1.c $(CFLAGS) -fPIC -shared -o $@
	strip $@

libfib2.so : libfib2.o
	$(CC) libfib2.c $(CFLAGS) -fPIC -shared -o $@
	strip $@

unknown: unknown.o libfib1.so libfib2.so
	$(CC) $(CFLAGS) unknown.o -o $@ -L. -lfib1 -lfib2

$(TARGETS):  %:%.o
	$(CC) -ldl -o $@ $(CFLAGS) $(LDFLAGS) $*.o

libabc.so:
	$(CC) -fPIC $(CFLAGS) $(LDFLAGS) -o libabc.so -shared sharedlib1.c

libxyz.so:
	$(CC) -fPIC $(CFLAGS) $(LDFLAGS) -o libxyz.so -shared sharedlib2.c

itlb_test.o: itlb_test.c
	$(CC) -c -falign-functions=16384 $<

clean:
	rm -f *.o $(ALL_TARGETS) *.so

distclean: clean

depend:
	$(MKDEP) $(SRCS)

install: $(ALL_TARGETS)
	-mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 pfdbg $(DESTDIR)$(BINDIR)/pfdbg
# DO NOT DELETE

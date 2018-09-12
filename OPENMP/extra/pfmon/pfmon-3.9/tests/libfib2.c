/*
 * libfib.c: test for sampling with unknown symbols
 *
 * Copyright (c) 2009 Google, Inc
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 */
#include <sys/types.h>
#include <inttypes.h>

static uint64_t _dofib2(uint64_t n) __attribute__((noinline));

/*
 * actual library entry point
 */
uint64_t fib2(uint64_t n)
{
	return _dofib2(n);
}

/*
 * static function where time is actually spent
 * but if symbol table stripped, symbol disappear
 */
static uint64_t _dofib2(uint64_t n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 2;

	return _dofib2(n-1) + _dofib2(n-2);
}

/*
 * fake call to surround _dofib() with visible symbols
 */
uint64_t fake_fib2(uint64_t n)
{
	return _dofib2(n);
}

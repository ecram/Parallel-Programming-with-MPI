/* karp.f -- translated by f2c (version 20061008).
   You must link the resulting object file with libf2c:
	on Microsoft Windows system, link with libf2c.lib;
	on Linux or Unix systems, link with .../path/to/libf2c.a -lm
	or, if you install libf2c.a in a standard place, with -lf2c -lm
	-- in that order, at the end of the command line, as in
		cc *.o -lf2c -lm
	Source for libf2c is in /netlib/f2c/libf2c.zip, e.g.,

		http://www.netlib.org/f2c/libf2c.zip
*/

#include "f2c.h"

/* Table of constant values */

static integer c__9 = 9;
static integer c__1 = 1;
static integer c__4 = 4;

/* Main program */ int MAIN__(void)
{
    /* System generated locals */
    integer i__1;
    real r__1;

    /* Builtin functions */
    double atan(doublereal);
    integer s_wsle(cilist *), do_lio(integer *, integer *, char *, ftnlen), 
	    e_wsle(void);

    /* Local variables */
    static integer i__, n;
    static real w, pi, err, sum;
    extern /* Subroutine */ int exit_(void);

    /* Fortran I/O blocks */
    static cilist io___7 = { 0, 6, 0, 0, 0 };



/* This simple program approximates pi by computing pi = integral */
/* from 0 to 1 of 4/(1+x*x)dx which is approximated by sum from */
/* k=1 to N of 4 / ((1 + (k-1/2)**2 ).  The only input data required is N. */

/* NOTE: Comments that begin with "cspmd" are hints for part b of the */
/*       lab exercise, where you convert this into an MPI program. */

/* spmd  Each process could be given a chunk of the interval to do. */

    pi = atan(1.f) * 4.f;
/* spmd  call startup routine that returns the number of tasks and the */
/* spmd  taskid of the current instance. */

/* Now solicit a new value for N.  When it is 0, then you should depart. */
    n = 1000;
    if (n <= 0) {
	exit_();
    }
    w = 1.f / n;
    sum = 0.f;
    i__1 = n;
    for (i__ = 1; i__ <= i__1; ++i__) {
	r__1 = (i__ - .5f) * w;
	sum += 4.f / (1.f + r__1 * r__1);
    }
    sum *= w;
    err = sum - pi;
    s_wsle(&io___7);
    do_lio(&c__9, &c__1, "sum, err =", (ftnlen)10);
    do_lio(&c__4, &c__1, (char *)&sum, (ftnlen)sizeof(real));
    do_lio(&c__4, &c__1, (char *)&err, (ftnlen)sizeof(real));
    e_wsle();
    return 0;
} /* MAIN__ */

/* Main program alias */ int karp_ () { MAIN__ (); return 0; }

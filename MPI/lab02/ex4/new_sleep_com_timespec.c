#include <errno.h>
#include <time.h>
#include <sys/types.h>
int new_sleep (int amount)
{
        struct timespec Requested, Remaining;
        double famount = amount;
        int rc;
        while (famount > 0.0) {
                Requested.tv_sec = (int) famount;
                Requested.tv_nsec =
                        (int) ((famount - Requested.tv_sec)*1000000000.);
                rc = nanosleep ( &Requested, &Remaining );
                if ((rc == -1) && (errno == EINTR)) {
                        /* Sleep interrupted.  Resume it */
                        famount = Remaining.tv_sec + Remaining.tv_nsec /
                                                        1000000000.;
                        continue;
                }
                else /* Completed sleep.  Set return to zero */
                {
                                return (0);
                }
        }    /* end of while */

        /* famount = 0.; exit */
        return (0);
}

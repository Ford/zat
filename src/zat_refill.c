/* ZAT module: zat_refill.c
 * Description: Refill stream management functions. */

#include "zat_stream.h"

/* zat_refill: implemented in zat_refill.c. */
int zat_refill( zat_stream *zatp ) {
	if (zatp->in) {
		zatp->infl.avail_in = zatp->in(zatp->in_desc, & zatp->infl.next_in );
		return zatp->infl.avail_in;
	} else {
		zatp->infl.avail_in = 0;
		return 0;
	}
}
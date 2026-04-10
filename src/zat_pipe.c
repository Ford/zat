/* ZAT module: zat_pipe.c
 * Description: Stream pipeline utilities. */

#include "zat_tools.h"

int zat_pipe( zat_stream *zatp, size_t length, out_func out, void *out_desc )
{
	while (zatp->infl.avail_in < length) {
		if (out)
			out ( out_desc, zatp->infl.next_in, zatp->infl.avail_in );
		length -= zatp->infl.avail_in;

		if (zat_refill(zatp) == 0)
			return Z_BUF_ERROR;
	}

	if (out)
		out ( out_desc, zatp->infl.next_in, length );

	zatp->infl.next_in	+= length;
	zatp->infl.avail_in	-= length;
	return Z_OK;
}

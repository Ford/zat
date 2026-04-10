/* ZAT module: zat_read.c
 * Description: Buffered input stream read utilities. */

#include <string.h>
#include "zat_tools.h"

int zat_read( zat_stream *zatp, size_t length, Bytef *dat)
{
	while (zatp->infl.avail_in < length) {
		memcpy ( dat, zatp->infl.next_in, zatp->infl.avail_in );
		dat		+= zatp->infl.avail_in;
		length	-= zatp->infl.avail_in;

		if (zat_refill(zatp) == 0)
			return Z_BUF_ERROR;
	}

	memcpy( dat, zatp->infl.next_in, length );
	zatp->infl.next_in	+= length;
	zatp->infl.avail_in	-= length;
	return Z_OK;
}
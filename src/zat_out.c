/* ZAT module: zat_out.c
 * Description: Output buffering and flush logic. */

#include "zat_stream.h"

int zat_out( zat_stream *zatp, Bytef *dat, unsigned length )
{
	return ! zatp->out ? Z_OK :
		zatp->out ( zatp->out_desc, dat, length );
}
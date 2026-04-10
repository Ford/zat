/* ZAT module: zat_outbuf.c
 * Description: Output buffer management. */

#include <stdlib.h>
#include <string.h>
#include "zat_tools.h"

/* zat_outbuf: implemented in zat_outbuf.c. */
int zat_outbuf( zat_stream *zatp, Bytef *dat, unsigned length) {
	if (dat == NULL) return 0;

	while (zatp->defl.avail_out < length ) {
		memcpy ( zatp->defl.next_out, dat, zatp->defl.avail_out);
		dat		+= zatp->defl.avail_out;
		length	-= zatp->defl.avail_out;
		zatp->obuf.size += zatp->defl.avail_out;
		{
			zatbuf_node *temp = malloc(sizeof(zatbuf_node));
			if (temp == NULL) return Z_MEM_ERROR;
			zatp->obuf.tail	= zatbuf_node_push(zatp->obuf.tail, temp);
		}
		zatp->defl.next_out = zatp->obuf.tail->buf;
		zatp->defl.avail_out = ZATBUF_SIZE;
	}
	
	memcpy (zatp->defl.next_out, dat, length);
	zatp->defl.next_out += length;
	zatp->defl.avail_out -= length;
	zatp->obuf.size 	+= length;
	
	return Z_OK;
}
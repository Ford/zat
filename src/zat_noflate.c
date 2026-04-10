/* ZAT module: zat_noflate.c
 * Description: Pass-through support for non-deflate data streams. */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "zat_tools.h"

/* zat_noflate: implemented in zat_noflate.c. */
int zat_noflate( zat_stream *zatp, Bytef *dat, unsigned length) {
	zatbuf_node *temp;
	if (dat == NULL) {
		if (zatp->obuf.tail == NULL) {
			temp = malloc(sizeof(zatbuf_node));
			if (temp == NULL) return Z_MEM_ERROR;
			zatp->obuf.tail = zatbuf_node_push(NULL, temp);

			assert (zatp->defl.total_out == 0); // zero-size data detected
			zatp->defl.total_out = 2;
			zatp->obuf.size = 2;
			zatp->obuf.tail->buf[0] = 0x03;
			zatp->obuf.tail->buf[1] = 0x00;
		} else {
			int leftover = (ZATBUF_SIZE - 5) - zatp->defl.avail_out;
			zatp->obuf.tail->buf[0] = 0x01;
			*(uint16_t *)(zatp->obuf.tail->buf+1) = htole16( leftover	);
			*(uint16_t *)(zatp->obuf.tail->buf+3) = htole16( ~leftover	);
		}
		return Z_OK;
	}
	zatp->defl.total_in	+=  length;
	while (zatp->defl.avail_out < length ) {
		memcpy ( zatp->defl.next_out, dat, zatp->defl.avail_out);

		zatp->defl.total_out	+= zatp->defl.avail_out + 5;

		dat		+= zatp->defl.avail_out;
		length		-= zatp->defl.avail_out;
		zatp->obuf.size += zatp->defl.avail_out;

		temp = malloc(sizeof(zatbuf_node));
		if (temp == NULL) return Z_MEM_ERROR;
		zatp->obuf.tail	= zatbuf_node_push(zatp->obuf.tail, temp);

		zatp->obuf.tail->buf[0] = 0x00;
		zatp->obuf.tail->buf[1] = 0xFF;
		zatp->obuf.tail->buf[2] = 0xFF;
		zatp->obuf.tail->buf[3] = 0x00;
		zatp->obuf.tail->buf[4] = 0x00;
		zatp->obuf.size += 5;
		zatp->defl.next_out = zatp->obuf.tail->buf + 5;
		zatp->defl.avail_out = ZATBUF_SIZE - 5;
	}
	
	memcpy (zatp->defl.next_out, dat, length);
	zatp->defl.next_out += length;
	zatp->defl.avail_out -= length;
	zatp->obuf.size 	+= length;
	zatp->defl.total_out	+= length;

	return Z_OK;
}
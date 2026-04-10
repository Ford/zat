/* ZAT module: zat_deflate.c
 * Description: Dynamic deflate-based recompression implementation. */

#include <stdlib.h>
#include <string.h>
#include "zat_tools.h"

/* zat_deflate: implemented in zat_deflate.c. */
int zat_deflate(zat_stream *zatp,Bytef *dat,unsigned length) {
	zatp->defl.next_in = dat;
	zatp->defl.avail_in = length;

	do {
		unsigned avail_out;
		int status;
		
		if (zatp->defl.avail_out == 0) {
			zatbuf_node *temp = malloc(sizeof(zatbuf_node));
			if (temp == NULL) return Z_MEM_ERROR;
			zatp->obuf.tail	= zatbuf_node_push(zatp->obuf.tail, temp);
			zatp->defl.next_out = zatp->obuf.tail->buf;
			zatp->defl.avail_out = ZATBUF_SIZE;
		}
		avail_out = zatp->defl.avail_out;
		status = deflate(& zatp->defl, dat ? Z_NO_FLUSH : Z_FINISH );
		zatp->obuf.size += avail_out - zatp->defl.avail_out;

		if (status == Z_BUF_ERROR) continue;
		if (status == Z_STREAM_END) break;
		if (status != Z_OK) return status;
	} while (zatp->defl.avail_out == 0 || zatp->defl.avail_in);

	return Z_OK;
}
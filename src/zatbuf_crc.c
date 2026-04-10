/* ZAT module: zatbuf_crc.c
 * Description: Compute CRC32 for a ZAT buffer. */

#include "zatbuf.h"

/* zatbuf_crc: implemented in zatbuf_crc.c. */
uint32_t zatbuf_crc(uint32_t crc, zatbuf *zbuf) {
	zatbuf_node *tail, *curr;

	if (zbuf == NULL) return crc;

	tail = zbuf->tail;
	if ( tail == NULL ) return crc;
	curr	= tail->next;

	for (size_t size = zbuf->size; size; curr = curr->next ) {
		size_t len = size < ZATBUF_SIZE ? size : ZATBUF_SIZE;
		size -= len;

		crc = crc32(crc, curr->buf, len);

		if (curr == tail) break;
	}
	
	return crc;
}
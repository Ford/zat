/* ZAT module: zatbuf_dump.c
 * Description: Debug dump utilities for buffer contents. */

#include <stdlib.h>
#include <assert.h>
#include "zatbuf.h"

static int _dump_( zatbuf_node *tail, size_t size, out_func out, void *out_desc ) {
	zatbuf_node *curr, *next;

	if ( tail == NULL ) return Z_OK;

	curr	= tail;
	next	= curr->next;

	do {
		curr = next;

		if (size) {
			int len = size < ZATBUF_SIZE ? size : ZATBUF_SIZE;
			size -= len;

			if (out) {
				int status = out ( out_desc, curr->buf, len );
				if (status) return status;
			}
		}
		next = curr->next;
		free ( curr );

	} while (curr != tail);
	
	return Z_OK;
}

/* zatbuf_dump: implemented in zatbuf_dump.c. */
int	zatbuf_dump(zatbuf *zbuf, out_func out, void *out_desc) {
	int status;
	assert(zbuf);

	if (zbuf->tail) {
		status = _dump_ ( zbuf->tail, zbuf->size, out, out_desc );
		zbuf->tail	= NULL;
	} else {
		status = out ? out ( out_desc, zbuf->data, zbuf->size ) : Z_OK;
		free(zbuf->data);
	}
	zbuf->data	= NULL;
	zbuf->size  = 0;
	return status;
}
/* ZAT module: zatbuf_out.c
 * Description: Output interface for buffer API. */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "zatbuf.h"
#include <string.h>

/* zatbuf_out: implemented in zatbuf_out.c. */
int	zatbuf_out(zatbuf *zbuf,Bytef *dat,unsigned len) {
	zatbuf_node *curr, *next;
	unsigned size;
	Bytef *membuf, *dest;
	
	if ( dat ) {
		if ( zbuf->data ) return Z_BUF_ERROR;
		while (len) {
			Bytef *next_out;
			unsigned avail_out,out_count;
			unsigned index_buf = zbuf->size % ZATBUF_SIZE;

			if (index_buf == 0) {
				zatbuf_node *temp = malloc(sizeof(zatbuf_node));
				if (temp == NULL) return Z_MEM_ERROR;
				zbuf->tail = zatbuf_node_push(zbuf->tail, temp);
			}
			next_out	= zbuf->tail->buf + index_buf;
			avail_out	= ZATBUF_SIZE - index_buf;
			out_count	= avail_out < len ? avail_out : len;

			memcpy(next_out, dat, out_count);

			dat			+= out_count;
			len			-= out_count;
			zbuf->size	+= out_count;
		}
		return Z_OK;
	}
	if ( zbuf->data || zbuf->tail == NULL )
		return Z_OK;

	curr = zbuf->tail ;
	next = curr->next;

	size = zbuf->size;
	dest = membuf = ( Bytef * )malloc( size );

	if ( membuf == NULL )
		return Z_MEM_ERROR;
	do {
		curr = next;
		if (size) {
			size_t chunk_size = size < ZATBUF_SIZE ? size : ZATBUF_SIZE;
			memcpy( dest, curr->buf, chunk_size );
			size -= chunk_size;
			dest += chunk_size;
		}
		next = curr->next;
		free ( curr );
	} while (curr != zbuf->tail);

	zbuf->data = membuf;
	zbuf->tail = NULL;

	return Z_OK;
}
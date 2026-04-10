/* ZAT module: zat.h
 * Description: The application binary interface for ZAT */

#ifndef zat_h
#define zat_h

#include <stdint.h>
#include "zlib.h"

typedef struct zatbuf {
	Bytef *data;
	size_t size;
	struct zatbuf_node *tail;
} zatbuf;

int zatbuf_out(zatbuf *zbuf,Bytef *dat,unsigned len);
int	zatbuf_dump(zatbuf *zbuf, out_func out, void *out_desc);
uint32_t zatbuf_crc(uint32_t crc, zatbuf *zbuf);

int zat_exec(int level, Bytef *data, unsigned length, in_func in, void *in_desc, out_func out, void *out_desc);

#endif
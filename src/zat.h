/* ZAT module: zat.h
 * Description: The application binary interface for ZAT */

#ifndef zat_h
#define zat_h

#include <stdint.h>

#if defined(GIT_ZLIB)
#include <string.h>
#include "git-zlib.h"

typedef git_zstream z_stream;

#define inflateInit(strm) git_inflate_init(strm)
#define deflateInit(strm, level) git_deflate_init(strm, level)
#define inflate(strm, flush) git_inflate(strm, flush)
#define deflate(strm, flush) git_deflate(strm, flush)
#define inflateEnd(strm) git_inflate_end(strm)
#define deflateEnd(strm) git_deflate_end(strm)
#define deflateBound(strm, len) git_deflate_bound(strm, len)

static inline int git_zlib_inflateInit2(z_stream *strm, int windowBits)
{
	if (windowBits == 15 + 16) {
		git_inflate_init_gzip_only(strm);
		return Z_OK;
	}
	if (windowBits == -MAX_WBITS) {
		memset(strm, 0, sizeof(*strm));
		return inflateInit2(&strm->z, windowBits);
	}
	memset(strm, 0, sizeof(*strm));
	return inflateInit2(&strm->z, windowBits);
}
#define inflateInit2(strm, windowBits) git_zlib_inflateInit2(strm, windowBits)

static inline int git_zlib_deflateInit2(z_stream *strm, int level, int method,
					 int windowBits, int mem_level, int strategy)
{
	if (method == Z_DEFLATED && mem_level == 8 && strategy == 0) {
		if (windowBits == -MAX_WBITS) {
			git_deflate_init_raw(strm, level);
			return Z_OK;
		}
		if (windowBits == 15 + 16) {
			git_deflate_init_gzip(strm, level);
			return Z_OK;
		}
		if (windowBits == MAX_WBITS) {
			git_deflate_init(strm, level);
			return Z_OK;
		}
	}
	memset(strm, 0, sizeof(*strm));
	return deflateInit2(&strm->z, level, method, windowBits, mem_level, strategy);
}
#define deflateInit2(strm, level, method, windowBits, mem_level, strategy) \
	git_zlib_deflateInit2(strm, level, method, windowBits, mem_level, strategy)
#else
#include "zlib.h"
#endif

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
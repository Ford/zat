/* ZAT module: zat_tools.h
 * Description: internal functions implemented for this ZAT module. */

#ifndef zat_tools_h
#define zat_tools_h

#include "zat_stream.h"

int zat_pipe( zat_stream *zatp, size_t length, out_func out, void *out_desc );
int zat_pipinflate( zat_stream *zatp, size_t length, out_func out, void *out_desc );

int zat_read( zat_stream *zatp, size_t length, Bytef *dat );

int zat_outbuf( zat_stream *zatp, Bytef *dat, unsigned length );

int zat_deflate( zat_stream *zatp, Bytef *dat, unsigned length );
int zat_noflate( zat_stream *zatp, Bytef *dat, unsigned length );

int zat_reinflate(zat_stream *zatp, int level);

#if ! defined(htobe16) || ! defined(htobe32) || ! defined(htole16) || ! defined(htole32)
#ifdef __GNUC__
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		#define htobe16(x) __builtin_bswap16(x)
		#define htobe32(x) __builtin_bswap32(x)
		#define be16toh(x) __builtin_bswap16(x)
		#define be32toh(x) __builtin_bswap32(x)

		#define htole16(x) (x)
		#define htole32(x) (x)
		#define le16toh(x) (x)
		#define le32toh(x) (x)
	#else/*__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__*/
		#define htobe16(x) (x)
		#define htobe32(x) (x)
		#define be16toh(x) (x)
		#define be32toh(x) (x)

		#define htole16(x) __builtin_bswap16(x)
		#define htole32(x) __builtin_bswap32(x)
		#define le16toh(x) __builtin_bswap16(x)
		#define le32toh(x) __builtin_bswap32(x)
	#endif
#else
#include <endian.h>
#endif /*__GNUC__ */
#endif

#endif /* zat_tools_h */
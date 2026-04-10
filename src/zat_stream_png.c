/* ZAT module: zat_stream_png.c
 * Description: PNG stream block processing logic. */

#include <assert.h>
#include <string.h>
#include "zat_tools.h"

static int exec_png(zat_stream *zatp, int level);

/* zat_stream_png: implemented in zat_stream_png.c. */
int zat_stream_png(zat_stream *zatp, int level) {

	int status = inflateInit2( & zatp->infl, -MAX_WBITS);
	if (status == Z_OK)
	{
		status = deflateInit2( & zatp->defl, level,
			Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
			
		if (status == Z_OK) {
			status = exec_png( zatp, level );
			deflateEnd( & zatp->defl );
		}
		inflateEnd( & zatp->infl );
	}
	return status;
}

static const int8_t pnglvl[4] = {
	Z_BEST_SPEED,
	Z_BEST_SPEED + 1,
	Z_DEFAULT_COMPRESSION,
	Z_BEST_COMPRESSION,
};

static int exec_png(zat_stream *zatp, int level) {
	Bytef head[8], prefix[6], suffix[4], tail[4];
	uint32_t curr_type = 0;
	out_func deflator = (out_func) ( level ? zat_deflate : zat_noflate );

	zat_pipe ( zatp, 8, (out_func)zat_out, zatp ); // PNG file signature (8 bytes)

	while (! zat_read( zatp, sizeof head, head)) {
		uint32_t size = be32toh(*(uint32_t *)(head+0));
		uint32_t type = be32toh(*(uint32_t *)(head+4));
		uint32_t offset = 0, crc = 0;
		int status;

		if (type == 0x49444154) // "IDAT" chunk type
			offset = 2;		// ZLIB header (2 bytes)
		else
		if (type == 0x66644154) // "fdAT" chunk type
			offset = 6;		// fdAT sequence & ZLIB header (4 + 2 bytes)
		else {
			if (
				zat_out ( zatp, head, sizeof head ) ||
				zat_pipe ( zatp, size + sizeof tail, (out_func) zat_out, zatp )
				)
					return Z_BUF_ERROR;
			continue;
		}
		if ( zatp->infl.total_in == 0 ) {
			zat_read( zatp, offset, prefix);
			if (level == Z_DEFAULT_COMPRESSION) {
				uint8_t option = (prefix[offset - 1] >> 6) & 0x3;	// Compression level
				deflateParams(&zatp->defl, pnglvl[option], 0);
			}
			curr_type = type;
		} else {
			if (curr_type != type) return Z_BUF_ERROR;
		}
		status = zat_pipinflate ( zatp, size - offset - sizeof suffix, deflator, zatp );
		zat_read( zatp, sizeof suffix, suffix);	// Is this ZLIB adler32? (4 bytes)
		zat_read( zatp, sizeof tail, tail );	// Throw out chunk CRC32 (4 bytes)

		if ( status != Z_STREAM_END) {
			deflator ( zatp, suffix, sizeof suffix );	// More ZLIB data, not adler32
			continue;
		}
		deflator (zatp, NULL, 0);	// Flush deflator

		// Update chunk size @ head[0:3]
		size = offset + zatp->defl.total_out + sizeof suffix;
		*(uint32_t *)(head+0) = htobe32( size );

		// Update chunk CRC32 @ tail[0:3]
		crc = crc32( crc, head + 4, (sizeof head) - 4 );
		crc = crc32( crc, prefix, offset );
		crc = zatbuf_crc( crc, & zatp->obuf );
		crc = crc32( crc, suffix, sizeof suffix );
		*(uint32_t *)(tail+0) = htobe32( crc );

		// Emit chunk
		zat_out( zatp, head, sizeof head );
		zat_out( zatp, prefix, offset );
		zatbuf_dump( & zatp->obuf, (out_func) zat_out, zatp );
		zat_out( zatp, suffix, sizeof suffix );
		zat_out( zatp, tail, sizeof tail );

		// Reset recompressor state
		zatp->defl.next_out = NULL;
		zatp->defl.avail_out = 0;
		inflateReset( &zatp->infl );
		deflateReset( &zatp->defl );
	}
	return Z_STREAM_END;
}
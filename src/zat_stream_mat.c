/* ZAT module: zat_stream_mat.c
 * Description: MAT file stream handling and structure parsing. */

#include <assert.h>
#include <string.h>
#include "zat_tools.h"

static int exec_mat(zat_stream *zatp, int level);

/* zat_stream_mat: implemented in zat_stream_mat.c. */
int zat_stream_mat(zat_stream *zatp, int level) {
	int status = inflateInit( & zatp->infl );
	if (status == Z_OK)
	{
		status = deflateInit( & zatp->defl, level );
		if (status == Z_OK) {
			status = exec_mat( zatp, level );
			deflateEnd( & zatp->defl );
		}
		inflateEnd( & zatp->infl );
	}
	return status;
}

static int exec_mat(zat_stream *zatp, int level) {
	uint32_t i_offset = le32toh( *(uint32_t *)(zatp->infl.next_in + 116) );
	uint32_t xpos = 0;
	uint32_t ypos = 0;
	Bytef dp[4] = { 15, 0, 0, 0 };
	Bytef sp[10], *dp1, *dp2;
	unsigned len;

	if (i_offset < 128)
		assert (i_offset == 0);	

	zat_pipe ( zatp, 128, (out_func)zat_outbuf, zatp );
	xpos += 128;
	ypos += 128;

	while (! zat_read( zatp, 8, sp)) {
		uint32_t type = le32toh( *(uint32_t *)( sp ) );
		uint32_t size = le32toh( *(uint32_t *)( sp + 4) );
		xpos = xpos + 8 + size;

		if (level == Z_NO_COMPRESSION) {
			if (type == 15) {
				int infl_status = zat_pipinflate ( zatp, size, (out_func) zat_outbuf, zatp );
				if (infl_status != Z_STREAM_END) return infl_status;
	
				ypos	+= zatp->infl.total_out;
				inflateReset( &zatp->infl );
			} else {
				zat_outbuf( zatp, sp, 8 );
				zat_pipe ( zatp, size, (out_func)zat_outbuf, zatp );
				ypos += 8 + size;
			}
		} else {
			zat_outbuf (zatp, dp, 4);	// Header, type=15
			ypos += 4;

			len = zatp->defl.avail_out;
			dp1 = zatp->defl.next_out;
			{
				zat_outbuf( zatp, dp, 4 );	// Header, size=TBD
				ypos += 4;
			}
			dp2 = zatp->defl.next_out - len;

			if (type == 15) {
				int infl_status = zat_pipinflate ( zatp, size, (out_func) zat_deflate, zatp );
				if (infl_status != Z_STREAM_END) return infl_status;
				inflateReset( &zatp->infl );
			} else {
				zat_deflate( zatp, sp, 8 );
				zat_pipe( zatp, size, (out_func)zat_deflate, zatp );
			}
			zat_deflate (zatp, NULL, 0);
			ypos	+= zatp->defl.total_out;

			*(uint32_t *)sp = htole32( zatp->defl.total_out);
			if (len < 4) {
				memcpy(dp1, sp, len);
				memcpy(dp2, sp+len, 4 - len);
			} else
				memcpy(dp1, sp, 4);

			deflateReset( &zatp->defl );
		}

		if (i_offset == 0)
			;
/* if: implemented in zat_stream_mat.c. */
		else if ( i_offset == xpos ) {
			i_offset = 0;
			*(uint32_t *)(zatp->obuf.tail->next->buf + 116) = htole32( ypos );
		} else
			continue;

		zatbuf_dump( & zatp->obuf, (out_func) zat_out, zatp );
		zatp->defl.next_out = NULL;
		zatp->defl.avail_out = 0;
	}
	
	assert( zatp->infl.avail_in == 0 );
	assert( i_offset == 0 );
	return Z_STREAM_END;
}
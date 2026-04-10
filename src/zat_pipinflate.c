/* ZAT module: zat_pipinflate.c
 * Description: Pipe inflate helper functions. */

#include <assert.h>
#include "zat_tools.h"

#define INFLATE(FLUSH) do {\
	Bytef window[1 << MAX_WBITS];\
	infl->next_out = window;\
	infl->avail_out = sizeof window;\
	switch (status = inflate( infl, FLUSH)) {\
	case Z_NEED_DICT:\
	case Z_DATA_ERROR:\
	case Z_MEM_ERROR:\
		return status;\
	case Z_STREAM_ERROR:\
		assert(0);\
	}\
	if (out) out(out_desc, window, (sizeof window) - infl->avail_out);\
} while (infl->avail_in || infl->avail_out == 0)

int zat_pipinflate(zat_stream *zatp, size_t length, out_func out, void *out_desc )
{
	z_stream *infl = & zatp->infl;
	int status;
	
	while (infl->avail_in < length) {
		length -= infl->avail_in;
		INFLATE( Z_NO_FLUSH );
		if (zat_refill(zatp) == 0)
			return Z_BUF_ERROR;
	}
	{
		uint32_t leftover = infl->avail_in - length;
		infl->avail_in = length;
		INFLATE( Z_FINISH );
		infl->avail_in = leftover;
	}
	return status;
}
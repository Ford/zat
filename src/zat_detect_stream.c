/* ZAT module: zat_detect_stream.c
 * Description: Stream format detection logic. */

#include <string.h>
#include "zat_stream.h"

zat_stream_fn
zat_detect_stream ( Bytef *dat, size_t length ) {
	if (length >= 10 &&
		! memcmp("PK\3\4",dat,4)
		&& !(dat[4] > 20) && !dat[5]	//	Version 2.0 or lower
		&& !(dat[6]& 0x61)				//		with no encryption or patched data
		&& !(dat[8]&~0x08)&& !dat[9])	//	Method 0x0000 (STORE) or 0x0008 (DEFLATE) only
	{
		return zat_stream_zip;
	}
	if (length >= 128 &&
		! memcmp("MATLAB 5.0 MAT-file", dat, 19) &&
		! memcmp("\0\0\0\0\0\1IM", dat + 120, 8))
	{
		return zat_stream_mat;
	}	
	
	if (length >= 8 &&
		! memcmp("\x89PNG\r\n\x1A\n", dat, 8))
	{
		return zat_stream_png;
	}
	return NULL;
}
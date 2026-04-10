/* ZAT module: zat_stream.h
 * Description: The ZAT stream data structure. */

#ifndef zat_stream_h
#define zat_stream_h

#include "zatbuf.h"

typedef struct zat_stream {
	void	*in_desc;	in_func 	in;
	void	*out_desc;	out_func	out;
	z_stream infl;
	z_stream defl;
	zatbuf ibuf;
	zatbuf obuf;
} zat_stream;

int zat_out( zat_stream *zatp, Bytef *dat, unsigned length );
int zat_refill( zat_stream *zatp );

typedef int (*zat_stream_fn)(zat_stream *zatp, int level);
zat_stream_fn zat_detect_stream( Bytef *dat, size_t length );

int zat_stream_zip(zat_stream *zatp, int level);
int zat_stream_mat(zat_stream *zatp, int level);
int zat_stream_png(zat_stream *zatp, int level);


#endif
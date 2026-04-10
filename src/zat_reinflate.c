/* ZAT module: zat_reinflate.c
 * Description: Re-inflation and output rewriting operations. */

#include <stdlib.h>
#include "zat_tools.h"

static unsigned _in_(zat_stream *zatp, Bytef **bufP);

static int	_out_deflate_	( zat_stream *zatp, Bytef *dat, unsigned length );
static int	_out_noflate_	( zat_stream *zatp, Bytef *dat, unsigned length );

/* zat_reinflate: implemented in zat_reinflate.c. */
int zat_reinflate(zat_stream *zatp, int level) {
	int status;
	out_func deflator = (out_func) ( level ? _out_deflate_ : _out_noflate_ );

	zatp->infl.adler = 0;	// reset our CRC-32

	zatp->infl.total_in = zatp->infl.avail_in;
	status = inflateBack( &zatp->infl, 
		( in_func  ) _in_,		zatp,
		( out_func ) deflator,	zatp );
	zatp->infl.total_in	-=	zatp->infl.avail_in;

	deflator = (out_func) ( level ? zat_deflate : zat_noflate );

	if (zatp->ibuf.size) {
		zatbuf_out( & zatp->ibuf, NULL, 0); // flush
		zat_exec (	Z_NO_COMPRESSION,
					zatp->ibuf.data,zatp->ibuf.size,
					NULL,0,	deflator,(void *)zatp);
		free (zatp->ibuf.data);
		zatp->ibuf.data = NULL;
		zatp->ibuf.size = 0;
	}
	deflator( zatp, NULL, 0); // flush

	return status;
}

unsigned _in_(zat_stream *zatp, Bytef **bufP)
{
	unsigned avail_in = zatp->in ? zatp->in(zatp->in_desc, bufP ) : 0;
	zatp->infl.total_in += avail_in;
	return avail_in;
}

static int _out_deflate_(zat_stream *zatp,Bytef *dat,unsigned length) {
	zat_stream_fn streamer;
	zatp->infl.adler = crc32( zatp->infl.adler, dat, length ); // compute our CRC-32
	if (zatp->ibuf.size || 
		(zatp->defl.total_out &&
		 (streamer = zat_detect_stream (dat, length)) && streamer != zat_stream_zip))
	{ 
		return zatbuf_out( & zatp->ibuf, dat, length );
	} else
		return zat_deflate(zatp, dat, length);
}

static int _out_noflate_(zat_stream *zatp,Bytef *dat,unsigned length) {
	zat_stream_fn streamer;
	zatp->infl.adler = crc32( zatp->infl.adler, dat, length ); // compute our CRC-32
	if (zatp->ibuf.size || 
		(zatp->defl.total_out &&
		 (streamer = zat_detect_stream (dat, length)) && streamer != zat_stream_zip))
	{ 
		return zatbuf_out( & zatp->ibuf, dat, length );
	} else
		return zat_noflate(zatp, dat, length);
}
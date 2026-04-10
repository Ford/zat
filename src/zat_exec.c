/* ZAT module: zat_exec.c
 * Description: Core execution engine wiring data flows and options. */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "zat_stream.h"

/* zat_exec: implemented in zat_exec.c. */
int zat_exec(int level, Bytef *data, unsigned length, in_func in, void *in_desc, out_func out, void *out_desc) {
	int status;
	zat_stream zat = { in_desc, in, out_desc, out, };
	zat_stream_fn streamer;

	if (data) {
		streamer = zat_detect_stream( data, length );
		if (!streamer) return Z_STREAM_ERROR;
	} else {
		if (!in) return Z_STREAM_ERROR;
		length = in(in_desc, & data);
		streamer = zat_detect_stream( data, length );
		if (!streamer) {
			while (length) {
				int status = out(out_desc, data, length );
				if (status) return status;
				length = in(in_desc, & data);
			}
			out( out_desc, NULL, 0 ); // flush
			return Z_STREAM_END;
		}
	}

	zat.infl.next_in = data;
	zat.infl.avail_in = length;

	status = streamer( &zat, level );
	assert (status != Z_STREAM_ERROR); // would conflict with !streamer and !in checks

	if (status == Z_STREAM_END && out)
		out( out_desc, NULL, 0 ); // flush

	return status;
}
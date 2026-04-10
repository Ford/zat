/* ZAT module: zat_stream_zip.c
 * Description: ZIP stream handling and entry processing. */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "zat_tools.h"

static int exec_zip(zat_stream *zatp, int level);

/* zat_stream_zip: implemented in zat_stream_zip.c. */
int zat_stream_zip(zat_stream *zatp, int level) {
	Bytef window[1 << MAX_WBITS];
	int status;

	status = inflateBackInit( & zatp->infl, MAX_WBITS, window);
	if (status == Z_OK)
	{
		status = deflateInit2( & zatp->defl, level, 8, -MAX_WBITS, 8, 0);
		//	NOTE: window size set to -MAX_WBITS, so that we can compute/handle CRC-32 ourself
		if (status == Z_OK) {
			status = exec_zip( zatp, level );
			deflateEnd( & zatp->defl );
		}
		inflateBackEnd( & zatp->infl );
	}
	return status;
}

static const int8_t ziplvl[4] = {
	Z_DEFAULT_COMPRESSION,
	Z_BEST_COMPRESSION,
	Z_BEST_SPEED + 1,
	Z_BEST_SPEED,
};

#define zipFILE	htole32(0x04034b50)
#define zipTAIL	htole32(0x08074b50)
#define zipCDIR	htole32(0x02014b50)
#define zipEOCD	htole32(0x06054b50)

typedef struct zip_entry {
	struct zip_entry *prev;
	uint32_t xset,yset;
	uint32_t icrc,xlen,ilen;
} ZIP_ENTRY;

static uint32_t _grab_uint32_( zat_stream *zatp );

static int _compar_(const void *a,const void *b);

static int exec_zip(zat_stream *zatp, int level) {
	ZIP_ENTRY *file, **filep, **files, *file_list = NULL;
	int file_count = 0;
	Bytef sp[46];
	uint16_t n,m,k;

	uint32_t ypos = 0;
	uint32_t xpos = 0;
	uint32_t CDIR_xset,CDIR_yset,EOCD_xset,CDIR_size;

	uint32_t code = _grab_uint32_( zatp );

	while ( code == zipFILE ) {
		uint16_t version,flags,method;
		uint32_t icrc,xlen,ilen;
		
		file = (ZIP_ENTRY *) alloca( sizeof *file );
		file->xset	= xpos;
		file->yset	= ypos;
		file->prev	= file_list;
		file_list	= file;
		file_count++;

		*(uint32_t *)sp = zipFILE;
		if ( zat_read ( zatp, 26, sp + 4 )) return Z_BUF_ERROR;

		version = le16toh( *(uint16_t *)(sp + 4) );
		flags	= le16toh( *(uint16_t *)(sp + 6) );
		method  = le16toh( *(uint16_t *)(sp + 8) );
		
		n	= le16toh( *(uint16_t *)(sp + 26) );
		m	= le16toh( *(uint16_t *)(sp + 28) );

		xpos += n+m + 30;
		ypos += n+m + 30;

		if ((flags & 0x61) == 0x00			//	No encryption or patched data allowed
			&& (version <= 20));			//	Version 2.0 or lower
		else
			return Z_DATA_ERROR;

		icrc	= le32toh( *(uint32_t *)(sp + 14) );	// ZIP file CRC-32
		xlen	= le32toh( *(uint32_t *)(sp + 18) ); // Stored length
		ilen	= le32toh( *(uint32_t *)(sp + 22) );	// Actual length

		if ( method == 0x00 ) {	//	STORE only (no compression)

			if (xlen != ilen)	return Z_DATA_ERROR;
			if ( zat_out ( zatp, sp, 30 ) ||
				zat_pipe ( zatp, n+m+xlen, (out_func) zat_out, zatp ) )	return Z_BUF_ERROR;

			xpos += ilen;
			ypos += xlen;
			
			file->icrc	= icrc;
			file->xlen	= xlen;
			file->ilen	= ilen;
			
			code = _grab_uint32_( zatp);	
		}
/* if: implemented in zat_stream_zip.c. */
		else if ( method == 0x08 ) {	// use DEFLATE compression

			zatbuf mbuf = {};
			zat_pipe ( zatp, n+m, (out_func) zatbuf_out, & mbuf );

			if ((flags & 0x08) == 0 && xlen == 0) {
				fprintf(stderr,"WARNING: zat_stream_zip: zero-size deflated data at offset 0x%08x.\n",xpos);
			} else {
				if (level == Z_DEFAULT_COMPRESSION) {
					uint8_t option = (flags >> 1) & 0x3;		// Deflate options
					deflateParams(&zatp->defl, ziplvl[option], 0);
				}
				else {
					deflateParams(&zatp->defl, level, 0);
				}
				{
					int infl_status = zat_reinflate ( zatp, level );
					if (infl_status != Z_STREAM_END) return infl_status;
				}
				*(uint16_t *)(sp +  6) = htole16(	flags &~ 0x08	);	// do not create zipTAIL block
				*(uint32_t *)(sp + 14) = htole32(	file->icrc = zatp->infl.adler		);	// our CRC-32
				*(uint32_t *)(sp + 18) = htole32(	file->xlen = zatp->defl.total_out	);
				*(uint32_t *)(sp + 22) = htole32(	file->ilen = zatp->defl.total_in	);

				xpos	+= zatp->infl.total_in;
				ypos	+= zatp->defl.total_out;

				deflateReset( &zatp->defl );
			}

			if ( zat_out ( zatp, sp, 30 ) )	return Z_BUF_ERROR;

			zatbuf_dump( & mbuf,		(out_func) zat_out, zatp );
			zatbuf_dump( & zatp->obuf,	(out_func) zat_out, zatp );
			zatp->defl.next_out = NULL;
			zatp->defl.avail_out = 0;

			code = _grab_uint32_( zatp );

			if (flags & 0x08) {
				if (code == zipTAIL) {
					zat_read( zatp, 12, sp );
					xpos += 16;
				}
				else {
					zat_read( zatp, 8, sp+4 );
					xpos += 12;
				}
				code = _grab_uint32_( zatp );
			}
		}
		else
			return Z_DATA_ERROR;	// Method STORE (0x00) or DEFLATE (0x08) only	
	}
	xpos += 4;

	files = alloca(file_count * sizeof (ZIP_ENTRY *));

	for (int i = file_count; file_list ; file_list = file_list->prev) {
		files[--i] = file_list;
	}
	
	CDIR_xset	= xpos - 4;
	CDIR_yset	= ypos;
	
	while ( code == zipCDIR ) {
		uintptr_t file_at;
		
		if ( zat_read( zatp, 42, sp + 4)) return Z_BUF_ERROR;
		xpos += 42;
		*(uint32_t *)sp = zipCDIR;

		n = le16toh( *(uint16_t *)(sp + 28) );
		m = le16toh( *(uint16_t *)(sp + 30) );
		k = le16toh( *(uint16_t *)(sp + 32) );
		file_at = le32toh( *(uint32_t *)(sp + 42) );

		filep = (ZIP_ENTRY **)
			bsearch( (void *)file_at, files, file_count, sizeof *files, _compar_ );
		if (filep == NULL)	return Z_DATA_ERROR;
		assert( (file = *filep) );
	
		*(uint32_t *)(sp + 16) = htole32( file->icrc );
		*(uint32_t *)(sp + 20) = htole32( file->xlen );
		*(uint32_t *)(sp + 24) = htole32( file->ilen );
		*(uint32_t *)(sp + 42) = htole32( file->yset );	// update offset

		if ( zat_out ( zatp, sp, 46 ) ||
			 zat_pipe ( zatp, n+m+k, (out_func) zat_out, zatp ) )	return Z_BUF_ERROR;

		xpos += n+m+k;
		ypos += n+m+k + 46;

		code = _grab_uint32_(zatp);
		xpos += 4;
	}

	EOCD_xset = xpos - 4;
	CDIR_size = EOCD_xset - CDIR_xset;

	if (zipEOCD == code) {
		uint32_t length, xset;

		if ( zat_read( zatp, 18, sp + 4)) return Z_BUF_ERROR;
		xpos += 18;
		*(uint32_t *)sp = zipEOCD;

		length	= le32toh( *(uint32_t *)(sp + 12) );
		xset	= le32toh( *(uint32_t *)(sp + 16) );
		n	= le16toh( *(uint16_t *)(sp + 20) );

		if (CDIR_size == length	&&
			CDIR_xset == xset); else return 0;

		*(uint32_t *)(sp + 16) = htole32( CDIR_yset );	// update offset

		if ( zat_out ( zatp, sp, 22 ) ||
			 zat_pipe ( zatp, n, (out_func) zat_out, zatp ) )	return Z_BUF_ERROR;

		xpos += n;
		ypos += n + 22;

		return Z_STREAM_END;
	}
	return code ? Z_DATA_ERROR : Z_BUF_ERROR ;
}

static uint32_t _grab_uint32_( zat_stream *zatp )
{
	unsigned char dat[4];
	zat_read( zatp, 4, dat);
	return le32toh( *(uint32_t *)(dat) );
}

static int _compar_(const void *a,const void *b) {
	int32_t A = (int32_t )(uintptr_t )a;
	int32_t B = (**(const ZIP_ENTRY **)b).xset;
	return A - B;
}
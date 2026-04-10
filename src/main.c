/*
 * zat - ZLIB-based data recompression filter
 *
 * This program applies ZLIB recompression to remove compression from
 * .zip, .mat, and .png files, optimizing them for Git storage.
 *
 * Usage:
 *   zat [options] < input > output
 *   zat --file input output
 *
 * Options:
 *   -[0-9]    Set compression level (0-9)
 *   --version Display version
 *   --file    Process files instead of stdin/stdout
 *
 * ----------------------------------------------------------------------
 * Written by Hasdi R Hashim <hhashim@ford.com>
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "zat.h"

/* Cross-platform binary mode handling for Windows/Cygwin */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define BUFFER_SIZE (1 << MAX_WBITS)  /* 32KB buffer for I/O operations */

/*
 * read_stdin - Read data from stdin into a buffer
 * @ptr: Pointer to buffer (unused, buffer is static)
 * @bufP: Output pointer to buffer containing data
 * Returns: Number of bytes read
 */
unsigned read_stdin(void *ptr, Bytef **bufP) {
	*bufP = (Bytef *)ptr;
	return fread(ptr, 1, BUFFER_SIZE, stdin);
}

/*
 * read_FILE - Read data from a FILE stream into a static buffer
 * @ptr: FILE pointer to read from
 * @bufP: Output pointer to static buffer containing data
 * Returns: Number of bytes read
 */
unsigned read_FILE(void *ptr, Bytef **bufP) {
	static unsigned char buf[BUFFER_SIZE];
	*bufP = buf;
	return fread(buf, 1, BUFFER_SIZE, (FILE *)ptr);
}

/*
 * write_FILE - Write data to a FILE stream
 * @ptr: FILE pointer to write to
 * @dat: Data buffer to write (NULL indicates end of stream)
 * @length: Number of bytes to write
 * Returns: Z_OK on success, Z_BUF_ERROR on failure
 */
int write_FILE(void *ptr, Bytef *dat, unsigned length) {
	if (dat == NULL) return Z_OK;
	return fwrite(dat, 1, length, (FILE *)ptr) == length ? Z_OK : Z_BUF_ERROR ;
}

/*
 * zerr - Print zlib error messages to stderr
 * @ret: Zlib return code
 */
void zerr(int ret)
{
	fputs("zat: ", stderr);
	switch (ret) {
	case Z_ERRNO:
		if (ferror(stdin))
			fputs("error reading stdin\n", stderr);
		if (ferror(stdout))
			fputs("error writing stdout\n", stderr);
		break;
	case Z_STREAM_ERROR:
		fputs("invalid compression level\n", stderr);
		break;
	case Z_NEED_DICT:
		fputs("needed dictionary not available\n", stderr);
		break;
	case Z_DATA_ERROR:
		fputs("invalid or incomplete deflate data\n", stderr);
		break;
	case Z_MEM_ERROR:
		fputs("out of memory\n", stderr);
		break;
	case Z_VERSION_ERROR:
		fputs("zlib version mismatch!\n", stderr);
		break;
	default:
		fprintf(stderr,"return code %i\n", ret);
	}
}

/*
 * main - Entry point for zat program
 * Processes command-line arguments and performs compression/decompression
 * @argc: Number of command-line arguments
 * @argv: Array of command-line argument strings
 * Returns: EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc, char **argv)
{
	int level = Z_DEFAULT_COMPRESSION;  /* Default compression level */
	int status = EXIT_FAILURE;          /* Default exit status */

	/* Ensure binary mode for stdin/stdout to avoid CRLF conversions */
	SET_BINARY_MODE(stdin);
	SET_BINARY_MODE(stdout);

	/* Parse command-line arguments */
	for(int i = 1; i < argc; ++i)
	{
		/* Check for compression level option: -[0-9] */
		if (argv[i][0] == '-'
			&& isdigit((int)argv[i][1])
			&& argv[i][2] == 0)
		{
			level = (argv[i][1]) - '0';
			continue;
		}
		/* Version information */
		else if (! strcmp(argv[i],"--version"))
		{
			fputs("zat-v1.00",stdout);
			status = EXIT_SUCCESS;
		}
		/* File processing mode: --file input output */
		else if (! strcmp(argv[i],"--file") && (argc - i) == 3) {
			FILE *src = fopen(argv[i+1],"rb");
			assert(src);  /* Assert file opened successfully */
			FILE *dst = fopen(argv[i+2],"wb");
			assert(dst);  /* Assert file opened successfully */

			/* Execute zat on files */
			int ret = zat_exec(level, NULL,0, read_FILE,src, write_FILE,dst);
			fflush(dst);
			if (ret != Z_OK)
			{
				zerr(ret);
				return EXIT_FAILURE;
			}
			return EXIT_SUCCESS;
		}
		/* Unknown argument */
		else {
			fprintf(stderr,"zat: '%s' is an unknown command.\n",argv[i]);
		}
		return status;
	}

	/* Default mode: process stdin to stdout */
	unsigned char buf[BUFFER_SIZE];
	int ret = zat_exec(level, NULL,0, read_stdin,buf, write_FILE,stdout);
	fflush(stdout);
	if (ret != Z_STREAM_END)
	{
		zerr(ret);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

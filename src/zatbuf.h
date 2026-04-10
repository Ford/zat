/* ZAT module: zatbuf.h
 * Description: Buffer data structure as a list of 64 kB blocks. */

#ifndef zbuf_h
#define zbuf_h

#include "zat.h"

typedef struct zatbuf_node zatbuf_node;

#define ZATBUF_SIZE (65535+5)
struct zatbuf_node {
	zatbuf_node	*next;
	Bytef	buf[ZATBUF_SIZE];
};

zatbuf_node *zatbuf_node_push(zatbuf_node *tail,zatbuf_node *node);

#endif
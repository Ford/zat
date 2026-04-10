/* ZAT module: zatbuf_node_push.c
 * Description: Linked list node operations for buffer structures. */

#include <assert.h>
#include "zatbuf.h"

zatbuf_node *
zatbuf_node_push(zatbuf_node *tail,zatbuf_node *node) {
	assert(node);
	if (tail == NULL)
		node->next = node;
	else {
		node->next = tail->next;
		tail->next = node;
	}
	return node;
}
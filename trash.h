#ifndef TRASH_H
#define TRASH_H

#include <stddef.h>

typedef struct header {
	unsigned int size_of_block;
	struct header* next_block;
} head;

void trash_init_and_find_stack_bottom(void);
void* trash_malloc(size_t size_of_malloc);
void trash_collection(void);

#endif

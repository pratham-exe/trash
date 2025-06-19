#include "trash.h"
#include <stdint.h>
#include <stdio.h>

int is_marked_live(void* ptr)
{
	if (!ptr)
		return 0;
	head* block = (head*)ptr - 1;
	return ((uintptr_t)(block->next_block) & 1);
}

void dump_liveness(const char* label, void* ptr)
{
	printf("%s -> %s\n", label, is_marked_live(ptr) ? "LIVE" : "DEAD");
}

int main()
{
	trash_init_and_find_stack_bottom();

	int* a = (int*)trash_malloc(sizeof(int));
	int* arr = (int*)trash_malloc(5 * sizeof(int));
	for (int i = 0; i < 5; ++i) {
		arr[i] = i * 10;
	}

	*a = 42;

	trash_mark_block_live();
	dump_liveness("a", a);
	dump_liveness("arr", arr);
	trash_collection();

	a = NULL;
	printf("\na is NULL\n\n");

	trash_mark_block_live();
	dump_liveness("a", a);
	dump_liveness("arr", arr);
	trash_collection();

	arr = NULL;
	printf("\narr is NULL\n\n");

	dump_liveness("a", a);
	dump_liveness("arr", arr);
	trash_collection();

	return 0;
}

#include "trash.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#define MAX_ALLOWED_UNITS 4096
#define CLEAR_LSB_BITS(pointer) (((uintptr_t)(pointer)) & 0xfffffffc)

static head start;
static head* free_block = &start;
static head* used_block;
unsigned long stack_bottom_address;

static void add_block_to_free_block_list(head* add_block)
{
	head* temp;
	for (temp = free_block; !(add_block > temp && add_block < temp->next_block);
		temp = temp->next_block) {
		if (temp >= temp->next_block && (add_block > temp || add_block < temp->next_block)) {
			break;
		}
	}

	if (add_block + add_block->size_of_block == temp->next_block) {
		add_block->size_of_block += temp->next_block->size_of_block;
		add_block->next_block = temp->next_block->next_block;
	} else {
		add_block->next_block = temp->next_block;
	}

	if (temp + temp->size_of_block == add_block) {
		temp->size_of_block += add_block->size_of_block;
		temp->next_block = add_block->next_block;
	} else {
		temp->next_block = add_block;
	}

	free_block = temp;
}

static head* request_more_memory_from_kernel(size_t number_of_units)
{
	void* sbrk_return;
	head* new_block_from_memory;

	if (number_of_units > MAX_ALLOWED_UNITS) {
		number_of_units = MAX_ALLOWED_UNITS / sizeof(head);
	}

	if ((sbrk_return = sbrk(number_of_units * sizeof(head))) == (void*)-1) {
		return NULL;
	}

	new_block_from_memory = (head*)sbrk_return;
	new_block_from_memory->size_of_block = number_of_units;
	add_block_to_free_block_list(new_block_from_memory);
	return free_block;
}

void* trash_malloc(size_t size_of_malloc)
{
	size_t number_of_units;
	head *temp, *prev;

	number_of_units = ((size_of_malloc + sizeof(head) - 1) / sizeof(head)) + 1;
	prev = free_block;

	for (temp = prev->next_block;; prev = temp, temp = temp->next_block) {
		if (temp->size_of_block >= number_of_units) {
			if (temp->size_of_block == number_of_units) {
				prev->next_block = temp->next_block;
			} else {
				temp->size_of_block -= number_of_units;
				temp += temp->size_of_block;
				temp->size_of_block = number_of_units;
			}

			free_block = prev;
			if (used_block == NULL) {
				temp->next_block = temp;
				used_block = temp;
			} else {
				temp->next_block = used_block->next_block;
				used_block->next_block = temp;
			}

			return (void*)temp + 1;
		}
		if (temp == free_block) {
			temp = request_more_memory_from_kernel(number_of_units);
			if (temp == NULL) {
				return NULL;
			}
		}
	}
}

static void scan_region_and_mark(uintptr_t* start_p, uintptr_t* end_p)
{
	head* used_temp;
	for (; start_p < end_p; start_p++) {
		uintptr_t* stack_temp = start_p;
		used_temp = used_block;

		do {
			if (((uintptr_t*)(used_temp + 1) <= stack_temp)
				&& (uintptr_t*)(used_temp + 1 + used_temp->size_of_block) > stack_temp) {
				used_temp->next_block = (head*)(((uintptr_t)used_temp->next_block) | 1);
				break;
			}
		} while ((used_temp = (head*)(CLEAR_LSB_BITS(used_temp->next_block))) != used_block);
	}
}

static void scan_heap_region_and_mark(void)
{
	head *used_temp, *used_t;
	uintptr_t* payload;
	for (used_temp = (head*)(CLEAR_LSB_BITS(used_block->next_block)); used_temp != used_block;
		used_temp = (head*)(CLEAR_LSB_BITS(used_temp->next_block))) {
		if (!((uintptr_t)(used_temp->next_block) & 1)) {
			continue;
		}
		for (payload = (uintptr_t*)(used_temp + 1);
			payload < (uintptr_t*)(used_temp + 1 + used_temp->size_of_block); payload++) {
			uintptr_t* payload_temp = payload;
			used_t = (head*)(CLEAR_LSB_BITS(used_temp->next_block));
			do {
				if ((used_t != used_temp) && ((uintptr_t*)(used_t + 1) <= payload_temp)
					&& (uintptr_t*)(used_t + 1 + used_t->size_of_block) > payload_temp) {
					used_t->next_block = (head*)(((uintptr_t)used_t->next_block) | 1);
					break;
				}
			} while ((used_t = (head*)(CLEAR_LSB_BITS(used_t->next_block))) != used_temp);
		}
	}
}

void trash_init_and_find_stack_bottom(void)
{
	static int started;
	FILE* fp;

	if (started) {
		return;
	}

	started = 1;

	fp = fopen("/proc/self/stat", "r");
	assert(fp != NULL);

	fscanf(fp,
		"%*d %*s %*c %*d %*d %*d %*d %*d %*u"
		"%*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld"
		"%*ld %*ld %*ld %*ld %*llu %*lu %*ld"
		"%*lu %*lu %*lu %lu",
		&stack_bottom_address);
	fclose(fp);

	used_block = NULL;
	start.next_block = &start;
	free_block = &start;
	start.size_of_block = 0;
}

void trash_collection(void)
{
	unsigned int stack_top_address;
	extern char etext, end;
	head *prev, *curr, *free_block_to_collect;

	if (used_block == NULL) {
		return;
	}

	uintptr_t end_of_text_segment = (uintptr_t)&etext;
	uintptr_t end_of_bss = (uintptr_t)&end;
	scan_region_and_mark(&end_of_text_segment, &end_of_bss);

	asm("movl %%ebp, %0" : "=r"(stack_top_address));
	scan_region_and_mark((uintptr_t*)&stack_top_address, &stack_bottom_address);

	scan_heap_region_and_mark();

	for (prev = used_block, curr = (head*)(CLEAR_LSB_BITS(used_block->next_block));;
		prev = curr, curr = (head*)(CLEAR_LSB_BITS(curr->next_block))) {
	label_for_collection:
		if (!((uintptr_t)(curr->next_block) & 1)) {
			free_block_to_collect = curr;
			curr = (head*)(CLEAR_LSB_BITS(curr->next_block));
			add_block_to_free_block_list(free_block_to_collect);

			if (used_block == free_block_to_collect) {
				used_block = NULL;
				return;
			}

			prev->next_block = (head*)((uintptr_t)curr | ((uintptr_t)curr->next_block & 1));
			goto label_for_collection;
		}
		curr->next_block = (head*)(((uintptr_t)curr->next_block) & 0xfffffffe);
		if (curr == used_block) {
			break;
		}
	}
}

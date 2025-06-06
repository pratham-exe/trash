#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#define MAX_ALLOWED_UNITS 4096
#define CLEAR_LSB_BITS(pointer) (((uintptr_t)(pointer)) & 0xfffffffc)

typedef struct header {
	unsigned int size_of_block;
	struct header* next_block;
} head;

static head start;
static head* free_block = &start;
static head* used_block;

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

static void scan_stack_region_and_mark(uintptr_t* start_p, uintptr_t* end_p)
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

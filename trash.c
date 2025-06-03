#include <stddef.h>
#include <unistd.h>
#define MAX_ALLOWED_UNITS 4096

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

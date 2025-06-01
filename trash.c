typedef struct header {
	unsigned int size_of_block;
	struct header* next_block;
} head;

static head start;
static head* free_block = &start;
static head* used_block;

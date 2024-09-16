#include "ek.h"

#if EK_USE_VEC
#	include <stdlib.h>
#endif
#if EK_USE_PACKET
#	include <math.h>
#endif

#include <limits.h>

#if EK_USE_STDLIB_MALLOC

#ifndef NDEBUG
static size_t mem_stdlib_bytes;
#endif

static void *mem_realloc_default(void *usr, void *blk, size_t newsize) {
#ifndef NDEBUG
	size_t *mem = (size_t *)blk - 1;
	if (!blk) {
		mem_stdlib_bytes += newsize;
		mem = malloc(newsize + sizeof(size_t));
		*mem = newsize;
		return mem + 1;
	} else if (!newsize) {
		mem_stdlib_bytes -= *mem;
		free(mem);
		return NULL;
	} else {
		mem_stdlib_bytes -= *mem;
		mem = realloc(mem, newsize + sizeof(size_t));
		if (!mem) return NULL;
		mem_stdlib_bytes += newsize;
		*mem = newsize;
		return mem + 1;
	}
#else
	return realloc(blk, newsize);
#endif
}
mem_alloc_t mem_stdlib_alloc(void) {
	static mem_alloc_fn *const fn = mem_realloc_default;
	return &fn;
}
size_t mem_stdlib_allocated_bytes(void) {
	return mem_stdlib_bytes;
}
#endif

//
// EK_USE_STRVIEW
//
#if EK_USE_STRVIEW
size_t strview_buf_str(char *str, size_t len, const strview_t *const view) {
	len -= 1;
	if (view->len < len) len = view->len;
	memcpy(str, view->str, len);
	str[len] = '\0';
	return len;
}

const char *strview_as_str(const strview_t *const view) {
	static char str[STRVIEW_TO_STR_TMP_MAX];
	strview_buf_str(str, STRVIEW_TO_STR_TMP_MAX, view);
	return str;
}
#endif

//
// EK_USE_LOG
//
#if EK_USE_LOG
log_fn *global_log;
#endif

//
// EK_USE_VEC
//
#if EK_USE_VEC

#define vec_size(elems, capacity) (sizeof(vec_t) * (capacity) * (elems))
#define vec_from_data(data) ((vec_t *)((uintptr_t)data - offsetof(vec_t, data)))

void *vec_init(mem_alloc_t alloc, size_t elem_size, size_t capacity) {
	vec_t *vec = mem_alloc(alloc, NULL, vec_size(capacity, elem_size));
	vec->alloc = alloc;
	vec->len = 0;
	vec->capacity = capacity;
	return vec->data;
}
void *vec_deinit(void *data) {
	vec_t *vec = vec_from_data(data);
	return mem_alloc(vec->alloc, vec, 0);
}
void *_vec_push(void *data, size_t elem_size, size_t nelems, const void *elems) {
	vec_t *vec = vec_from_data(data);
	if (vec->len + nelems > vec->capacity) {
		vec->capacity *= 2;
		vec = mem_alloc(vec->alloc, vec, vec_size(elem_size, vec->capacity));
		if (!vec) return NULL;
	}

	if (elems) {
		memcpy(vec->data + vec->len * elem_size,
			elems, nelems * elem_size);
	}
	vec->len += nelems;
	return vec->data;
}
void _vec_pop(void *data, size_t elem_size, size_t nelems, void *elems) {
	vec_t *vec = vec_from_data(data);
	vec->len -= nelems;
	if (!elems) return;
	memcpy(elems, vec->data + vec->len * elem_size, nelems * elem_size);
}
#endif

//
// EK_USE_TEST
//
#if EK_USE_TEST
static const char *test_err_file;
static int test_err_line;
static bool test_was_bad;

#define COLOR(str) "\x1b[" str "m"

bool test_bad(const char *file, int line) {
	test_err_file = file;
	test_err_line = line;
	test_was_bad = true;
	return false;
}
bool tests_run_foreach(bool (*setup_test)(const test_t *test),
		const test_t *test_list, size_t list_len, FILE *out) {
	int passed = 0, tests = 0;

	for (int i = 0; i < list_len; i++) {
		test_was_bad = false;

		if (!test_list[i].name && !test_list[i].pfn) {
			fprintf(out, "\n");
			continue;
        	}

		tests++;
        	fprintf(out, "%-48s ", test_list[i].name);
		if (setup_test && !setup_test(test_list + i)) {
			fprintf(out, COLOR("41;1") "<SETUP FAIL>" COLOR("0") "\n");
			continue;
		}
        	if (test_list[i].pfn() && !test_was_bad) {
			fprintf(out, " " COLOR("42;1") "PASS" COLOR("0") "\n");
			passed++;
        	} else {
			fprintf(out, COLOR("41;1") "<FAIL");
			if (test_was_bad) {
				fprintf(out, " AT %s:%d", test_err_file, test_err_line);
			}
			fprintf(out, ">" COLOR("0") "\n");
        	}
	}

	fprintf(out, "\n%d/%d tests passing (%d%%)\n", passed, tests,
		(100 * passed) / tests);

	return passed == tests;
}
#endif

//
// EK_USE_HASH
//
#if EK_USE_HASH

static uint64_t load_u64_unaligned(const uint8_t *data) {
	return (uint64_t)data[0]	<< 8 * 7
		| (uint64_t)data[1]	<< 8 * 6
		| (uint64_t)data[2] 	<< 8 * 5
		| (uint64_t)data[3] 	<< 8 * 4
		| (uint64_t)data[4] 	<< 8 * 3
		| (uint64_t)data[5] 	<< 8 * 2
		| (uint64_t)data[6] 	<< 8 * 1
		| (uint64_t)data[7];
}

void swapmem(void *const restrict _a, void *const restrict _b, size_t len) {
	uint8_t *restrict a = _a, *restrict b = _b;

	while (len--) {
		const uint8_t t = *a;
		*a++ = *b;
		*b++ = t;
	}
}

uint64_t xxhash64_single_lane(const uint8_t *data, size_t len) {
	static const uint64_t p1 = 0x9e3779b185ebca87ull;
	static const uint64_t p2 = 0xc2b2ae3d27d4eb4full;
	static const uint64_t p3 = 0x165667b19e3779f9ull;
	static const uint64_t p4 = 0x85ebca77c2b2ae63ull;
	static const uint64_t p5 = 0x27d4eb2f165667c5ull;

	uint64_t acc = p5 + len + 1;
	while (len >= 8) {
		acc ^= load_u64_unaligned(data);
		acc = rotl(acc, 27) * p1;
		acc += p4;

		data += 8, len -= 8;
	}
	while (len--) {
		acc ^= *data++ * p5;
		acc = rotl(acc, 11) * p1;
	}

	acc ^= acc >> 33;
	acc *= p2;
	acc ^= acc >> 29;
	acc *= p3;
	acc ^= acc >> 32;

	return acc;
}

#define HSET_ENTRY_PSL_BITS 15
#define HSET_ENTRY_HASH_BITS 48

typedef struct hset_entry_t {
	uint64_t used	: 1;
	uint64_t psl	: HSET_ENTRY_PSL_BITS;
	uint64_t hash	: HSET_ENTRY_HASH_BITS;
	uint64_t data[];
} hset_entry_t;

typedef struct hset {
	mem_alloc_t alloc;

	// Size of key value pair
	uint32_t kvsize, entsize;

	// Number of entries and capacity
	uint32_t nents, capacity;

	hset_hash_fn *hash;

	hset_eq_fn *eq;

	hset_entry_t kv[];
} hset_t;

#define hset_from_data(_data) (hset_t *)((uintptr_t)_data \
				- offsetof(hset_entry_t, data) \
				- offsetof(hset_t, kv))

void *hset_init(mem_alloc_t alloc,
		uint32_t capacity, uint32_t kvsize,
		hset_hash_fn *hash, hset_eq_fn *eq) {
	const size_t entsize = align_up(kvsize + sizeof(hset_entry_t),
				sizeof(hset_entry_t));
	hset_t *set = mem_alloc(alloc, NULL, sizeof(hset_t) + entsize * (capacity + 1));
	set->alloc = alloc;

	set->nents = 0;
	set->capacity = capacity;

	set->kvsize = kvsize;
	set->entsize = entsize / sizeof(hset_entry_t);

	set->hash = hash;
	set->eq = eq;

	set->kv[set->entsize * set->capacity] = (hset_entry_t){
		.used = true,
		.psl = -1,
	};

	return set->kv->data;
}
void *hset_deinit(void *_set) {
	hset_t *set = hset_from_data(_set);
	return mem_alloc(set->alloc, set, 0);
}
hset_t *hset_grow(hset_t *set) {
	void *newset = hset_init(set->alloc, set->capacity * 2, set->kvsize,
				set->hash, set->eq);
	if (!newset) return NULL;
	
	for (void *iter = hset_next(set->kv->data, NULL); iter;
		iter = hset_next(set->kv->data, iter)) {
		// No need to set newset to insert's newset, no growth should happen
		hset_insert(newset, iter);
	}

	hset_deinit(set->kv->data);
	return hset_from_data(newset);
}
void *hset_insert(void *_set, const void *kv) {
	hset_t *set = hset_from_data(_set);

	// Grow the hash set
	if (set->nents > set->capacity * 3 / 4) set = hset_grow(set);

	// Get the new entry, entry and keyvalue pair iterators
	const uint64_t hash = set->hash(kv);

	hset_entry_t *tmp = set->kv + set->entsize * set->capacity;
	*tmp = (hset_entry_t){
		.used = true,
		.psl = 0,
		.hash = hash,
	};
	memcpy(tmp->data, kv, set->kvsize);
	hset_entry_t *iter = set->kv + set->entsize * (hash % set->capacity);

	while (iter->used) {
		if (tmp->psl > iter->psl) {
			// Swap
			swapmem(iter, tmp, set->entsize * sizeof(hset_entry_t));
		}
		if (tmp->hash == iter->hash && set->eq(tmp->data, kv)) {
			memcpy(iter->data, kv, set->kvsize);
			goto ret;
		}

		iter += set->entsize;
		if (iter == tmp) iter = set->kv;
		tmp->psl++;
	}

	memcpy(iter, tmp, set->entsize * sizeof(hset_entry_t));
	set->nents++;

ret:
	*tmp = (hset_entry_t){ .psl = -1, .used = true };
	return set->kv->data;
}
void hset_remove(void *_set, const void *kv) {
	hset_t *set = hset_from_data(_set);

	hset_entry_t *ent = (hset_entry_t *)kv - 1;

	ent->used = false;
	ent->psl = 0;
	ent->hash = 0;
	set->nents--;
}
void *hset_get(void *_set, const void *key) {
	hset_t *set = hset_from_data(_set);

	const uint64_t mask = (1ull << HSET_ENTRY_HASH_BITS) - 1;
	const uint64_t hash = set->hash(key) & mask;
	hset_entry_t *iter = set->kv + set->entsize * (hash % set->capacity);
	hset_entry_t *const end = set->kv + set->entsize * set->capacity;
	int psl = 0;

	while (iter->used) {
		if (psl > iter->psl) return NULL;
		if (hash == iter->hash && set->eq(iter->data, key)) return iter->data;

		iter += set->entsize;
		if (iter == end) iter = set->kv;
		psl++;
	}

	return NULL;
}
void *hset_next(void *_set, void *iter) {
	hset_t *set = hset_from_data(_set);
	if (!iter) return set->kv->data;

	hset_entry_t *ent = (hset_entry_t *)iter - 1;
	do {
		ent += set->entsize;
	} while (!ent->used);

	if (ent->psl == (-1ull & (1 << HSET_ENTRY_PSL_BITS) - 1)) return NULL;
	else return ent->data;
}

uint64_t str_hash(const char *str) {
	return xxhash64_single_lane((const uint8_t *)str, strlen(str));
}
bool str_eq(const char *a, const char *b) {
	return strcmp(a, b) == 0;
}

#if EK_USE_STRVIEW
uint64_t strview_hash(const strview_t *sv) {
	return xxhash64_single_lane((const uint8_t *)sv->str, sv->len);
}
#endif

#endif

#if EK_USE_ARENA

typedef struct fixed_arena {
	mem_alloc_fn *alloc_fn;
	uint8_t *ptr;
	uint8_t data[];
} fixed_arena_t;

static void *fixed_arena_realloc(void *buf, void *blk, size_t newsize) {
	if (!blk) return fixed_arena_alloc(buf, newsize);
	else return NULL;
}
bool fixed_arena_init(void *buf, size_t bufsize) {
	fixed_arena_t *arena = buf;
	if (bufsize < sizeof(*arena)) return false;
	arena->alloc_fn = fixed_arena_realloc;
	arena->ptr = arena->data + bufsize - sizeof(*arena);
	return true;
}
void *fixed_arena_alloc(void *buf, size_t size) {
	fixed_arena_t *arena = buf;
	arena->ptr = (uint8_t *)align_dn((uintptr_t)arena->ptr - size, 8);
	return arena->ptr >= arena->data ? arena->ptr : NULL;
}
void fixed_arena_reset_to(void *buf, void *to) {
	((fixed_arena_t *)buf)->ptr = to;
}
mem_alloc_t fixed_arena_alloc_fn(void *buf) {
	return &((fixed_arena_t *)buf)->alloc_fn;
}

#endif

#if EK_USE_POOL

typedef struct dynpool_block dynpool_block_t;
typedef struct dynpool_chunk {
	union {
		dynpool_block_t *parent;
		struct dynpool_chunk *nextfree;
	};
	uint8_t data[];
} dynpool_chunk_t;

struct dynpool_block {
	dynpool_block_t *next, *nextfree, *lastfree;
	size_t nchunks, freeid, elemsz, nalloced;
	dynpool_chunk_t *free;
	uint8_t data[];
};

struct dynpool {
	mem_alloc_fn *interface;
	size_t elemsz;
	mem_alloc_t alloc;
	dynpool_block_t *head, *free;
	dynpool_block_t first;
};

dynpool_t *dynpool_init(mem_alloc_t alloc, size_t initial_chunks, size_t elemsz) {
	elemsz = align_up(elemsz, sizeof(dynpool_chunk_t)) + sizeof(dynpool_chunk_t);
	dynpool_t *self = mem_alloc(alloc, NULL, sizeof(*self) + sizeof(dynpool_block_t)
					+ initial_chunks * elemsz);
	*self = (struct dynpool){
		.elemsz = elemsz,
		.alloc = alloc,
		.head = &self->first,
		.free = &self->first,
		.first = (dynpool_block_t){
			.elemsz = elemsz,
			.nchunks = initial_chunks,
			.next = NULL,
			.nextfree = NULL,
			.lastfree = NULL,
			.free = NULL,
			.freeid = 0,
			.nalloced = 0,
		},
	};

	return self;
}
void dynpool_deinit(dynpool_t *self) {
	dynpool_block_t *blk = self->head;
	while (blk) {
		dynpool_block_t *next = blk->next;
		if (blk != &self->first) mem_alloc(self->alloc, blk, 0);
		blk = next;
	}
	mem_alloc(self->alloc, self, 0);
}
static void remove_free_block(dynpool_t *self, dynpool_block_t *blk) {
	if (blk->lastfree) blk->lastfree->nextfree = blk->nextfree;
	else self->free = blk->nextfree;
	if (blk->nextfree) blk->nextfree->lastfree = blk->lastfree;
}
void *dynpool_alloc(dynpool_t *self) {
	if (!self->free) {
		dynpool_block_t *blk = mem_alloc(self->alloc, NULL,
				sizeof(*blk) + self->head->nchunks * self->elemsz);
		*blk = (dynpool_block_t){
			.elemsz = self->elemsz,
			.nchunks = self->head->nchunks,
			.next = self->head,
			.nextfree = NULL,
			.lastfree = NULL,
			.free = NULL,
			.freeid = 0,
			.nalloced = 0,
		};
		self->head = blk;
		self->free = blk;
	}

	dynpool_block_t *blk = self->free;
	dynpool_chunk_t *chunk;
	if (blk->freeid != blk->nchunks) {
		chunk = (dynpool_chunk_t *)(blk->data + (blk->freeid++ * blk->elemsz));
	} else {
		chunk = blk->free;
		blk->free = chunk->nextfree;
	}
	chunk->parent = blk;
	if (++blk->nalloced == blk->nchunks) remove_free_block(self, blk);
	return chunk->data;
}
void dynpool_free(dynpool_t *self, void *ptr) {
	if (!ptr) {
		return;
	}
	dynpool_chunk_t *chunk = (dynpool_chunk_t *)((uintptr_t)ptr - offsetof(dynpool_chunk_t, data));
	dynpool_block_t *blk = chunk->parent;
	if (blk->nalloced-- == blk->nchunks) {
		blk->lastfree = NULL;
		blk->nextfree = self->free;
		if (blk->nextfree) blk->nextfree->lastfree = blk;
		self->free = blk;
	}

	chunk->nextfree = blk->free;
	blk->free = chunk;
}
bool dynpool_empty(const dynpool_t *self) {
	size_t free_blocks = 0, nblocks = 0;
	
	const dynpool_block_t *blk = self->head;
	while (blk) {
		nblocks++;
		blk = blk->next;
	}

	blk = self->free;
	while (blk) {
		if (blk->nalloced != 0) return false;
		free_blocks++;
		blk = blk->nextfree;
	}

	return free_blocks == nblocks;
}
void dynpool_fast_clear(dynpool_t *self) {
	dynpool_block_t *blk = self->head;
	self->free = NULL;
	while (blk) {
		blk->nalloced = 0;
		blk->free = NULL;
		blk->freeid = 0;
		
		blk->lastfree = NULL;
		blk->nextfree = self->free;
		if (blk->nextfree) blk->nextfree->lastfree = blk;
		self->free = blk;

		blk = blk->next;
	}
}
size_t dynpool_num_chunks(const dynpool_t *self) {
	size_t nchunks = 0;
	dynpool_block_t *blk = self->head;
	while (blk) {
		nchunks += blk->nalloced;
		blk = blk->next;
	}
	return nchunks;
}

typedef struct fixedpool_chunk {
	struct fixedpool_chunk *next;
} fixedpool_chunk_t;

typedef struct fixedpool {
	size_t nchunks, freeid, elemsz;
	fixedpool_chunk_t *free;
	uint8_t data[];
} fixedpool_t;

void fixedpool_init(void *buf, size_t elemsz, size_t buflen) {
	fixedpool_t *self = buf;

	elemsz = align_up(elemsz, sizeof(fixedpool_chunk_t)) + sizeof(fixedpool_chunk_t);
	self->nchunks = (buflen - sizeof(*self)) / elemsz;
	self->free = NULL;
	self->freeid = 0;
	self->elemsz = elemsz;
}
void fixedpool_fast_clear(void *buf) {
	fixedpool_t *self = buf;

	self->freeid = 0;
	self->free = NULL;
}
void *fixedpool_alloc(void *buf) {
	fixedpool_t *self = buf;

	if (self->freeid != self->nchunks) {
		return self->data + self->elemsz*(self->freeid++);
	}
	if (!self->free) return NULL;
	void *ptr = self->free;
	self->free = self->free->next;
	return ptr;
}
void fixedpool_free(void *buf, void *blk) {
	fixedpool_t *self = buf;

	if (!blk) return;
	((fixedpool_chunk_t *)blk)->next = self->free;
	self->free = blk;
}
bool fixedpool_empty(const void *buf) {
	const fixedpool_t *self = buf;

	if (self->freeid == 0) return 1;
	size_t nfree = 0;
	fixedpool_chunk_t *chunk = self->free;
	while (chunk) {
		nfree++;
		chunk = chunk->next;
	}
	return nfree == self->nchunks;
}

#endif

//
// EK_USE_PACKET
//
#if EK_USE_PACKET

uint32_t packet_htonf(float in) {
	uint8_t *bytes = (uint8_t*)&in;
	return bytes[0] << 24 |
		bytes[1] << 16 |
		bytes[2] << 8 |
		bytes[3];
}
float packet_ntohf(uint32_t in) {
	uint8_t* bytes = (uint8_t*)&in;
	uint32_t val = bytes[0] << 24 |
		bytes[1] << 16 |
		bytes[2] << 8 |
		bytes[3];
	return *((float *)&val);
}

int packet_bytecount(int head) {
	return (head / 8) + ((head % 8) ? 1 : 0);
}
void packet_write_bit(uint8_t *buf, int *bitlen, int value) {
	if (value) buf[*bitlen / 8] |= (1 << (*bitlen % 8));
	else buf[*bitlen / 8] &= ~(1 << (*bitlen % 8));
	(*bitlen)++;
}
int packet_read_bit(const uint8_t *buf, int *bitlen) {
	int value = buf[*bitlen / 8] & (1 << (*bitlen % 8));
	(*bitlen)++;
	return value ? 1 : 0;
}
void packet_write_bits(uint8_t *buf, int *bitlen, uint64_t value, int numbits) {
	int i;
	for (i = 0; i < numbits; i++)
		packet_write_bit(buf, bitlen, value & (uint64_t)1 << (uint64_t)i);
}
uint64_t packet_read_bits(const uint8_t *buf, int *bitlen, int numbits) {
	uint64_t value = 0, i;
	for (i = 0; i < numbits; i++) {
		value |= (uint64_t)packet_read_bit(buf, bitlen) << i;
	}
	return value;
}
void packet_write_u8(uint8_t *buf, int *bitlen, uint8_t value) {
	if (!value) {
		packet_write_bit(buf, bitlen, 0);
	}
	else if (!(value & ~0xf)) {
		packet_write_bits(buf, bitlen, 1, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 4);
	}
	else {
		packet_write_bits(buf, bitlen, 3, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 8);
	}
}
void packet_write_u16(uint8_t *buf, int *bitlen, uint16_t value) {
	if (!value) {
		packet_write_bits(buf, bitlen, 0, 2);
	}
	else if (!(value & ~0x1f)) {
		packet_write_bits(buf, bitlen, 1, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 5);
	}
	else if (!(value & ~0xff)) {
		packet_write_bits(buf, bitlen, 2, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 8);
	}
	else {
		packet_write_bits(buf, bitlen, 3, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 16);
	}
}
void packet_write_u32(uint8_t *buf, int *bitlen, uint32_t value) {
	if (!value) {
		packet_write_bits(buf, bitlen, 0, 2);
	}
	else if (!(value & ~0x3f)) {
		packet_write_bits(buf, bitlen, 1, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 6);
	}
	else if (!(value & ~0x7ff)) {
		packet_write_bits(buf, bitlen, 2, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 11);
	}
	else {
		packet_write_bits(buf, bitlen, 3, 2);
		packet_write_bits(buf, bitlen, (uint64_t)value, 32);
	}
}
void packet_write_s16(uint8_t *buf, int *bitlen, int16_t value) {
	if (!value) {
		packet_write_bits(buf, bitlen, 0, 2);
	}
	else if (value >= -0x10 && value < 0x10) {
		packet_write_bits(buf, bitlen, 1, 2);
		packet_write_bits(buf, bitlen, (uint64_t)(value+0x10), 5);
	}
	else if (value >= -0x80 && value < 0x80) {
		packet_write_bits(buf, bitlen, 2, 2);
		packet_write_bits(buf, bitlen, (uint64_t)(value+0x80), 8);
	}
	else {
		packet_write_bits(buf, bitlen, 3, 2);
		packet_write_bits(buf, bitlen, (uint64_t)((int64_t)value+0x8000), 16);
	}
}
void packet_write_s32(uint8_t *buf, int *bitlen, int32_t value) {
	if (!value) {
		packet_write_bits(buf, bitlen, 0, 2);
	}
	else if (value >= -0x20 && value < 0x20) {
		packet_write_bits(buf, bitlen, 1, 2);
		packet_write_bits(buf, bitlen, (uint64_t)(value+0x20), 6);
	}
	else if (value >= -0x400 && value < 0x400) {
		packet_write_bits(buf, bitlen, 2, 2);
		packet_write_bits(buf, bitlen, (uint64_t)(value+0x400), 11);
	}
	else {
		packet_write_bits(buf, bitlen, 3, 2);
		packet_write_bits(buf, bitlen, (uint64_t)((int64_t)value+0x80000000), 32);
	}
}
void packet_write_float(uint8_t *buf, int *bitlen, float value) {
	float a = fabsf(value);
	uint16_t fixed = 0;

	if (a < 0.001f) {
		packet_write_bits(buf, bitlen, 0, 2);
	}
	else if (a < 8.0f) {
		packet_write_bits(buf, bitlen, 1, 2);
		fixed |= value < 0;
		fixed |= ((uint16_t)a & 0x7) << 1;
		fixed |= (uint16_t)(fmodf(a, 1.0f) * 1024.0f) << 4;
		packet_write_bits(buf, bitlen, (uint64_t)fixed, 14);
	}
	else if (a < 127.0f) {
		packet_write_bits(buf, bitlen, 2, 2);
		fixed |= value < 0;
		fixed |= ((uint16_t)a & 0x7f) << 1;
		fixed |= (uint16_t)(fmodf(a, 1.0f) * 256.0f) << 8;
		packet_write_bits(buf, bitlen, (uint64_t)fixed, 16);
	}
	else {
		packet_write_bits(buf, bitlen, 3, 2);
		packet_write_bits(buf, bitlen, (uint64_t)packet_htonf(value), 32);
	}
}

uint8_t packet_read_u8(const uint8_t *buf, int *bitlen) {
	uint64_t value;
	if (!packet_read_bit(buf, bitlen)) {
		return 0;
	}
	else if (!packet_read_bit(buf, bitlen)) {
		value = packet_read_bits(buf, bitlen, 4);
		return (uint8_t)value;
	}
	else {
		value = packet_read_bits(buf, bitlen, 8);
		return (uint8_t)value;
	}
}
uint16_t packet_read_u16(const uint8_t *buf, int *bitlen) {
	uint64_t value = packet_read_bits(buf, bitlen, 2);

	if (!value) {
		return 0;
	}
	else if (value == 1) {
		value = packet_read_bits(buf, bitlen, 5);
		return (uint16_t)value;
	}
	else if (value == 2) {
		value = packet_read_bits(buf, bitlen, 8);
		return (uint16_t)value;
	}
	else {
		value = packet_read_bits(buf, bitlen, 16);
		return (uint16_t)value;
	}
}
uint32_t packet_read_u32(const uint8_t *buf, int *bitlen) {
	uint64_t value = packet_read_bits(buf, bitlen, 2);

	if (!value) {
		return 0;
	}
	else if (value == 1) {
		value = packet_read_bits(buf, bitlen, 6);
		return (uint32_t)value;
	}
	else if (value == 2) {
		value = packet_read_bits(buf, bitlen, 11);
		return (uint32_t)value;
	}
	else {
		value = packet_read_bits(buf, bitlen, 32);
		return (uint32_t)value;
	}
}
int16_t packet_read_s16(const uint8_t *buf, int *bitlen) {
	uint64_t value = packet_read_bits(buf, bitlen, 2);

	if (!value) {
		return 0;
	}
	else if (value == 1) {
		return (int16_t)packet_read_bits(buf, bitlen, 5)-0x10;
	}
	else if (value == 2) {
		return (int16_t)packet_read_bits(buf, bitlen, 8)-0x80;
	}
	else {
		return (int16_t)((int32_t)packet_read_bits(buf, bitlen, 16)-0x8000);
	}
}
int32_t packet_read_s32(const uint8_t *buf, int *bitlen) {
	uint64_t value = packet_read_bits(buf, bitlen, 2);

	if (!value) {
		return 0;
	}
	else if (value == 1) {
		return (int32_t)packet_read_bits(buf, bitlen, 6)-0x20;
	}
	else if (value == 2) {
		return (int32_t)packet_read_bits(buf, bitlen, 11)-0x400;
	}
	else {
		return (int32_t)((int64_t)packet_read_bits(buf, bitlen, 32)-0x80000000);
	}
}
float packet_read_float(const uint8_t *buf, int *bitlen) {
	uint64_t type;
	float a, sign;

	type = packet_read_bits(buf, bitlen, 2);
	if (type == 0) {
		sign = 1.0f;
		a = 0.0f;
	}
	else if (type == 1) {
		sign = packet_read_bit(buf, bitlen) ? -1.0f : 1.0f;
		a = (float)packet_read_bits(buf, bitlen, 3);
		a += (float)packet_read_bits(buf, bitlen, 10) / 1024.0f;
	}
	else if (type == 2) {
		sign = packet_read_bit(buf, bitlen) ? -1.0f : 1.0f;
		a = (float)packet_read_bits(buf, bitlen, 7);
		a += (float)packet_read_bits(buf, bitlen, 8) / 256.0f;
	}
	else {
		return packet_ntohf((unsigned int)packet_read_bits(buf, bitlen, 32));
	}

	return a * sign;
}

#endif


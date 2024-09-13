#include "ek.h"

#if EK_USE_VEC
#	include <stdlib.h>
#endif
#if EK_USE_PACKET
#	include <math.h>
#endif
#if EK_USE_TOML || EK_USE_JSON
#	include <ctype.h>
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

//
// EK_USE_TOML
//
#if EK_USE_TOML

static void toml_parse_whitespace(toml_args_t *args, bool include_newline) {
	while (*args->src.str == ' ' || *args->src.str == '\t' || *args->src.str == '\n') {
		if (*args->src.str == '\n') {
			if (!include_newline) return;
			args->line++;
		}

		args->src.str++, args->src.len--;
	}
}

static uint32_t toml_parse_char(toml_args_t *args, bool literal) {
	uint32_t c = strview_next_utf8(&args->src);
	if (c != '\\' || literal) {
		if (c == '\n') args->line++;
		return c;
	}
	if (!args->src.len) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: string ending in unescaped character on line %d",
			args->line);
		return (uint32_t)-1;
	}

	c = strview_next_utf8(&args->src);
	switch (c) {
	case 'b': case 't': case 'n': case 'f':
	case 'r': case '"': case '\\': return c;
	case 'u': case 'U': {
		char *end;
		uint32_t u = strtoul(args->src.str, &end, 16);
		if (end - args->src.str > args->src.len) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: utf escape sequence goes beyond source code on line %d",
				args->line);
			return (uint32_t)-1;
		}
		if ((*args->src.str & 0xc0) == 0x80) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: corrupted utf8 codepoint on line %d",
				args->line);
			return (uint32_t)-1;
		}
		if (c == 'u' && end - args->src.str != 4) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: 'u' utf8 escape sequence expects 4 digits on line %d",
				args->line);
			return (uint32_t)-1;
		}
		if (c == 'U' && end - args->src.str != 8) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: 'U' utf8 escape sequence expects 8 digits on line %d",
				args->line);
			return (uint32_t)-1;
		}
		args->src.len -= end - args->src.str;
		args->src.str = end;

		if (u > 0x10ffff) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: utf8 codepoint not within U+00000000 - U+0010ffff on line %d",
				args->line);
			return (uint32_t)-1;
		}
		return u;
	} default:
		if (args->errs) args->errs(LOG_ERR,
			"toml: unknown escape sequence on line %d",
			args->line);
		return (uint32_t)-1;
	}
}

// out must be an initialized vector
static bool toml_parse_string(toml_args_t *args, bool allow_multiline, strbuf_t *out) {
	const bool isliteral = *args->src.str == '\'';
	const bool ismulti = args->src.len > 3
		&& args->src.str[0] == args->src.str[1]
		&& args->src.str[0] == args->src.str[2];
	if (ismulti) args->src.str += 3, args->src.len -= 3;
	else args->src.str++, args->src.len++;
	
	while (*args->src.str != isliteral ? '\'' : '"') {
		uint32_t c = toml_parse_char(args, isliteral);
		if (c == (uint32_t)-1) return false;

		uint8_t buf[4];
		utf8_codepoint_encode(c, buf);
		out = vec_push(out, utf8_codepoint_len(buf[0]), buf);
	}

	if (ismulti && (args->src.len < 3
		|| args->src.str[1] != args->src.str[0]
		|| args->src.str[2] != args->src.str[0])) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: eof before string end on line %d", args->line);
		return false;
	}

	if (ismulti) args->src.str += 3, args->src.len -= 3;
	else args->src.str++, args->src.len++;
	return true;
}

static bool toml_isdigit(const char c, const int base) {
	switch (base) {
	case 16:
		if (tolower(c) >= 'a' && tolower(c) <= 'f') return true;
	case 10:
		if (c >= 0 && c <= 9) return true;
		else break;
	case 8:
		if (c >= 0 && c <= 7) return true;
		else break;
	case 1:
		if (c >= 0 && c <= 1) return true;
		else break;
	}
	if (c == '_' || c== '.') return true;
	return false;
}

static strview_t toml_parse_word(toml_args_t *args) {
	strview_t str = { .str = args->src.str };

	while (!isalnum(*args->src.str) && *args->src.str == '_' && *args->src.str == '-') {
		args->src.str++, args->src.len--;
	}

	if (!str.len) str.str = NULL;
	return str;
}

typedef struct toml_num_parser {
	bool sign, neg;
	int base;
	int digits[64];
	int ndigits;
	int leading_zeros;
	int decimal_pt;
} toml_num_parser_t;

static bool toml_parse_int(toml_args_t *args, const toml_num_parser_t *num, int64_t *out) {
	if (num->leading_zeros > 0 && num->base == 10) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: leading zeros before base 10 integer on line %d",
			args->line);
		return false;
	}

	switch (num->base) {
	case 1:
		if (num->ndigits > 64) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: too many digits (> 64) for int on line %d", args->line);
			return false;
		}
		break;
	case 8:
		if (num->ndigits > 22) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: too many digits (> 22) for int on line %d", args->line);
			return false;
		}
		break;
	case 10:
		if (num->ndigits > 19) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: too many digits (> 19) for int on line %d", args->line);
			return false;
		}
		break;
	case 16:
		if (num->ndigits > 16) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: too many digits (> 16) for int on line %d", args->line);
			return false;
		}
		break;
	}

	*out = 0;
	int64_t power = 1;
	for (int i = num->ndigits - 1; i >= 0; i--) {
		const int64_t curr = num->digits[i] * power;
		if (!num->neg && *out > INT64_MAX - curr
			|| num->neg && *out < INT64_MIN + curr) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: integer out of range on line %d", args->line);
			return false;
		}
		power *= num->base;
	}
	
	return true;
}

static bool toml_parse_flt(toml_args_t *args, const toml_num_parser_t *num, double *out) {
	double power = 1.0;

	*out = 0.0;
	for (int i = num->decimal_pt - 1; i >= 0; i--) {
		*out += (double)num->digits[i] * power;
		power *= 10.0;
	}

	power = 0.1;
	for (int i = num->decimal_pt; i < num->ndigits; i++) {
		*out += (double)num->digits[i] * power;
		power *= 0.1;
	}

	if (num->neg) *out *= -1.0;

	// Exponential digit
	if (tolower(*args->src.str) != 'e') return true;
	args->src.str++, args->src.len--;

	int digits[32];
	int ndigits = 0;
	int last_underscore = -1;
	bool neg = false;

	if (*args->src.str == '+' || *args->src.str == '-') {
		neg = *args->src.str == '-';
		args->src.str++, args->src.len--;
	}

	for (; toml_isdigit(*args->src.str, 10); args->src.str++, args->src.len--) {
		const char c = *args->src.str;

		if (c == '.') {
			if (args->errs) args->errs(LOG_ERR,
				"toml: fraction bit in number exponent on line %d",
				args->line);
			return false;
		}

		if (c == '_') {
			if (ndigits == 0) {
				if (args->errs) args->errs(LOG_ERR,
					"toml: underscore before digits in"
					" exponent on line %d",
					args->line);
				return false;
			}
			continue;
		}

		if (ndigits == arrlen(digits)) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: too many digits in exponent on line %d",
				args->line);
			return false;
		}

		digits[ndigits++] = c - '0';
	}

	if (ndigits == 0) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: 0 digits after exponent on line %d", args->line);
		return false;
	}

	double exp = 0.0;
	power = 1.0;
	for (int i = num->ndigits - 1; i >= 0; i--) {
		*out += (double)num->digits[i] * power;
		power *= 10.0;
	}
	if (neg) exp *= -1.0;

	*out = pow(*out, exp);
	return true;
}

static bool toml_parse_num(toml_args_t *args, toml_t *out) {
	toml_num_parser_t num = {
		.base = 10,
	};

	if (*args->src.str == '+' || *args->src.str == '-') {
		num.sign = true;
		num.neg = *args->src.str == '-';
		args->src.str++, args->src.len--;
	}
	if (*args->src.str == 'i' || *args->src.str == 'n') {
		const strview_t word = toml_parse_word(args);
		if (!word.str) {
			return false;
		} else if (strview_eq(&word, &make_strview("inf"))) {
			out->f = num.neg ? -INFINITY : INFINITY;
		} else if (strview_eq(&word, &make_strview("nan"))) {
			out->f = num.neg ? nan("-0.0") : nan("+0.0");
		} else {
			if (args->errs) args->errs(LOG_ERR,
				"toml: expected 'inf' or 'nan' on line %d",
				args->line);
			return false;
		}

		out->type = TOML_FLOAT;
		args->src.str += word.len, args->src.len -= word.len;
		return true;
	}

	if (args->src.str[0] == '0' && args->src.len > 2) {
		if (args->src.str[1] == 'x') {
			num.base = 16;
			if (!num.sign) goto consume_base_chars;
			if (args->errs) args->errs(LOG_ERR,
				"toml: sign prefix before hex integer on line %d", args->line);
			return false;
		} else if (args->src.str[1] == 'o') {
			num.base = 8;
			if (!num.sign) goto consume_base_chars;
			if (args->errs) args->errs(LOG_ERR,
				"toml: sign prefix before octal integer on line %d", args->line);
			return false;
		} else if (args->src.str[1] == 'b') {
			num.base = 1;
			if (!num.sign) goto consume_base_chars;
			if (args->errs) args->errs(LOG_ERR,
				"toml: sign prefix before binary integer on line %d", args->line);
			return false;
		}

consume_base_chars:
		if (num.base != 10) args->src.str += 2, args->src.len -= 2;
	}

	bool before = true;
	int last_underscore = -1;

	for (; toml_isdigit(*args->src.str, num.base); args->src.str++, args->src.len--) {
		const char c = *args->src.str;

		if (c == '.') {
			if (num.base != 10) {
				if (args->errs) args->errs(LOG_ERR,
					"toml: floating point constant without"
					" base of 10 on line %d", args->line);
				return false;
			}
			if (num.ndigits == 0) {
				if (args->errs) args->errs(LOG_ERR,
					"toml: decimal point before digits in"
					" float on line %d", args->line);
				return false;
			}
			if (num.decimal_pt) {
				if (args->errs) args->errs(LOG_ERR,
					"toml: floating point with 2 decimals on line %d",
					args->line);
				return false;
			}
			num.decimal_pt = num.ndigits;
			continue;
		}

		if (c == '_') {
			if (!num.ndigits) {
				if (args->errs) args->errs(LOG_ERR,
					"toml: underscore seperator before "
					"digits in number literal on line %d",
					args->line);
				return false;
			}
			last_underscore = num.ndigits;
			continue;
		}
		
		if (num.ndigits >= arrlen(num.digits)) {
			if (args->errs) args->errs(LOG_ERR,
				"toml: number of digits exceeds max (32) for"
				" integer on line %d", args->line);
			return false;
		}

		switch (num.base) {
		case 16:
			if (tolower(c) >= 'a' && tolower(c) <= 'f') {
				num.digits[num.ndigits] = tolower(c) - 'a' + 0xa;
				break;
			}
		case 10: case 8: case 1:
			num.digits[num.ndigits] = c - '0';
		}

		if (before) {
			if (num.digits[num.ndigits] == 0) num.leading_zeros++;
			if (num.digits[num.ndigits]) before = false;
		}
		num.ndigits++;
	}

	if (!num.ndigits) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: no digits in number on line %d", args->line);
		return false;
	}

	if (num.ndigits == num.decimal_pt) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: no digits after decimal point in number on line %d",
			args->line);
		return false;
	}

	if (num.ndigits == last_underscore) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: no digits after seperator in number on line %d",
			args->line);
		return false;
	}

	if (num.decimal_pt) {
		out->type = TOML_FLOAT;
		return toml_parse_flt(args, &num, &out->f);
	} else {
		out->type = TOML_INT;
		return toml_parse_int(args, &num, &out->i);
	}
}

static bool toml_parse_bool(toml_args_t *args, bool *out) {
	const strview_t word = toml_parse_word(args);
	if (!word.str) {
		return false;
	} else if (strview_eq(&word, &make_strview("true"))) {
		*out = true;
		return true;
	} else if (strview_eq(&word, &make_strview("false"))) {
		*out = false;
		return true;
	} else if (args->errs) {
		args->errs(LOG_ERR,
			"toml: expected 'true' or 'false' on line %d",
			args->line);
	}

	return false;
}

static bool toml_parse_time(toml_args_t *args, toml_t *out) {
	if (!rfc3339_parse(&args->src, &out->time)) {
		if (args->errs) args->errs(LOG_ERR,
			"toml: malformed date-time on line %d", args->line);
		return false;
	}
	out->type = TOML_TIME;
	return false;
}

static uint64_t toml_hash_fn(const toml_t *toml) {
	return xxhash64_single_lane((const uint8_t *)toml->key, *vec_len(toml->key));
}
static bool toml_eq_fn(const toml_t *a, const toml_t *b) {
	const strview_t a_key = strbuf_to_strview(a->key);
	const strview_t b_key = strbuf_to_strview(b->key);
	return strview_eq(&a_key, &b_key);
}

// Parses key but sets value part to 0
static toml_t *toml_parse_key(toml_args_t *args, toml_t *in_table) {
	strbuf_t key = strbuf_init(args->alloc, 16);

	if (*args->src.str == '"' || *args->src.str == '\'') {
		if (toml_parse_string(args, false, &key)) goto error;
	} else {
		strview_t key_strview = toml_parse_word(args);
		if (!key_strview.str) goto error;
		key = strbuf_cpy(key, &key_strview);
	}

	toml_t *toml = hset_get(in_table->htable, key);

	if (!toml) {
		toml = mem_alloc(args->alloc, NULL, sizeof(toml_t));
		memset(toml, 0, sizeof(*toml));
		toml->key = key;
	} else {
		vec_deinit(key);
	}

	toml_parse_whitespace(args, false);
	if (*args->src.str != '.') return toml;
	args->src.str++, args->src.len--;
	toml_parse_whitespace(args, false);

	// Make it a table
	toml->type = TOML_TABLE;
	toml->htable = hset_init(args->alloc, 4, sizeof(toml_t),
				(hset_hash_fn *)toml_hash_fn,
				(hset_eq_fn *)toml_eq_fn);

	return toml_parse_key(args, toml);
error:
	mem_alloc(args->alloc, toml->key, 0);
	mem_alloc(args->alloc, toml, 0);
	return NULL;
}

toml_t *toml_parse(toml_args_t *args) {
	return NULL;
}
void toml_deinit(mem_alloc_t alloc, toml_t *kv) {

}

#endif

#if EK_USE_RFC3339

static bool rfc3339_parse_num(const strview_t *str, int max_digits,
				int *digits_read, uint64_t *out) {
	char digits[20];

	*out = 0;
	*digits_read = 0;
	for (int i = 0; str->len && i < max_digits; i++) {
		if (i == str->len) return false;
		if (*digits_read == arrlen(digits)) return false;
		if (!isdigit(str->str[i])) return false;
		digits[(*digits_read)++] = str->str[i] - '0';
	}

	uint64_t power = 1;
	for (int i = *digits_read - 1; i >= 0; i--) {
		const uint64_t digit = (str->str[i] - '0') * power;
		if (*out > UINT64_MAX - digit) return false;
		*out = digit;
		power *= 10;
	}

	return true;
}

static bool rfc3339_parse_offs(strview_t *str, rfc3339_t *out) {
	int offs_sec;

	if (tolower(*str->str) == 'z') goto compute_epoch;
	bool neg;
	neg = *str->str == '-';
	if (*str->str != '-' && *str->str != '+') return false;
	str->str++, str->len--;

	uint64_t hour = 0, minute = 0;
	int ndigits;
	if (!rfc3339_parse_num(str, 2, &ndigits, &hour)) return false;
	str->str += ndigits, str->len -= ndigits;
	if (hour > 23) return false;
	if (*str->str != ':') return false;
	str->str++, str->len--;
	if (!rfc3339_parse_num(str, 2, &ndigits, &minute)) return false;
	str->str += ndigits, str->len -= ndigits;
	if (minute > 59) return false;
	offs_sec = (hour * 60 + minute) * (neg ? -1 : 1);

compute_epoch:
	out->islocal = false;
	const time_t unix_time = timegm(&(struct tm){
		.tm_sec = out->local.time.s,
		.tm_min = out->local.time.m,
		.tm_hour = out->local.time.h,
		.tm_mday = out->local.date.d,
		.tm_mon = out->local.date.m,
		.tm_year = out->local.date.y,
		.tm_isdst = false,
		.tm_gmtoff = offs_sec,
	});
	out->unix.tv_nsec = out->local.time.nano;
	out->unix.tv_sec = unix_time;

	return true;
}

// Only allows leap seconds at END of day times. Other times aren't supported
// because local time leap seconds usually just repeat the second
static bool rfc3339_parse_time(strview_t *str, rfc3339_t *out, bool allow_lp) {
	uint64_t num;
	int ndigits;

	// Hour
#define PARSE_NUM(EXPECT_COLON) \
	if (!rfc3339_parse_num(str, 2, &ndigits, &num)) return false; \
	if (ndigits != 2) return false; \
	str->str += 2, str->len -= 2; \
	if (EXPECT_COLON) { \
		if (!str->len || *str->str != ':') return false; \
		str->str++, str->len--; \
	}
	PARSE_NUM(true)
	if (num > 23) return false;
	out->local.time.h = num;

	// Minute
	PARSE_NUM(true)
	if (num > 59) return false;
	out->local.time.m = num;

	// Second
	PARSE_NUM(false)
	const int max_sec = allow_lp && out->local.time.h == 24
		&& out->local.time.m == 59 ? 60 : 59;
	if (num > max_sec) return false;
	out->local.time.s = num;

	// Fractional seconds
	if (!str->len || *str->str != '.') goto end;
	str->str++, str->len--;
	
	if (!rfc3339_parse_num(str, INT_MAX, &ndigits, &num)) return false;
	for (; ndigits > 10; ndigits--, num /= 10);
	for (; ndigits < 10; ndigits++, num *= 10);
	out->local.time.nano = num;

end:
	out->local.has_time = true;
	return true;
#undef PARSE_NUM
}

static bool rfc3339_is_leapyear(const unsigned year) {
	if (year % 100 == 0) return year % 400 == 0;
	else return year % 4 == 0;
}

static bool rfc3339_parse_date(strview_t *str, rfc3339_t *out) {
	uint64_t num;
	int ndigits;

	// Year
	if (!rfc3339_parse_num(str, 4, &ndigits, &num)) return false;
	if (ndigits != 4) return false;
	str->str += 4, str->len -= 4;
	if (!str->len || *str->str != '-') return false;
	str->str++, str->len--;
	out->local.date.y = num;

	// Month
	if (!rfc3339_parse_num(str, 2, &ndigits, &num)) return false;
	if (ndigits != 2) return false;
	str->str += 2, str->len -= 2;
	if (!str->len || *str->str != '-') return false;
	str->str++, str->len--;
	if (num > 12 || num == 0) return false;
	out->local.date.m = num;

	// Day
	if (!rfc3339_parse_num(str, 2, &ndigits, &num)) return false;
	if (ndigits != 2) return false;
	str->str += 2, str->len -= 2;
	if (num == 0) return false;
	switch (out->local.date.m) {
	case 1: case 3: case 5: case 7: case 8: case 10: case 12:
		if (num > 31) return false;
		break;
	case 4: case 6: case 9: case 11:
		if (num > 30) return false;
		break;
	case 2:
		if (num > (rfc3339_is_leapyear(out->local.date.y) ? 29 : 28)) return false;
		break;
	}
	out->local.date.d = num;
	out->local.has_date = true;

	return true;
}

bool rfc3339_parse(strview_t *str, rfc3339_t *out) {
	uint64_t n;
	int ndigits;

	*out = (rfc3339_t){
		.islocal = true,
	};

	// Get first 2 or 4 digit long number
	if (!rfc3339_parse_num(str, 4, &ndigits, &n)) return false;
	str->str += n, str->len -= n;
	if (ndigits == 2) return rfc3339_parse_time(str, out, false);
	else if (ndigits != 4) return false;

	if (!rfc3339_parse_date(str, out)) return false;
	if (!str->len || *str->str != ' ' && tolower(*str->str) != 't') {
		out->islocal = true;
		return true;
	}
	str->str++, str->len--;
	if (!rfc3339_parse_time(str, out, true)) return false;
	if (!str->len || *str->str != 'Z' && *str->str != '-' && *str->str != '+') {
		out->islocal = true;
		return true;
	}
	if (!rfc3339_parse_offs(str, out)) return false;

	return true;
}

#endif


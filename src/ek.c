#include "ek.h"

#if EK_USE_VEC
#	include <stdlib.h>
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
static log_fn *_logcb;

void logcb(log_fn *cb) {
	_logcb = cb;
}
#define LOGFN(name, level) \
	void name(const char *msg, ...) { \
		va_list args; \
		va_start(args, msg); \
		if (_logcb) _logcb(level, msg, args); \
		va_end(args); \
	}
LOGFN(_logdbg, LOG_DBG)
LOGFN(_loginfo, LOG_INFO)
LOGFN(_logwarn, LOG_WARN)
LOGFN(_logerr, LOG_ERR)
#undef LOGFN
#endif

//
// EK_USE_VEC
//
#if EK_USE_VEC

#define vec_size(elems, capacity) (sizeof(vec_t) * (capacity) * (elems))
#define vec_from_data(data) ((vec_t *)((uintptr_t)data - offsetof(vec_t, data)))

void *vec_init(size_t elem_size, size_t capacity) {
	vec_t *vec = EK_MALLOC(vec_size(capacity, elem_size));
	vec->len = 0;
	vec->capacity = capacity;
	vec->elem_size = elem_size;
	return vec->data;
}
void *vec_deinit(void *data) {
	EK_FREE(vec_from_data(data));
	return NULL;
}
void *vec_push(void *data, size_t nelems, const void *elems) {
	vec_t *vec = vec_from_data(data);
	if (vec->len + nelems > vec->capacity) {
		vec->capacity *= 2;
		vec = EK_REALLOC(vec, vec_size(vec->elem_size, vec->capacity));
	}

	memcpy(vec->data + vec->len * vec->elem_size, elems, nelems * vec->elem_size);
	vec->len += nelems;
	return vec->data;
}
void vec_pop(void *data, size_t nelems, void *elems) {
	vec_t *vec = vec_from_data(data);
	vec->len -= nelems;
	if (!elems) return;
	memcpy(elems, vec->data + vec->len * vec->elem_size, nelems * vec->elem_size);
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

#define hset_from_data(_data) (hset_t *)((uintptr_t)_data \
				- offsetof(hset_entry_t, data) \
				- offsetof(hset_t, kv))

void *hset_init(uint32_t capacity, uint32_t kvsize,
		hset_hash_fn *hash, hset_eq_fn *eq) {
	const size_t entsize = align_up(kvsize + sizeof(hset_entry_t),
				sizeof(hset_entry_t));
	hset_t *set = EK_MALLOC(sizeof(hset_t) + entsize * (capacity + 1));

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
	EK_FREE(hset_from_data(_set));
	return NULL;
}
hset_t *hset_grow(hset_t *set) {
	void *newset = hset_init(set->capacity * 2, set->kvsize, set->hash, set->eq);
	
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


//
// ekutils version 0.1.0, made by _ e k l 1 p 3 d _
//
#ifndef _ek_h_
#define _ek_h_

#ifndef EK_FEATURE_OFF
#	define EK_FEATURE_OFF 0
#endif


// Change EK_FEATURE_OFF to 1 if you want to use the feature
#ifndef EK_USE_UTIL
#	define EK_USE_UTIL EK_FEATURE_OFF
#endif
#ifndef EK_USE_STRVIEW
#	define EK_USE_STRVIEW EK_FEATURE_OFF
#endif
#ifndef EK_USE_LOG
#	define EK_USE_LOG EK_FEATURE_OFF
#endif
#ifndef EK_USE_VEC
#	define EK_USE_VEC EK_FEATURE_OFF
#endif
#ifndef EK_USE_UTF8
#	define EK_USE_UTF8 EK_FEATURE_OFF
#endif
#ifndef EK_USE_TEST
#	define EK_USE_TEST EK_FEATURE_OFF
#endif
#ifndef EK_USE_HASH
#	define EK_USE_HASH EK_FEATURE_OFF
#endif
#ifndef EK_USE_ARENA
#	define EK_USE_ARENA EK_FEATURE_OFF
#endif
#ifndef EK_USE_POOL
#	define EK_USE_POOL EK_FEATURE_OFF
#endif
#ifndef EK_USE_STRBUF
#	define EK_USE_STRBUF EK_FEATURE_OFF
#endif

//
// standard library includes
//
#include <stddef.h>
#if EK_USE_STRVIEW || EK_USE_STRBUF
#	include <string.h>
#endif
#if EK_USE_LOG
#	include <stdarg.h>
#endif
#if EK_USE_UTF8 || EK_USE_VEC || EK_USE_HASH
#	include <stdint.h>
#endif
#if EK_USE_TEST
#	include <stdio.h>
#endif
#if EK_USE_STRVIEW || EK_USE_HASH
#	include <stdbool.h>
#endif

// Allocation interface
typedef void *(mem_alloc_fn)(void *usr, void *blk, size_t size);
typedef mem_alloc_fn *const *mem_alloc_t;
void *mem_alloc(mem_alloc_t fn, void *blk, size_t size) {
	if (!fn) return NULL;
	return (*fn)((void *)fn, blk, size);
}

//
// Always included in ek.h.
// Change these to your own allocation functions.
// These are meant if you need general purpose allocation.
// There are also general purpose allocation interfaces for runtime
//
#ifndef EK_MALLOC
#	define EK_USE_STDLIB_MALLOC 1
mem_alloc_t mem_stdlib_alloc(void);

#ifndef NDEBUG
size_t mem_stdlib_allocated_bytes(void);
#endif

#endif

//
// EK_USE_UTIL
//
#if EK_USE_UTIL
#	define arrlen(a) (sizeof(a) / sizeof((a)[0]))
#	define max(a, b) ({ \
		const typeof(a) _a = a; \
		const typeof(b) _b = b; \
		_a > _b ? _a : _b; \
	})
#	define min(a, b) ({ \
		const typeof(a) _a = a; \
		const typeof(b) _b = b; \
		_a < _b ? _a : _b; \
	})

#	define align_up(x, a) ({ \
		const typeof(x) _x = x; \
		const typeof(a) _a = a; \
		const bool is_signed = _Generic(_x, \
			char: true, int: true, long: true, long long: true, \
			default: false); \
		typeof(x) _out; \
		if (!is_signed) { \
			_out = ((_x - 1) / _a + 1) * _a; \
		} else { \
			if (_x == 0) _out = 0; \
			else if (_x < 0) _out = _x / _a * _a; \
			else _out = ((_x - 1) / _a + 1) * _a; \
		} \
		_out; \
	})
#	define align_dn(x, a) ({ \
		const typeof(x) _x = x; \
		const typeof(a) _a = a; \
		const bool is_signed = _Generic(_x, \
			char: true, int: true, long: true, long long: true, \
			default: false); \
		typeof(x) _out; \
		if (!is_signed) { \
			_out = _x / _a * _a; \
		} else { \
			if (_x == 0) _out = 0; \
			else if (_x > 0) _out = _x / _a * _a; \
			else _out = ((_x - 1) / _a + 1) * _a; \
		} \
		_out; \
	})
#	define rotl(x, r) ({ \
		const typeof(x) _x = x; \
		const typeof(r) _r = r; \
		(_x << _r) | (_x >> (sizeof(_x) * 8 - _r)); \
	})
#	define rotr(x, r) ({ \
		const typeof(x) _x = x; \
		const typeof(r) _r = r; \
		(_x >> _r) | (_x << (sizeof(_x) * 8 - _r)); \
	})

#endif

//
// EK_USE_STRVIEW
//
#if EK_USE_STRVIEW
// A normal ascii string view
typedef struct strview {
	const char *str;
	size_t len;
} strview_t;

#define make_strview(literal) ((strview_t){ .str = literal, .len = sizeof(literal) - 1 })

// Checks if 2 string views have equal contents
static inline bool strview_eq(const strview_t *const restrict lhs,
				const strview_t *const restrict rhs) {
	if (!lhs->str && !rhs->str) return true;
	if (!!lhs->str != !!rhs->str || lhs->len != rhs->len) return false;
	return memcmp(lhs->str, rhs->str, lhs->len) == 0;
}

// Will store a stringview into a character buffer of size len. This will always
// make sure the null-terminator is stored, even in view is larger than the buffer.
// It returns the number of characters writen EXCLUDING the null terminator.
// 
// Notes: if the string view has a null terminator inside it, it will copy
// the until the length of the string view or the length of the buffer anyways.
size_t strview_buf_str(char *str, size_t len, const strview_t *const view);

static inline strview_t str_to_strview(const char *str) {
	return (strview_t){ .str = str, .len = strlen(str) };
}

#define STRVIEW_TO_STR_TMP_MAX 1024

// Will allocate a temporary buffer on the data segment for the string view will a
// null terminator. Will only be there until another call to strview_to_str_cmp
const char *strview_as_str(const strview_t *const view);
#endif

//
// EK_USE_UTF8
//
#if EK_USE_UTF8
static inline int utf8_codepoint_len(const uint8_t start) {
	const int clz = __builtin_clz(~start << 24);
	return clz + !clz;
}
static inline int utf32_to_utf8_len(const uint32_t c) {
	if (c < 0x80) return 1;
	else if (c < 0x800) return 2;
	else if (c < 0x10000) return 3;
	else return 4;
}
static uint32_t utf8_codepoint_decode(const uint8_t *buf) {
	uint32_t out;

	switch (utf8_codepoint_len(*buf)) {
	case 1:
		out = *buf++;
		break;
	case 2: 
		out = (*buf++ & 0x1f) << 6;
		out |= *buf++ & 0x3f;
		break;
	case 3: 
		out = (*buf++ & 0x0f) << 18;
		out |= (*buf++ & 0x3f) << 6;
		out |= *buf++ & 0x3f;
		break;
	case 4: 
		out = (*buf++ & 0x07) << 24;
		out |= (*buf++ & 0x1f) << 18;
		out |= (*buf++ & 0x3f) << 6;
		out |= *buf++ & 0x3f;
		break;
	}

	return out;
}
static void utf8_codepoint_encode(const uint32_t c, uint8_t *buf) {
	switch (utf32_to_utf8_len(c)) {
	case 1:
		*buf++ = c;
		break;
	case 2: 
		*buf++ = c >> 6 & 0x1f | 0xc0;
		*buf++ = c & 0x3f | 0x80;
		break;
	case 3: 
		*buf++ = c >> 18 & 0x0f | 0xe0;
		*buf++ = c >> 6 & 0x3f | 0x80;
		*buf++ = c & 0x3f | 0x80;
		break;
	case 4: 
		*buf++ = c >> 24 & 0x07 | 0xf0;
		*buf++ = c >> 18 & 0x3f | 0x80;
		*buf++ = c >> 6 & 0x3f | 0x80;
		*buf++ = c & 0x3f | 0x80;
		break;
	}
}
#endif

//
// EK_USE_LOG
//
#if EK_USE_LOG
typedef enum log_lvl {
	LOG_DBG,
	LOG_INFO,
	LOG_WARN,
	LOG_ERR,
} log_lvl_t;

typedef void (log_fn)(log_lvl_t l, const char *msg, va_list args);

void logcb(log_fn *cb);
#define logdbg(msg) \
	_logdbg("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define loginfo(msg) \
	_loginfo("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define logwarn(msg) \
	_logwarn("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define logerr(msg) \
	_logerr("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define logdbgf(msg, ...) \
	_logdbg("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__, \
		__VA_ARGS__)
#define loginfof(msg, ...) \
	_loginfo("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__, \
		__VA_ARGS__)
#define logwarnf(msg, ...) \
	_logwarn("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__, \
		__VA_ARGS__)
#define logerrf(msg, ...) \
	_logerr("%s in %s: " msg, \
		__func__, \
		strrchr(__FILE__, '/') + 1 ? strrchr(__FILE__, '/') + 1 : __FILE__, \
		__VA_ARGS__)
void _logdbg(const char *msg, ...);
void _loginfo(const char *msg, ...);
void _logwarn(const char *msg, ...);
void _logerr(const char *msg, ...);
#endif

//
// EK_USE_VEC
//
#if EK_USE_VEC
typedef struct vec {
	mem_alloc_t alloc;
	size_t len;
	size_t capacity;
	size_t elem_size;
	uint8_t data[];
} vec_t;

// Returns a pointer starting at the data part
void *vec_init(mem_alloc_t alloc, size_t elem_size, size_t capacity);
void *vec_deinit(void *data);

// Returns a new pointer if its being reallocated
void *vec_push(void *data, size_t nelems, const void *elems);
void vec_pop(void *data, size_t nelems, void *elems);

#define vec_from_data(data) ((vec_t *)((uintptr_t)data - offsetof(vec_t, data)))
static inline size_t *vec_len(const void *data) {
	return &vec_from_data(data)->len;
}
static inline size_t vec_capacity(const void *data) {
	return vec_from_data(data)->capacity;
}
#undef vec_from_data
#endif

//
// EK_USE_STRBUF
//
#if EK_USE_STRBUF
#if !EK_USE_VEC
#	error ek.h: to use string buffers, enable vector library
#endif

char *strbuf_init(mem_alloc_t fn, size_t initial_capacity) {
	char *s = vec_init(fn, 1, initial_capacity);
	if (!s) return NULL;
	vec_push(s, 1, &(char){ '\0' });
	return s;
}
char *strbuf_cpy(char *sb, const strview_t *sv) {
	*vec_len(sb) = 0;
	if (!(sb = vec_push(sb, sv->len, sv->str))) return NULL;
	return vec_push(sb, 1, &(char){ '\0' });
}
char *strbuf_from_sv(mem_alloc_t fn, const strview_t *sv) {
	char *sb = vec_init(fn, 1, sv->len + 1);
	if (!sb) return NULL;
	return strbuf_cpy(sb, sv);
}
char *strbuf_svcat(char *sb, const strview_t *sv) {
	vec_pop(sb, 1, NULL);
	if (!(sb = vec_push(sb, sv->len, NULL))) return NULL;
	return vec_push(sb, 1, &(char){ '\0' });
}
static inline strview_t strbuf_to_strview(char *sb) {
	return (strview_t){ .str = sb, .len = *vec_len(sb) };
}

#endif

//
// EK_USE_HASH
//
#if EK_USE_HASH
#if !EK_USE_VEC
#	error ek.h: include the EK_USE_VEC feature to use hashmaps
#endif
#if !EK_USE_UTIL
#	error ek.h: include the EK_USE_UTIL feature to use hashmaps
#endif

typedef uint64_t (hset_hash_fn)(const void *data);
typedef bool (hset_eq_fn)(const void *a, const void *b);

// xxhash64 single lange implementation
uint64_t xxhash64_single_lane(const uint8_t *data, size_t len);

// Returns the array of keyvalue pairs
void *hset_init(mem_alloc_t alloc,
		uint32_t capacity, uint32_t kvsize,
		hset_hash_fn *hash, hset_eq_fn *eq);

// Frees the hashset
void *hset_deinit(void *set);

// Insert a new kv pair into the hashset, where it inserts according to
// the key. If kv already exists, the kv pair will be overwritten.
// Returns new pointer to the hashset if it needed to grow
// Note: kv is non-const because it is used and chagned when swapping
// entries
void *hset_insert(void *set, const void *kv);

// Remove pre-existing keyvalue pair. Pointer must be to kv in the hashset.
void hset_remove(void *set, const void *kv);

// Returns kv pair in the hashset that has the same key as key
void *hset_get(void *set, const void *key);

// Loops through a hashset.
// if iter is NULL, it will return the first kv pair it can find.
// will return NULL when there is no kv pairs left
void *hset_next(void *set, void *iter);

uint64_t str_hash(const char *str);
bool str_eq(const char *a, const char *b);

#if EK_USE_STRVIEW
uint64_t strview_hash(const strview_t *sv);
#endif

#endif

//
// EK_USE_ARENA
//
#if EK_USE_ARENA

bool fixed_arena_init(void *buf, size_t bufsize);
void *fixed_arena_alloc(void *buf, size_t size);
void fixed_arena_reset_to(void *buf, void *to);
mem_alloc_t fixed_arena_alloc_fn(void *buf);

#endif

//
// EK_USE_POOL
//
#if EK_USE_POOL

typedef struct dynpool dynpool_t;

dynpool_t *dynpool_init(mem_alloc_t alloc, size_t initial_chunks, size_t elemsz);
void dynpool_deinit(dynpool_t *self);
void *dynpool_alloc(dynpool_t *self);
void dynpool_free(dynpool_t *self, void *ptr);
bool dynpool_empty(const dynpool_t *self);
void dynpool_fast_clear(dynpool_t *self);
size_t dynpool_num_chunks(const dynpool_t *self);

void fixedpool_init(void *buf, size_t elemsz, size_t buflen);
void fixedpool_fast_clear(void *buf);
void *fixedpool_alloc(void *buf);
void fixedpool_free(void *buf, void *blk);
bool fixedpool_empty(const void *buf);

#endif

//
// EK_USE_TEST
//
#if EK_USE_TEST

bool test_bad(const char *file, int line);
#define TEST_BAD (test_bad(__FILE__, __LINE__))

typedef bool (test_fn)(void);
typedef struct test_s {
    test_fn *pfn;
    const char *name;
} test_t;

// Helper macro for adding tests
#define TEST_ADD(func_name) ((test_t){ .pfn = func_name, .name = #func_name }),

// Adds padding into the test output
#define TEST_PAD ((test_t){ .pfn = NULL, .name = NULL }),

// Returns true if all tests passed
bool tests_run_foreach(bool (*setup_test)(const test_t *test),
		const test_t *test_list, size_t list_len, FILE *out);

#endif

#endif


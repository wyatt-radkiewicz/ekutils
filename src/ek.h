#ifndef _ek_h_
#define _ek_h_

#ifndef EK_FEATURE_OFF
#	define EK_FEATURE_OFF 0
#endif


// Change EK_FEATURE_OFF to 1 if you want to use the feature
#define EK_USE_MATH EK_FEATURE_OFF
#define EK_USE_UTIL EK_FEATURE_OFF
#define EK_USE_STRVIEW EK_FEATURE_OFF
#define EK_USE_LOG EK_FEATURE_OFF
#define EK_USE_VEC EK_FEATURE_OFF
#define EK_USE_UTF8 EK_FEATURE_OFF
#define EK_USE_TEST EK_FEATURE_OFF

//
// standard library includes
//
#if EK_USE_STRVIEW
#	include <stdbool.h>
#	include <string.h>
#endif
#if EK_USE_LOG
#	include <stdarg.h>
#endif
#if EK_USE_STRVIEW || EK_USE_VEC
#	include <stddef.h>
#endif
#if EK_USE_UTF8 || EK_USE_VEC
#	include <stdint.h>
#endif
#if EK_USE_TEST
#	include <stdio.h>
#endif

//
// Always included in ek.h.
// Change these to your own allocation functions.
// These are meant if you need general purpose allocation.
// There are also general purpose allocation interfaces for runtime
//
#define EK_MALLOC malloc
#define EK_REALLOC realloc
#define EK_FREE free

// Used to realloc, free and malloc
typedef void *(ek_realloc_fn)(void *user, void *prev_block, size_t new_size);

//
// EK_USE_UTIL
//
#if EK_USE_UTIL
#	define arrlen(a) (sizeof(a) / sizeof((a)[0]))
#endif

//
// EK_USE_MATH
//
#if EK_USE_MATH
#	define max(a, b) ({ \
		typeof(a) _a = a; \
		typeof(b) _b = b; \
		_a > _b ? _a : _b; \
	})
#	define min(a, b) ({ \
		typeof(a) _a = a; \
		typeof(b) _b = b; \
		_a < _b ? _a : _b; \
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
size_t strview_to_str(char *str, size_t len, const strview_t *const view);
#endif

//
// EK_USE_UTF8
//
#if EK_USE_UTF8
static inline int utf8_codepoint_len(const uint8_t start) {
	const int clz = __builtin_clz(~start << 24);
	return clz + !clz;
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
	size_t len;
	size_t capacity;
	size_t elem_size;
	uint8_t data[];
} vec_t;

// Returns a pointer starting at the data part
void *vec_init(size_t elem_size, size_t capacity);
void vec_deinit(void *data);

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

#define TEST_LIST_START(name) static const test_t name[] = {
#define TEST_LIST_END };

// Returns true if all tests passed
bool tests_run_foreach(bool (*setup_test)(const test_t *test),
		const test_t *test_list, size_t list_len, FILE *out);

#endif

#endif


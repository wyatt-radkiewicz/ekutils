#include "ek.h"

#if EK_USE_VEC
#	include <stdlib.h>
#endif

//
// EK_USE_STRVIEW
//
#if EK_USE_STRVIEW
size_t strview_to_str(char *str, size_t len, const strview_t *const view) {
	len -= 1;
	if (view->len < len) len = view->len;
	memcpy(str, view->str, len);
	str[len] = '\0';
	return len;
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
void vec_deinit(void *data) {
	EK_FREE(vec_from_data(data));
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



#include <stdio.h>

#include "../ek.h"

// EK_USE_TEST
bool test_test1(void) {
	return true;
}

// EK_USE_STRVIEW
bool test_strview1(void) {
	strview_t a = make_strview("hello");
	strview_t b = make_strview("hello");

	if (!strview_eq(&a, &b)) return TEST_BAD;

	return true;
}
bool test_strview2(void) {
	strview_t a = make_strview("hello");
	strview_t b = make_strview("world");

	if (strview_eq(&a, &b)) return TEST_BAD;

	return true;
}
bool test_strview3(void) {
	char s[64];
	strview_t v = make_strview("hello world!");

	if (strview_buf_str(s, arrlen(s), &v) != v.len) return TEST_BAD;
	if (strcmp(s, "hello world!") != 0) return TEST_BAD;
	
	return true;
}
bool test_strview4(void) {
	char s[5];
	strview_t v = make_strview("world!");

	if (strview_buf_str(s, arrlen(s), &v) != 4) return TEST_BAD;
	if (strcmp(s, "worl") != 0) return TEST_BAD;
	
	return true;
}

typedef struct test_person {
	strview_t name;
	int age;
} test_person_t;

bool test_xxhash64_single_lane(void) {
	if (xxhash64_single_lane((const uint8_t *)"hello", 5) !=
		xxhash64_single_lane((const uint8_t *)"hello world!", 5)) return TEST_BAD;
	return true;
}

bool test_hset1(void) {
	test_person_t *map = hset_init(mem_stdlib_alloc(), 4, sizeof(*map),
					(hset_hash_fn *)strview_hash,
					(hset_eq_fn *)strview_eq);
	if (!map) return TEST_BAD;

	map = hset_insert(map, &(test_person_t){ .name = make_strview("carl"), .age = 23 });
	map = hset_insert(map, &(test_person_t){ .name = make_strview("chloe"), .age = 23 });
	map = hset_insert(map, &(test_person_t){ .name = make_strview("wyatt"), .age = 20 });
	map = hset_insert(map, &(test_person_t){ .name = make_strview("kagami"), .age = 18 });
	map = hset_insert(map, &(test_person_t){ .name = make_strview("konata"), .age = 18 });
	map = hset_insert(map, &(test_person_t){ .name = make_strview("tsukasa"), .age = 18 });

	struct {
		strview_t name;
		int ntimes_found;
	} sv[] = {
		{ .name = make_strview("carl") },
		{ .name = make_strview("chloe") },
		{ .name = make_strview("wyatt") },
		{ .name = make_strview("kagami") },
		{ .name = make_strview("konata") },
		{ .name = make_strview("tsukasa") },
	};

	if (!hset_get(map, sv + 0)) return TEST_BAD;
	if (!hset_get(map, sv + 1)) return TEST_BAD;
	if (!hset_get(map, sv + 2)) return TEST_BAD;
	if (!hset_get(map, sv + 3)) return TEST_BAD;
	if (!hset_get(map, sv + 4)) return TEST_BAD;
	if (!hset_get(map, sv + 5)) return TEST_BAD;

	for (strview_t *iter = hset_next(map, NULL); iter;
			iter = hset_next(map, iter)) {
		for (int i = 0; i < arrlen(sv); i++) {
			if (!strview_eq(iter, &sv[i].name)) continue;
			if (!sv[i].ntimes_found++) continue;
			fprintf(stderr, "%s found multiple times!\n",
				strview_as_str(&sv[i].name));
			return TEST_BAD;
		}
	}

	hset_remove(map, hset_get(map, sv + 2));
	if (hset_get(map, sv + 2)) return TEST_BAD;
	if (!hset_get(map, sv + 4)) return TEST_BAD;
	hset_remove(map, hset_get(map, sv + 5));
	if (hset_get(map, sv + 5)) return TEST_BAD;

	hset_deinit(map);
	return true;
}

static const test_t tests[] = {
	TEST_ADD(test_test1)
	TEST_PAD
	TEST_ADD(test_strview1)
	TEST_ADD(test_strview2)
	TEST_ADD(test_strview3)
	TEST_ADD(test_strview4)
	TEST_PAD
	TEST_ADD(test_xxhash64_single_lane)
	TEST_PAD
	TEST_ADD(test_hset1)
};

int main(int argc, char **argv) {
	return !tests_run_foreach(NULL, tests, arrlen(tests), stdout);
}


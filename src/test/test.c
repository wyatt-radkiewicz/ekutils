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

	if (strview_to_str(s, arrlen(s), &v) != v.len) return TEST_BAD;
	if (strcmp(s, "hello world!") != 0) return TEST_BAD;
	
	return true;
}
bool test_strview4(void) {
	char s[5];
	strview_t v = make_strview("world!");

	if (strview_to_str(s, arrlen(s), &v) != 4) return TEST_BAD;
	if (strcmp(s, "worl") != 0) return TEST_BAD;
	
	return true;
}

TEST_LIST_START(tests)
	TEST_ADD(test_test1)
	TEST_PAD
	TEST_ADD(test_strview1)
	TEST_ADD(test_strview2)
	TEST_ADD(test_strview3)
	TEST_ADD(test_strview4)
TEST_LIST_END

int main(int argc, char **argv) {
	return !tests_run_foreach(NULL, tests, arrlen(tests), stdout);
}


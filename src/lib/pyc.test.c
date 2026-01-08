#include "pyc.h"
#include <criterion/criterion.h>
#include <string.h>

Test(next, str) {
    struct str stack = STR("hello world\n");
}

Test(ss, length_copy) {
    char greeting[] = "hello world";
    struct string s = ss(greeting, sizeof(greeting) - 1);

    cr_assert_eq(
        s.length,
        sizeof(greeting) - 1,
        "ss() should copy length"
    );
}


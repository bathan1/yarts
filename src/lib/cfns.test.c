#include "cfns.h"
#include <criterion/criterion.h>
#include <string.h>

Test(ss, shallow_copy_buffer) {
    char stack_greeting[] = "hello world";
    char *heap_greeting = strdup(stack_greeting);

    struct string stack_string = ss(stack_greeting, sizeof(stack_greeting) - 1);
    struct string heap_string = ss(heap_greeting, sizeof(stack_greeting) - 1);

    cr_assert_eq(
        stack_string.hd,
        stack_greeting,
        "ss() should shallow copy stack pointer"
    );

    cr_assert_eq(
        heap_string.hd,
        heap_greeting,
        "ss() should shallow copy heap pointer"
    );
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


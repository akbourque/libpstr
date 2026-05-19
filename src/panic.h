#ifndef PANIC_H
#define PANIC_H

#include <stddef.h>
#include <stdbool.h>

typedef void (*panic_handler_t)(const char* file, unsigned line, const char* msg);

void set_panic_handler(panic_handler_t handler);
extern _Noreturn void panic(const char* file, unsigned line, const char* msg);

#define PANIC(msg) (panic(__FILE__, __LINE__, (msg)))

/* --- Standalone Testing Infrastructure --- */
#ifdef ENABLE_PANIC_TESTING
#include <setjmp.h>
#include <assert.h>

extern jmp_buf testing_jmp_buf;
extern bool testing_panic_triggered;

void testing_panic_catch_handler(const char* f, unsigned l, const char* m);

#define ASSERT_PANIC(code_block) do { \
    testing_panic_triggered = false; \
    set_panic_handler(testing_panic_catch_handler); \
    if (setjmp(testing_jmp_buf) == 0) { \
        code_block; \
    } \
    set_panic_handler(NULL); \
    assert(testing_panic_triggered && "Code block failed to panic!"); \
} while (0)
#endif 

#endif

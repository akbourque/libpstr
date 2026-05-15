#include "panic.h"
#include <stdio.h>
#include <stdlib.h>

static void default_handler(const char* f, unsigned l, const char* m) {
    fprintf(stderr, "panic in: '%s' line: %u\nmessage: %s\n", f, l, m);
    exit(EXIT_FAILURE);
}

static panic_handler_t current_handler = default_handler;

void set_panic_handler(panic_handler_t handler) {
    current_handler = (handler == NULL) ? default_handler : handler;
}

#ifdef ENABLE_PANIC_TESTING
jmp_buf testing_jmp_buf;
bool testing_panic_triggered = false;

void testing_panic_catch_handler(const char* , unsigned , const char* ) {
    testing_panic_triggered = true;
    longjmp(testing_jmp_buf, 1);
}
#endif

_Noreturn void panic(const char* file, unsigned line, const char* msg) {
    current_handler(file, line, msg);
    exit(EXIT_FAILURE); 
}

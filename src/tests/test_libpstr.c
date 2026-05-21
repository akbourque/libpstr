#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "libpstr.h"
#include "panic.h"

/* --- Legacy Mock Transform Drivers Migrated to 1.0.0 Syntax --- */

static uint32_t shift_transform(uint32_t cp) {
    return cp + 1;
}

static uint32_t invalid_transform(uint32_t cp) {
    if (cp == '!') return 0x110000; // Out of Unicode range threshold
    return cp;
}

static uint32_t grow_fn(uint32_t cp) {
    return (cp == '!') ? 0x1F680 : cp; // Replace '!' with 4-byte Rocket emoji
}

static void test_pstr_delegation_and_safety(void) {
    printf("  -> Executing libpstr.pstr delegation & UTF-8 safety assertions...\n");

    // 1. Trim Delegation
    libpstr_pstr_t *s1 = libpstr.pstr.from_cstr(" \t Hello API \n ");
    libpstr_slice_t trim_res = libpstr.pstr.trim(s1);
    assert(trim_res.len == 9);
    assert(memcmp(trim_res.ptr, "Hello API", 9) == 0);
    libpstr.pstr.free(s1);

    // 2. Starts/Ends With Delegation
    libpstr_pstr_t *s2 = libpstr.pstr.from_cstr("🚀 Launch Sequence");
    assert(libpstr.pstr.starts_with(s2, "🚀") == true);
    assert(libpstr.pstr.starts_with(s2, "Launch") == false);
    assert(libpstr.pstr.ends_with(s2, "Sequence") == true);
    assert(libpstr.pstr.ends_with(s2, "🚀") == false);
    libpstr.pstr.free(s2);

    // 3. Split Once Delegation
    libpstr_pstr_t *s3 = libpstr.pstr.from_cstr("key=value=extra");
    libpstr_slice_t left, right;
    assert(libpstr.pstr.split_once(s3, "=", &left, &right) == true);
    assert(left.len == 3);
    assert(memcmp(left.ptr, "key", 3) == 0);
    assert(right.len == 11);
    assert(memcmp(right.ptr, "value=extra", 11) == 0);
    libpstr.pstr.free(s3);

    // 4. Strict UTF-8 Validation Rejection
    const char bad_utf8[] = { 'a', 'b', (char)0xFF, 'c' };
    libpstr_slice_t bad_slice = { .ptr = bad_utf8, .len = 4 };
    
    assert(libpstr.slice.trim(bad_slice).ptr == NULL);
    assert(libpstr.slice.starts_with(bad_slice, "ab") == false);
    assert(libpstr.slice.ends_with(bad_slice, "c") == false);
    
    // 5. UTF-8 Boundary Enforcement (Prevent splitting multi-byte chars)
    // U+1F680 is 4 bytes: F0 9F 9A 80
    const char rocket[] = "A\xF0\x9F\x9A\x80Z"; 
    libpstr_slice_t rocket_slice = { .ptr = rocket, .len = 6 };
    
    // Delimiter is maliciously just the inner two bytes of the rocket emoji
    const char bad_delim[] = "\x9F\x9A"; 
    
    // The memmem will find it, but our new boundary check must catch and reject it!
    assert(libpstr.slice.split_once(rocket_slice, bad_delim, &left, &right) == false);
}

/* =========================================================================
 * 🔬 PART 1: THE NEW 1.0.0 NAMESPACE UTILITY ASSERTIONS
 * ========================================================================= */

static void test_slice_trim_new(void) {
    printf("  -> Executing libpstr.slice.trim assertions...\n");

    libpstr_slice_t src1 = { .ptr = "  \t  hello systems dev  \r\n ", .len = 27 };
    libpstr_slice_t res1 = libpstr.slice.trim(src1);
    assert(res1.len == 17);
    assert(memcmp(res1.ptr, "hello systems dev", 17) == 0);

    const char nbs_buf[] = { 0xC2, 0xA0, 'x', 'y', 'z', 0xC2, 0xA0 }; // U+00A0 non-breaking space
    libpstr_slice_t src2 = { .ptr = nbs_buf, .len = 7 };
    libpstr_slice_t res2 = libpstr.slice.trim(src2);
    assert(res2.len == 3);
    assert(memcmp(res2.ptr, "xyz", 3) == 0);
}

static void test_slice_split_once_new(void) {
    printf("  -> Executing libpstr.slice.split_once assertions...\n");

    libpstr_slice_t input = { .ptr = "compiler_target=cpack_test", .len = 26 };
    libpstr_slice_t left, right;

    bool matched = libpstr.slice.split_once(input, "=", &left, &right);
    assert(matched == true);
    assert(left.len == 15);
    assert(memcmp(left.ptr, "compiler_target", 15) == 0);
    assert(right.len == 10);
    assert(memcmp(right.ptr, "cpack_test", 10) == 0);
}

static void test_pstr_strip_suffix_new(void) {
    printf("  -> Executing libpstr.pstr.strip_suffix assertions...\n");

    libpstr_pstr_t *s = libpstr.pstr.from_cstr("cpack_vector_t");
    assert(s != NULL);

    libpstr_slice_t slice1 = libpstr.pstr.strip_suffix(s, "_t");
    assert(slice1.len == 12);
    assert(memcmp(slice1.ptr, "cpack_vector", 12) == 0);

    libpstr.pstr.free(s);
}

static void test_case_transformations_new(void) {
    printf("  -> Executing libpstr owned case mutation assertions...\n");

    libpstr_pstr_t *s = libpstr.pstr.from_cstr("Fedora 44 x86_64");
    assert(s != NULL);

    libpstr_status_t status1 = libpstr.pstr.to_uppercase(s);
    assert(status1 == LIBPSTR_OK);
    assert(strcmp(s->buf, "FEDORA 44 X86_64") == 0);

    libpstr_status_t status2 = libpstr.pstr.to_lowercase(s);
    assert(status2 == LIBPSTR_OK);
    assert(strcmp(s->buf, "fedora 44 x86_64") == 0);

    libpstr.pstr.free(s);
}

/* =========================================================================
 * 🔬 PART 2: THE CONVERTED LEGACY INTERFACE TESTS
 * ========================================================================= */

static void basic_test(void) {
    printf("  -> Running legacy basic tests...\n");
    {
        const char* str = "Hello World!";
        libpstr_pstr_t *s = libpstr.pstr.from_cstr(str);
        assert(s);
        assert(s->len == strlen(str));
        assert(strcmp(s->buf, str) == 0);
        libpstr.pstr.free(s);

        str = "Result: 42 append testing";
        libpstr_builder_t sb;
        libpstr.builder.init(&sb);
        libpstr.builder.appendf(&sb, "Result: %d", 42);
        libpstr.builder.appendf(&sb, " append testing");
        s = libpstr.builder.build(&sb);
        libpstr.builder.cleanup(&sb);
        assert(s);
        assert(s->len == strlen(str));
        assert(strcmp(s->buf, str) == 0);
        libpstr.pstr.free(s);
    }
    {
        const char* str = "Result: 42 append testing";
        libpstr_builder_t sb;
        libpstr.builder.init(&sb);
        libpstr.builder.append_cstr(&sb, "Result: 42");
        libpstr.builder.append_cstr(&sb, " append testing");
        libpstr_pstr_t* s = libpstr.builder.build(&sb);
        libpstr.builder.cleanup(&sb);
        assert(s);
        assert(s->len == strlen(str));
        assert(strcmp(s->buf, str) == 0);
        libpstr.pstr.free(s);
    }
}

static void test_builder_ensure(void) {
    printf("  -> Running builder ensure tests...\n");
    const char* str = "The quick brown fox jumps over the lazy hound.";
    size_t len = strlen(str);
    size_t count = 1 + LIBPSTR_BUILDER_SBO / len;
    
    libpstr_builder_t sb;
    libpstr.builder.init(&sb);
    for (size_t n = 0; n < count; ++n) {
        libpstr.builder.append_cstr(&sb, str);
    }
    assert(sb.vec.data != NULL);
    assert(sb.vec.len == len * count);
    assert(sb.vec.cap > LIBPSTR_BUILDER_SBO);
    assert((void*)sb.sbo != (void*)sb.vec.data);
    
    libpstr_pstr_t* s = libpstr.builder.build(&sb);
    libpstr.builder.cleanup(&sb);
    assert(s->len == len * count);
    assert(strncmp(s->buf, str, len) == 0);
    assert(strncmp(s->buf + s->len - len, str, len) == 0);
    libpstr.pstr.free(s);
}

static void test_utf8_iter(void) {
    printf("  -> Running UTF-8 iterator tests...\n");
    libpstr_builder_t sb;
    libpstr.builder.init(&sb);
    
    libpstr.builder.append_cstr(&sb, "Hello ");
    assert(strcmp(sb.sbo, "Hello ") == 0);
    libpstr.builder.append_utf8(&sb, 0x1F680); // 🚀
    
    libpstr_pstr_t *s = libpstr.builder.build(&sb);
    assert(s);
    assert(s->len == 10);    
    assert(libpstr.utf8.len(s) == 7);
    assert(memcmp(s->buf, "Hello \xF0\x9F\x9A\x80", s->len) == 0);

    libpstr_iter_t it;
    libpstr.utf8.iter.init(&it, s);
    int char_count = 0;
    while (libpstr.utf8.iter.next(&it)) {
        char_count++;
    }
    assert(char_count == 7);

    libpstr.builder.cleanup(&sb);
    libpstr.pstr.free(s);
}

static void test_substring(void) {
    printf("  -> Running substring tests...\n");
    libpstr_pstr_t *s = libpstr.pstr.from_cstr("A🚀B");
    assert(s);
    assert(s->len == 6);
    
    libpstr_slice_t sub1 = libpstr.slice.substring(s, 1, 4);
    assert(sub1.ptr);
    assert(sub1.len == 4);
    assert(memcmp(sub1.ptr, "\xF0\x9F\x9A\x80", 4) == 0);

    // Assert protective boundary panics
    ASSERT_PANIC(libpstr.slice.substring(s, 10, 1));
    ASSERT_PANIC(libpstr.slice.substring(s, 1, 10)); // Strict 1.0.0 boundary enforcement

    libpstr.pstr.free(s);
}

static void test_vec(void) {
    printf("  -> Running byte vector primitives tests...\n");
    libpstr_vec_t v;
    
    libpstr.vec.init(&v, 4);
    assert(v.cap == 4);
    assert(v.len == 0);

    for (uint8_t i = 0; i < 10; i++) {
        libpstr.vec.push(&v, i);
    }

    assert(v.len == 10);
    assert(v.cap >= 10);
    assert(v.data[0] == 0);
    assert(v.data[9] == 9);

    libpstr.vec.swap_remove(&v, 2); 
    assert(v.len == 9);
    assert(v.data[2] == 9); 
    
    libpstr.vec.remove(&v, 1);
    assert(v.len == 8);
    assert(v.data[1] == 9);
    assert(v.data[2] == 3);

    libpstr.vec.pop(&v);
    assert(v.len == 7);

    libpstr.vec.resize(&v, 256);
    assert(v.len == 256);
    assert(v.data[1] == 9);
    assert(v.data[6] == 7);
    
    libpstr.vec.free(&v);
    assert(v.data == NULL);
}

static void test_replace_range(void) {
    printf("  -> Running builder replace_range tests...\n");
    libpstr_builder_t sb;
    libpstr.builder.init(&sb);
    
    libpstr.builder.append_cstr(&sb, "I 🍎 C");
    
    libpstr_status_t status = libpstr.builder.replace_range(&sb, 2, 4, "🚀", 4);
    assert(status == LIBPSTR_OK);
    assert(strcmp((char*)sb.vec.data, "I 🚀 C") == 0);
    assert(sb.vec.len == 8);

    libpstr.builder.replace_range(&sb, 2, 0, "very ", 5);
    assert(strcmp((char*)sb.vec.data, "I very 🚀 C") == 0);
    assert(sb.vec.len == 13);

    libpstr.builder.replace_range(&sb, 2, 5, NULL, 0); 
    assert(strcmp((char*)sb.vec.data, "I 🚀 C") == 0);
    assert(sb.vec.len == 8);

    libpstr.builder.replace_range_cstr(&sb, 7, 1, "Code");
    assert(strcmp((char*)sb.vec.data, "I 🚀 Code") == 0);

    libpstr.builder.cleanup(&sb);
}

static void test_utf8_validation(void) {
    printf("  -> Running UTF-8 layout validation tests...\n");

    assert(libpstr.utf8.validate_cstr("Hello") == true);
    assert(libpstr.utf8.validate_cstr("🚀 Rocket") == true);
    assert(libpstr.utf8.validate_cstr("κόσμε") == true);

    assert(libpstr.utf8.validate_cstr("\x80") == false);
    assert(libpstr.utf8.validate_cstr("\xFE") == false);

    assert(libpstr.utf8.validate("\xF0\x9F\x9A", 3) == false);
    assert(libpstr.utf8.validate("\xC1\x81", 2) == false);

    libpstr_pstr_t* bad = libpstr.pstr.from_cstr("\xFF\xFF");
    assert(bad == NULL);

    libpstr_builder_t sb;
    libpstr.builder.init(&sb);
    libpstr_status_t status = libpstr.builder.append_cstr(&sb, "\xED\xA0\x80");
    assert(status == LIBPSTR_ERR_UTF8);

    libpstr.builder.cleanup(&sb);
}

static void test_utf8_lossy(void) {
    printf("  -> Running UTF-8 repair lossy tests...\n");
    
    const char *bad_input = "Hi \xFF!"; 
    libpstr_pstr_t *s = libpstr.pstr.from_utf8_lossy(bad_input, 5);
    
    assert(s != NULL);
    assert(s->len == 7);
    assert(libpstr.utf8.len(s) == 5);
    
    libpstr.pstr.free(s);
}

static void test_transform(void) {
    printf("  -> Running string structural transformation tests...\n");

    {
        libpstr_pstr_t *s = libpstr.pstr.from_cstr("abc");
        libpstr_status_t status = libpstr.pstr.transform(&s, shift_transform);
        assert(status == LIBPSTR_OK);
        assert(strcmp(s->buf, "bcd") == 0);
        assert(s->len == 3);
        libpstr.pstr.free(s);
    }
    {
        libpstr_pstr_t *s = libpstr.pstr.from_cstr("Hi 🚀");
        libpstr_status_t status = libpstr.pstr.transform(&s, shift_transform);
        assert(status == LIBPSTR_OK);
        assert(s->len == 7);
        libpstr.pstr.free(s);
    }
    {
        libpstr_pstr_t *s = libpstr.pstr.from_cstr("Go!");
        libpstr_status_t status = libpstr.pstr.transform(&s, grow_fn);
        assert(status == LIBPSTR_OK);
        assert(s->len == 6);
        assert(strcmp(s->buf, "Go🚀") == 0);
        libpstr.pstr.free(s);
    }
    {
        libpstr_pstr_t *s = libpstr.pstr.from_cstr("Safe!");
        libpstr_pstr_t *original_ptr = s;

        libpstr_status_t status = libpstr.pstr.transform(&s, invalid_transform);
        assert(status == LIBPSTR_ERR_UTF8);
        assert(s == original_ptr);
        assert(strcmp(s->buf, "Safe!") == 0);
        libpstr.pstr.free(s);
    }
}

static void test_utilities(void) {
    printf("  -> Running matching and list segmentation tests...\n");

    {
        libpstr_pstr_t *s = libpstr.pstr.from_cstr(" \n 🚀 ");
        // Upgrade: slice.trim expects a slice parameter natively!
        libpstr_slice_t sliced = libpstr.slice.trim((libpstr_slice_t){s->buf, s->len});
        
        assert(sliced.len == 4); 
        assert(memcmp(sliced.ptr, "\xF0\x9F\x9A\x80", 4) == 0);
        
        libpstr_pstr_t *clean = libpstr.pstr.from_slice(sliced);
        assert(clean->len == 4);
        libpstr.pstr.free(clean);
        libpstr.pstr.free(s);
    }
    {
        libpstr_pstr_t *s = libpstr.pstr.from_cstr("apple,banana,cherry");
        libpstr_list_t list;
        libpstr.list.init(&list);

        libpstr_status_t status = libpstr.slice.split(s, ",", &list);
        assert(status == LIBPSTR_OK);

        size_t count = libpstr.list.len(&list);
        assert(count == 3);

        libpstr_slice_t b = libpstr.list.get(&list, 1);
        assert(b.len == 6);
        assert(strncmp(b.ptr, "banana", 6) == 0);

        libpstr.list.free(&list);
        libpstr.pstr.free(s);
    }
    {
        libpstr_pstr_t *s = libpstr.pstr.from_cstr("lib_pstr.so");
        libpstr_slice_t sl = {s->buf, s->len};
        assert(libpstr.slice.starts_with(sl, "lib_") == true);
        assert(libpstr.slice.ends_with(sl, ".so") == true);
        assert(libpstr.slice.ends_with(sl, ".dll") == false);
        libpstr.pstr.free(s);
    }
}

static void test_adversarial(void) {
    printf("  -> Running adversarial null/utf8 input safety tests...\n");

    libpstr_builder_t sb;
    libpstr.builder.init(&sb);
    assert(libpstr.builder.appendf(&sb, NULL) == LIBPSTR_ERR_NULL);

    libpstr_pstr_t *s = libpstr.pstr.from_cstr("Hello");
    libpstr_slice_t bad_match = libpstr.slice.find_cstr(s, "\xFF\xFF");
    assert(bad_match.ptr == NULL); 
    
    libpstr_status_t status = libpstr.builder.replace_range(&sb, 0, 0, "\xFF", 1);
    assert(status == LIBPSTR_ERR_UTF8);

    libpstr.pstr.free(s);
    libpstr.builder.cleanup(&sb);
}

static void test_defensive_panic_boundaries(void) {
    printf("  -> Running out-of-bounds defensive panic assertions...\n");

    libpstr_pstr_t *s = libpstr.pstr.from_cstr("A🚀B");

    ASSERT_PANIC(libpstr.slice.substring(s, 2, 2)); 
    ASSERT_PANIC(libpstr.slice.substring(s, 10, 1));

    libpstr_list_t empty_list;
    libpstr.list.init(&empty_list);
    ASSERT_PANIC(libpstr.list.pop(&empty_list));

    libpstr.pstr.free(s);
    libpstr.list.free(&empty_list);
}

static void test_file_io(void) {
    printf("  -> Running file streaming and slurp tests...\n");
    const char* const str = "Line 1: Hello\nLine 2: 🚀\nLine 3: Goodbye";
    const char* filename = "test_io.txt";
    FILE* f = fopen(filename, "w");
    fprintf(f, "%s", str);
    fclose(f);

    libpstr_pstr_t* full = libpstr.pstr.read_from_file(filename);
    assert(full != NULL);
    assert(libpstr.utf8.len(full) == 39);
    libpstr.pstr.free(full);

    libpstr_builder_t sb;
    libpstr.builder.init(&sb);
    FILE* in = fopen(filename, "r");
    
    assert(libpstr.builder.read_line(&sb, in) == LIBPSTR_OK);
    assert(libpstr.builder.find_cstr(&sb, "Hello").ptr != NULL);
    
    assert(libpstr.builder.read_line(&sb, in) == LIBPSTR_OK);
    assert(libpstr.utf8.validate_cstr((char*)sb.vec.data) == true);
    
    assert(libpstr.builder.read_line(&sb, in) == LIBPSTR_OK);
    assert(libpstr.builder.find_cstr(&sb, "Goodbye").ptr != NULL);

    assert(libpstr.builder.read_line(&sb, in) == LIBPSTR_ERR_BOUNDS);

    fclose(in);
    libpstr.builder.cleanup(&sb);
    remove(filename);
}

static void test_pstr_list_complete(void) {
    printf("  -> Running list sequential allocation storage metrics...\n");
    libpstr_list_t list;
    
    libpstr.list.init(&list);
    assert(libpstr.list.len(&list) == 0);
    
    libpstr_slice_t slice_a = { .ptr = "alpha", .len = 5 };
    libpstr_slice_t slice_b = { .ptr = "beta",  .len = 4 };
    libpstr_slice_t slice_c = { .ptr = "gamma", .len = 5 };
    
    libpstr.list.push(&list, slice_a);
    libpstr.list.push(&list, slice_b);
    libpstr.list.push(&list, slice_c);
    assert(libpstr.list.len(&list) == 3);
    
    libpstr_slice_t res_a = libpstr.list.get(&list, 0);
    assert(res_a.len == slice_a.len);
    assert(memcmp(res_a.ptr, slice_a.ptr, slice_a.len) == 0);
    
    libpstr_slice_t res_b = libpstr.list.get(&list, 1);
    assert(res_b.len == slice_b.len);
    assert(memcmp(res_b.ptr, slice_b.ptr, slice_b.len) == 0);
    
    libpstr_slice_t res_c = libpstr.list.get(&list, 2);
    assert(res_c.len == slice_c.len);
    assert(memcmp(res_c.ptr, slice_c.ptr, slice_c.len) == 0);

    size_t initial_count = libpstr.list.len(&list);
    size_t stress_additions = 50;
    libpstr_slice_t stress_slice = { .ptr = "stress_test_element_validation", .len = 30 };
    
    for (size_t i = 0; i < stress_additions; i++) {
        libpstr.list.push(&list, stress_slice);
    }
    
    assert(libpstr.list.len(&list) == (initial_count + stress_additions));
    
    libpstr_slice_t deep_check = libpstr.list.get(&list, libpstr.list.len(&list) - 1);
    assert(deep_check.len == stress_slice.len);
    assert(memcmp(deep_check.ptr, stress_slice.ptr, stress_slice.len) == 0);

    libpstr.list.free(&list);
    assert(list.vec.data == NULL);
    assert(list.vec.len == 0);
}

/* =========================================================================
 * 👑 DRIVER EXECUTION POINT
 * ========================================================================= */

int main(void) {
    printf("==================================================\n");
    printf("🔬 STARTING COMPLETE COMPREHENSIVE LIBPSTR 1.0.0 TEST SUITE\n");
    printf("==================================================\n");

    test_pstr_delegation_and_safety();
    test_slice_trim_new();
    test_slice_split_once_new();
    test_pstr_strip_suffix_new();
    test_case_transformations_new();
    
    basic_test();
    test_builder_ensure();
    test_utf8_iter();
    test_substring();
    test_vec();
    test_replace_range();
    test_utf8_validation();
    test_utf8_lossy();
    test_transform();
    test_utilities();
    test_adversarial();
    test_defensive_panic_boundaries();
    test_file_io();
    test_pstr_list_complete();

    printf("==================================================\n");
    printf("🚀 ALL MAINSTAGE AND LEGACY 1.0.0 TESTS PASSED CLEANLY!\n");
    printf("==================================================\n");
    return 0;
}

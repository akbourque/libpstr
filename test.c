#include <assert.h>
#include <stdio.h>
#ifndef ENABLE_PANIC_TESTING
#define ENABLE_PANIC_TESTING
#endif
#include "libpstr.h"

void basic_test();
void test_builder_ensure();
void test_utf8_iter();
void test_substring();
void test_vec();
void test_replace_range(); 
void test_utf8_validation();
void test_utf8_lossy();
void test_transform();
void test_utilities();
void test_adversarial();
void test_adversarial_boundaries();
void test_file_io();

int main() {
    printf("Tests running...\n");
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
    test_adversarial_boundaries();
    test_file_io();
    printf("Testing complete.\n");
    return 0;
}

void basic_test() {
    printf("\nbasic tests...\n");
    {
        const char* str = "Hello World!";
        pstr_t *s = pstr_from_cstr(str);
        assert(s);
        assert(s->len == strlen(str));
        assert(strcmp(s->buf, str) == 0);
        pstr_free(s);

        str = "Result: 42 append testing";
        pstr_builder_t sb;
        pstr_builder_init(&sb);
        pstr_builder_appendf(&sb, "Result: %d", 42);
        pstr_builder_appendf(&sb, " append testing");
        s = pstr_builder_build(&sb);
        pstr_builder_cleanup(&sb);
        assert(s);
        assert(s->len == strlen(str));
        assert(strcmp(s->buf, str) == 0);
        pstr_free(s);
    }
    {
        const char* str = "Result: 42 append testing";
        pstr_builder_t sb;
        pstr_builder_init(&sb);
        pstr_builder_append_cstr(&sb, "Result: 42");
        pstr_builder_append_cstr(&sb, " append testing");
        pstr_t* s = pstr_builder_build(&sb);
        pstr_builder_cleanup(&sb);
        assert(s);
        assert(s->len == strlen(str));
        assert(strcmp(s->buf, str) == 0);
        pstr_free(s);
    }
    printf("basic tests: PASSED\n");
}

void test_builder_ensure() {
    printf("\nbuilder ensure tests...\n");
    const char* str = "The quick brown fox jumps over the lazy hound.";
    size_t len = strlen(str);
    // make sure we exceed PSTR_BUILDER_SBO
    size_t count = 1 + PSTR_BUILDER_SBO / len;
    pstr_builder_t sb;
    pstr_builder_init(&sb);
    for (size_t n = 0; n < count; ++n) {
        pstr_builder_append_cstr(&sb, str);
    }
    assert(sb.vec.data != NULL);
    assert(sb.vec.len == len * count);
    assert(sb.vec.cap > PSTR_BUILDER_SBO);
    assert(sb.sbo != (char*)sb.vec.data);
    pstr_t* s = pstr_builder_build(&sb);
    pstr_builder_cleanup(&sb);
    assert(s->len == len * count);
    assert(strncmp(s->buf, str, len) == 0);
    assert(strncmp(s->buf + s->len - len, str, len) == 0);
    pstr_free(s);

    printf("builder.ensure test: PASSED\n");
}

void test_utf8_iter() {
    printf("\nutf8 iter tests...\n");
    pstr_builder_t sb;
    pstr_builder_init(&sb);
    
    // Append ASCII and a 4-byte Emoji
    pstr_builder_append_cstr(&sb, "Hello ");
    assert(strcmp(sb.sbo, "Hello ") == 0);
    pstr_builder_append_utf8(&sb, 0x1F680); // 🚀
    
    pstr_t *s = pstr_builder_build(&sb);
    // printf("String: %s\n", s->buf);
    // printf("Byte Length: %zu\n", s->len); // Should be 6 (Hello) + 4 (Rocket) = 10
    assert(s);
    assert(s->len == 10);    
    assert(pstr_utf8_len(s) == 7);
    assert(memcmp(s->buf, "Hello \xF0\x9F\x9A\x80", s->len) == 0);


    // Use the iterator to count characters
    pstr_iter_t it;
    pstr_iter_init(&it, s);
    int char_count = 0;
    while (pstr_iter_next(&it)) {
        // printf("Character found: U+%04X (size %d bytes)\n", it.code_point, it.byte_len);
        char_count++;
    }
    // printf("Logical Character Count: %d\n", char_count); // Should be 7 (Hello + space + Rocket)
    assert(char_count == 7);

    pstr_builder_cleanup(&sb);
    pstr_free(s);
    printf("utf8 iter test: PASSED\n");
}

void test_substring() {
    printf("\nsubstring tests...\n");
    pstr_t *s = pstr_from_cstr("A🚀B"); // 'A' (1b), 🚀 (4b), 'B' (1b)
    assert(s);
    assert(s->len == 6);
    
    // Test basic substring
    pstr_slice_t sub1 = pstr_substring(s, 1, 4);
    assert(sub1.ptr);
    assert(sub1.len == 4); // The emoji is 4 bytes
    assert(memcmp(sub1.ptr, "\xF0\x9F\x9A\x80", 4) == 0);

    // Test out of bounds start
    ASSERT_PANIC(pstr_substring(s, 10, 1));

    // Test count larger than remaining
    pstr_slice_t sub3 = pstr_substring(s, 1, 10);
    assert(sub3.ptr);
    assert(sub3.len == 5); // Should get "🚀B"

    pstr_free(s);
    printf("substring test: PASSED\n");
}

void test_substring_bytes() {
    printf("\nsubstring bytes tests...\n");
    pstr_t *s = pstr_from_cstr("A🚀B"); // Bytes: [A], [f0, 9f, 9a, 80], [B]
    
    // for (size_t n = 0; n < s ->len; ++n) {
    //     printf("byte %lu. 0x%X\n", n, (int)s->buf[n]);
    // }
    // Success: Index 1 is the start of the rocket (4 bytes long)
    // pstr_t *sub = pstr_substring(s, 1, 4); 
    // assert(sub != NULL);
    // assert(sub->len == 4);
    // pstr_free(sub);

    // This would trigger the assert if we uncommented it:
    // pstr_t *bad = pstr_substring(s, 2, 2); // Error: Index 2 is middle of rocket
    
    pstr_free(s);
    printf("substring bytes test: PASSED\n");
}

void test_vec() {
    printf("\nvector tests...\n");
    pstr_vec_t v;
    
    // 1. Test Initialization and Push
    pstr.vec.init(&v, 4); // Start small to force growth
    assert(v.cap == 4);
    assert(v.len == 0);

    for (uint8_t i = 0; i < 10; i++) {
        pstr.vec.push(&v, i);
    }

    assert(v.len == 10);
    assert(v.cap >= 10); // Should have grown (likely to 16)
    assert(v.data[0] == 0);
    assert(v.data[9] == 9);

    // 2. Test swap_remove (Branchless logic)
    // Vector is [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
    // Remove index 2 (value '2'). Should swap with index 9 (value '9')
    pstr.vec.swap_remove(&v, 2); 
    assert(v.len == 9);
    assert(v.data[2] == 9); // The '9' moved here
    
    // 3. Test remove (Shifting logic)
    // Vector is now [0, 1, 9, 3, 4, 5, 6, 7, 8]
    // Remove index 1 (value '1'). Should shift everything left.
    pstr.vec.remove(&v, 1);
    assert(v.len == 8);
    assert(v.data[1] == 9);
    assert(v.data[2] == 3);

    // 4. Test pop
    pstr.vec.pop(&v);
    assert(v.len == 7);

    pstr.vec.resize(&v, 256);
    assert(v.len == 256);
    assert(v.data[1] == 9);
    assert(v.data[6] == 7);
    
    pstr.vec.free(&v);
    assert(v.data == NULL);
    printf("vector tests: PASSED\n");
}

void test_replace_range() {
    printf("\nreplace_range tests...\n");
    pstr_builder_t sb;
    pstr.builder.init(&sb);
    
    // Initial state: "I 🍎 C"
    pstr.builder.append_cstr(&sb, "I 🍎 C");
    
    // 1. REPLACEMENT: Replace 🍎 (4 bytes) with 🚀 (4 bytes)
    // "I " is 2 bytes, so 🍎 starts at index 2.
    pstr_status_t status = pstr.builder.replace_range(&sb, 2, 4, "🚀", 4);
    assert(status == PSTR_OK);
    assert(strcmp((char*)sb.vec.data, "I 🚀 C") == 0);
    assert(sb.vec.len == 8); // "I "(2) + "🚀"(4) + " C"(2)

    // 2. INSERTION: Insert "very " before 🚀 (len=0)
    pstr.builder.replace_range(&sb, 2, 0, "very ", 5);
    assert(strcmp((char*)sb.vec.data, "I very 🚀 C") == 0);
    assert(sb.vec.len == 13);

    // 3. DELETION: Delete "very " (replace with NULL/empty, len=5)
    pstr.builder.replace_range(&sb, 2, 5, NULL, 0); 
    assert(strcmp((char*)sb.vec.data, "I 🚀 C") == 0);
    assert(sb.vec.len == 8);

    // 4. CONVENIENCE: Test the _cstr wrapper
    // Change "I 🚀 C" to "I 🚀 Code"
    pstr.builder.replace_range_cstr(&sb, 7, 1, "Code");
    assert(strcmp((char*)sb.vec.data, "I 🚀 Code") == 0);

    pstr.builder.cleanup(&sb);
    printf("replace_range tests: PASSED\n");
}

void test_utf8_validation() {
    printf("\nutf8 validation tests...\n");

    // 1. Valid Sequences
    assert(pstr.utf8.validate_cstr("Hello") == true);
    assert(pstr.utf8.validate_cstr("🚀 Rocket") == true);
    assert(pstr.utf8.validate_cstr("κόσμε") == true); // Greek

    // 2. Invalid Start Bytes
    assert(pstr.utf8.validate_cstr("\x80") == false); // Continuation byte as start
    assert(pstr.utf8.validate_cstr("\xFE") == false); // Invalid UTF-8 byte

    // 3. Truncated Sequences
    // 🚀 is \xF0\x9F\x9A\x80 (4 bytes). Let's try 3.
    assert(pstr.utf8.validate("\xF0\x9F\x9A", 3) == false);

    // 4. Overlong Encodings (Security risk!)
    // ASCII 'A' (0x41) encoded as 2 bytes (\xC1\x81)
    assert(pstr.utf8.validate("\xC1\x81", 2) == false);

    // 5. Test Integration with pstr_new
    pstr_t* bad = pstr.from_cstr("\xFF\xFF");
    assert(bad == NULL); // Should reject invalid data

    // 6. Test Integration with Builder
    pstr_builder_t sb;
    pstr.builder.init(&sb);
    pstr_status_t status = pstr.builder.append_cstr(&sb, "\xED\xA0\x80"); // Surrogate pair
    assert(status == PSTR_ERR_UTF8);

    pstr.builder.cleanup(&sb);
    printf("utf8 validation tests: PASSED\n");
}

void test_utf8_lossy() {
    printf("\nutf8 lossy tests...\n");
    
    // String with a "hole": valid, invalid (\xFF), valid
    const char *bad_input = "Hi \xFF!"; 
    pstr_t *s = pstr_from_utf8_lossy(bad_input, 5);
    
    assert(s != NULL);
    // Should be "Hi !" -> "Hi " (3) + "" (3 bytes) + "!" (1) = 7 bytes
    assert(s->len == 7);
    assert(pstr_utf8_len(s) == 5);
    
    pstr_free(s);
    printf("utf8 lossy tests: PASSED\n");
}


// Example transformation: A "Simple" Caesar Cipher for Unicode
// Shifts code points by 1 (e.g., 'A' -> 'B', '🚀' -> '🛸')
uint32_t shift_transform(uint32_t cp) {
    return cp + 1;
}

// A "Dangerous" transform that returns an invalid Unicode code point
uint32_t invalid_transform(uint32_t cp) {
    if (cp == '!') return 0x110000; // Out of Unicode range
    return cp;
}

// 'z' (1 byte) -> '{' (1 byte)
// Let's use a custom transform to force growth specifically
// Transformation: Replace '!' (1 byte) with '🚀' (4 bytes)
uint32_t grow_fn(uint32_t cp) {
    return (cp == '!') ? 0x1F680 : cp;
}

void test_transform() {
    printf("\ntransform tests...\n");

    // 1. Basic ASCII Test (Same Size)
    {
        pstr_t *s = pstr_from_cstr("abc");
        pstr_status_t status = pstr_transform(&s, shift_transform);
        assert(status == PSTR_OK);
        assert(strcmp(s->buf, "bcd") == 0);
        assert(s->len == 3);
        pstr_free(s);
    }

    // 2. Multi-byte UTF-8 Test (Same Size)
    {
        // 🚀 is 4 bytes. We shift it to 🛸 (also 4 bytes).
        pstr_t *s = pstr_from_cstr("Hi 🚀");
        pstr_status_t status = pstr_transform(&s, shift_transform);
        assert(status == PSTR_OK);
        // Verify byte length remains the same
        assert(s->len == 7); // "Hi " (3) + 🚀 (4)
        pstr_free(s);
    }

    // 3. Growth Test (Size Changes)
    {

        pstr_t *s = pstr_from_cstr("Go!");
        pstr_status_t status = pstr_transform(&s, grow_fn);
        
        assert(status == PSTR_OK);
        assert(s->len == 6); // "Go" (2) + "🚀" (4)
        assert(strcmp(s->buf, "Go🚀") == 0);
        pstr_free(s);
    }

    // 4. Transactional Safety Test (Error Handling)
    {
        pstr_t *s = pstr_from_cstr("Safe!");
        pstr_t *original_ptr = s;

        // This transform will fail when it hits the '!'
        pstr_status_t status = pstr_transform(&s, invalid_transform);
        
        assert(status == PSTR_ERR_UTF8);
        // CRITICAL: The pointer should NOT have changed, 
        // and the content should be exactly as it was.
        assert(s == original_ptr);
        assert(strcmp(s->buf, "Safe!") == 0);
        
        pstr_free(s);
    }

    printf("transform tests: PASSED\n");
}


void test_utilities() {
    printf("\nutility tests (trim/split/starts_with)...\n");

    // 1. Test Trim with UTF-8
    {
        // " \n 🚀 " -> Space (1), LF (1), Space (1), Rocket (4), Space (1) = 8 bytes
        pstr_t *s = pstr_from_cstr(" \n 🚀 ");
        pstr_slice_t sliced = pstr.trim(s);
        
        assert(sliced.len == 4); 
        assert(memcmp(sliced.ptr, "\xF0\x9F\x9A\x80", 4) == 0);
        
        pstr_t *clean = pstr.from_slice(sliced);
        assert(clean->len == 4);
        pstr.free(clean);
        pstr.free(s);
    }

    // 2. Test Split (Accumulator Style)
    {
        pstr_t *s = pstr_from_cstr("apple,banana,cherry");
        pstr_list_t list;
        pstr.list.init(&list); // User MUST init

        pstr_status_t status = pstr.split(s, ",", &list);
        assert(status == PSTR_OK);

        // Check count: (vec.len / sizeof(pstr_slice_t))
        size_t count = list.vec.len / sizeof(pstr_slice_t);
        assert(count == 3);

        // Verify the second element "banana"
        pstr_slice_t b;
        memcpy(&b, list.vec.data + sizeof(pstr_slice_t), sizeof(pstr_slice_t));
        assert(b.len == 6);
        assert(strncmp(b.ptr, "banana", 6) == 0);

        pstr.list.free(&list);
        pstr.free(s);
    }

    // 3. Test Starts/Ends With
    {
        pstr_t *s = pstr_from_cstr("lib_pstr.so");
        assert(pstr.starts_with(s, "lib_") == true);
        assert(pstr.ends_with(s, ".so") == true);
        assert(pstr.ends_with(s, ".dll") == false);
        pstr.free(s);
    }

    printf("utility tests: PASSED\n");
}
void test_adversarial() {
    printf("\nadversarial tests...\n");

    // 1. Test NULL format in appendf (Issue #10)
    pstr_builder_t sb;
    pstr.builder.init(&sb);
    assert(pstr.builder.appendf(&sb, NULL) == PSTR_ERR_NULL); // Should fail gracefully

    // 2. Test Invalid UTF-8 Needle (Issue #3)
    pstr_t *s = pstr.from_cstr("Hello");
    pstr_slice_t bad_match = pstr.find_cstr(s, "\xFF\xFF"); // Invalid UTF-8
    assert(bad_match.ptr == NULL); 
    
    // 3. Test Invalid UTF-8 in Builder Replace (Issue #11)
    pstr_status_t status = pstr.builder.replace_range(&sb, 0, 0, "\xFF", 1);
    assert(status == PSTR_ERR_UTF8); // Should return error, not panic

    pstr.free(s);
    pstr.builder.cleanup(&sb);
    printf("adversarial tests: PASSED\n");
}

void test_adversarial_boundaries() {
    pstr_t *s = pstr.from_cstr("A🚀B");

    // This is clean, automated, and extremely fast
    ASSERT_PANIC( pstr.substring(s, 2, 2) ); 
    ASSERT_PANIC( pstr.substring(s, 10, 1) );

    pstr.free(s);
    printf("Adversarial boundary tests: PASSED\n");
}

void test_file_io() {
    printf("\nfile I/O tests...\n");
    const char* const str = "Line 1: Hello\nLine 2: 🚀\nLine 3: Goodbye";
    const char* filename = "test_io.txt";
    FILE* f = fopen(filename, "w");
    fprintf(f, str);
    fclose(f);

    // 1. Test Whole-File Slurp
    pstr_t* full = pstr.read_from_file(filename);
    assert(full != NULL);
    assert(pstr.utf8.len(full) == 39); // Total chars including \n
    pstr.free(full);

    // 2. Test Line-by-Line Builder Processing
    pstr_builder_t sb;
    pstr.builder.init(&sb);
    FILE* in = fopen(filename, "r");
    
    // Read Line 1
    assert(pstr.builder.read_line(&sb, in) == PSTR_OK);
    assert(pstr.builder.find_cstr(&sb, "Hello").ptr != NULL);
    
    // Read Line 2 (The Emoji line)
    assert(pstr.builder.read_line(&sb, in) == PSTR_OK);
    assert(pstr.utf8.validate_cstr((char*)sb.vec.data) == true);
    
    // Read Line 3 (No newline at end)
    assert(pstr.builder.read_line(&sb, in) == PSTR_OK);
    assert(pstr.builder.find_cstr(&sb, "Goodbye").ptr != NULL);

    // Final EOF check
    assert(pstr.builder.read_line(&sb, in) == PSTR_ERR_BOUNDS);

    fclose(in);
    pstr.builder.cleanup(&sb);
    remove(filename); // Clean up the temp file
    printf("file I/O tests: PASSED\n");
}


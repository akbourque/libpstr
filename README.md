# libpstr: A Modern UTF-8 String Library for C
`libpstr` is a Pascal-style (length-prefixed) string library for C11,
designed to bring the safety and ergonomics of modern languages like
Rust to systems-level C development.  

By enforcing **UTF-8 integrity** at the boundary and utilizing
**Small Buffer Optimization (SBO)**, `libpstr` provides a high-performance
alternative to the dangerous standard `<string.h>` utilities.

## 🚀 Key Features

- * **Safety First**: Guaranteed UTF-8 validation on all string creations and manipulations.
- * **Small Buffer Optimization (SBO)**: Uses a stack-allocated buffer for strings up to
512 bytes, drastically reducing heap churn.
- * **Allocation-Free Slices**: Manipulate substrings using non-owning `libpstr_slice_t` views.
- * **Hierarchical API**: A clean, discoverable namespace (e.g., `libpstr.utf8.len()`).
- * **Transactional Integrity**: Operations like `pstr.transform` protect your data if an error occurs.
- * **Adversarial Testing**: Built-in support for intercepting panics during testing.

## Architecture and Design Decisions: The Module Pattern
The libpstr library purposefully rejects the flat global symbol namespace
paradigm traditional to C development. Instead, it exposes a modern,
encapsulated module layout by leveraging compile-time immutable structures
containing focused function pointers.

The public-facing footprint of the entire library is contained within exactly
one single global symbol: extern const libpstr_module_t libpstr;. All underlying
implementation functions are declared with internal linkage (static), keeping
the global linker space entirely free of symbol pollution.
```
libpstr (👑 Global Module Interface)
├── version(void)
│
├── pstr (Owned Pascal Strings)
│   ├── alloc(const char*, size_t)
│   ├── free(libpstr_pstr_t*)
│   ├── from_cstr(const char*)
│   ├── from_slice(libpstr_slice_t)
│   ├── from_utf8_lossy(const char*, size_t)
│   ├── format(const char*, ...)
│   ├── format_v(const char*, va_list)
│   ├── read_from_file(const char*)
│   ├── read_from_file_lossy(const char*)
│   ├── transform(libpstr_pstr_t**, libpstr_transform_fn)
│   ├── to_uppercase(libpstr_pstr_t*)
│   ├── to_lowercase(libpstr_pstr_t*)
│   ├── strip_suffix(const libpstr_pstr_t*, const char*)
│   ├── substring(const libpstr_pstr_t*, size_t, size_t)
│   ├── trim(const libpstr_pstr_t*)
│   ├── split_once(const libpstr_pstr_t*, const char*, libpstr_slice_t*, libpstr_slice_t*)
│   ├── starts_with(const libpstr_pstr_t*, const char*)
│   └── ends_with(const libpstr_pstr_t*, const char*)
│
├── slice (Zero-Allocation Views)
│   ├── substring(libpstr_slice_t, size_t, size_t)
│   ├── find_cstr(const libpstr_pstr_t*, const char*)
│   ├── find_pstr(const libpstr_pstr_t*, const libpstr_pstr_t*)
│   ├── from_pstr(const libpstr_pstr_t*)
│   ├── byte_to_char(const libpstr_pstr_t*, size_t)
│   ├── split(const libpstr_pstr_t*, const char*, libpstr_list_t*)
│   ├── slice_to_index(const libpstr_pstr_t*, libpstr_slice_t)
│   ├── trim(libpstr_slice_t)
│   ├── split_once(libpstr_slice_t, const char*, libpstr_slice_t*, libpstr_slice_t*)
│   ├── starts_with(libpstr_slice_t, const char*)
│   └── ends_with(libpstr_slice_t, const char*)
│
├── sb (Stack-Buffered Assembly)
│   ├── init(libpstr_sb_t*)
│   ├── cleanup(libpstr_sb_t*)
│   ├── build(libpstr_sb_t*)
│   ├── append(libpstr_sb_t*, const char*, size_t)
│   ├── append_cstr(libpstr_sb_t*, const char*)
│   ├── append_pstr(libpstr_sb_t*, libpstr_pstr_t*)
│   ├── append_slice(libpstr_sb_t*, libpstr_slice_t)
│   ├── appendf(libpstr_sb_t*, const char*, ...)
│   ├── vappendf(libpstr_sb_t*, const char*, va_list)
│   ├── append_utf8(libpstr_sb_t*, uint32_t)
│   ├── append_substring(libpstr_sb_t*, const libpstr_pstr_t*, size_t, size_t)
│   ├── replace_range(libpstr_sb_t*, size_t, size_t, const char*, size_t)
│   ├── replace_range_cstr(libpstr_sb_t*, size_t, size_t, const char*)
│   ├── replace_range_pstr(libpstr_sb_t*, size_t, size_t, const libpstr_pstr_t*)
│   ├── find_cstr(libpstr_sb_t*, const char*)
│   ├── read_line(libpstr_sb_t*, FILE*)
│   └── read_line_lossy(libpstr_sb_t*, FILE*)
│
├── utf8 (Unicode Validation)
│   ├── len(const libpstr_pstr_t*)
│   ├── is_char_boundary(const libpstr_pstr_t*, size_t)
│   ├── is_char_boundary_at_ptr(const char*, size_t, size_t)
│   ├── validate(const char*, size_t)
│   ├── validate_cstr(const char*)
│   ├── validate_pstr(const libpstr_pstr_t*)
│   └── iter
│       ├── init(libpstr_iter_t*, const libpstr_pstr_t*)
│       └── next(libpstr_iter_t*)
│
├── vec (Raw Byte Storage Flows)
│   ├── init(libpstr_vec_t*, size_t)
│   ├── init_external(libpstr_vec_t*, uint8_t*, size_t)
│   ├── free(libpstr_vec_t*)
│   ├── reserve(libpstr_vec_t*, size_t)
│   ├── resize(libpstr_vec_t*, size_t)
│   ├── push(libpstr_vec_t*, uint8_t)
│   ├── pop(libpstr_vec_t*)
│   ├── remove(libpstr_vec_t*, size_t)
│   ├── swap_remove(libpstr_vec_t*, size_t)
│   └── extend(libpstr_vec_t*, const uint8_t*, size_t)
│
└── list (Fast Slice Tracking Arrays)
    ├── init(libpstr_list_t*)
    ├── free(libpstr_list_t*)
    ├── len(const libpstr_list_t*)
    ├── get(const libpstr_list_t*, size_t)
    ├── push(libpstr_list_t*, libpstr_slice_t)
    └── pop(libpstr_list_t*)
```
## Architecture FAQ 
1. Does jumping through a struct-bound function pointer destroy performance?  
    No. In modern computing architectures, the micro-overhead of a function pointer
    indirection is negligible. Because the libpstr module table is established at
    compile-time as an immutable (const) target, modern CPU branch predictors guess
    the execution targets with near 100% accuracy. In performance-critical string
    pipelines, layout optimizations like our built-in Small Buffer Optimization (SBO)
    and contiguous block memory allocations save far more clock cycles than raw global
    symbol linkage ever could.

2. Does this pattern break compiler optimization and inline expansions?  
    Not with a modern toolchain. Historically, static functions bound to structure
    pointers could not be easily inlined across separate translation units. However,
    under standard optimization profiles featuring Link-Time Optimization (LTO)—such
    as compiling with -flto via GCC or Clang—the compiler flattens the call graph
    across object boundaries during the final linking stage. It will completely
    bypass the interface structure and inline the underlying static code directly
    into the calling pipeline wherever it yields a performance advantage.

3. Why use libpstr.builder.append() instead of standard prefixes like pstr_builder_append()?  
    Encapsulation, safety, and readability. The character counts are virtually identical, but
    the dot-notation replaces messy C namespace naming conventions with clean, modern language
    ergonomics similar to Rust or Go. More importantly, it enforces modular boundaries. Because
    the internal functions carry static scope inside their translation units, they can use short,
    meaningful names natively without risking a linker naming collision with any other system
    library asset or nested dependency in your compilation graph.

## 📦 Installation

To use `libpstr` in your project, follow these steps:

```bash
make static
```

You may run the test suite with:
```bash
make test
```

## 🛠 Usage Example

- Basic String Handling

```c
    #include "libpstr.h"

    int main() {
        pstr_t *s = pstr.from_cstr("Hello 🚀");
        printf("Logical length: %zu\n", pstr.utf8.len(s));
        pstr.free(s);
        return 0;
    }
```

- Basic Stream Processing

```c
    pstr_builder_t sb;
    pstr.builder.init(&sb);

    while (pstr.builder.read_line(&sb, input_file) == PSTR_OK) {
        pstr_slice_t match = pstr.builder.find_cstr(&sb, "$TAG$");
        if (match.ptr) {
            size_t idx = (size_t)(match.ptr - (char*)sb.vec.data);
            pstr.builder.replace_range(&sb, idx, match.len, "Replacement", 11);
        }
        fwrite(sb.vec.data, 1, sb.vec.len, output_file);
    }
    pstr.builder.cleanup(&sb);
```

## Dependencies
* gcc     (http://gcc.gnu.org/) 
* make    (http://www.gnu.org/software/make/)

## 📄 License
This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

* **Gemini (Google)** - Acted as an AI pair programmer for porting logic,
structural code lowering, and expanding test coverage.

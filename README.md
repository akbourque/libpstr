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
- * **Allocation-Free Slices**: Manipulate substrings using non-owning `pstr_slice_t` views.
- * **Hierarchical API**: A clean, discoverable namespace (e.g., `pstr.utf8.len()`).
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
├── version()
│
├── pstr (Owned Pascal Strings)
│   ├── alloc()
│   ├── free()
│   ├── from_cstr()
│   ├── from_slice()
│   ├── from_utf8_lossy()
│   ├── format()
│   ├── format_v()
│   ├── read_from_file()
│   ├── read_from_file_lossy()
│   ├── transform()
│   ├── to_uppercase()
│   ├── to_lowercase()
│   ├── strip_suffix()
│   ├── trim()
│   ├── split_once()
│   ├── starts_with()
│   └── ends_with()
│
├── slice (Zero-Allocation Views)
│   ├── substring()
│   ├── find_cstr()
│   ├── find_pstr()
│   ├── byte_to_char()
│   ├── split()
│   ├── slice_to_index()
│   ├── trim()
│   ├── split_once()
│   ├── starts_with()
│   └── ends_with()
│
├── builder (Stack-Buffered Assembly)
│   ├── init()
│   ├── cleanup()
│   ├── build()
│   ├── append()
│   ├── append_cstr()
│   ├── append_pstr()
│   ├── appendf()
│   ├── vappendf()
│   ├── append_utf8()
│   ├── append_substring()
│   ├── replace_range()
│   ├── replace_range_cstr()
│   ├── replace_range_pstr()
│   ├── find_cstr()
│   ├── read_line()
│   └── read_line_lossy()
│
├── utf8 (Unicode Validation)
│   ├── len()
│   ├── is_char_boundary()
│   ├── is_char_boundary_at_ptr()
│   ├── validate()
│   ├── validate_cstr()
│   ├── validate_pstr()
│   └── iter
│       ├── init()
│       └── next()
│
├── vec (Raw Byte Storage Flows)
│   ├── init()
│   ├── init_external()
│   ├── free()
│   ├── reserve()
│   ├── resize()
│   ├── push()
│   ├── pop()
│   ├── remove()
│   ├── swap_remove()
│   └── extend()
│
└── list (Fast Slice Tracking Arrays)
    ├── init()
    ├── free()
    ├── len()
    ├── get()
    ├── push()
    └── pop()
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

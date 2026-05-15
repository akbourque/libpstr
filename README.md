# libpstr: A Modern UTF-8 String Library for C

`libpstr` is a Pascal-style (length-prefixed) string library for C11, designed to bring the safety and ergonomics of modern languages like Rust to systems-level C development.

By enforcing **UTF-8 integrity** at the boundary and utilizing **Small Buffer Optimization (SBO)**, `libpstr` provides a high-performance alternative to the dangerous standard `\<string.h\>` utilities.

## 🚀 Key Features

- **Safety First**: Guaranteed UTF-8 validation on all string creations and manipulations.

- **Small Buffer Optimization (SBO)**: Uses a stack-allocated buffer for strings up to 512 bytes, drastically reducing heap churn for common string operations.

- **Allocation-Free Slices**: Manipulate substrings using non-owning `pstr\_slice\_t` views to avoid unnecessary memory allocations.

- **Hierarchical API**: A clean, discoverable namespace (e.g., `pstr.utf8.len()`) inspired by modern object-oriented and functional languages.

- **Transactional Integrity**: Operations like `pstr.transform` ensure your original string remains untouched if an error occurs mid-process.

- **Adversarial Testing**: Built-in support for intercepting panics during testing, allowing for 100% coverage of error paths without crashing the test suite.

## 📦 Installation

`libpstr` can be compiled as a static or shared library using the provided Makefile. For tools like code generators, static linking is recommended.

Bash

```
`make static`
```

## 🛠 Usage Example

### Basic String Handling

C

```
`\#include "libpstr.h"`


`int main() \{`

`    // Safety: pstr.from\_cstr validates UTF-8 immediately`

`    pstr\_t \*s = pstr.from\_cstr("Hello 🚀");`


`    // Efficient character counting`

`    printf("Logical length: %zu\\n", pstr.utf8.len(s));`


`    pstr.free(s);`

`    return 0;`

`\}`
```

### Basic Stream Processing

C

```
`pstr\_builder\_t sb;`

`pstr.builder.init(&sb);`


`// Zero-allocation steady-state line processing`

`while (pstr.builder.read\_line(&sb, input\_file) == PSTR\_OK) \{`

`    pstr\_slice\_t match = pstr.builder.find\_cstr(&sb, "$TAG$");`

`    if (match.ptr) \{`

`        // Simplified index finding`

`        size\_t idx = pstr.slice\_to\_index(NULL, match); `

`        pstr.builder.replace\_range(&sb, idx, match.len, "Replacement", 11);`

`    \}`

`    fwrite(sb.vec.data, 1, sb.vec.len, output\_file);`

`\}`

`pstr.builder.cleanup(&sb);`
```

## 🧪 Testing

To run the comprehensive test suite (including ASan leak detection and adversarial boundary tests):

Bash

```
`make test`
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](https://www.google.com/search?q=LICENSE) file for details.


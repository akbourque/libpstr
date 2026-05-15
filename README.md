# libpstr: A Modern UTF-8 String Library for C

`libpstr` is a Pascal-style (length-prefixed) string library for C11, designed to bring the safety and ergonomics of modern languages like Rust to systems-level C development.

By enforcing **UTF-8 integrity** at the boundary and utilizing **Small Buffer Optimization (SBO)**, `libpstr` provides a high-performance alternative to the dangerous standard `<string.h>` utilities.

## 🚀 Key Features

* **Safety First**: Guaranteed UTF-8 validation on all string creations and manipulations.
* **Small Buffer Optimization (SBO)**: Uses a stack-allocated buffer for strings up to 512 bytes, drastically reducing heap churn.
* **Allocation-Free Slices**: Manipulate substrings using non-owning `pstr_slice_t` views.
* **Hierarchical API**: A clean, discoverable namespace (e.g., `pstr.utf8.len()`).
* **Transactional Integrity**: Operations like `pstr.transform` protect your data if an error occurs.
* **Adversarial Testing**: Built-in support for intercepting panics during testing.

## 📦 Installation

```bash
make static

🛠 Usage Example

Basic String Handling

#include "libpstr.h"

int main() {
    pstr_t *s = pstr.from_cstr("Hello 🚀");
    printf("Logical length: %zu\n", pstr.utf8.len(s));
    pstr.free(s);
    return 0;
}

Basic Stream Processing

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

📄 License
This project is licensed under the MIT License.

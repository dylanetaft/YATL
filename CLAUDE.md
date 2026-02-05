# CLAUDE.md - AI Assistant Guide for YATL

## Project Overview

YATL (Yet Another TOML Library) is a **format-preserving** TOML parser and editor written in C11. Unlike traditional parsers that build ASTs, YATL provides a DOM-like view into TOML documents allowing in-place editing with round-trip serialization that preserves formatting, comments, and whitespace.

## Build System

```bash
# Standard build
mkdir build && cd build
cmake ..
make

# Build options (set with -D flag)
YATL_BUILD_SHARED=OFF    # Build shared library (default: static)
YATL_BUILD_EXAMPLES=ON   # Build example programs
YATL_BUILD_TESTS=ON      # Build test suite
YATL_BUILD_FUZZERS=OFF   # Build fuzzing targets
YATL_ENABLE_LOGGING=OFF  # Enable debug logging

# Run tests (from tests directory - loads .toml fixtures)
cd tests && ../build/tests/test_yatl
# or via ctest (handles working directory automatically)
cd build && ctest
```

## Project Structure

```
include/yatl.h       # Public API header (opaque types + functions)
src/yatl.c           # Core implementation
src/yatl_lexer.c     # TOML lexer/parser
src/yatl_lexer.h     # Lexer internals
src/yatl_writer.c    # Document serialization
src/yatl_private.h   # Internal structures (not public API)
examples/            # Usage examples
tests/               # munit-based test suite
fuzzers/             # Fuzz testing targets
```

## Architecture

### Core Concepts

1. **Document (`YATL_Doc_t`)**: Doubly-linked list of lines. Each line holds raw text from the source file.

2. **Span (`YATL_Span_t`)**: A view into the document defined by start/end cursors. Spans are lightweight (no data copy) and represent:
   - Entire document (`YATL_S_NONE`)
   - Tables (`YATL_S_NODE_TABLE`, `YATL_S_NODE_ARRAY_TABLE`)
   - Key-value pairs (`YATL_S_LEAF_KEYVAL`)
   - Values/arrays/inline tables

3. **Cursor (`YATL_Cursor_t`)**: Points to a specific position (line + character offset). Used for iteration and span boundaries.

### Opaque Types

Public types are opaque byte arrays. Internal code casts to `_YATL_*_t` to access fields. Magic numbers validate initialization:
- `YATL_DOC_MAGIC = 0x444F4354`
- `YATL_SPAN_MAGIC = 0x5350414E`
- `YATL_CURSOR_MAGIC = 0x43555253`
- `YATL_LINE_MAGIC = 0x4C494E45`

### Memory Model

- **Read operations**: Zero allocation (views into document's line buffers)
- **Write operations**: Stack memory for temporary structures; heap for new line content
- **Boneyard**: Edited lines are moved to a boneyard (not freed) for potential rollback on parse failure

## API Patterns

### Initialization (Required)

Always initialize structures before use as input parameters:
```c
YATL_Doc_t doc = YATL_doc_create();
YATL_Cursor_t cursor = YATL_cursor_create();
YATL_Span_t span = YATL_span_create();
```

Output parameters are initialized by the API. Uninitialized input parameters return `YATL_ERR_INVALID_ARG`.

### Error Handling

Functions return `YATL_Result_t`:
- `YATL_OK (0)` - Success
- `YATL_DONE (1)` - Iteration complete (not an error)
- Negative values are errors (`YATL_ERR_IO`, `YATL_ERR_SYNTAX`, `YATL_ERR_NOT_FOUND`, etc.)

### Typical Usage Pattern

```c
YATL_Doc_t doc = YATL_doc_create();
YATL_doc_load(&doc, "config.toml");

YATL_Span_t doc_span;
YATL_doc_span(&doc, &doc_span);

// Find a table
YATL_Span_t table_span;
YATL_span_find_name(&doc_span, "database", &table_span);

// Find a key-value within table
YATL_Span_t kv_span, key_span, val_span;
YATL_span_find_name(&table_span, "host", &kv_span);
YATL_span_keyval_slice(&kv_span, &key_span, &val_span);

// Get value text
const char *text;
size_t len;
YATL_span_text(&val_span, &text, &len);

// Modify value
YATL_span_set_value(&val_span, "newhost", 7);

YATL_doc_save(&doc, "config.toml");
YATL_doc_free(&doc);
```

## Critical Gotchas

### 1. Dotted Table Names Are Literal

Table names like `[server.http]` are stored as the literal string `"server.http"`. Do NOT try nested lookups:

```c
// CORRECT:
YATL_span_find_name(&doc_span, "server.http", &table_span);

// WRONG - will not work:
YATL_span_find_name(&doc_span, "server", &server);
YATL_span_find_name(&server, "http", &http);  // Fails!
```

### 2. String Spans Exclude Quotes (Semantic Bounds)

When getting string values via `YATL_span_keyval_slice()`, the value span text excludes quote delimiters:

```c
// TOML: name = "Alice"
YATL_span_text(&val_span, &text, &len);
// text = "Alice", len = 5  (NOT "\"Alice\"")
```

### 3. Value Updates and Semantic Bounds

`YATL_span_set_value()` and `YATL_span_ml_set_value()` **update the passed-in span** to point to the new content. The span remains valid after modification.

Spans track both **lexical bounds** (full syntax) and **semantic bounds** (content only). When updating values, semantic bounds determine what you provide:

**Single-line strings**: Semantic bounds exclude quotes - provide content only:
```c
// TOML: name = "Alice"  ->  name = "Bob"
YATL_span_set_value(&val_span, "Bob", 3);  // No quotes needed
```

**Array elements** (via `span_find_next` iteration): Semantic bounds exclude commas and quotes:
```c
// TOML: ports = [1000, 2000, 3000]
// When iterating and updating element "2000":
YATL_span_set_value(&elem_span, "9999", 4);  // No comma needed

// TOML: names = ["a", "b", "c"]
// When updating element "b":
YATL_span_set_value(&elem_span, "x", 1);  // No quotes needed
```

**Whole value replacement** (from `keyval_slice`): Include full syntax:
```c
// Replacing entire array value - include brackets
YATL_span_set_value(&val_span, "[ 10000, 20000 ]", 16);
```

### 4. Multiline String Updates Require Delimiters

Multiline strings (`"""`) do NOT have semantic bounds set. When updating, you must provide the full syntax including `"""` at the beginning and end:

```c
const char *lines[] = {"\"\"\"", "new first line", "new second line", "\"\"\""};
size_t lengths[] = {3, 14, 15, 3};
YATL_span_ml_set_value(&val_span, lines, lengths, 4);
```

### 5. Multi-line Iteration

For multi-line spans (multiline strings, arrays), use `YATL_span_iter_line()`:

```c
YATL_Cursor_t iter = YATL_cursor_create();
const char *text;
size_t len;
while (YATL_span_iter_line(&span, &iter, &text, &len) == YATL_OK) {
    printf("%.*s\n", (int)len, text);
}
```

### 6. Array of Tables Iteration

Use `YATL_span_find_next_by_name()` with cursor to iterate `[[array.tables]]`:

```c
YATL_Cursor_t cursor = YATL_cursor_create();
YATL_Span_t item;
while (YATL_span_find_next_by_name(&doc_span, "items", &cursor, &cursor, &item) == YATL_OK) {
    // Process each [[items]] section
}
```

## Testing

Tests use the [munit](https://nemequ.github.io/munit/) framework. Test files are in `tests/`:

- `test_yatl.c` - Main test suite covering find, unlink, and update operations
- `test_find.toml`, `test_updates.toml`, etc. - Test fixture files

**Important**: Tests must be run from the `tests/` directory (they load `.toml` fixture files from the current directory):

```bash
cd tests && ../build/tests/test_yatl
```

Or use `ctest` which handles the working directory automatically:
```bash
cd build && ctest
```

## Internal Functions (Not Public API)

Functions prefixed with `_YATL_` or `_` are internal:
- `_YATL_span_unlink()` / `_YATL_span_relink()` - Atomic edit support
- `_line_alloc()`, `_line_free()` - Line memory management
- `_toml_value_parse()`, `_toml_key_parse()` - Lexer internals

## Code Style

- C11 standard
- Opaque types with size constants in public header
- Magic number validation on all input parameters
- Result codes for all functions (not exceptions)
- Conditional logging via `YATL_LOG()` macro (requires `YATL_ENABLE_LOGGING`)

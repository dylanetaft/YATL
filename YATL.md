# YATL - Yet Another TOML Library


## Description

YATL is a format-preserving TOML parser and writer for C. Unlike traditional
parsers that build an abstract syntax tree or convert to native data structures,
YATL provides a lightweight toolset to navigate a TOML document with a
DOM-like view allowing for: in-line editing, round-trip serialization and
deserialization without loss of formatting. For read operations, it aims
to be zero-allocation. Stack memory is primarily used for write operations
and temporary structures.

## Architecture

### Document Model

Internally YATL represents documents as a doubly-linked list of lines.
Each line contains a pointer to the next and previous lines, along with
the raw text from the source file. The complexity of this is abstracted away
from the user via simple function calls.

This design enables:

- Round-trip editing without losing formatting
- Minimal memory overhead for read-only access
- In-place modification of values
- Avoiding boilerplate code to manage complex data structures

### Spans

A **Span** is a view into the document, internally defined by a start cursor and end cursor.
Spans are lightweight and do not copy data. They can represent:

- The entire document
- A table (standard or array-of-tables)
- A key-value pair
- A value (string, integer, array, inline table, etc.)

Spans have a **type** (`YATL_SpanType_t`) indicating what they represent.

### Cursors

A **Cursor** points to a specific position within the document (line + offset).
Cursors are used for iteration and to define span boundaries.

## Potential Stumbling Blocks

### Create Functions

Functions `YATL_doc_create, YATL_cursor_create, YATL_span_create` initialize
structures to known values but do not allocate memory.
They are always safe to use when declaring a cursor, span, or document,
they are required to call on any object that will be passed as 
an input parameter to the public API.

Output parameters are initialized by the API.
If you do not initialize, for example, an input cursor, the behavior is defined.
The API will fail with an error code.

tl;dr - `YATL_Cursor_t cursor = YATL_cursor_create()` = good.

### Dotted Table Names Are Literal

Table headers with dotted keys like `[server.http]` are stored with the
literal name `"server.http"`. The library does **not** create an implicit
nested structure for these.

To find such a table:

```c
YATL_span_find_name(&doc_span, "server.http", &table_span);  // Correct

// NOT like this:
YATL_span_find_name(&doc_span, "server", &server_span);
YATL_span_find_name(&server_span, "http", &http_span);  // Wrong
```

This preserves the original TOML syntax and avoids ambiguity with inline tables.

For table arrays, for example:

```toml
[[server]]
name = "alpha"
[[server]]
name = "beta"
```

One would find the table array with:

```c
//finds alpha
YATL_span_find_next_by_name(&doc_span, "server", &iter_cursor, &table_array_span);
//finds beta
YATL_span_find_next_by_name(&doc_span, "server", &iter_cursor, &table_array_span);
```

### String Span Text Excludes Quotes

When using `YATL_span_keyval_slice()` to get a string value span, the span
text returned by `YATL_span_text()` contains the **decoded string content**
without the surrounding quote delimiters.

```c
// TOML: name = "Alice"

YATL_span_text(&val_span, &text, &len);
// text = "Alice", len = 5  (not "\"Alice\"", len = 7)
```

### Lines and the Boneyard

When spans are unlinked (for editing operations), the original lines are
moved to a "boneyard" - they remain allocated but are removed from the
document's line list. This allows for:

- Relinking spans back to their original position
- Building new content from prefix/suffix fragments
- Atomic edit operations with rollback on failure

### Security ###

The parser has had limited fuzz testing performed but has not undergone extensive security review.
Thus far, no memory safety issues have been found, however infinute loops have been
found and corrected via fuzz testing. The software is provided as-is without warranty. 
Use at your own risk. 


## Common Operations

### Finding a Value

As an example, for the following TOML document:

```toml
[appsettings]
debug = true
window_width = 800
window_height = 600

[database]
host = "localhost"
port = 5432
```

The code below:

```c
YATL_Doc_t doc;
YATL_doc_load(&doc, "config.toml");

YATL_Span_t doc_span;
YATL_doc_span(&doc, &doc_span);

// Find table
YATL_Span_t table_span;
YATL_span_find_name(&doc_span, "database", &table_span);
```

Would provide a span covering the `[database]` section:

```
[appsettings]
debug = true
window_width = 800
window_height = 600

[database]         <-- span starts here
host = "localhost"
port = 5432        <-- span ends here
```

```c
// Find key within table
YATL_Span_t keyval_span;
YATL_span_find_name(&table_span, "host", &keyval_span);
```

Would result in a span covering:

```
[database]
host = "localhost"  <-- this line
port = 5432
```

```c
// Get value
YATL_Span_t key_span, val_span;
YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);

const char *text;
size_t len;
YATL_span_text(&val_span, &text, &len);
```

Would yield `localhost` and `len = 9`.

### Iterating Array Elements

```c
YATL_Cursor_t cursor = YATL_cursor_create();
YATL_Span_t elem_span;

while (YATL_span_find_next(&array_span, &cursor, &elem_span) == YATL_OK) {
    // Process elem_span
}
```

### Drilling Into Inline Tables

Inline tables can be navigated the same way as regular tables:

```c
// TOML: user = { name = "Alice", role = "admin" }

YATL_span_find_name(&table_span, "user", &user_kv);
YATL_span_keyval_slice(&user_kv, &key, &user_val);

// user_val is now the inline table, drill into it:
YATL_span_find_name(&user_val, "name", &name_kv);
```

## Internal Functions

Functions prefixed with `_YATL_` or `_` are internal and subject to change.
Test code may include private headers to access these for testing purposes.

- `_YATL_span_unlink()` - Remove span lines from document
- `_YATL_span_relink()` - Restore span lines to document
- `_line_alloc()`, `_line_free()` - Line memory management

## See Also

- [TOML specification](https://toml.io/)

## Author

Dylan Taft

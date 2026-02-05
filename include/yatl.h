/**
 * @file yatl.h
 * @brief YATL - Yet Another TOML Library
 *
 * A format-preserving TOML parser and writer for C. Provides a DOM-like
 * view for in-line editing with round-trip serialization without loss
 * of formatting.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @defgroup yatl_types Opaque Types and Constants
 * @brief Core opaque structures and size constants.
 *
 * @defgroup yatl_logging Logging
 * @brief Logging macros and utilities.
 *
 * @defgroup yatl_enums Enumerations
 * @brief TOML type enumerations and result codes.
 *
 * @defgroup yatl_init Structure Initialization
 * @brief Functions to create and initialize YATL structures.
 *
 * @defgroup yatl_doc Document Lifecycle
 * @brief Functions for loading, saving, and managing TOML documents.
 *
 * @defgroup yatl_span Span Operations
 * @brief Functions for navigating, querying, and modifying spans within a
 * document.
 *
 * @defgroup yatl_span_nav Span Navigation
 * @brief Functions for navigating and iterating through spans.
 * @ingroup yatl_span
 *
 * @defgroup yatl_span_query Span Query
 * @brief Functions for querying span content and properties.
 * @ingroup yatl_span
 *
 * @defgroup yatl_span_modify Span Modification
 * @brief Functions for modifying span values.
 * @ingroup yatl_span
 */

/**
 * @brief Size of opaque YATL_Line_t structure in bytes
 * @ingroup yatl_types
 */
#define YATL_LINE_SIZE 96

/**
 * @brief Size of opaque YATL_Cursor_t structure in bytes
 * @ingroup yatl_types
 */
#define YATL_CURSOR_SIZE 96

/**
 * @brief Size of opaque YATL_Span_t structure in bytes
 * @ingroup yatl_types
 */
#define YATL_SPAN_SIZE 160

/**
 * @brief Size of opaque YATL_Doc_t structure in bytes
 * @ingroup yatl_types
 */
#define YATL_DOC_SIZE 64

/**
 * @brief Opaque line structure.
 * @ingroup yatl_types
 *
 * Represents a single line in a TOML document. Users should not
 * access internal fields directly.
 */
typedef struct YATL_Line {
  _Alignas(max_align_t) unsigned char _opaque[YATL_LINE_SIZE];
} YATL_Line_t;

/**
 * @brief Opaque cursor structure.
 * @ingroup yatl_types
 *
 * Points to a specific position within a document (line + character offset).
 * Used for iteration and defining span boundaries.
 *
 * @note Always initialize with YATL_cursor_create() for input cursors.
 */
typedef struct YATL_Cursor {
  _Alignas(max_align_t) unsigned char _opaque[YATL_CURSOR_SIZE];
} YATL_Cursor_t;

/**
 * @brief Opaque span structure.
 * @ingroup yatl_types
 *
 * A view into the document defined by start and end cursors.
 * Spans are lightweight and do not copy data. They can represent
 * the entire document, a table, a key-value pair, or a value.
 *
 * @note Spans become invalid if the underlying document is modified
 *       in a way that affects the spanned region.
 */
typedef struct YATL_Span {
  _Alignas(max_align_t) unsigned char _opaque[YATL_SPAN_SIZE];
} YATL_Span_t;

/**
 * @brief Opaque document structure.
 * @ingroup yatl_types
 *
 * Represents a complete TOML document as a doubly-linked list of lines.
 *
 * @note Always initialize with YATL_doc_create() before use.
 *       Always call YATL_doc_free() when done to release resources.
 */
typedef struct YATL_Doc {
  _Alignas(max_align_t) unsigned char _opaque[YATL_DOC_SIZE];
} YATL_Doc_t;

/**
 * @brief Log levels for YATL diagnostic messages.
 * @ingroup yatl_logging
 */
typedef enum {
  YATL_LOG_DEBUG, /**< Debug-level messages */
  YATL_LOG_INFO,  /**< Informational messages */
  YATL_LOG_WARN,  /**< Warning messages */
  YATL_LOG_ERROR  /**< Error messages */
} YATL_log_level_t;

#ifdef YATL_ENABLE_LOGGING
#ifndef LOG_LEVEL
#define LOG_LEVEL YATL_LOG_DEBUG
#endif
#endif

/**
 * @brief Convert log level to string.
 * @ingroup yatl_logging
 * @param level Log level value
 * @return String representation of the log level
 */
static inline const char *log_level_str(YATL_log_level_t level) {
  switch (level) {
  case YATL_LOG_DEBUG:
    return "DEBUG";
  case YATL_LOG_INFO:
    return "INFO";
  case YATL_LOG_WARN:
    return "WARN";
  case YATL_LOG_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Log a message with level, file, line, and function context.
 * @ingroup yatl_logging
 * @param level Log level (YATL_LOG_DEBUG, YATL_LOG_INFO, etc.)
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#ifdef YATL_ENABLE_LOGGING
#define YATL_LOG(level, fmt, ...)                                              \
  do {                                                                         \
    if ((level) >= LOG_LEVEL) {                                                \
      fprintf(stderr, "[%s] %s:%d (%s): " fmt "\n", log_level_str(level),      \
              __FILE__, __LINE__, __func__, ##__VA_ARGS__);                    \
    }                                                                          \
  } while (0)
#else
#define YATL_LOG(level, fmt, ...)                                              \
  do {                                                                         \
  } while (0)
#endif

/**
 * @brief Span type enumeration.
 * @ingroup yatl_enums
 *
 * Indicates what kind of TOML construct a span represents.
 *
 * - For TABLE/ARRAY_TABLE: covers header line through last line before
 *   next table header (the full section extent).
 * - For KEYVAL: covers the key = value (may span lines for multiline strings).
 */
typedef enum {
  YATL_S_NONE,              /**< Entire document or untyped span */
  YATL_S_NODE_TABLE,        /**< Table header: [table.name] */
  YATL_S_NODE_ARRAY,        /**< Array value: [val1, val2, ...] */
  YATL_S_NODE_ARRAY_TABLE,  /**< Array of tables: [[array.table]] */
  YATL_S_NODE_INLINE_TABLE, /**< Inline table: {key = val, ...} */
  YATL_S_LEAF_KEYVAL,       /**< Key-value pair: key = value */
  YATL_S_LEAF_COMMENT,      /**< Comment: # comment */
  YATL_S_SLICE_KEY,         /**< Key portion of a key-value pair */
  YATL_S_SLICE_VALUE, /**< Value portion (string, int, float, bool, datetime) */
} YATL_SpanType_t;

/**
 * @brief Value type enumeration.
 * @ingroup yatl_enums
 *
 * Indicates the type of a TOML value for parsing purposes.
 */
typedef enum {
  YATL_TYPE_BAREVALUE,    /**< Bare value: number, bool, date, time - user
                             interprets */
  YATL_TYPE_STRING,       /**< Quoted string (basic or literal) */
  YATL_TYPE_ARRAY,        /**< Array: [...] */
  YATL_TYPE_INLINE_TABLE, /**< Inline table: {...} */
} YATL_ValueType_t;

/**
 * @brief Result/error codes returned by YATL functions.
 * @ingroup yatl_enums
 */
typedef enum {
  YATL_DONE = 1,           /**< Iteration complete (not an error) */
  YATL_OK = 0,             /**< Success */
  YATL_ERR_IO = -1,        /**< I/O error (file read/write) */
  YATL_ERR_SYNTAX = -2,    /**< TOML syntax error */
  YATL_ERR_NOT_FOUND = -3, /**< Requested item not found */
  YATL_ERR_TYPE = -4,      /**< Type mismatch error */
  YATL_ERR_BUFFER = -5,   /**< Buffer too small */
  YATL_ERR_NOMEM = -6,     /**< Memory allocation failed */
  YATL_ERR_INVALID_ARG =
      -7, /**< Invalid argument (NULL pointer, uninitialized struct) */
} YATL_Result_t;

/**
 * @brief Create an initialized cursor.
 * @ingroup yatl_init
 *
 * Creates a cursor with proper initialization.
 * The cursor starts with no position (NULL line).
 *
 * @return Initialized cursor structure
 *
 * @code
 * YATL_Cursor_t cursor = YATL_cursor_create();
 * @endcode
 */
YATL_Cursor_t YATL_cursor_create(void);

/**
 * @brief Create an initialized span.
 * @ingroup yatl_init
 *
 * The span has no bounds set.
 *
 * @return Initialized span structure
 */
YATL_Span_t YATL_span_create(void);

/**
 * @brief Create an initialized document.
 * @ingroup yatl_doc
 *
 * @return Initialized document structure
 *
 * @code
 * YATL_Doc_t doc = YATL_doc_create();
 * YATL_doc_load(&doc, "config.toml");
 * @endcode
 */
YATL_Doc_t YATL_doc_create(void);

/**
 * @brief Load a TOML document from a file.
 * @ingroup yatl_doc
 *
 * Reads and parses a TOML file into the document structure.
 *
 * @param doc  Pointer to initialized document
 * @param path Path to the TOML file
 *
 * @return YATL_OK on success
 * @return YATL_ERR_IO if file cannot be opened or read
 * @return YATL_ERR_NOMEM if memory allocation fails
 * @return YATL_ERR_INVALID_ARG if doc or path is NULL
 */
YATL_Result_t YATL_doc_load(YATL_Doc_t *doc, const char *path);

/**
 * @brief Load a TOML document from a string.
 * @ingroup yatl_doc
 *
 * Parses a TOML string into the document structure.
 * The document must be initialized first.
 *
 * @param doc Pointer to initialized document
 * @param str TOML content string
 * @param len Length of the string in bytes
 *
 * @return YATL_OK on success
 * @return YATL_ERR_NOMEM if memory allocation fails
 * @return YATL_ERR_INVALID_ARG if doc or str is NULL
 */
YATL_Result_t YATL_doc_loads(YATL_Doc_t *doc, const char *str, size_t len);

/**
 * @brief Save a document to a file.
 * @ingroup yatl_doc
 *
 * Writes the document content to a file, preserving formatting.
 *
 * @param doc  Pointer to document
 * @param path Path to output file
 *
 * @return YATL_OK on success
 * @return YATL_ERR_IO if file cannot be written
 * @return YATL_ERR_INVALID_ARG if doc or path is NULL
 */
YATL_Result_t YATL_doc_save(YATL_Doc_t *doc, const char *path);

/**
 * @brief Free document resources.
 * @ingroup yatl_doc
 *
 * Releases all memory associated with the document, including
 * active lines and boneyard lines.
 *
 * @param doc Pointer to document to free
 *
 * @note Safe to call with NULL pointer (no-op).
 * @note The document structure itself is not freed, only its contents.
 */
void YATL_doc_free(YATL_Doc_t *doc);

/**
 * @brief Clear the document boneyard.
 * @ingroup yatl_doc
 *
 * Frees all lines in the boneyard (lines removed during editing
 * that are kept for potential rollback).
 *
 * @param doc Pointer to document
 *
 * @return YATL_OK on success
 * @return YATL_ERR_INVALID_ARG if doc is NULL or not initialized
 *
 * @note Call this after edits are finalized to free memory.
 */
YATL_Result_t YATL_doc_clear_boneyard(YATL_Doc_t *doc);

/**
 * @brief Get a span covering the entire document.
 * @ingroup yatl_span_nav
 *
 * Creates a span of type YATL_S_NONE that encompasses all lines
 * in the document.
 *
 * @param doc      Pointer to document
 * @param out_span Output span covering the document
 *
 * @return YATL_OK on success
 * @return YATL_ERR_INVALID_ARG if doc or out_span is NULL
 */
YATL_Result_t YATL_doc_span(const YATL_Doc_t *doc, YATL_Span_t *out_span);

/**
 * @brief Find the next span within a boundary span.
 * @ingroup yatl_span_nav
 *
 * Iterates through spans within in_span. Use with a cursor to iterate
 * through all children of a span.
 *
 * @param in_span  Boundary span to search within
 * @param cursor   Cursor tracking iteration position (NULL to start from
 * beginning). Initialize with YATL_cursor_create(). Advanced automatically.
 * @param out_span Output span for the found element
 *
 * @return YATL_OK if a span was found
 * @return YATL_DONE if no more spans (iteration complete)
 * @return YATL_ERR_NOT_FOUND if no spans exist
 * @return YATL_ERR_INVALID_ARG if in_span or out_span is NULL/uninitialized
 *
 * @code
 * YATL_Cursor_t cursor = YATL_cursor_create();
 * YATL_Span_t span;
 * while (YATL_span_find_next(&doc_span, &cursor, &span) == YATL_OK) {
 *     // Process span
 * }
 * @endcode
 */
YATL_Result_t YATL_span_find_next(const YATL_Span_t *in_span,
                                  YATL_Cursor_t *cursor, YATL_Span_t *out_span);

/**
 * @brief Find a table or key-value by name.
 * @ingroup yatl_span_nav
 *
 * Searches for the first TABLE, ARRAY_TABLE, or KEYVAL with the
 * specified name within in_span.
 *
 * @param in_span  Span to search within
 * @param name     Name to search for (literal match, including dots)
 * @param out_span Output span for the found element
 *
 * @return YATL_OK if found
 * @return YATL_ERR_NOT_FOUND if no match
 * @return YATL_ERR_INVALID_ARG if any parameter is NULL/uninitialized
 *
 * @note Dotted table names like [server.http] are matched literally.
 *       Use "server.http", not nested searches.
 */
YATL_Result_t YATL_span_find_name(const YATL_Span_t *in_span, const char *name,
                                  YATL_Span_t *out_span);

/**
 * @brief Find next table or key-value by name with cursor support.
 * @ingroup yatl_span_nav
 *
 * Like YATL_span_find_name() but supports iteration to find multiple
 * elements with the same name (e.g., array of tables).
 *
 * @param in_span    Span to search within
 * @param name       Name to search for
 * @param in_cursor  Starting position (NULL to start from beginning)
 * @param out_cursor Output cursor position after match (NULL to ignore)
 * @param out_span   Output span for the found element
 *
 * @return YATL_OK if found
 * @return YATL_ERR_NOT_FOUND if no more matches
 * @return YATL_ERR_INVALID_ARG if required parameters are NULL/uninitialized
 *
 * @code
 * YATL_Cursor_t cursor = YATL_cursor_create();
 * YATL_Span_t span;
 * // Find all [[items]] sections
 * while (YATL_span_find_next_by_name(&doc_span, "items", &cursor, &cursor,
 * &span) == YATL_OK) {
 *     // Process each [[items]] section
 * }
 * @endcode
 */
YATL_Result_t YATL_span_find_next_by_name(const YATL_Span_t *in_span,
                                          const char *name,
                                          const YATL_Cursor_t *in_cursor,
                                          YATL_Cursor_t *out_cursor,
                                          YATL_Span_t *out_span);

/**
 * @brief Iterate over line segments within a span.
 * @ingroup yatl_span_nav
 *
 * For multi-line spans, iterates through each line segment,
 * returning the text portion within the span boundaries.
 *
 * @param span     Span to iterate
 * @param cursor   Cursor tracking iteration (initialize with
 * YATL_cursor_create())
 * @param out_text Output pointer to line text (not null-terminated)
 * @param out_len  Output length of line segment
 *
 * @return YATL_OK for each line segment
 * @return YATL_DONE when iteration is complete
 * @return YATL_ERR_INVALID_ARG if any parameter is NULL/uninitialized
 *
 * @code
 * YATL_Cursor_t cursor = YATL_cursor_create();
 * const char *text;
 * size_t len;
 * while (YATL_span_iter_line(&span, &cursor, &text, &len) == YATL_OK) {
 *     printf("%.*s\n", (int)len, text);
 * }
 * @endcode
 */
YATL_Result_t YATL_span_iter_line(const YATL_Span_t *span,
                                  YATL_Cursor_t *cursor, const char **out_text,
                                  size_t *out_len);

/**
 * @brief Move a cursor by a character offset.
 * @ingroup yatl_span_nav
 *
 * Moves the cursor forward (positive) or backward (negative)
 * by the specified number of characters, crossing line boundaries
 * as needed.
 *
 * @param cursor Cursor to move
 * @param npos   Number of positions to move (negative for backward)
 *
 * @return YATL_OK if moved successfully
 * @return YATL_DONE if reached document boundary
 * @return YATL_ERR_INVALID_ARG if cursor is NULL/uninitialized
 */
YATL_Result_t YATL_cursor_move(YATL_Cursor_t *cursor, long npos);

/**
 * @brief Get the string name of a span type.
 * @ingroup yatl_span_query
 *
 * Returns a human-readable string for the span type.
 *
 * @param type Span type value
 *
 * @return String representation (e.g., "YATL_S_NODE_TABLE")
 */
const char *YATL_span_type_name(YATL_SpanType_t type);

/**
 * @brief Slice a key-value span into separate key and value spans.
 * @ingroup yatl_span_query
 *
 * Given a span of type YATL_S_LEAF_KEYVAL, extracts the key and value
 * as separate spans.
 *
 * @param in_span Input key-value span (must be YATL_S_LEAF_KEYVAL type)
 * @param key  Output span for the key portion
 * @param val  Output span for the value portion. The span type is set
 *             according to the value type (e.g., YATL_S_NODE_ARRAY for
 *             arrays, YATL_S_NODE_INLINE_TABLE for inline tables).
 *             For string values, the span text excludes quote delimiters.
 *
 * @return YATL_OK on success
 * @return YATL_ERR_TYPE if span is not YATL_S_LEAF_KEYVAL
 * @return YATL_ERR_INVALID_ARG if any parameter is NULL/uninitialized
 */
YATL_Result_t YATL_span_keyval_slice(const YATL_Span_t *in_span,
                                     YATL_Span_t *key, YATL_Span_t *val);

/**
 * @brief Get the type of a span.
 * @ingroup yatl_span_query
 *
 * @param span Span to query
 *
 * @return Span type, or YATL_S_NONE if span is NULL/uninitialized
 */
YATL_SpanType_t YATL_span_type(const YATL_Span_t *span);

/**
 * @brief Get text content of a single-line span.
 * @ingroup yatl_span_query
 *
 * Returns a pointer to the text content of a span that fits
 * on a single line. For multi-line spans, use YATL_span_iter_line().
 *
 * @param in_span     Span to get text from
 * @param out_text Output pointer to text (not null-terminated)
 * @param out_len  Output length of text
 *
 * @return YATL_OK on success
 * @return YATL_ERR_TYPE if span is multi-line
 * @return YATL_ERR_INVALID_ARG if any parameter is NULL/uninitialized
 *
 * @note The returned pointer is into the document's internal buffer
 *       and becomes invalid if the document is modified or freed.
 */
YATL_Result_t YATL_span_text(const YATL_Span_t *in_span, const char **out_text,
                             size_t *out_len);

/**
 * @brief Convenience function to find a key and get its string value.
 * @ingroup yatl_span_query
 *
 * Combines YATL_span_find_name(), YATL_span_keyval_slice(), and
 * YATL_span_text() into a single call for simple key-value lookups.
 *
 * @param in_span  Span to search within
 * @param key      Key name to find
 * @param out_text Output pointer to value text (not null-terminated)
 * @param out_len  Output length of value text
 *
 * @return YATL_OK on success
 * @return YATL_ERR_NOT_FOUND if key not found
 * @return YATL_ERR_TYPE if value is multi-line or not a key-value
 * @return YATL_ERR_INVALID_ARG if any parameter is NULL/uninitialized
 *
 * @code
 * const char *host;
 * size_t len;
 * if (YATL_span_get_string(&table_span, "host", &host, &len) == YATL_OK) {
 *     printf("Host: %.*s\n", (int)len, host);
 * }
 * @endcode
 */
YATL_Result_t YATL_span_get_string(const YATL_Span_t *in_span, const char *key,
                                   const char **out_text, size_t *out_len);

/**
 * @brief Set the value of a span, supporting multi-line values.
 * @ingroup yatl_span_modify
 *
 * Replaces the value content of a span with new content, which may
 * span multiple lines. The operation is atomic - if parsing fails,
 * the original content is preserved.
 *
 * @param span       Span containing the value to replace
 * @param lines      Array of line content strings
 * @param lengths    Array of lengths for each line
 * @param line_count Number of lines in the arrays
 *
 * @return YATL_OK on success
 * @return YATL_ERR_SYNTAX if new value would create invalid TOML
 * @return YATL_ERR_NOMEM if memory allocation fails
 * @return YATL_ERR_INVALID_ARG if any parameter is NULL/uninitialized or
 * line_count is 0
 *
 * @code
 * // Single-line example
 * const char *line = "new_value";
 * size_t len = strlen(line);
 * YATL_span_set_value(&span, &line, &len, 1);
 *
 * // Multi-line example (e.g., for multiline strings)
 * const char *lines[] = {"first line", "second line", "third line"};
 * size_t lengths[] = {10, 11, 10};
 * YATL_span_set_value(&span, lines, lengths, 3);
 * @endcode
 */
YATL_Result_t YATL_span_ml_set_value(YATL_Span_t *span, const char **lines,
                                     const size_t *lengths, size_t line_count);
/**
 * @brief Set the value of a span with a single-line value.
 * @ingroup yatl_span_modify
 * Replaces the value content of a span with new content provided
 * as a single string. The operation is atomic - if parsing fails,
 * the original content is preserved.
 * @param span   Span containing the value to replace
 * @param value  New value string
 * @param length Length of the new value string in bytes
 * @return YATL_OK on success
 * @return YATL_ERR_SYNTAX if new value would create invalid TOML
 * @return YATL_ERR_NOMEM if memory allocation fails
 * @return YATL_ERR_INVALID_ARG if any parameter is NULL/uninitialized
 */

YATL_Result_t YATL_span_set_value(YATL_Span_t *span, const char *value,
                                  size_t length);

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define YATL_LINE_SIZE 64 
#define YATL_CURSOR_SIZE 64 
#define YATL_SPAN_SIZE 64 
#define YATL_DOC_SIZE 64 

typedef struct YATL_Line {
    _Alignas(max_align_t) unsigned char _opaque[YATL_LINE_SIZE];
} YATL_Line_t;

typedef struct YATL_Cursor {
    _Alignas(max_align_t) unsigned char _opaque[YATL_CURSOR_SIZE];
} YATL_Cursor_t;

typedef struct YATL_Span {
    _Alignas(max_align_t) unsigned char _opaque[YATL_SPAN_SIZE];
} YATL_Span_t;

typedef struct YATL_Doc {
    _Alignas(max_align_t) unsigned char _opaque[YATL_DOC_SIZE];
} YATL_Doc_t;


typedef enum {
    YATL_LOG_DEBUG,
    YATL_LOG_INFO,
    YATL_LOG_WARN,
    YATL_LOG_ERROR
} YATL_log_level_t;

#define YATL_ENABLE_LOGGING

#ifdef YATL_ENABLE_LOGGING
  #ifndef LOG_LEVEL
  #define LOG_LEVEL YATL_LOG_DEBUG
  #endif
#endif


static inline const char* log_level_str(YATL_log_level_t level) {
    switch (level) {
        case YATL_LOG_DEBUG: return "DEBUG";
        case YATL_LOG_INFO:  return "INFO";
        case YATL_LOG_WARN:  return "WARN";
        case YATL_LOG_ERROR: return "ERROR";
        default:        return "UNKNOWN";
    }
}

#ifdef YATL_ENABLE_LOGGING
#define YATL_LOG(level, fmt, ...)                                 \
    do {                                                          \
      if ((level) >= LOG_LEVEL) {                                 \
        fprintf(stderr,                                           \
            "[%s] %s:%d (%s): " fmt "\n",                         \
            log_level_str(level),                                 \
            __FILE__,                                             \
            __LINE__,                                             \
            __func__,                                             \
            ##__VA_ARGS__);                                       \
      }                                                           \
    } while (0)
#else
#define YATL_LOG(level, fmt, ...) do {} while (0)
#endif


// ---------------------------------------------------------------------
// Span - 2D region in document
//
// For TABLE/ARRAY_TABLE: covers header line through last line before
// next table header (the full section extent).
// For KEYVAL: covers the key = value (may span lines for multiline strings).
// ---------------------------------------------------------------------

typedef enum {
    YATL_S_NONE,
    YATL_S_NODE_TABLE,        // [table.name]
    YATL_S_NODE_ARRAY,        // [val1, val2, ...]
    YATL_S_NODE_ARRAY_TABLE,  // [[array.table]]
    YATL_S_NODE_INLINE_TABLE, // {key = val, ...}
    YATL_S_LEAF_KEYVAL,       // key = value
    YATL_S_LEAF_COMMENT,      // # comment
    YATL_S_SLICE_KEY,         // key part of key = value 
    YATL_S_SLICE_VALUE,       // atomic value (string, int, float, bool, datetime)
} YATL_SpanType_t;


// ---------------------------------------------------------------------
// Value types (for YATL_span_get_value out_type)
// ---------------------------------------------------------------------

// TODO: Date/time types returned as strings for now.
// TODO: Inline tables - API TBD.

typedef enum {
    YATL_TYPE_BAREVALUE,     // number, bool, date, time - user interprets
    YATL_TYPE_STRING,        // quoted string (basic or literal)
    YATL_TYPE_ARRAY,         // [...]
    YATL_TYPE_INLINE_TABLE,  // {...}
} YATL_ValueType_t;

// ---------------------------------------------------------------------
// Error codes
// ---------------------------------------------------------------------

typedef enum {
    YATL_OK = 0,
    YATL_DONE,              // Iteration complete (not an error)
    YATL_ERR_IO,
    YATL_ERR_SYNTAX,
    YATL_ERR_NOT_FOUND,
    YATL_ERR_TYPE,
    YATL_ERR_BUFFER,
    YATL_ERR_NOMEM,
    YATL_ERR_INVALID_ARG,
} YATL_Result_t;

// ---------------------------------------------------------------------
// Document lifecycle
// ---------------------------------------------------------------------

void YATL_doc_init(YATL_Doc_t *doc);
YATL_Result_t YATL_doc_load(YATL_Doc_t *doc, const char *path);
YATL_Result_t YATL_doc_loads(YATL_Doc_t *doc, const char *str, size_t len);
YATL_Result_t YATL_doc_save(YATL_Doc_t *doc, const char *path);
void YATL_doc_free(YATL_Doc_t *doc);

// ---------------------------------------------------------------------
// Span utilities
// ---------------------------------------------------------------------

// Get a span covering the entire document (type NONE)
YATL_Result_t YATL_doc_span(const YATL_Doc_t *doc, YATL_Span_t *out_span);

// Find next span within in_span boundary, starting at cursor (or in_span->c_start if NULL)
// Cursor is advanced past the returned span, ready for the next iteration
YATL_Result_t YATL_span_find_next(const YATL_Span_t *in_span, YATL_Cursor_t *cursor,
                              YATL_Span_t *out_span);

// Find a TABLE or KEYVAL by name within in_span (literal match)
// Set find_nested=true to search within nested components like tables
YATL_Result_t YATL_span_find_name(const YATL_Span_t *in_span, const char *name, YATL_Span_t *out_span);

// Iterate over line segments within a span
// Caller initializes cursor to zero or to starting position
// Returns YATL_OK and sets out_text/out_len for each line segment
// Returns YATL_DONE when iteration is complete
// Cursor is advanced automatically for next iteration
YATL_Result_t YATL_span_iter_line(const YATL_Span_t *span,
                                   YATL_Cursor_t *cursor,
                                   const char **out_text,
                                   size_t *out_len);

YATL_Result_t YATL_cursor_move(YATL_Cursor_t *cursor, long npos); 
//TODO append or delete element may need to return new cursor pos

const char *YATL_span_type_name(YATL_SpanType_t type);

YATL_Result_t YATL_span_keyval_slice(const YATL_Span_t *span, YATL_Span_t *key, YATL_Span_t *val);

YATL_SpanType_t YATL_span_type(const YATL_Span_t *span);

// Get text content of a single-line span directly
// Returns YATL_OK for single-line spans, YATL_ERR_TYPE for multi-line
YATL_Result_t YATL_span_text(const YATL_Span_t *span, const char **out_text, size_t *out_len);

// Convenience: find key by name and get its value text (single-line only)
// Combines find_name → keyval_slice → span_text
YATL_Result_t YATL_span_get_string(const YATL_Span_t *in_span, const char *key,
                                   const char **out_text, size_t *out_len);

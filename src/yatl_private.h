#pragma once
#include "yatl.h"
#include <stdint.h>

#define YATL_CURSOR_MAGIC 0x43555253 // 'CURS'
#define YATL_SPAN_MAGIC 0x5350414E   // 'SPAN'
#define YATL_DOC_MAGIC 0x444F4354    // 'DOCT'
#define YATL_LINE_MAGIC 0x4C494E45   // 'LINE'

// YATL private header - not part of public API
//
// NOTE: Public types (YATL_*_t) are opaque byte arrays. Internal code casts
// to _YATL_*_t to access fields. Size constants in yatl.h must be >= sizeof
// internal structs. Use static_assert in yatl.c to verify.

// Forward declaration for back-pointer
typedef struct _YATL_Doc _YATL_Doc_t;

typedef struct _YATL_Line {
  uint32_t magic; // YATL_LINE_MAGIC
  char *text;
  size_t len;
  uint32_t linenum; // line number in document (starting from 1)
  struct _YATL_Line *prev, *next;
  _YATL_Doc_t *doc; // Back-pointer to owning document (for boneyard access)
} _YATL_Line_t;

typedef struct _YATL_Cursor {
  uint32_t magic; // YATL_CURSOR_MAGIC
  _YATL_Line_t *line;
  size_t pos;
  bool complete;
} _YATL_Cursor_t;

typedef struct {
  uint32_t magic; // YATL_SPAN_MAGIC
  YATL_SpanType_t type;
  _YATL_Cursor_t c_start; // Lexical start (includes quotes, brackets, etc.)
  _YATL_Cursor_t c_end;   // Lexical end
  _YATL_Cursor_t
      s_c_start; // Semantic start (content only, NULL line if same as lexical)
  _YATL_Cursor_t s_c_end; // Semantic end (NULL line if same as lexical)
} _YATL_Span_t;

// ---------------------------------------------------------------------
// Document - doubly linked list of lines
// ---------------------------------------------------------------------

struct _YATL_Doc {
  uint32_t magic; // YATL_DOC_MAGIC
  _YATL_Line_t *head;
  _YATL_Line_t *tail;
  _YATL_Line_t *boneyard_head; // Head of deleted lines list (freed on doc_free
                               // or clear_boneyard)
  _YATL_Line_t *boneyard_tail; // Tail for O(1) append
};

static const _YATL_Span_t _YATL_EMPTY_SPAN = {
    .magic = YATL_SPAN_MAGIC,
    .c_start = {.magic = YATL_CURSOR_MAGIC},
    .c_end = {.magic = YATL_CURSOR_MAGIC},
    .s_c_start = {.magic = YATL_CURSOR_MAGIC},
    .s_c_end = {.magic = YATL_CURSOR_MAGIC}};

static const _YATL_Cursor_t _YATL_EMPTY_CURSOR = {.magic = YATL_CURSOR_MAGIC};

static const _YATL_Doc_t _YATL_EMPTY_DOC = {.magic = YATL_DOC_MAGIC};

static const _YATL_Line_t _YATL_EMPTY_LINE = {.magic = YATL_LINE_MAGIC};

// ---------------------------------------------------------------------
// Magic value validation
// Use at entry points of functions that receive these types as input
// Returns YATL_OK if valid, YATL_ERR_INVALID_ARG if not initialized
// ---------------------------------------------------------------------

static inline YATL_Result_t _YATL_check_span(const _YATL_Span_t *s) {
  if (s && s->magic != YATL_SPAN_MAGIC) {
    YATL_LOG(YATL_LOG_ERROR,
             "Span not initialized (magic: 0x%08X, expected: 0x%08X)", s->magic,
             YATL_SPAN_MAGIC);
    return YATL_ERR_INVALID_ARG;
  }
  return YATL_OK;
}

static inline YATL_Result_t _YATL_check_cursor(const _YATL_Cursor_t *c) {
  if (c && c->magic != YATL_CURSOR_MAGIC) {
    YATL_LOG(YATL_LOG_ERROR,
             "Cursor not initialized (magic: 0x%08X, expected: 0x%08X)",
             c->magic, YATL_CURSOR_MAGIC);
    return YATL_ERR_INVALID_ARG;
  }
  return YATL_OK;
}

static inline YATL_Result_t _YATL_check_doc(const _YATL_Doc_t *d) {
  if (d && d->magic != YATL_DOC_MAGIC) {
    YATL_LOG(YATL_LOG_ERROR,
             "Doc not initialized (magic: 0x%08X, expected: 0x%08X)", d->magic,
             YATL_DOC_MAGIC);
    return YATL_ERR_INVALID_ARG;
  }
  return YATL_OK;
}

static inline YATL_Result_t _YATL_check_line(const _YATL_Line_t *l) {
  if (l && l->magic != YATL_LINE_MAGIC) {
    YATL_LOG(YATL_LOG_ERROR,
             "Line not initialized (magic: 0x%08X, expected: 0x%08X)", l->magic,
             YATL_LINE_MAGIC);
    return YATL_ERR_INVALID_ARG;
  }
  return YATL_OK;
}

static YATL_Result_t _toml_value_parse(const _YATL_Cursor_t *value_start,
                                       _YATL_Span_t *out_span,
                                       YATL_ValueType_t *out_type);

static YATL_Result_t _toml_key_parse(const _YATL_Span_t *key_span,
                                     _YATL_Span_t *out_span,
                                     YATL_ValueType_t *out_type);

// ---------------------------------------------------------------------
// Internal helpers (defined in yatl.c)
// ---------------------------------------------------------------------

_YATL_Line_t *_line_alloc(const char *text, size_t len);
void _line_free(_YATL_Line_t *line);
void _line_unlink(_YATL_Line_t *line);
void _line_relink(_YATL_Doc_t *doc, _YATL_Line_t *line, _YATL_Line_t *before);
void _boneyard_append(_YATL_Doc_t *doc, _YATL_Line_t *first);

// ---------------------------------------------------------------------
// Span unlink/relink - atomic modification support
//
// These functions support atomic span modifications with rollback:
// 1. span_unlink: moves span's lines to boneyard as backup
// 2. Create new lines with modified content, insert into doc
// 3. Try to parse new content
// 4. On failure: span_relink restores original lines from boneyard
// ---------------------------------------------------------------------

// Unlinks span's lines from document, moves to boneyard as backup
// out_reinsert_pos: line after removed span (line=NULL if span was at end), pos
// always 0 out_prefix_cursor: prefix line created if span started mid-line
// (line=NULL if none) out_suffix_cursor: suffix line created if span ended
// mid-line (line=NULL if none) Span's cursors remain valid (point to full
// original lines in boneyard)
YATL_Result_t _YATL_span_unlink(YATL_Span_t *span,
                                YATL_Cursor_t *out_reinsert_pos,
                                YATL_Cursor_t *out_prefix_cursor,
                                YATL_Cursor_t *out_suffix_cursor);

// Relinks span's lines back into document, restoring original structure
// Removes prefix/suffix lines (if not NULL) before relinking span
// If reinsert_pos->line is NULL, appends to end of document
YATL_Result_t _YATL_span_relink(YATL_Doc_t *doc, YATL_Span_t *span,
                                const YATL_Cursor_t *reinsert_pos,
                                const YATL_Cursor_t *prefix_cursor,
                                const YATL_Cursor_t *suffix_cursor);

static inline bool _compare_cursor(const _YATL_Cursor_t *a,
                                   const _YATL_Cursor_t *b) {
  if (a->line == b->line && a->pos == b->pos) {
    return true;
  }
  return false;
}

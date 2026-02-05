#include "yatl_lexer.h"
#include "yatl_private.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static_assert(sizeof(_YATL_Cursor_t) <= YATL_CURSOR_SIZE,
              "YATL_CURSOR_SIZE too small");
static_assert(sizeof(_YATL_Span_t) <= YATL_SPAN_SIZE,
              "YATL_SPAN_SIZE too small");
static_assert(sizeof(_YATL_Doc_t) <= YATL_DOC_SIZE, "YATL_DOC_SIZE too small");
static_assert(sizeof(_YATL_Line_t) <= YATL_LINE_SIZE,
              "YATL_LINE_SIZE too small");

// ---------------------------------------------------------------------
// Structure initialization
// ---------------------------------------------------------------------

YATL_Cursor_t YATL_cursor_create(void) {
  YATL_Cursor_t c;
  *(_YATL_Cursor_t *)&c = _YATL_EMPTY_CURSOR;
  return c;
}

YATL_Span_t YATL_span_create(void) {
  YATL_Span_t s;
  *(_YATL_Span_t *)&s = _YATL_EMPTY_SPAN;
  return s;
}

YATL_Doc_t YATL_doc_create(void) {
  YATL_Doc_t d;
  *(_YATL_Doc_t *)&d = _YATL_EMPTY_DOC;
  return d;
}

// ---------------------------------------------------------------------
// Line allocation
// ---------------------------------------------------------------------

_YATL_Line_t *_line_alloc(const char *text, size_t len) {
  _YATL_Line_t *line = malloc(sizeof(_YATL_Line_t));
  if (!line)
    return NULL;

  line->text = malloc(len);
  if (!line->text) {
    free(line);
    return NULL;
  }

  line->magic = YATL_LINE_MAGIC;
  if (text)
    memcpy(line->text, text, len);
  line->len = len;
  line->prev = NULL;
  line->next = NULL;
  line->doc = NULL; // Set when added to doc
  return line;
}

void _line_free(_YATL_Line_t *line) {
  if (line) {
    free(line->text);
    free(line);
  }
}

// Appends a line (or chain of lines) to the end of the boneyard
// Clears doc pointers for all lines in the chain
// O(1) append using boneyard_tail
void _boneyard_append(_YATL_Doc_t *doc, _YATL_Line_t *first) {
  if (!doc || !first)
    return;

  // Find last line in chain and clear doc pointers
  _YATL_Line_t *last = first;
  for (_YATL_Line_t *line = first; line; line = line->next) {
    line->doc = NULL;
    last = line;
  }

  if (!doc->boneyard_head) {
    first->prev = NULL;
    doc->boneyard_head = first;
    doc->boneyard_tail = last;
  } else {
    doc->boneyard_tail->next = first;
    first->prev = doc->boneyard_tail;
    doc->boneyard_tail = last;
  }
}

// Relinks a line from boneyard back into document, inserting before 'before'
// If before is NULL, appends to document end
void _line_relink(_YATL_Doc_t *doc, _YATL_Line_t *line, _YATL_Line_t *before) {
  if (!doc || !line)
    return;

  // Remove from boneyard
  if (line->prev)
    line->prev->next = line->next;
  else
    doc->boneyard_head = line->next;

  if (line->next)
    line->next->prev = line->prev;
  else
    doc->boneyard_tail = line->prev;

  // Insert into document
  if (before) {
    // Insert before 'before'
    line->next = before;
    line->prev = before->prev;
    if (before->prev)
      before->prev->next = line;
    else
      doc->head = line;
    before->prev = line;
  } else {
    // Append to end
    line->prev = doc->tail;
    line->next = NULL;
    if (doc->tail)
      doc->tail->next = line;
    else
      doc->head = line;
    doc->tail = line;
  }

  line->doc = doc;
}

// Unlinks line from document and adds to boneyard (deferred free)
// Uses line->doc back-pointer, so no doc parameter needed
void _line_unlink(_YATL_Line_t *line) {
  if (!line || !line->doc)
    return;

  _YATL_Doc_t *doc = line->doc;

  // Update prev/next pointers in document
  if (line->prev) {
    line->prev->next = line->next;
  } else {
    doc->head = line->next; // Was head
  }

  if (line->next) {
    line->next->prev = line->prev;
  } else {
    doc->tail = line->prev; // Was tail
  }

  // Add to boneyard
  line->next = NULL;
  _boneyard_append(doc, line);
}

static void _doc_append_line(_YATL_Doc_t *doc, _YATL_Line_t *line) {
  line->doc = doc; // Set back-pointer
  if (!doc->head) {
    doc->head = line;
    doc->tail = line;
    line->linenum = 1;
  } else {
    line->prev = doc->tail;
    line->linenum = line->prev->linenum + 1;
    doc->tail->next = line;
    doc->tail = line;
  }
}

// ---------------------------------------------------------------------
// Document lifecycle
// ---------------------------------------------------------------------

void YATL_doc_free(YATL_Doc_t *doc) {
  if (!doc)
    return;
  _YATL_Doc_t *_doc = (_YATL_Doc_t *)doc;
  if (_YATL_check_doc(_doc) != YATL_OK)
    return;

  // Free active lines
  _YATL_Line_t *line = _doc->head;
  while (line) {
    _YATL_Line_t *next = line->next;
    _line_free(line);
    line = next;
  }

  // Free boneyard lines
  line = _doc->boneyard_head;
  while (line) {
    _YATL_Line_t *next = line->next;
    _line_free(line);
    line = next;
  }

  _doc->head = NULL;
  _doc->tail = NULL;
  _doc->boneyard_head = NULL;
  _doc->boneyard_tail = NULL;
}

YATL_Result_t YATL_doc_clear_boneyard(YATL_Doc_t *doc) {
  if (!doc)
    return YATL_ERR_INVALID_ARG;

  _YATL_Doc_t *_doc = (_YATL_Doc_t *)doc;
  YATL_Result_t res = _YATL_check_doc(_doc);
  if (res != YATL_OK)
    return res;
  _YATL_Line_t *line = _doc->boneyard_head;
  while (line) {
    _YATL_Line_t *next = line->next;
    _line_free(line);
    line = next;
  }
  _doc->boneyard_head = NULL;
  _doc->boneyard_tail = NULL;
  return YATL_OK;
}

YATL_Result_t YATL_doc_loads(YATL_Doc_t *doc, const char *str, size_t str_len) {
  if (!doc || !str)
    return YATL_ERR_NOMEM;
  *doc = YATL_doc_create();
  _YATL_Doc_t *_doc = (_YATL_Doc_t *)doc;

  const char *line_start = str;
  const char *end = str + str_len;

  for (const char *p = str; p < end; p++) {
    if (*p == '\n') {
      size_t len = p - line_start;
      if (len > 0 && line_start[len - 1] == '\r') // no windows newline
        len--;

      _YATL_Line_t *line = _line_alloc(line_start, len); // no newline
      if (!line) {
        YATL_doc_free(doc);
        return YATL_ERR_NOMEM;
      }
      _doc_append_line(_doc, line);
      line_start = p + 1;
    }
  }

  // Last line
  if (end > line_start) {
    size_t len = end - line_start;
    if (len > 0 && line_start[len - 1] == '\r')
      len--;

    _YATL_Line_t *line = _line_alloc(line_start, len);
    if (!line) {
      YATL_doc_free(doc);
      return YATL_ERR_NOMEM;
    }
    _doc_append_line(_doc, line);
  }

  return YATL_OK;
}

YATL_Result_t YATL_doc_load(YATL_Doc_t *doc, const char *path) {
  if (!doc || !path)
    return YATL_ERR_IO;

  FILE *f = fopen(path, "rb");
  if (!f)
    return YATL_ERR_IO;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size < 0) {
    fclose(f);
    return YATL_ERR_IO;
  }

  char *buf = malloc(size + 1);
  if (!buf) {
    fclose(f);
    return YATL_ERR_NOMEM;
  }

  size_t nread = fread(buf, 1, size, f);
  fclose(f);

  YATL_Result_t err = YATL_doc_loads(doc, buf, nread);
  free(buf);

  return err;
}

// Helper: check if cursor is past boundary
static inline bool _cursor_past(_YATL_Line_t *line, size_t pos,
                                const _YATL_Cursor_t *bound) {
  if (!bound->line)
    return false; // No boundary

  if (line == bound->line)
    return pos >= bound->pos;
  // Check if line comes after bound->line by walking forward
  for (_YATL_Line_t *l = bound->line->next; l; l = l->next) {
    if (l == line)
      return true;
  }
  return false;
}

const char *YATL_span_type_name(YATL_SpanType_t type) {
  switch (type) {
  case YATL_S_NONE:
    return "YATL_S_NONE";
  case YATL_S_NODE_TABLE:
    return "YATL_S_NODE_TABLE";
  case YATL_S_NODE_ARRAY:
    return "YATL_S_NODE_ARRAY";
  case YATL_S_NODE_ARRAY_TABLE:
    return "YATL_S_NODE_ARRAY_TABLE";
  case YATL_S_LEAF_KEYVAL:
    return "YATL_S_LEAF_KEYVAL";
  case YATL_S_LEAF_COMMENT:
    return "YATL_S_LEAF_COMMENT";
  case YATL_S_SLICE_VALUE:
    return "YATL_S_SLICE_VALUE";
  case YATL_S_SLICE_KEY:
    return "YATL_S_SLICE_KEY";
  default:
    return "UNKNOWN_S_TYPE";
  }
}

YATL_Result_t YATL_cursor_move(YATL_Cursor_t *cursor, long npos) {
  _YATL_Cursor_t *_cursor = (_YATL_Cursor_t *)cursor;
  if (!cursor || !_cursor->line)
    return YATL_ERR_INVALID_ARG;
  YATL_Result_t res = _YATL_check_cursor(_cursor);
  if (res != YATL_OK)
    return res;
  _YATL_Line_t *line = _cursor->line;
  long pos = _cursor->pos;

  pos = pos + npos;

  if (npos > 0) {
    while (pos >= line->len) {
      if (!line->next) {
        _cursor->pos = line->len > 0 ? line->len - 1 : 0;
        _cursor->line = line;
        return YATL_DONE; // end of document
      }
      pos = pos - line->len;
      line = line->next;
    }
  } else if (npos < 0) {
    while (pos < 0) {
      if (!line->prev) {
        _cursor->pos = 0;
        _cursor->line = line;
        return YATL_DONE; // beginning of document
      }
      line = line->prev;
      pos = line->len > 0 ? pos + line->len - 1 : 0;
    }
  }

  _cursor->line = line;
  _cursor->pos = pos;
  return YATL_OK;
}

static inline bool _consume_bool(bool *b) {
  if (*b) {
    *b = false;
    return true;
  }
  return false;
}

static inline bool _valid_for_find_next(const _YATL_Span_t *span) {
  switch (span->type) {
  case YATL_S_LEAF_COMMENT:
  case YATL_S_LEAF_KEYVAL:
  case YATL_S_SLICE_KEY:
  case YATL_S_SLICE_VALUE:

    return false;
  default:
    return true;
  }
}
YATL_Result_t YATL_span_find_next(const YATL_Span_t *in_span,
                                  YATL_Cursor_t *cursor,
                                  YATL_Span_t *out_span) {
  // Detect start, consume to end, emit immediately
  // No complex state tracking needed
  const _YATL_Span_t *_in_span = (const _YATL_Span_t *)in_span;
  _YATL_Cursor_t *_cursor = (_YATL_Cursor_t *)cursor;
  _YATL_Span_t *_out_span = (_YATL_Span_t *)out_span;

  if (!in_span || !_in_span->c_start.line || !out_span)
    return YATL_ERR_INVALID_ARG;
  YATL_Result_t res = _YATL_check_span(_in_span);
  if (res != YATL_OK)
    return res;
  if (_cursor) {
    res = _YATL_check_cursor(_cursor);
    if (res != YATL_OK)
      return res;
  }

  if (!_valid_for_find_next(_in_span)) {
    return YATL_ERR_INVALID_ARG;
  }

  // Needs to come before the check on cursor->line
  if (_cursor && _cursor->complete) {
    YATL_LOG(YATL_LOG_INFO,
             "Cursor is already complete, no more spans to find");
    return YATL_DONE;
  }

  // Initialize output span from template (sets magic, NULL lines for
  // s_c_start/s_c_end)
  *_out_span = _YATL_EMPTY_SPAN;

  // NOTE: A freshly created cursor (via YATL_cursor_create()) has NULL line,
  // which is treated same as NULL cursor - starts from span beginning.
  _YATL_Cursor_t cr = (cursor && _cursor->line) ? *_cursor : _in_span->c_start;
  _YATL_Cursor_t *out_cursor = _cursor; // may be NULL

  bool skip_first = ((_in_span->type != YATL_S_NONE) &&
                     (!cursor || _compare_cursor(&cr, &_in_span->c_start)));
  if (skip_first) {
    YATL_LOG(YATL_LOG_INFO,
             "Skipping first span at cursor position (line %u, pos %zu)",
             cr.line->linenum, cr.pos); // FIXME
  }

  // === Array iteration ===
  if (_in_span->type == YATL_S_NODE_ARRAY) {
    // Skip [ if at start of span
    if (_compare_cursor(&cr, &_in_span->c_start) && cr.line &&
        cr.pos < cr.line->len && cr.line->text[cr.pos] == '[') {
      cr.pos++;
      _skipWS(&cr);
    }

    if (!cr.line || cr.pos >= cr.line->len)
      return YATL_ERR_NOT_FOUND;

    if (cr.line->text[cr.pos] == ']')
      return YATL_DONE;

    // Parse value - this sets both lexical and semantic bounds
    YATL_ValueType_t val_type;
    YATL_Result_t res = _toml_value_parse(&cr, _out_span, &val_type);
    if (res != YATL_OK)
      return res;

    // Advance cr past the value's lexical end
    cr = _out_span->c_end;

    // Extend lexical end to include trailing comma/whitespace (for unlinking)
    // NOTE: Order matters - can't combine into skipany(" \t,") or we'd skip
    // into the next element's leading content.
    _skipWS(&cr);
    _skipany(&cr, ",");
    _out_span->c_end = cr; // Lexical end includes comma/whitespace
    _skipWS(&cr);

    if (out_cursor)
      *out_cursor = cr;
    return YATL_OK;
  }

  // === Inline table iteration ===
  if (_in_span->type == YATL_S_NODE_INLINE_TABLE) {
    _YATL_Cursor_t temp_c = cr;
    YATL_LOG(YATL_LOG_DEBUG, "Inline table iteration at line %u, pos %zu",
             temp_c.line->linenum, temp_c.pos);
    // Skip { if at start of span
    if (_compare_cursor(&cr, &_in_span->c_start) && cr.line &&
        cr.pos < cr.line->len && cr.line->text[cr.pos] == '{') {
      cr.pos++;
      _skipWS(&cr);
      YATL_LOG(YATL_LOG_DEBUG, "Skipped '{', now at line %u, pos %zu",
               cr.line->linenum, cr.pos);
    }

    if (!cr.line || cr.pos >= cr.line->len)
      return YATL_ERR_NOT_FOUND;

    if (cr.line->text[cr.pos] == '}')
      return YATL_DONE;

    // Parse key-value pair
    _out_span->type = YATL_S_LEAF_KEYVAL;
    _out_span->c_start = cr;
    YATL_LOG(YATL_LOG_DEBUG, "Parsing key-value pair at line %u, pos %zu",
             cr.line->linenum, cr.pos);
    res = _consume(&cr, _TOML_KEY);
    if (res != YATL_OK)
      return res;
    cr.pos++; // Skip =
    res = _consume(&cr, _TOML_VALUE);
    if (res != YATL_OK)
      return res;

    // Skip whitespace, comma, whitespace
    _skipWS(&cr);
    _skipany(&cr, ",");
    _out_span->c_end = cr; // End to include comma/whitespace
    _skipWS(&cr);

    if (out_cursor)
      *out_cursor = cr;
    return YATL_OK;
  }

  // The loop below looks for the start of the next span to pass to _consume
  // _consume then skips all of those characters
  // The user will drill into elements as needed, this returns the parent
  // elements
next_span:

  if (_skipWS(&cr) == YATL_DONE)
    return YATL_DONE;

  if (!cr.line || cr.pos >= cr.line->len)
    return YATL_ERR_NOT_FOUND;
  char c = cr.line->text[cr.pos];
  char c1 = (cr.pos + 1 < cr.line->len) ? cr.line->text[cr.pos + 1] : '\0';

  // === Table [[...]] or [...] at line start ===
  if (c == '[') {
    _out_span->c_start = cr;

    if (c1 == '[') {
      // Array Table [[...]]
      _out_span->type = YATL_S_NODE_ARRAY_TABLE;
      res = _consume(&cr, _TOML_TABLE_ARRAY_HEADER);
      if (res != YATL_OK)
        return res;
      if (_consume_bool(&skip_first))
        goto next_span;
      YATL_Result_t res = _consume(&cr, _TOML_TABLE_ARRAY_BODY);
      if (res != YATL_OK)
        return res;
      _out_span->c_end = cr;
      if (out_cursor)
        *out_cursor = cr;
      return YATL_OK;
    } else {
      // Table [...]
      _out_span->type = YATL_S_NODE_TABLE;
      res = _consume(&cr, _TOML_TABLE_HEADER);
      if (res != YATL_OK)
        return res;
      if (_consume_bool(&skip_first))
        goto next_span;
      res = _consume(&cr, _TOML_TABLE_BODY);
      if (res != YATL_OK)
        return res;
      _out_span->c_end = cr;
      if (out_cursor)
        *out_cursor = cr;
      return YATL_OK;
    }
  }

  // === Comment ===
  if (c == '#') {
    _out_span->type = YATL_S_LEAF_COMMENT;
    _out_span->c_start = cr;
    _consume(&cr, _TOML_COMMENT);
    _out_span->c_end = cr;
    if (_consume_bool(&skip_first))
      goto next_span;
    if (out_cursor)
      *out_cursor = cr;
    return YATL_OK;
  }

  // === Keyval (bare key or quoted key) ===
  if (_is_bare_key_char(c) || c == '"' || c == '\'') {
    _out_span->type = YATL_S_LEAF_KEYVAL;
    _out_span->c_start = cr;
    res = _consume(&cr, _TOML_KEY);
    if (res != YATL_OK)
      return res;
    cr.pos++; // Skip = (key ends at =, need to move past it)
    res = _consume(&cr, _TOML_VALUE);
    if (res != YATL_OK)
      return res;
    _out_span->c_end = cr;
    if (_consume_bool(&skip_first))
      goto next_span;
    if (out_cursor)
      *out_cursor = cr;
    return YATL_OK;
  }

  // Unknown character - skip and try again
  cr.pos++;
  YATL_LOG(YATL_LOG_WARN,
           "Skipping unknown character: '%c' at line %u, pos %zu", c,
           cr.line->linenum, cr.pos - 1);
  goto next_span;
}

YATL_Result_t YATL_doc_span(const YATL_Doc_t *doc, YATL_Span_t *out_span) {
  if (!doc || !out_span)
    return YATL_ERR_INVALID_ARG;
  _YATL_Span_t *_out_span = (_YATL_Span_t *)out_span;
  const _YATL_Doc_t *_doc = (const _YATL_Doc_t *)doc;
  YATL_Result_t res = _YATL_check_doc(_doc);
  if (res != YATL_OK)
    return res;

  // Initialize from template (sets magic, NULL lines for s_c_start/s_c_end)
  *_out_span = _YATL_EMPTY_SPAN;
  _out_span->type = YATL_S_NONE;
  _out_span->c_start.line = _doc->head;
  _out_span->c_start.pos = 0;

  if (_doc->tail) {
    _out_span->c_end.line = _doc->tail;
    _out_span->c_end.pos = _doc->tail->len;
  } else {
    _out_span->c_end.line = NULL;
    _out_span->c_end.pos = 0;
  }

  return YATL_OK;
}

// Helper: extract name from a TABLE or KEYVAL span
// For TABLE: extracts between [ and ] (or [[ and ]])
// For KEYVAL: use internal API to extract key
// Returns pointer into the line text (not null-terminated), sets out_len
static const char *_span_get_name(const _YATL_Span_t *span, size_t *out_len) {
  if (!span || !span->c_start.line)
    return NULL;

  _YATL_Line_t *line = span->c_start.line;
  const char *text = line->text;
  size_t start = span->c_start.pos;
  size_t end = (line == span->c_end.line) ? span->c_end.pos : line->len;

  if (span->type == YATL_S_NODE_TABLE) {
    // Skip leading [
    if (start < end && text[start] == '[')
      start++;
    // Find closing ]
    size_t name_end = start;
    while (name_end < end && text[name_end] != ']')
      name_end++;
    *out_len = name_end - start;
    return text + start;
  }

  if (span->type == YATL_S_NODE_ARRAY_TABLE) {
    // Skip leading [[
    if (start + 1 < end && text[start] == '[' && text[start + 1] == '[')
      start += 2;
    // Find closing ]]
    size_t name_end = start;
    while (name_end + 1 < end &&
           !(text[name_end] == ']' && text[name_end + 1] == ']'))
      name_end++;
    *out_len = name_end - start;
    return text + start;
  }

  if (span->type == YATL_S_LEAF_KEYVAL) {
    _YATL_Cursor_t cr_start = span->c_start;
    _YATL_Cursor_t cr_end = cr_start;
    YATL_Result_t res = _consume(&cr_end, _TOML_KEY);
    if (res != YATL_OK)
      return NULL;
    _YATL_Span_t key_span = _YATL_EMPTY_SPAN;
    key_span.type = YATL_S_SLICE_KEY;
    key_span.c_start = cr_start;
    key_span.c_end = cr_end;
    YATL_ValueType_t key_type;

    res = _toml_key_parse(&key_span, &key_span, &key_type);
    if (res != YATL_OK)
      return NULL;
    // Use semantic bounds if available (quoted key), else lexical (bare key)
    _YATL_Cursor_t start =
        key_span.s_c_start.line ? key_span.s_c_start : key_span.c_start;
    _YATL_Cursor_t end =
        key_span.s_c_end.line ? key_span.s_c_end : key_span.c_end;
    *out_len = end.pos - start.pos; // key is always same line
    return start.line->text + start.pos;
  }

  *out_len = 0;
  return NULL;
}

YATL_Result_t YATL_span_find_next_by_name(const YATL_Span_t *in_span,
                                          const char *name,
                                          const YATL_Cursor_t *in_cursor,
                                          YATL_Cursor_t *out_cursor,
                                          YATL_Span_t *out_span) {
  if (!in_span || !name || !out_span)
    return YATL_ERR_INVALID_ARG;

  size_t name_len = strlen(name);
  _YATL_Span_t *_out_span = (_YATL_Span_t *)out_span;
  const _YATL_Span_t *_in_span = (const _YATL_Span_t *)in_span;
  YATL_Result_t res = _YATL_check_span(_in_span);
  if (res != YATL_OK)
    return res;
  if (in_cursor) {
    res = _YATL_check_cursor((const _YATL_Cursor_t *)in_cursor);
    if (res != YATL_OK)
      return res;
  }

  // If in_cursor is NULL or freshly created (NULL line), start from span
  // beginning
  _YATL_Cursor_t _c;
  if (in_cursor && ((_YATL_Cursor_t *)in_cursor)->line) {
    _c = *(const _YATL_Cursor_t *)in_cursor;
  } else {
    _c = _in_span->c_start;
  }
  YATL_Cursor_t *c = (YATL_Cursor_t *)&_c;

  while (YATL_span_find_next(in_span, c, out_span) == YATL_OK) {
    if (_out_span->type == YATL_S_NODE_TABLE ||
        _out_span->type == YATL_S_NODE_ARRAY_TABLE ||
        _out_span->type == YATL_S_LEAF_KEYVAL) {

      size_t span_name_len;
      const char *span_name = _span_get_name(_out_span, &span_name_len);
      YATL_LOG(YATL_LOG_INFO, "Checking span name: %.*s", (int)span_name_len,
               span_name ? span_name : "NULL");
      if (span_name && span_name_len == name_len &&
          memcmp(span_name, name, name_len) == 0) {
        if (out_cursor) {
          *out_cursor = *c;
        }
        return YATL_OK;
      }
    }
    // cursor already advanced by find_next
  }

  return YATL_ERR_NOT_FOUND;
}

YATL_Result_t YATL_span_find_name(const YATL_Span_t *in_span, const char *name,
                                  YATL_Span_t *out_span) {
  return YATL_span_find_next_by_name(in_span, name, NULL, NULL, out_span);
}

YATL_Result_t YATL_span_iter_line(const YATL_Span_t *span,
                                  YATL_Cursor_t *cursor, const char **out_text,
                                  size_t *out_len) {
  if (!span || !cursor || !out_text || !out_len)
    return YATL_ERR_INVALID_ARG;

  _YATL_Cursor_t *_cursor = (_YATL_Cursor_t *)cursor;
  const _YATL_Span_t *_span = (const _YATL_Span_t *)span;
  YATL_Result_t res = _YATL_check_span(_span);
  if (res != YATL_OK)
    return res;
  res = _YATL_check_cursor(_cursor);
  if (res != YATL_OK)
    return res;

  // Use semantic bounds if available, else lexical
  _YATL_Cursor_t span_start =
      _span->s_c_start.line ? _span->s_c_start : _span->c_start;
  _YATL_Cursor_t span_end = _span->s_c_end.line ? _span->s_c_end : _span->c_end;

  if (!_cursor->line) { // cursor is freshly created (NULL line)
    _cursor->line = span_start.line;
    _cursor->pos = span_start.pos;
  }
  _YATL_Line_t *line = _cursor->line;
  assert(span_end.line != NULL); // span must have an end line

  if (line == span_end.line && _cursor->pos >= span_end.pos)
    return YATL_DONE;

  // Calculate start and end positions for this line segment
  size_t start = _cursor->pos;
  size_t end;
  if (span_end.line && line == span_end.line) {
    end = span_end.pos;
  } else {
    end = line->len;
  }

  // Set output
  *out_text = line->text + start;
  *out_len = (end > start) ? (end - start) : 0;

  if (line == span_end.line) {
    // On end line - mark complete for next call
    _cursor->pos = span_end.pos;
  } else {
    _cursor->line = line->next;
    _cursor->pos = 0;
  }

  return YATL_OK;
}

// Parse key from a keyval span
// Safe to use same span for in and out
//
// Sets both lexical bounds (c_start/c_end) and semantic bounds
// (s_c_start/s_c_end). For quoted keys: lexical includes quotes, semantic
// excludes them. For bare keys: s_c_start.line is NULL, meaning semantic ==
// lexical.
static YATL_Result_t _toml_key_parse(const _YATL_Span_t *key_span,
                                     _YATL_Span_t *out_span,
                                     YATL_ValueType_t *out_type) {

  if (!key_span || !out_span || !out_type)
    return YATL_ERR_INVALID_ARG;

  _YATL_Cursor_t cr = key_span->c_start;

  // Skip leading whitespace
  if (_skipWS(&cr) == YATL_DONE)
    return YATL_ERR_SYNTAX;

  char first_char = cr.line->text[cr.pos];

  // Work on temp to handle in==out case safely
  _YATL_Span_t temp_span = _YATL_EMPTY_SPAN;
  temp_span.type = YATL_S_SLICE_KEY;

  // Double-quoted key: "key" - supports escape sequences
  if (first_char == '"') {
    temp_span.c_start = cr;   // Lexical start: at opening "
    cr.pos++;                 // skip opening quote
    temp_span.s_c_start = cr; // Semantic start: after opening "
    bool escaped = false;
    while (cr.pos < cr.line->len) {
      char c = cr.line->text[cr.pos];

      if (escaped) {
        escaped = false;
        cr.pos++;
        continue;
      }
      if (c == '\\') {
        escaped = true;
        cr.pos++;
        continue;
      }
      if (c == '"') {
        // end of string
        temp_span.s_c_end = cr; // Semantic end: at closing "
        *out_type = YATL_TYPE_STRING;
        // grammar check - after closing quote should be whitespace then =
        cr.pos++;             // move past closing quote
        temp_span.c_end = cr; // Lexical end: after closing "
        if (_skipWS(&cr) != YATL_DONE && cr.line->text[cr.pos] != '=') {
          YATL_LOG(YATL_LOG_ERROR, "Invalid character after key string");
          return YATL_ERR_SYNTAX;
        }
        *out_span = temp_span;
        return YATL_OK;
      }
      cr.pos++;
    }
    return YATL_ERR_SYNTAX; // unclosed quote
  }

  // Single-quoted key: 'key' - literal, no escape sequences
  if (first_char == '\'') {
    temp_span.c_start = cr;   // Lexical start: at opening '
    cr.pos++;                 // skip opening quote
    temp_span.s_c_start = cr; // Semantic start: after opening '
    while (cr.pos < cr.line->len) {
      char c = cr.line->text[cr.pos];

      if (c == '\'') {
        // end of string - no escape sequences in single-quoted
        temp_span.s_c_end = cr; // Semantic end: at closing '
        *out_type = YATL_TYPE_STRING;
        // grammar check - after closing quote should be whitespace then =
        cr.pos++;             // move past closing quote
        temp_span.c_end = cr; // Lexical end: after closing '
        if (_skipWS(&cr) != YATL_DONE && cr.line->text[cr.pos] != '=') {
          YATL_LOG(YATL_LOG_ERROR, "Invalid character after key string");
          return YATL_ERR_SYNTAX;
        }
        *out_span = temp_span;
        return YATL_OK;
      }
      cr.pos++;
    }
    return YATL_ERR_SYNTAX; // unclosed quote
  }

  // Bare key: only A-Za-z0-9_- allowed
  // semantic == lexical, so s_c_start/s_c_end stay NULL (zero-initialized)
  if (_is_bare_key_char(first_char)) {
    temp_span.c_start = cr;

    while (cr.pos < cr.line->len && _is_bare_key_char(cr.line->text[cr.pos])) {
      cr.pos++;
    }

    temp_span.c_end = cr; // one past last bare key char (exclusive)
    *out_type = YATL_TYPE_STRING;
    // grammar check - after bare key should be whitespace then =
    if (_skipWS(&cr) != YATL_DONE && cr.line->text[cr.pos] != '=') {
      YATL_LOG(YATL_LOG_ERROR, "Invalid character after bare key");
      return YATL_ERR_SYNTAX;
    }
    *out_span = temp_span;
    return YATL_OK;
  }

  return YATL_ERR_SYNTAX; // invalid key start character
}

// Parse value from a cursor position (should be after the =)
// Returns the value span and its type
//
// Sets both lexical bounds (c_start/c_end) and semantic bounds
// (s_c_start/s_c_end). For strings: lexical includes quotes, semantic excludes
// them. For non-strings: s_c_start.line is NULL, meaning semantic == lexical.
static YATL_Result_t _toml_value_parse(const _YATL_Cursor_t *value_start,
                                       _YATL_Span_t *out_span,
                                       YATL_ValueType_t *out_type) {

  if (!value_start || !out_span || !out_type)
    return YATL_ERR_INVALID_ARG;

  // Initialize from template (sets magic, NULL lines for s_c_start/s_c_end)
  *out_span = _YATL_EMPTY_SPAN;

  _YATL_Cursor_t cr = *value_start;

  // Skip leading whitespace after =
  if (_skipWS(&cr) == YATL_DONE)
    return YATL_ERR_SYNTAX; // no value after =

  if (!cr.line || cr.pos >= cr.line->len)
    return YATL_ERR_SYNTAX;

  out_span->type = YATL_S_SLICE_VALUE;
  out_span->c_start = cr;

  char c = cr.line->text[cr.pos];
  char c1 = (cr.pos + 1 < cr.line->len) ? cr.line->text[cr.pos + 1] : '\0';
  char c2 = (cr.pos + 2 < cr.line->len) ? cr.line->text[cr.pos + 2] : '\0';

  // === Multiline basic string """ ===
  // No semantic bounds for multiline - too complex for editing, user provides
  // full syntax
  if (c == '"' && c1 == '"' && c2 == '"') {
    // c_start already set at opening """
    cr.pos += 3; // skip opening """
    // Skip immediate newline after opening """ (per TOML spec)
    if (cr.pos >= cr.line->len) {
      cr.line = cr.line->next;
      if (!cr.line)
        return YATL_ERR_SYNTAX;
      cr.pos = 0;
    }
    YATL_Result_t res = _consume(&cr, _TOML_STR_ML_BASIC);
    if (res != YATL_OK)
      return res;
    cr.pos += 3;          // move past closing """
    out_span->c_end = cr; // Lexical end: after closing """
    *out_type = YATL_TYPE_STRING;
    return YATL_OK;
  }

  // === Multiline literal string ''' ===
  // No semantic bounds for multiline - too complex for editing, user provides
  // full syntax
  if (c == '\'' && c1 == '\'' && c2 == '\'') {
    // c_start already set at opening '''
    cr.pos += 3; // skip opening '''
    // Skip immediate newline after opening ''' (per TOML spec)
    if (cr.pos >= cr.line->len) {
      cr.line = cr.line->next;
      if (!cr.line)
        return YATL_ERR_SYNTAX;
      cr.pos = 0;
    }
    YATL_Result_t res = _consume(&cr, _TOML_STR_ML_LITERAL);
    if (res != YATL_OK)
      return res;
    cr.pos += 3;          // move past closing '''
    out_span->c_end = cr; // Lexical end: after closing '''
    *out_type = YATL_TYPE_STRING;
    return YATL_OK;
  }

  // === Basic string " ===
  if (c == '"') {
    // c_start already set at opening "
    cr.pos++;                 // skip opening "
    out_span->s_c_start = cr; // Semantic start: after opening "
    YATL_Result_t res = _consume(&cr, _TOML_STR_BASIC);
    if (res != YATL_OK)
      return res;
    out_span->s_c_end = cr; // Semantic end: at closing "
    cr.pos++;               // move past closing "
    out_span->c_end = cr;   // Lexical end: after closing "
    *out_type = YATL_TYPE_STRING;
    return YATL_OK;
  }

  // === Literal string ' ===
  if (c == '\'') {
    // c_start already set at opening '
    cr.pos++;                 // skip opening '
    out_span->s_c_start = cr; // Semantic start: after opening '
    YATL_Result_t res = _consume(&cr, _TOML_STR_LITERAL);
    if (res != YATL_OK)
      return res;
    out_span->s_c_end = cr; // Semantic end: at closing '
    cr.pos++;               // move past closing '
    out_span->c_end = cr;   // Lexical end: after closing '
    *out_type = YATL_TYPE_STRING;
    return YATL_OK;
  }

  // === Array ===
  if (c == '[') {
    *out_type = YATL_TYPE_ARRAY;
    out_span->type = YATL_S_NODE_ARRAY;
    out_span->c_start = cr;
    YATL_Result_t res = _consume(&cr, _TOML_ARRAY);
    if (res != YATL_OK)
      return res;
    out_span->c_end = cr;
    return YATL_OK;
  }

  // === Inline table ===
  if (c == '{') {
    *out_type = YATL_TYPE_INLINE_TABLE;
    out_span->type = YATL_S_NODE_INLINE_TABLE;
    out_span->c_start = cr;
    YATL_Result_t res = _consume(&cr, _TOML_INLINE_TABLE);
    if (res != YATL_OK)
      return res;
    out_span->c_end = cr;
    return YATL_OK;
  }

  // === Bare value (bool, number, date, time, inf, nan) ===
  // semantic == lexical for bare values
  *out_type = YATL_TYPE_BAREVALUE;
  out_span->c_start = cr;
  out_span->s_c_start = cr;
  YATL_Result_t res = _consume(&cr, _TOML_VALUE);
  if (res != YATL_OK)
    return res;
  out_span->c_end = cr;
  out_span->s_c_end = cr;
  return YATL_OK;
}

YATL_Result_t YATL_span_keyval_slice(const YATL_Span_t *span, YATL_Span_t *key,
                                     YATL_Span_t *val) {
  if (!span || !key || !val)
    return YATL_ERR_INVALID_ARG;

  const _YATL_Span_t *_span = (const _YATL_Span_t *)span;
  YATL_Result_t res = _YATL_check_span(_span);
  if (res != YATL_OK)
    return res;
  _YATL_Span_t *_key = (_YATL_Span_t *)key;
  _YATL_Span_t *_val = (_YATL_Span_t *)val;
  if (_span->type != YATL_S_LEAF_KEYVAL)
    return YATL_ERR_TYPE;

  _YATL_Cursor_t c = _span->c_start;

  // First, consume the raw key portion (up to =)
  res = _consume(&c, _TOML_KEY);
  if (res != YATL_OK) {
    YATL_LOG(YATL_LOG_ERROR, "Error consuming key in keyval span");
    return res;
  }

  // Build a temporary span for the raw key to pass to _toml_key_parse
  _YATL_Span_t raw_key = _YATL_EMPTY_SPAN;
  raw_key.type = YATL_S_SLICE_KEY;
  raw_key.c_start = _span->c_start;
  raw_key.c_end = c; // c is at =

  // Parse the key to extract just the key content (strips quotes, whitespace)
  YATL_ValueType_t key_type;
  res = _toml_key_parse(&raw_key, _key, &key_type);
  if (res != YATL_OK) {
    YATL_LOG(YATL_LOG_ERROR, "Error parsing key in keyval span");
    return res;
  }

  // Skip past the = sign
  c.pos++;

  // Parse the value
  YATL_ValueType_t val_type;
  res = _toml_value_parse(&c, _val, &val_type);
  if (res != YATL_OK) {
    YATL_LOG(YATL_LOG_ERROR, "Error parsing value in keyval span");
    return res;
  }

  return YATL_OK;
}

YATL_SpanType_t YATL_span_type(const YATL_Span_t *span) {
  if (!span)
    return YATL_S_NONE;
  const _YATL_Span_t *_span = (const _YATL_Span_t *)span;
  if (_YATL_check_span(_span) != YATL_OK)
    return YATL_S_NONE;
  return _span->type;
}

YATL_Result_t YATL_span_text(const YATL_Span_t *span, const char **out_text,
                             size_t *out_len) {
  if (!span || !out_text || !out_len)
    return YATL_ERR_INVALID_ARG;

  const _YATL_Span_t *_span = (const _YATL_Span_t *)span;
  YATL_Result_t res = _YATL_check_span(_span);
  if (res != YATL_OK)
    return res;

  // Use semantic bounds if available, else lexical
  _YATL_Cursor_t start =
      _span->s_c_start.line ? _span->s_c_start : _span->c_start;
  _YATL_Cursor_t end = _span->s_c_end.line ? _span->s_c_end : _span->c_end;

  if (!start.line)
    return YATL_ERR_INVALID_ARG;

  // Check if multi-line
  if (start.line != end.line)
    return YATL_ERR_TYPE; // use iter_line for multi-line spans

  *out_text = start.line->text + start.pos;
  *out_len = end.pos - start.pos;
  return YATL_OK;
}

YATL_Result_t YATL_span_get_string(const YATL_Span_t *in_span, const char *key,
                                   const char **out_text, size_t *out_len) {
  if (!in_span || !key || !out_text || !out_len)
    return YATL_ERR_INVALID_ARG;
  YATL_Result_t res = _YATL_check_span((const _YATL_Span_t *)in_span);
  if (res != YATL_OK)
    return res;

  // Find the keyval by name
  YATL_Span_t keyval;
  res = YATL_span_find_name(in_span, key, &keyval);
  if (res != YATL_OK)
    return res;

  // Must be a keyval span
  if (YATL_span_type(&keyval) != YATL_S_LEAF_KEYVAL)
    return YATL_ERR_TYPE;

  // Get key and value slices
  YATL_Span_t key_span, val_span;
  res = YATL_span_keyval_slice(&keyval, &key_span, &val_span);
  if (res != YATL_OK)
    return res;

  // Get the text from the value span
  return YATL_span_text(&val_span, out_text, out_len);
}

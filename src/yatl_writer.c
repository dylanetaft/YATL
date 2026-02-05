#include "yatl_lexer.h"
#include "yatl_private.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------
// Span unlink/relink implementation
// ---------------------------------------------------------------------

YATL_Result_t _YATL_span_unlink(YATL_Span_t *span,
                                YATL_Cursor_t *out_reinsert_pos,
                                YATL_Cursor_t *out_prefix_cursor,
                                YATL_Cursor_t *out_suffix_cursor) {
  if (!span || !out_reinsert_pos || !out_prefix_cursor || !out_suffix_cursor)
    return YATL_ERR_INVALID_ARG;

  _YATL_Span_t *_span = (_YATL_Span_t *)span;
  _YATL_Cursor_t *_out_reinsert = (_YATL_Cursor_t *)out_reinsert_pos;
  _YATL_Cursor_t *_out_prefix = (_YATL_Cursor_t *)out_prefix_cursor;
  _YATL_Cursor_t *_out_suffix = (_YATL_Cursor_t *)out_suffix_cursor;

  _YATL_Line_t *first = _span->c_start.line;
  _YATL_Line_t *last = _span->c_end.line;

  if (!first || !last)
    return YATL_ERR_INVALID_ARG;

  _YATL_Doc_t *doc = first->doc;
  if (!doc)
    return YATL_ERR_INVALID_ARG;

  size_t start_pos = _span->c_start.pos;
  size_t end_pos = _span->c_end.pos;
  size_t prefix_len = start_pos;
  size_t suffix_len = (end_pos < last->len) ? (last->len - end_pos) : 0;

  // For single-line spans with both prefix and suffix, merge into one line
  // For multi-line spans or spans with only prefix or suffix, create separate
  // lines
  _YATL_Line_t *prefix_line = NULL;
  _YATL_Line_t *suffix_line = NULL;
  bool single_line_span = (first == last);

  if (single_line_span && prefix_len > 0 && suffix_len > 0) {
    // Merge prefix + suffix into one line
    size_t merged_len = prefix_len + suffix_len;
    prefix_line = _line_alloc(NULL, merged_len);
    if (!prefix_line)
      return YATL_ERR_NOMEM;
    memcpy(prefix_line->text, first->text, prefix_len);
    memcpy(prefix_line->text + prefix_len, last->text + end_pos, suffix_len);
    // suffix_line stays NULL
  } else {
    // Separate prefix and suffix lines
    if (prefix_len > 0) {
      prefix_line = _line_alloc(NULL, prefix_len);
      if (!prefix_line)
        return YATL_ERR_NOMEM;
      memcpy(prefix_line->text, first->text, prefix_len);
    }

    if (suffix_len > 0) {
      suffix_line = _line_alloc(NULL, suffix_len);
      if (!suffix_line) {
        if (prefix_line)
          _line_free(prefix_line);
        return YATL_ERR_NOMEM;
      }
      memcpy(suffix_line->text, last->text + end_pos, suffix_len);
    }
  }

  // Save insertion points before unlinking
  _YATL_Line_t *insert_before = last->next;
  _YATL_Line_t *insert_after = first->prev;

  // Initialize output cursors
  _out_reinsert->line = insert_before;
  _out_reinsert->pos = 0;
  _out_prefix->line = NULL;
  _out_prefix->pos = 0;
  _out_suffix->line = NULL;
  _out_suffix->pos = 0;

  // Unlink each line (save next before _line_unlink nulls it)
  // Lines go to boneyard with full original content (no trimming)
  _YATL_Line_t *line = first;
  while (line) {
    _YATL_Line_t *next_in_chain = line->next;
    _line_unlink(line);
    if (line == last)
      break;
    line = next_in_chain;
  }

  // Link prefix line into document
  if (prefix_line) {
    prefix_line->doc = doc;
    prefix_line->prev = insert_after;
    prefix_line->next = insert_before;
    if (insert_after)
      insert_after->next = prefix_line;
    else
      doc->head = prefix_line;
    if (insert_before)
      insert_before->prev = prefix_line;
    else
      doc->tail = prefix_line;
    insert_after = prefix_line;
    _out_prefix->line = prefix_line;
  }

  // Link suffix line into document
  if (suffix_line) {
    suffix_line->doc = doc;
    suffix_line->prev = insert_after;
    suffix_line->next = insert_before;
    if (insert_after)
      insert_after->next = suffix_line;
    else
      doc->head = suffix_line;
    if (insert_before)
      insert_before->prev = suffix_line;
    else
      doc->tail = suffix_line;
    _out_suffix->line = suffix_line;
  }

  return YATL_OK;
}

YATL_Result_t _YATL_span_relink(YATL_Doc_t *doc, YATL_Span_t *span,
                                const YATL_Cursor_t *reinsert_pos,
                                const YATL_Cursor_t *prefix_cursor,
                                const YATL_Cursor_t *suffix_cursor) {
  if (!doc || !span || !reinsert_pos || !prefix_cursor || !suffix_cursor)
    return YATL_ERR_INVALID_ARG;

  _YATL_Doc_t *_doc = (_YATL_Doc_t *)doc;
  _YATL_Span_t *_span = (_YATL_Span_t *)span;
  const _YATL_Cursor_t *_reinsert = (const _YATL_Cursor_t *)reinsert_pos;
  const _YATL_Cursor_t *_prefix = (const _YATL_Cursor_t *)prefix_cursor;
  const _YATL_Cursor_t *_suffix = (const _YATL_Cursor_t *)suffix_cursor;

  _YATL_Line_t *first = _span->c_start.line;
  _YATL_Line_t *last = _span->c_end.line;

  if (!first || !last)
    return YATL_ERR_INVALID_ARG;

  // Remove prefix line from document if it exists
  if (_prefix->line)
    _line_unlink(_prefix->line);

  // Remove suffix line from document if it exists
  if (_suffix->line)
    _line_unlink(_suffix->line);

  // Relink each span line from boneyard into document
  _YATL_Line_t *insert_before = _reinsert->line;
  _YATL_Line_t *line = first;
  while (line) {
    _YATL_Line_t *next_in_chain = line->next;
    _line_relink(_doc, line, insert_before);
    if (line == last)
      break;
    line = next_in_chain;
  }

  return YATL_OK;
}
YATL_Result_t YATL_span_set_value(YATL_Span_t *span, const char *value,
                                  size_t length) {
  if (!span || !value)
    return YATL_ERR_INVALID_ARG;

  size_t value_len = strlen(value);
  const char *lines[] = {value};
  return YATL_span_ml_set_value(span, lines, &value_len, 1);
}

YATL_Result_t YATL_span_ml_set_value(YATL_Span_t *span, const char **lines,
                                     const size_t *lengths, size_t line_count) {
  if (!span || !lines || !lengths || line_count == 0)
    return YATL_ERR_INVALID_ARG;

  _YATL_Span_t *_span = (_YATL_Span_t *)span;
  YATL_Result_t res = _YATL_check_span(_span);
  if (res != YATL_OK)
    return res;

  // Use semantic bounds if available, else lexical
  _YATL_Cursor_t sem_start =
      _span->s_c_start.line ? _span->s_c_start : _span->c_start;
  _YATL_Cursor_t sem_end = _span->s_c_end.line ? _span->s_c_end : _span->c_end;

  _YATL_Line_t *first_old_line = sem_start.line;
  _YATL_Line_t *last_old_line = sem_end.line;
  if (!first_old_line || !last_old_line)
    return YATL_ERR_INVALID_ARG;

  _YATL_Doc_t *doc = first_old_line->doc;
  if (!doc)
    return YATL_ERR_INVALID_ARG;

  // Calculate prefix (content before semantic start on first line)
  size_t prefix_len = sem_start.pos;
  // Calculate suffix (content after semantic end on last line, includes closing
  // quotes)
  size_t suffix_len = last_old_line->len - sem_end.pos;

  // VLA for new line pointers
  _YATL_Line_t *new_lines[line_count];
  for (size_t i = 0; i < line_count; i++)
    new_lines[i] = NULL;

  // Error result for goto cleanup - default to NOMEM, set to SYNTAX if
  // validation fails
  YATL_Result_t error_result = YATL_ERR_NOMEM;

  // Allocate and build new lines
  for (size_t i = 0; i < line_count; i++) {
    size_t line_len = lengths[i];

    if (line_count == 1) {
      // Single line: prefix + content + suffix
      line_len = prefix_len + lengths[0] + suffix_len;
      new_lines[0] = _line_alloc(NULL, line_len);
      if (!new_lines[0])
        goto cleanup_error;
      memcpy(new_lines[0]->text, first_old_line->text, prefix_len);
      memcpy(new_lines[0]->text + prefix_len, lines[0], lengths[0]);
      memcpy(new_lines[0]->text + prefix_len + lengths[0],
             last_old_line->text + sem_end.pos, suffix_len);
    } else if (i == 0) {
      // First line: prefix + content
      line_len = prefix_len + lengths[0];
      new_lines[0] = _line_alloc(NULL, line_len);
      if (!new_lines[0])
        goto cleanup_error;
      memcpy(new_lines[0]->text, first_old_line->text, prefix_len);
      memcpy(new_lines[0]->text + prefix_len, lines[0], lengths[0]);
    } else if (i == line_count - 1) {
      // Last line: content + suffix
      line_len = lengths[i] + suffix_len;
      new_lines[i] = _line_alloc(NULL, line_len);
      if (!new_lines[i])
        goto cleanup_error;
      memcpy(new_lines[i]->text, lines[i], lengths[i]);
      memcpy(new_lines[i]->text + lengths[i], last_old_line->text + sem_end.pos,
             suffix_len);
    } else {
      // Middle lines: just content
      new_lines[i] = _line_alloc(NULL, lengths[i]);
      if (!new_lines[i])
        goto cleanup_error;
      memcpy(new_lines[i]->text, lines[i], lengths[i]);
    }
  }

  // Validate by parsing the first new line as a TOML value
  // Start parsing from the lexical start of the value (includes opening quote)
  _YATL_Cursor_t test_cursor = {.magic = YATL_CURSOR_MAGIC,
                                .line = new_lines[0],
                                .pos = _span->c_start.pos};

  // Link new lines temporarily for validation (needed for multi-line parsing)
  for (size_t i = 0; i < line_count; i++) {
    new_lines[i]->prev = (i > 0) ? new_lines[i - 1] : NULL;
    new_lines[i]->next = (i < line_count - 1) ? new_lines[i + 1] : NULL;
  }

  res = _consume(&test_cursor, _TOML_VALUE);

  // Calculate expected lexical end position after parsing:
  // For single line: prefix + content + suffix
  // For multi-line: last line at content + suffix
  _YATL_Line_t *expected_end_line = new_lines[line_count - 1];
  size_t expected_end_pos = (line_count == 1)
                                ? prefix_len + lengths[0] + suffix_len
                                : lengths[line_count - 1] + suffix_len;

  if (res != YATL_OK || test_cursor.line != expected_end_line ||
      test_cursor.pos != expected_end_pos) {
    YATL_LOG(YATL_LOG_WARN,
             "YATL_span_set_value: invalid value syntax after replacement");
    error_result = YATL_ERR_SYNTAX;
    goto cleanup_error;
  }

  // Save insertion points before modifying document
  _YATL_Line_t *insert_after = first_old_line->prev;
  _YATL_Line_t *insert_before = last_old_line->next;

  // Unlink old lines (move to boneyard)
  _YATL_Line_t *old_line = first_old_line;
  while (old_line) {
    _YATL_Line_t *next_old = old_line->next;
    _line_unlink(old_line);
    if (old_line == last_old_line)
      break;
    old_line = next_old;
  }

  // Link new lines into document
  for (size_t i = 0; i < line_count; i++) {
    new_lines[i]->doc = doc;
    new_lines[i]->prev = insert_after;
    new_lines[i]->next = insert_before;

    if (insert_after)
      insert_after->next = new_lines[i];
    else
      doc->head = new_lines[i];

    if (insert_before)
      insert_before->prev = new_lines[i];
    else
      doc->tail = new_lines[i];

    insert_after = new_lines[i];
  }

  // Update span cursors to point to new lines
  _span->c_start.line = new_lines[0];
  _span->c_end.line = new_lines[line_count - 1];

  // Calculate new lexical end position
  size_t last_line_content_len =
      (line_count == 1) ? prefix_len + lengths[0] : lengths[line_count - 1];
  _span->c_end.pos = last_line_content_len + suffix_len;

  // Update semantic cursors
  if (_span->s_c_start.line) {
    _span->s_c_start.line = new_lines[0];
    _span->s_c_start.pos = prefix_len;
    _span->s_c_end.line = new_lines[line_count - 1];
    _span->s_c_end.pos =
        (line_count == 1) ? prefix_len + lengths[0] : lengths[line_count - 1];
  }

  return YATL_OK;

cleanup_error:
  // Free any allocated lines on error
  for (size_t i = 0; i < line_count; i++) {
    if (new_lines[i])
      _line_free(new_lines[i]);
  }
  return error_result;
}

YATL_Result_t YATL_doc_save(YATL_Doc_t *doc, const char *path) {
  if (!doc || !path)
    return YATL_ERR_INVALID_ARG;

  const _YATL_Doc_t *_doc = (const _YATL_Doc_t *)doc;
  YATL_Result_t res = _YATL_check_doc(_doc);
  if (res != YATL_OK)
    return res;

  FILE *f = fopen(path, "wb");
  if (!f)
    return YATL_ERR_IO;

  _YATL_Line_t *line = _doc->head;
  while (line) {
    if (line->len > 0) {
      size_t written = fwrite(line->text, 1, line->len, f);
      if (written != line->len) {
        fclose(f);
        return YATL_ERR_IO;
      }
    }
    // Write newline after each line to preserve structure
    if (fputc('\n', f) == EOF) {
      fclose(f);
      return YATL_ERR_IO;
    }
    line = line->next;
  }

  fclose(f);
  return YATL_OK;
}

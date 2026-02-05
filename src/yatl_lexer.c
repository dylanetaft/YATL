#include "yatl_lexer.h"
#include "yatl_private.h"
#include <string.h>

const char *_TOMLToken_name(_TOMLToken_t token) {
  switch (token) {
  case _TOML_TABLE_HEADER:
    return "_TOML_TABLE_HEADER";
  case _TOML_TABLE_ARRAY_HEADER:
    return "_TOML_TABLE_ARRAY_HEADER";
  case _TOML_TABLE_BODY:
    return "_TOML_TABLE_BODY";
  case _TOML_TABLE_ARRAY_BODY:
    return "_TOML_TABLE_ARRAY_BODY";
  case _TOML_COMMENT:
    return "_TOML_COMMENT";
  case _TOML_KEY:
    return "_TOML_KEY";
  case _TOML_VALUE:
    return "_TOML_VALUE";
  case _TOML_STR_BASIC:
    return "_TOML_STR_BASIC";
  case _TOML_STR_LITERAL:
    return "_TOML_STR_LITERAL";
  case _TOML_STR_ML_BASIC:
    return "_TOML_STR_ML_BASIC";
  case _TOML_STR_ML_LITERAL:
    return "_TOML_STR_ML_LITERAL";
  case _TOML_ARRAY:
    return "_TOML_ARRAY";
  case _TOML_INLINE_TABLE:
    return "_TOML_INLINE_TABLE";
  default:
    return "UNKNOWN_TOKEN";
  }
}

YATL_Result_t _skipWS(_YATL_Cursor_t *cursor) {
  if (!cursor)
    return YATL_ERR_INVALID_ARG;
  if (!cursor->line)
    return YATL_ERR_INVALID_ARG;

  _YATL_Cursor_t cr = *cursor;

  while (cr.line) {
    while (cr.pos < cr.line->len) {
      char c = cr.line->text[cr.pos];
      if (!_is_ws(c)) {
        *cursor = cr;
        return YATL_OK;
      }
      cr.pos++;
    }
    cr.line = cr.line->next;
    cr.pos = 0;
  }
  cr.complete = true;
  *cursor = cr;
  return YATL_DONE;
}

YATL_Result_t _skipany(_YATL_Cursor_t *cursor, const char *chars) {
  if (!cursor)
    return YATL_ERR_INVALID_ARG;
  if (!cursor->line)
    return YATL_ERR_INVALID_ARG;
  if (!chars)
    return YATL_ERR_INVALID_ARG;

  _YATL_Cursor_t cr = *cursor;
  size_t chars_len = strlen(chars);

  while (cr.line) {
    while (cr.pos < cr.line->len) {
      char c = cr.line->text[cr.pos];
      bool skip = false;
      for (size_t i = 0; i < chars_len; i++) {
        if (c == chars[i]) {
          skip = true;
          break;
        }
      }
      if (!skip) {
        *cursor = cr;
        return YATL_OK;
      }
      cr.pos++;
    }
    cr.line = cr.line->next;
    cr.pos = 0;
  }
  cr.complete = true;
  *cursor = cr;
  return YATL_DONE;
}

YATL_Result_t _consume(_YATL_Cursor_t *cursor, _TOMLToken_t token) {
  if (!cursor)
    return YATL_ERR_INVALID_ARG;
  if (!cursor->line)
    return YATL_ERR_INVALID_ARG;
  YATL_LOG(YATL_LOG_DEBUG, "Consuming token %s at line %u pos %zu",
           _TOMLToken_name(token), cursor->line->linenum, cursor->pos);
  _YATL_Cursor_t cr = *cursor;

  switch (token) {

  case _TOML_COMMENT:
    cr.pos = cr.line->len;
    *cursor = cr;
    return YATL_OK;

  case _TOML_TABLE_HEADER:
    while (cr.pos < cr.line->len) {
      char c = cr.line->text[cr.pos];
      cr.pos++;
      if (c == ']') {
        *cursor = cr;
        return YATL_OK;
      }
    }
    return YATL_ERR_NOT_FOUND;

  case _TOML_TABLE_ARRAY_HEADER:
    while (cr.pos < cr.line->len) {
      char c = cr.line->text[cr.pos];
      if (c == ']' && cr.pos + 1 < cr.line->len &&
          cr.line->text[cr.pos + 1] == ']') {
        cr.pos += 2;
        *cursor = cr;
        return YATL_OK;
      }
      cr.pos++;
    }
    return YATL_ERR_NOT_FOUND;

  case _TOML_TABLE_BODY:
    while (cr.line) {
      if (cr.pos == 0 && cr.line->len > 0 && cr.line->text[0] == '[') {
        *cursor = cr;
        return YATL_OK;
      }
      if (!cr.line->next) {
        cr.pos = cr.line->len;
        *cursor = cr;
        return YATL_OK;
      }
      cr.line = cr.line->next;
      cr.pos = 0;
    }
    return YATL_ERR_NOT_FOUND;

  case _TOML_TABLE_ARRAY_BODY:
    while (cr.line) {
      if (cr.pos == 0 && cr.line->len > 0 && cr.line->text[0] == '[') {
        *cursor = cr;
        return YATL_OK;
      }
      if (!cr.line->next) {
        cr.pos = cr.line->len;
        *cursor = cr;
        return YATL_OK;
      }
      cr.line = cr.line->next;
      cr.pos = 0;
    }
    return YATL_ERR_NOT_FOUND;

  case _TOML_KEY: {
    char quote = 0;
    while (cr.line && cr.pos < cr.line->len) {
      char c = cr.line->text[cr.pos];

      if (quote) { // note, this continues the loop
        if (c == '\\' && quote == '"') {
          cr.pos += 2;
          continue;
        }
        if (c == quote)
          quote = 0;
        cr.pos++;
        continue;
      }

      if (c == '"' || c == '\'') {
        quote = c;
        cr.pos++;
        continue;
      }
      if (c == '=') {
        if (_compare_cursor(&cr, cursor)) {
          YATL_LOG(YATL_LOG_WARN, "_TOML_KEY: key has zero length");
          return YATL_ERR_NOT_FOUND;
        }
        *cursor = cr;
        return YATL_OK;
      }
      // Only bare key chars and whitespace are valid in unquoted context
      if (!_is_bare_key_char(c) && !_is_ws(c)) {
        YATL_LOG(YATL_LOG_WARN,
                 "_TOML_KEY: illegal char '%c' (0x%02X) before '='", c,
                 (unsigned char)c);
        return YATL_ERR_SYNTAX;
      }
      cr.pos++;
    }
    YATL_LOG(YATL_LOG_WARN, "_TOML_KEY: '=' not found");
    return YATL_ERR_NOT_FOUND;
  }

  case _TOML_VALUE: {
    // NOTE: _TOML_VALUE skips leading whitespace and consumes the entire value
    // INCLUDING delimiters (quotes, brackets). Cursor ends AFTER closing
    // delimiter. For strings, _toml_value_parse then adjusts span to EXCLUDE
    // quotes. Skip leading whitespace
    if (_skipWS(&cr) == YATL_DONE) {
      YATL_LOG(YATL_LOG_WARN, "_TOML_VALUE: no content after whitespace");
      return YATL_ERR_SYNTAX;
    }
    if (!cr.line || cr.pos >= cr.line->len) {
      YATL_LOG(YATL_LOG_WARN, "_TOML_VALUE: no content at cursor");
      return YATL_ERR_SYNTAX;
    }
    char c = cr.line->text[cr.pos];
    char c1 = (cr.pos + 1 < cr.line->len) ? cr.line->text[cr.pos + 1] : '\0';
    char c2 = (cr.pos + 2 < cr.line->len) ? cr.line->text[cr.pos + 2] : '\0';

    // Multiline basic string """
    if (c == '"' && c1 == '"' && c2 == '"') {
      cr.pos += 3;
      YATL_Result_t res = _consume(&cr, _TOML_STR_ML_BASIC);
      if (res != YATL_OK) {
        YATL_LOG(YATL_LOG_WARN,
                 "_TOML_VALUE: failed to consume multiline basic string");
        return res;
      }
      cr.pos += 3; // skip closing """
      *cursor = cr;
      return YATL_OK;
    }

    // Multiline literal string '''
    if (c == '\'' && c1 == '\'' && c2 == '\'') {
      cr.pos += 3;
      YATL_Result_t res = _consume(&cr, _TOML_STR_ML_LITERAL);
      if (res != YATL_OK) {
        YATL_LOG(YATL_LOG_WARN,
                 "_TOML_VALUE: failed to consume multiline literal string");
        return res;
      }
      cr.pos += 3; // skip closing '''
      *cursor = cr;
      return YATL_OK;
    }

    // Basic string "
    if (c == '"') {
      cr.pos++;
      YATL_Result_t res = _consume(&cr, _TOML_STR_BASIC);
      if (res != YATL_OK) {
        YATL_LOG(YATL_LOG_WARN, "_TOML_VALUE: failed to consume basic string");
        return res;
      }
      cr.pos++; // skip closing "
      *cursor = cr;
      return YATL_OK;
    }

    // Literal string '
    if (c == '\'') {
      cr.pos++;
      YATL_Result_t res = _consume(&cr, _TOML_STR_LITERAL);
      if (res != YATL_OK) {
        YATL_LOG(YATL_LOG_WARN,
                 "_TOML_VALUE: failed to consume literal string");
        return res;
      }
      cr.pos++; // skip closing '
      *cursor = cr;
      return YATL_OK;
    }

    // Array [
    if (c == '[') {
      YATL_Result_t res = _consume(&cr, _TOML_ARRAY);
      if (res != YATL_OK) {
        YATL_LOG(YATL_LOG_WARN, "_TOML_VALUE: failed to consume array");
        return res;
      }
      *cursor = cr;
      return YATL_OK;
    }

    // Inline table {
    if (c == '{') {
      YATL_Result_t res = _consume(&cr, _TOML_INLINE_TABLE);
      if (res != YATL_OK) {
        YATL_LOG(YATL_LOG_WARN, "_TOML_VALUE: failed to consume inline table");
        return res;
      }
      if (_compare_cursor(&cr, cursor)) {
        YATL_LOG(YATL_LOG_WARN, "_TOML_VALUE: inline table has zero length");
      }
      *cursor = cr;
      return YATL_OK;
    }

    // Bare value (number, bool, date, etc.)
    while (cr.pos < cr.line->len) {
      c = cr.line->text[cr.pos];
      if (_is_ws(c) || c == ',' || c == ']' || c == '}' || c == '#') {
        if (_compare_cursor(&cr, cursor)) {
          YATL_LOG(YATL_LOG_WARN, "_TOML_VALUE: bare value has zero length");
          return YATL_ERR_NOT_FOUND;
        }
        break;
      }
      cr.pos++;
      // This maybe could be improved by counting nested structures
      // But that's a lot of code to for little gain, exit a corrupt structure
      // earlier Instead _toml_value_parse and subsequent parsing will catch
      // errors
    }
    *cursor = cr;
    return YATL_OK;
  }

  case _TOML_STR_BASIC: {
    bool escaped = false;
    while (cr.pos < cr.line->len) {
      char ch = cr.line->text[cr.pos];
      if (escaped) {
        escaped = false;
        cr.pos++;
        continue;
      }
      if (ch == '\\') {
        escaped = true;
        cr.pos++;
        continue;
      }
      if (ch == '"') {
        *cursor = cr;
        return YATL_OK;
      }
      cr.pos++;
    }
    return YATL_ERR_SYNTAX;
  }

  case _TOML_STR_LITERAL:
    while (cr.pos < cr.line->len) {
      if (cr.line->text[cr.pos] == '\'') {
        *cursor = cr;
        return YATL_OK;
      }
      cr.pos++;
    }
    return YATL_ERR_SYNTAX;

  case _TOML_STR_ML_BASIC: {
    bool escaped = false;
    for (;;) {
      while (cr.pos < cr.line->len) {
        char ch = cr.line->text[cr.pos];
        if (escaped) {
          escaped = false;
          cr.pos++;
          continue;
        }
        if (ch == '\\') {
          escaped = true;
          cr.pos++;
          continue;
        }
        if (ch == '"' && cr.pos + 1 < cr.line->len &&
            cr.line->text[cr.pos + 1] == '"' && cr.pos + 2 < cr.line->len &&
            cr.line->text[cr.pos + 2] == '"') {
          *cursor = cr;
          return YATL_OK;
        }
        cr.pos++;
      }
      cr.line = cr.line->next;
      if (!cr.line)
        return YATL_ERR_SYNTAX;
      cr.pos = 0;
    }
  }

  case _TOML_STR_ML_LITERAL:
    for (;;) {
      while (cr.pos < cr.line->len) {
        char ch = cr.line->text[cr.pos];
        if (ch == '\'' && cr.pos + 1 < cr.line->len &&
            cr.line->text[cr.pos + 1] == '\'' && cr.pos + 2 < cr.line->len &&
            cr.line->text[cr.pos + 2] == '\'') {
          *cursor = cr;
          return YATL_OK;
        }
        cr.pos++;
      }
      cr.line = cr.line->next;
      if (!cr.line)
        return YATL_ERR_SYNTAX;
      cr.pos = 0;
    }

  case _TOML_ARRAY: {
    // Expects cursor AT '[', ends AFTER ']'
    if (cr.line->text[cr.pos] != '[')
      return YATL_ERR_SYNTAX;
    int depth = 1;
    cr.pos++;
    for (;;) {
      while (cr.line && cr.pos < cr.line->len) {
        char ch = cr.line->text[cr.pos];
        char ch1 =
            (cr.pos + 1 < cr.line->len) ? cr.line->text[cr.pos + 1] : '\0';
        char ch2 =
            (cr.pos + 2 < cr.line->len) ? cr.line->text[cr.pos + 2] : '\0';
        // Skip strings to avoid counting brackets inside them
        if (ch == '"' && ch1 == '"' && ch2 == '"') {
          cr.pos += 3;
          YATL_Result_t res = _consume(&cr, _TOML_STR_ML_BASIC);
          if (res != YATL_OK)
            return res; // multiline strings can progress line ptr to null
          cr.pos += 3;
          continue;
        }
        if (ch == '\'' && ch1 == '\'' && ch2 == '\'') {
          cr.pos += 3;
          YATL_Result_t res = _consume(&cr, _TOML_STR_ML_LITERAL);
          if (res != YATL_OK)
            return res; // multiline strings can progress line ptr to null
          cr.pos += 3;
          continue;
        }
        if (ch == '"') {
          cr.pos++;
          YATL_Result_t res = _consume(&cr, _TOML_STR_BASIC);
          if (res != YATL_OK)
            return res;
          cr.pos++;
          continue;
        }
        if (ch == '\'') {
          cr.pos++;
          YATL_Result_t res = _consume(&cr, _TOML_STR_LITERAL);
          if (res != YATL_OK)
            return res;
          cr.pos++;
          continue;
        }
        if (ch == '[')
          depth++;
        if (ch == ']') {
          depth--;
          cr.pos++;
          if (depth == 0) {
            *cursor = cr;
            return YATL_OK;
          }
          continue;
        }
        cr.pos++;
      }
      cr.line = cr.line->next;
      if (!cr.line)
        return YATL_ERR_SYNTAX;
      cr.pos = 0;
    }
  }

  case _TOML_INLINE_TABLE: {
    // Expects cursor AT '{', ends AFTER '}'
    // Inline tables must be on a single line per TOML spec
    if (cr.line->text[cr.pos] != '{')
      return YATL_ERR_SYNTAX;
    int depth = 1;
    cr.pos++;
    while (cr.line && cr.pos < cr.line->len) { // cr.line should not go null
      char ch = cr.line->text[cr.pos];
      char ch1 = (cr.pos + 1 < cr.line->len) ? cr.line->text[cr.pos + 1] : '\0';
      char ch2 = (cr.pos + 2 < cr.line->len) ? cr.line->text[cr.pos + 2] : '\0';
      // Skip strings
      if (ch == '"' && ch1 == '"' && ch2 == '"') {
        cr.pos += 3;
        YATL_Result_t res = _consume(&cr, _TOML_STR_ML_BASIC);
        if (res != YATL_OK)
          return res; // multiline strings can progress line ptr to null
        cr.pos += 3;
        continue;
      }
      if (ch == '\'' && ch1 == '\'' && ch2 == '\'') {
        cr.pos += 3;
        YATL_Result_t res = _consume(&cr, _TOML_STR_ML_LITERAL);
        if (res != YATL_OK)
          return res; // multiline strings can progress line ptr to null
        cr.pos += 3;
        continue;
      }
      if (ch == '"') {
        cr.pos++;
        YATL_Result_t res = _consume(&cr, _TOML_STR_BASIC);
        if (res != YATL_OK)
          return res;
        cr.pos++;
        continue;
      }
      if (ch == '\'') {
        cr.pos++;
        YATL_Result_t res = _consume(&cr, _TOML_STR_LITERAL);
        if (res != YATL_OK)
          return res;
        cr.pos++;
        continue;
      }
      if (ch == '{')
        depth++;
      if (ch == '}') {
        depth--;
        cr.pos++;
        if (depth == 0) {
          *cursor = cr;
          return YATL_OK;
        }
        continue;
      }
      cr.pos++;
    }
    return YATL_ERR_SYNTAX; // unclosed inline table
  }

  default:
    break;
  }

  return YATL_OK;
}

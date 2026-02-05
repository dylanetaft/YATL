#pragma once
// Private lexer header - not part of public API

#include "yatl_private.h"

// ---------------------------------------------------------------------
// Character classification helpers
// ---------------------------------------------------------------------

static inline bool _is_ws(char c) { return c == ' ' || c == '\t'; }

static inline bool _is_newline(char c) { return c == '\n' || c == '\r'; }

static inline bool _is_bare_key_char(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static inline bool _is_digit(char c) { return c >= '0' && c <= '9'; }

static inline bool _is_hex(char c) {
  return _is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// ---------------------------------------------------------------------
// Token types for lexical consumption
// ---------------------------------------------------------------------

typedef enum {
  _TOML_TABLE_HEADER,
  _TOML_TABLE_ARRAY_HEADER,
  _TOML_TABLE_BODY,
  _TOML_TABLE_ARRAY_BODY,
  _TOML_COMMENT,
  _TOML_KEY,
  _TOML_VALUE,
  _TOML_STR_BASIC,      // "..." single line, with escapes
  _TOML_STR_LITERAL,    // '...' single line, no escapes
  _TOML_STR_ML_BASIC,   // """...""" multiline, with escapes
  _TOML_STR_ML_LITERAL, // '''...''' multiline, no escapes
  _TOML_ARRAY,          // [...] inline array (handles nesting)
  _TOML_INLINE_TABLE,   // {...} inline table (handles nesting)
} _TOMLToken_t;

// ---------------------------------------------------------------------
// Lexer functions
// ---------------------------------------------------------------------

// Get string name for token type (for debugging)
const char *_TOMLToken_name(_TOMLToken_t token);

// Skip whitespace, leaving cursor at first non-whitespace character
// Returns YATL_DONE if end of document reached
YATL_Result_t _skipWS(_YATL_Cursor_t *cursor);

// Skip any characters in the given set (plus whitespace), crossing newlines
// Returns YATL_DONE if end of document reached
YATL_Result_t _skipany(_YATL_Cursor_t *cursor, const char *chars);

// Consume a token, advancing cursor to end of token
YATL_Result_t _consume(_YATL_Cursor_t *cursor, _TOMLToken_t token);

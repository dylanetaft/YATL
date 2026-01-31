#pragma once
#include "yatl.h"
#include <stdint.h>


// YATL private header - not part of public API

typedef struct _YATL_Line {
    char *text;
    size_t len;
    uint32_t linenum;  // line number in document (starting from 1)
    struct _YATL_Line *prev, *next;
} _YATL_Line_t;

typedef struct _YATL_Cursor {
    _YATL_Line_t *line;
    size_t pos;
} _YATL_Cursor_t;

typedef struct {
    YATL_SpanType_t type;
    _YATL_Cursor_t c_start;
    _YATL_Cursor_t c_end;
} _YATL_Span_t;



// ---------------------------------------------------------------------
// Document - doubly linked list of lines
// ---------------------------------------------------------------------

typedef struct {
    _YATL_Line_t *head;
    _YATL_Line_t *tail;
} _YATL_Doc_t;

static YATL_Result_t _toml_value_parse(const _YATL_Cursor_t *value_start,
    _YATL_Span_t *out_span, YATL_ValueType_t *out_type); 

static YATL_Result_t _toml_key_parse(const _YATL_Span_t *key_span,
    _YATL_Span_t *out_span, YATL_ValueType_t *out_type);

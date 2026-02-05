#include "yatl.h"
#include <assert.h>
#include "yatl_private.h"

#define DEPTH_LIMIT 1000
int LLVMFuzzerInitialize(int *argc, char ***argv) {
    return 0;
}

YATL_Result_t findNext(YATL_Span_t *in_span, int depth) {
    if (depth > DEPTH_LIMIT) {
        assert(false && "Exceeded depth limit, likely stuck cursor");
    }
    YATL_Span_t span;
    YATL_Cursor_t cursor = YATL_cursor_create();
    YATL_Cursor_t last_test_cursor = YATL_cursor_create();
    _YATL_Cursor_t *_last_test_cursor = (_YATL_Cursor_t *)&last_test_cursor;
    _YATL_Cursor_t *_cursor = (_YATL_Cursor_t *)&cursor;
    YATL_Result_t nextres;

    while ((nextres = YATL_span_find_next(in_span, &cursor, &span)) == YATL_OK) {
      YATL_Span_t key_span;
      YATL_Span_t val_span;
      YATL_Result_t res = YATL_span_keyval_slice(&span, &key_span, &val_span);
      if (res == YATL_OK) { //it was a keyval, dig deeper
          YATL_Result_t res = findNext(&val_span, depth + 1);
          if (res < YATL_OK) { //exit early, could comment out, parser should still be able to advance
             // return res;
          }
      }
      last_test_cursor = cursor;
    }
    return nextres;
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  YATL_Doc_t doc;
  YATL_Result_t res = YATL_doc_loads(&doc, (const char *)data, size);
  if (res != YATL_OK) {
    YATL_doc_free(&doc);
    return 0;
  }
  YATL_Span_t doc_span;
  YATL_doc_span(&doc, &doc_span);
  findNext(&doc_span, 0);
  YATL_doc_free(&doc);
  return 0;
}

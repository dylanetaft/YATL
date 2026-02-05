#include "yatl.h"
#include <string.h>

void pretty_print(YATL_Span_t *span) {
    printf("Span Type: %s\n", YATL_span_type_name(YATL_span_type(span)));
    YATL_Cursor_t iter = YATL_cursor_create();
    const char *text;
    size_t len;
    while (YATL_span_iter_line(span, &iter, &text, &len) == YATL_OK) {
        printf("%.*s\n", (int)len, text);
    }
}

int main(void) {
  YATL_Doc_t doc;
  YATL_Result_t res = YATL_doc_load(&doc, "sample1.toml");
  if (res != YATL_OK) return -1; //for brevity i wont check errors further
  YATL_Span_t doc_span;
  YATL_doc_span(&doc, &doc_span);
  YATL_Span_t db_span;
  res = YATL_span_find_name(&doc_span, "database", &db_span);
  YATL_Span_t ports_span;
  res = YATL_span_find_name(&db_span, "ports", &ports_span);
  const char *text;
  size_t len;
  res = YATL_span_text(&ports_span, &text, &len);
  printf("Ports: %.*s\n", (int)len, text);
  YATL_Span_t key_span, val_span;
  res = YATL_span_keyval_slice(&ports_span, &key_span, &val_span);
  const char *new_ports_val = "[ 10000, 20000 ]";
  YATL_span_set_value(&val_span, new_ports_val, strlen(new_ports_val));
  pretty_print(&doc_span);
  //we can do multiple lines too!
  const char *ml_value[] = {
      "[8000,",
      "9000,",
      "10000]"
  };
  size_t ml_lengths[] = {
      strlen(ml_value[0]),
      strlen(ml_value[1]),
      strlen(ml_value[2])
  };
  YATL_span_ml_set_value(&val_span, ml_value, ml_lengths, 3);
  pretty_print(&doc_span);
  YATL_doc_free(&doc);
  return 0; 

}

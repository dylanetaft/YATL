#include <stdio.h>
#include "yatl.h"

void pretty_print(YATL_Span_t *span) {
    printf("Span Type: %s\n", YATL_span_type_name(YATL_span_type(span)));
    YATL_Cursor_t iter = YATL_cursor_create();
    const char *text;
    size_t len;
    while (YATL_span_iter_line(span, &iter, &text, &len) == YATL_OK) {
        printf("%.*s\n", (int)len, text);
    }
}

void database_props(YATL_Doc_t *doc) {
  printf("\nDatabase properties:\n");
  YATL_Span_t doc_span;
  YATL_doc_span(doc, &doc_span);

  YATL_Span_t iterspan;
  YATL_Span_t db_table_span;
  YATL_span_find_name(&doc_span, "database", &db_table_span);
  pretty_print(&db_table_span);
  YATL_span_find_name(&db_table_span, "ports", &iterspan);
  pretty_print(&iterspan);
  YATL_Span_t key, val;
  YATL_span_keyval_slice(&iterspan, &key, &val);
  pretty_print(&val); 
  YATL_span_find_next(&val, NULL, &iterspan); 
  pretty_print(&iterspan);
  
}

int main(void) {
    YATL_Doc_t doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "sample1.toml");
    if (res != YATL_OK) {
        fprintf(stderr, "Failed to load sample1.toml: %d\n", res);
        return 1;
    }

    printf("Parsed spans:\n");
    printf("-------------------------------------------\n");

    YATL_Span_t span;
    YATL_Span_t doc_span;
    YATL_doc_span(&doc, &doc_span);
    int count = 0;
    YATL_Cursor_t cursor = YATL_cursor_create();

    while (YATL_span_find_next(&doc_span, &cursor, &span) == YATL_OK) {
        count++;
        pretty_print(&span);
        // cursor automatically advanced by find_next
    }

    printf("-------------------------------------------\n");
    printf("Total spans: %d\n", count);
  
    database_props(&doc);
    YATL_doc_free(&doc);
    return 0;
}

#include "yatl.h"
#include "yatl_private.h"
#include "munit.h"
#include <string.h>
#include <stdio.h>
// =============================================================================
// Common helpers
// =============================================================================

static void assert_span_text(YATL_Span_t *span, const char *expected) {
    const char *text;
    size_t len;
    YATL_Result_t res = YATL_span_text(span, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, strlen(expected));
    munit_assert_memory_equal(len, text, expected);
}

static YATL_Result_t get_value_span(YATL_Span_t *doc_span, const char *key, YATL_Span_t *val_span) {
    YATL_Span_t keyval_span, key_span;
    YATL_Result_t res = YATL_span_find_name(doc_span, key, &keyval_span);
    if (res != YATL_OK) return res;
    return YATL_span_keyval_slice(&keyval_span, &key_span, val_span);
}


static void pretty_print(YATL_Span_t *span) {
    printf("Span Type: %s\n", YATL_span_type_name(YATL_span_type(span)));
    YATL_Cursor_t iter = YATL_cursor_create();
    const char *text;
    size_t len;
    while (YATL_span_iter_line(span, &iter, &text, &len) == YATL_OK) {
        printf("%.*s\n", (int)len, text);
    }
}

// =============================================================================
// Find tests
// =============================================================================

static MunitResult test_find_toplevel_var(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_find.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t keyval_span, key_span, val_span;

    res = YATL_span_find_name(&doc_span, "title", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "Test Document");

    res = YATL_span_find_name(&doc_span, "version", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "42");

    res = YATL_span_find_name(&doc_span, "enabled", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "true");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_find_table(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_find.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t table_span;
    res = YATL_span_find_name(&doc_span, "database", &table_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t keyval_span, key_span, val_span;
    res = YATL_span_find_name(&table_span, "host", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "localhost");

    res = YATL_span_find_name(&table_span, "port", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "5432");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_find_dotted_table(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_find.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t http_span;
    res = YATL_span_find_name(&doc_span, "server.http", &http_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t keyval_span, key_span, val_span;
    res = YATL_span_find_name(&http_span, "port", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "8080");

    YATL_Span_t https_span;
    res = YATL_span_find_name(&doc_span, "server.https", &https_span);
    munit_assert_int(res, ==, YATL_OK);

    res = YATL_span_find_name(&https_span, "cert", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "/etc/ssl/cert.pem");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_find_inline_table_drill(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_find.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t users_span;
    res = YATL_span_find_name(&doc_span, "users", &users_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t admin_kv, key_span, admin_val;
    res = YATL_span_find_name(&users_span, "admin", &admin_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&admin_kv, &key_span, &admin_val);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t name_kv, name_val;
    res = YATL_span_find_name(&admin_val, "name", &name_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&name_kv, &key_span, &name_val);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&name_val, "Alice");

    YATL_Span_t role_kv, role_val;
    res = YATL_span_find_name(&admin_val, "role", &role_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&role_kv, &key_span, &role_val);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&role_val, "admin");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_find_deeply_nested_inline(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_find.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Cursor_t cursor = YATL_cursor_create();
    YATL_Span_t items_span;
    res = YATL_span_find_next_by_name(&doc_span, "items", NULL, &cursor, &items_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t data_kv, key_span, data_val;
    res = YATL_span_find_name(&items_span, "data", &data_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&data_kv, &key_span, &data_val);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t nested_kv, nested_val;
    res = YATL_span_find_name(&data_val, "nested", &nested_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&nested_kv, &key_span, &nested_val);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t value_kv, value_val;
    res = YATL_span_find_name(&nested_val, "value", &value_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&value_kv, &key_span, &value_val);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&value_val, "100");

    // Second [[items]]
    res = YATL_span_find_next_by_name(&doc_span, "items", &cursor, &cursor, &items_span);
    munit_assert_int(res, ==, YATL_OK);

    res = YATL_span_find_name(&items_span, "data", &data_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&data_kv, &key_span, &data_val);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_find_name(&data_val, "nested", &nested_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&nested_kv, &key_span, &nested_val);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_find_name(&nested_val, "value", &value_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&value_kv, &key_span, &value_val);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&value_val, "200");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_find_next_by_name_cursor(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_find.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Cursor_t cursor = YATL_cursor_create();
    YATL_Span_t items_span;
    res = YATL_span_find_next_by_name(&doc_span, "items", NULL, &cursor, &items_span);
    munit_assert_int(res, ==, YATL_OK);

    res = YATL_span_find_next_by_name(&doc_span, "items", &cursor, &cursor, &items_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t data_kv, key_span, data_val;
    res = YATL_span_find_name(&items_span, "data", &data_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&data_kv, &key_span, &data_val);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t x_kv, x_val;
    res = YATL_span_find_name(&data_val, "x", &x_kv);
    munit_assert_int(res, ==, YATL_OK);
    res = YATL_span_keyval_slice(&x_kv, &key_span, &x_val);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&x_val, "30");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_find_not_found(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_find.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t span;
    res = YATL_span_find_name(&doc_span, "nonexistent", &span);
    munit_assert_int(res, ==, YATL_ERR_NOT_FOUND);

    YATL_Span_t db_span;
    res = YATL_span_find_name(&doc_span, "database", &db_span);
    munit_assert_int(res, ==, YATL_OK);

    res = YATL_span_find_name(&db_span, "missing", &span);
    munit_assert_int(res, ==, YATL_ERR_NOT_FOUND);

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitTest find_tests[] = {
    { "/toplevel_var", test_find_toplevel_var, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/table", test_find_table, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/dotted_table", test_find_dotted_table, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/inline_table_drill", test_find_inline_table_drill, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/deeply_nested_inline", test_find_deeply_nested_inline, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/next_by_name_cursor", test_find_next_by_name_cursor, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/not_found", test_find_not_found, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

// =============================================================================
// Unlink tests
// =============================================================================

static MunitResult test_unlink_nested_array(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_unlink.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t header_span;
    res = YATL_span_find_name(&doc_span, "header", &header_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t keyval_span;
    res = YATL_span_find_name(&header_span, "testarray", &keyval_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t key_span, val_span;
    res = YATL_span_keyval_slice(&keyval_span, &key_span, &val_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Cursor_t cursor = YATL_cursor_create();
    YATL_Span_t elem_span;
    int elem_count = 0;
    YATL_Span_t nested_span = {0};

    while (YATL_span_find_next(&val_span, &cursor, &elem_span) == YATL_OK) {
        elem_count++;
        if (elem_count == 3) {
            nested_span = elem_span;
        }
    }

    munit_assert_int(elem_count, >=, 3);

    YATL_Cursor_t reinsert_pos, prefix_cursor, suffix_cursor;
    res = _YATL_span_unlink(&nested_span, &reinsert_pos, &prefix_cursor, &suffix_cursor);
    munit_assert_int(res, ==, YATL_OK);

    res = _YATL_span_relink(&doc, &nested_span, &reinsert_pos, &prefix_cursor, &suffix_cursor);
    munit_assert_int(res, ==, YATL_OK);

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_unlink_standalone_table(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_unlink.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t table_span;
    res = YATL_span_find_name(&doc_span, "standalone", &table_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Cursor_t reinsert_pos, prefix_cursor, suffix_cursor;
    res = _YATL_span_unlink(&table_span, &reinsert_pos, &prefix_cursor, &suffix_cursor);
    munit_assert_int(res, ==, YATL_OK);

    res = _YATL_span_relink(&doc, &table_span, &reinsert_pos, &prefix_cursor, &suffix_cursor);
    munit_assert_int(res, ==, YATL_OK);

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitTest unlink_tests[] = {
    { "/nested_array", test_unlink_nested_array, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/standalone_table", test_unlink_standalone_table, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

// =============================================================================
// Updates tests
// =============================================================================

static MunitResult test_updates_longer(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "name", &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "short");

    const char *new_val = "much longer value here";
    res = YATL_span_set_value(&val_span, new_val, strlen(new_val));
    munit_assert_int(res, ==, YATL_OK);

    assert_span_text(&val_span, "much longer value here");

    YATL_Span_t val_span2;
    res = get_value_span(&doc_span, "name", &val_span2);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span2, "much longer value here");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_shorter(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "quoted", &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "hello");

    const char *new_val = "hi";
    res = YATL_span_set_value(&val_span, new_val, strlen(new_val));
    munit_assert_int(res, ==, YATL_OK);

    assert_span_text(&val_span, "hi");

    YATL_Span_t val_span2;
    res = get_value_span(&doc_span, "quoted", &val_span2);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span2, "hi");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_same_size(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "same", &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "12345");

    const char *new_val = "abcde";
    res = YATL_span_set_value(&val_span, new_val, strlen(new_val));
    munit_assert_int(res, ==, YATL_OK);

    assert_span_text(&val_span, "abcde");

    YATL_Span_t val_span2;
    res = get_value_span(&doc_span, "same", &val_span2);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span2, "abcde");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_invalid(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "quoted", &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "hello");

    // This would result in: quoted = "broken"here" which is invalid TOML
    const char *bad_val = "broken\"here";
    res = YATL_span_set_value(&val_span, bad_val, strlen(bad_val));
    munit_assert_int(res, ==, YATL_ERR_SYNTAX);

    // Original value unchanged
    assert_span_text(&val_span, "hello");

    YATL_Span_t val_span2;
    res = get_value_span(&doc_span, "quoted", &val_span2);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span2, "hello");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_boneyard_preserves(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span1, val_span2;
    res = get_value_span(&doc_span, "name", &val_span1);
    munit_assert_int(res, ==, YATL_OK);
    res = get_value_span(&doc_span, "name", &val_span2);
    munit_assert_int(res, ==, YATL_OK);

    assert_span_text(&val_span1, "short");
    assert_span_text(&val_span2, "short");

    const char *new_val = "updated";
    res = YATL_span_set_value(&val_span1, new_val, strlen(new_val));
    munit_assert_int(res, ==, YATL_OK);

    assert_span_text(&val_span1, "updated");

    // span2 still points to old line in boneyard
    const char *text;
    size_t len;
    res = YATL_span_text(&val_span2, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, strlen("short"));
    munit_assert_memory_equal(len, text, "short");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_integer(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "count", &val_span);
    munit_assert_int(res, ==, YATL_OK);
    assert_span_text(&val_span, "42");

    const char *new_val = "12345";
    res = YATL_span_set_value(&val_span, new_val, strlen(new_val));
    munit_assert_int(res, ==, YATL_OK);

    assert_span_text(&val_span, "12345");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_multiline_valid(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "multiline", &val_span);
    munit_assert_int(res, ==, YATL_OK);

    // Edit the multiline string with new content
    // User must provide full syntax including """ delimiters
    const char *new_lines[] = {"\"\"\"", "new first", "new second", "new third", "\"\"\""};
    size_t new_lengths[] = {3, 9, 10, 9, 3};
    res = YATL_span_ml_set_value(&val_span, new_lines, new_lengths, 5);
    munit_assert_int(res, ==, YATL_OK);

    // Verify by iterating through the lines (includes delimiters now)
    YATL_Cursor_t iter = YATL_cursor_create();
    const char *text;
    size_t len;

    res = YATL_span_iter_line(&val_span, &iter, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, 3);
    munit_assert_memory_equal(len, text, "\"\"\"");

    res = YATL_span_iter_line(&val_span, &iter, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, 9);
    munit_assert_memory_equal(len, text, "new first");

    res = YATL_span_iter_line(&val_span, &iter, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, 10);
    munit_assert_memory_equal(len, text, "new second");

    res = YATL_span_iter_line(&val_span, &iter, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, 9);
    munit_assert_memory_equal(len, text, "new third");

    res = YATL_span_iter_line(&val_span, &iter, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, 3);
    munit_assert_memory_equal(len, text, "\"\"\"");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_multiline_invalid(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "multiline", &val_span);
    munit_assert_int(res, ==, YATL_OK);

    // Try to set invalid content - missing closing """ makes it invalid TOML
    const char *bad_lines[] = {"\"\"\"", "content line", "no closing quotes"};
    size_t bad_lengths[] = {3, 12, 17};
    res = YATL_span_ml_set_value(&val_span, bad_lines, bad_lengths, 3);
    munit_assert_int(res, ==, YATL_ERR_SYNTAX);

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_array_valid(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "numbers", &val_span);
    munit_assert_int(res, ==, YATL_OK);

    // Edit the multiline array - user provides full syntax including brackets
    const char *new_lines[] = {"[", "    10,", "    20,", "    30,", "    40", "]"};
    size_t new_lengths[] = {1, 6, 6, 6, 6, 1};
    res = YATL_span_ml_set_value(&val_span, new_lines, new_lengths, 6);
    munit_assert_int(res, ==, YATL_OK);

    // Verify by iterating through the lines
    YATL_Cursor_t iter = YATL_cursor_create();
    const char *text;
    size_t len;

    res = YATL_span_iter_line(&val_span, &iter, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, 1);
    munit_assert_memory_equal(len, text, "[");

    res = YATL_span_iter_line(&val_span, &iter, &text, &len);
    munit_assert_int(res, ==, YATL_OK);
    munit_assert_size(len, ==, 6);
    munit_assert_memory_equal(len, text, "    10,");

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitResult test_updates_array_invalid(const MunitParameter params[], void *data) {
    (void)params; (void)data;

    YATL_Doc_t doc;
    doc = YATL_doc_create();

    YATL_Result_t res = YATL_doc_load(&doc, "test_updates.toml");
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t doc_span;
    res = YATL_doc_span(&doc, &doc_span);
    munit_assert_int(res, ==, YATL_OK);

    YATL_Span_t val_span;
    res = get_value_span(&doc_span, "numbers", &val_span);
    munit_assert_int(res, ==, YATL_OK);

    // Try to set invalid content - missing closing bracket
    const char *bad_lines[] = {"[", "    10,", "    20"};
    size_t bad_lengths[] = {1, 6, 6};
    res = YATL_span_ml_set_value(&val_span, bad_lines, bad_lengths, 3);
    munit_assert_int(res, ==, YATL_ERR_SYNTAX);

    YATL_doc_free(&doc);
    return MUNIT_OK;
}

static MunitTest updates_tests[] = {
    { "/longer", test_updates_longer, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/shorter", test_updates_shorter, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/same_size", test_updates_same_size, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/invalid", test_updates_invalid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/boneyard_preserves", test_updates_boneyard_preserves, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/integer", test_updates_integer, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/multiline_valid", test_updates_multiline_valid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/multiline_invalid", test_updates_multiline_invalid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/array_valid", test_updates_array_valid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/array_invalid", test_updates_array_invalid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

// =============================================================================
// Suite definitions
// =============================================================================

static MunitSuite child_suites[] = {
    { "/find", find_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE },
    { "/unlink", unlink_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE },
    { "/updates", updates_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE },
    { NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE }
};

static const MunitSuite main_suite = {
    "/yatl",
    NULL,
    child_suites,
    1,
    MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char *argv[]) {
    return munit_suite_main(&main_suite, NULL, argc, argv);
}

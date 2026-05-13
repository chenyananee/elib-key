/* test_elib_key.c - Key Detection Unit Tests */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/elib_key.h"

static elib_key_ctx_t test_ctx;

/* Mock hardware: controllable pin levels */
static uint8_t mock_levels[ELIB_KEY_MAX_KEYS];

static int mock_read_level(uint8_t key_id) {
    return (int)mock_levels[key_id];
}

static uint8_t last_event_key;
static elib_key_event_t last_event_type;
static uint8_t last_event_clicks;
static uint8_t last_event_threshold;
static int event_count;

static void reset_event_log(void) {
    event_count = 0;
    last_event_key = 0xFF;
}

static void mock_on_event(uint8_t key_id, elib_key_event_t event,
                          uint8_t click_count, uint8_t threshold_idx) {
    last_event_key = key_id;
    last_event_type = event;
    last_event_clicks = click_count;
    last_event_threshold = threshold_idx;
    event_count++;
}

static uint8_t last_combo_id;
static elib_key_combo_mode_t last_combo_mode;
static int combo_count;

static void reset_combo_log(void) {
    combo_count = 0;
    last_combo_id = 0xFF;
}

static void mock_on_combo(uint8_t combo_id, elib_key_combo_mode_t mode) {
    last_combo_id = combo_id;
    last_combo_mode = mode;
    combo_count++;
}

static const uint16_t thresholds_single[] = {1000};
static const elib_key_item_cfg_t single_key_cfg[] = {
    { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_single, .threshold_count = 1 },
};

static elib_key_driver_cfg_t make_single_key_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = single_key_cfg,
        .key_count = 1,
        .read_level = mock_read_level,
        .on_event = mock_on_event,
    };
    return cfg;
}

static void reset_test_single(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    elib_key_driver_cfg_t drv = make_single_key_driver();
    elib_key_init(&test_ctx, &drv);
}

/* --- Init tests --- */

static void test_init_valid(void) {
    printf("Test: init with valid parameters... ");
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));

    elib_key_driver_cfg_t drv = make_single_key_driver();
    elib_key_err_t err = elib_key_init(&test_ctx, &drv);
    assert(err == ELIB_KEY_OK);
    assert(test_ctx.initialized == 1);
    assert(test_ctx.driver_cfg == &drv);

    printf("PASSED\n");
}

static void test_init_null_ctx(void) {
    printf("Test: init with null ctx... ");
    elib_key_driver_cfg_t drv = make_single_key_driver();
    assert(elib_key_init(NULL, &drv) == ELIB_KEY_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

static void test_init_null_cfg(void) {
    printf("Test: init with null cfg... ");
    memset(&test_ctx, 0, sizeof(test_ctx));
    assert(elib_key_init(&test_ctx, NULL) == ELIB_KEY_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

static void test_init_null_callback(void) {
    printf("Test: init with null callback... ");
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    elib_key_driver_cfg_t drv = make_single_key_driver();
    drv.read_level = NULL;
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

static void test_init_zero_keys(void) {
    printf("Test: init with zero keys... ");
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    elib_key_driver_cfg_t drv = make_single_key_driver();
    drv.key_count = 0;
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_ERR_TOO_MANY_KEYS);
    printf("PASSED\n");
}

/* --- Deinit tests --- */

static void test_deinit_valid(void) {
    printf("Test: deinit with valid ctx... ");
    reset_test_single();
    elib_key_err_t err = elib_key_deinit(&test_ctx);
    assert(err == ELIB_KEY_OK);
    assert(test_ctx.initialized == 0);
    printf("PASSED\n");
}

static void test_deinit_null(void) {
    printf("Test: deinit with null ctx... ");
    assert(elib_key_deinit(NULL) == ELIB_KEY_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

static void test_uninitialized_rejects_scan(void) {
    printf("Test: uninitialized context rejects scan... ");
    reset_test_single();
    elib_key_deinit(&test_ctx);
    assert(elib_key_scan(&test_ctx, 10) == ELIB_KEY_ERR_NOT_INITIALIZED);
    printf("PASSED\n");
}

static const elib_key_driver_cfg_t *key_cfg;

/* --- IDLE state tests --- */

static void test_idle_no_event_when_released(void) {
    printf("Test: idle state, key stays released, no event... ");
    reset_test_single();
    key_cfg = test_ctx.driver_cfg;
    reset_event_log();

    /* Key already released (level=0), scan multiple times */
    elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 10);
    assert(event_count == 0);

    printf("PASSED\n");
}

static void test_idle_detects_rising_edge(void) {
    printf("Test: idle state detects rising edge, transitions to PRESSED... ");
    reset_test_single();
    key_cfg = test_ctx.driver_cfg;
    reset_event_log();

    /* Press key */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);

    /* Should be in PRESSED state, no event yet */
    assert(test_ctx.key_states[0].state == ELIB_KEY_STATE_PRESSED);
    assert(event_count == 0);

    printf("PASSED\n");
}

int main(void) {
    printf("=== elib-key tests ===\n\n");

    test_init_valid();
    test_init_null_ctx();
    test_init_null_cfg();
    test_init_null_callback();
    test_init_zero_keys();
    test_deinit_valid();
    test_deinit_null();
    test_uninitialized_rejects_scan();
    test_idle_no_event_when_released();
    test_idle_detects_rising_edge();

    printf("\n=== All tests passed ===\n");
    return 0;
}

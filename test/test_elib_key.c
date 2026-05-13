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

static const uint16_t thresholds_multi[] = {2000};
static const elib_key_item_cfg_t multi_key_cfg[] = {
    { .key_id = 0, .max_click = 3, .multi_click_window_ms = 300,
      .long_press_thresholds = thresholds_multi, .threshold_count = 1 },
};

static elib_key_driver_cfg_t make_multi_key_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = multi_key_cfg,
        .key_count = 1,
        .read_level = mock_read_level,
        .on_event = mock_on_event,
    };
    return cfg;
}

static void reset_test_multi(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    elib_key_driver_cfg_t drv = make_multi_key_driver();
    elib_key_init(&test_ctx, &drv);
    key_cfg = test_ctx.driver_cfg;
}

static const uint8_t combo_keys_ab[] = {0, 1};
static const elib_key_combo_cfg_t linkage_combos[] = {
    { .mode = ELIB_KEY_COMBO_SHORT, .combo_keys = combo_keys_ab,
      .combo_count = 2, .window_ms = 500, .combo_id = 0 },
};

static elib_key_linkage_cfg_t make_linkage_cfg(void) {
    elib_key_linkage_cfg_t cfg = {
        .combos = linkage_combos,
        .combo_count = 1,
        .on_combo = mock_on_combo,
    };
    return cfg;
}

static const uint16_t thresholds_two[] = {2000};
static const elib_key_item_cfg_t two_key_cfg[] = {
    { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_two, .threshold_count = 1 },
    { .key_id = 1, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_two, .threshold_count = 1 },
};

static elib_key_driver_cfg_t make_two_key_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = two_key_cfg,
        .key_count = 2,
        .read_level = mock_read_level,
        .on_event = mock_on_event,
    };
    return cfg;
}

static void reset_test_two_combo(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    elib_key_driver_cfg_t drv = make_two_key_driver();
    elib_key_init(&test_ctx, &drv);
    elib_key_linkage_cfg_t lnk = make_linkage_cfg();
    elib_key_linkage_init(&test_ctx, &lnk);
    key_cfg = test_ctx.driver_cfg;
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

/* --- PRESSED state tests --- */

static void test_single_click_short_press(void) {
    printf("Test: single key, short press -> SHORT_CLICK... ");
    reset_test_single();
    key_cfg = test_ctx.driver_cfg;
    reset_event_log();

    /* Press */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);
    /* Hold briefly */
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);

    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);
    assert(last_event_clicks == 1);

    printf("PASSED\n");
}

static void test_single_click_long_press(void) {
    printf("Test: single key, long press -> LONG_PRESS... ");
    reset_test_single();
    key_cfg = test_ctx.driver_cfg;
    reset_event_log();

    /* Press and hold past threshold (1000ms) */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);

    /* Hold for 1010ms total (10 already + 1000 more) */
    elib_key_scan(&test_ctx, 1000);

    /* Release */
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);

    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_threshold == 0);

    printf("PASSED\n");
}

static void test_single_click_exceed_max_threshold(void) {
    printf("Test: single key, exceed max threshold -> no event... ");
    reset_test_single();
    key_cfg = test_ctx.driver_cfg;
    reset_event_log();

    /* Press and hold well past threshold */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 2000);

    /* Release */
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);

    assert(event_count == 0);

    printf("PASSED\n");
}

/* --- Multi-click tests --- */

static void test_double_click_within_window(void) {
    printf("Test: double click within window -> SHORT_CLICK(2)... ");
    reset_test_multi();
    reset_event_log();

    /* First click */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);
    assert(event_count == 0); /* waiting for more clicks */

    /* Second click within window */
    elib_key_scan(&test_ctx, 100); /* idle 100ms */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);

    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);
    assert(last_event_clicks == 2);

    printf("PASSED\n");
}

static void test_triple_click_immediate_emit(void) {
    printf("Test: triple click with max_click=3 -> immediate emit... ");
    reset_test_multi();
    reset_event_log();

    /* First click */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    /* Second click */
    elib_key_scan(&test_ctx, 100);
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    /* Third click (reaches max_click=3) */
    elib_key_scan(&test_ctx, 100);
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    assert(event_count == 1);
    assert(last_event_clicks == 3);

    printf("PASSED\n");
}

static void test_single_click_timeout(void) {
    printf("Test: single click then timeout -> SHORT_CLICK(1)... ");
    reset_test_multi();
    reset_event_log();

    /* Single click */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 0);

    /* Wait for window timeout (300ms) */
    elib_key_scan(&test_ctx, 300);

    assert(event_count == 1);
    assert(last_event_clicks == 1);

    printf("PASSED\n");
}

static void test_multi_click_second_press_exceeds_threshold(void) {
    printf("Test: multi-click, second press exceeds max threshold -> no event... ");
    reset_test_multi();
    reset_event_log();

    /* First click */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    /* Second click, hold past max threshold (2000ms) */
    elib_key_scan(&test_ctx, 100);
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 2000);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    assert(event_count == 0);

    printf("PASSED\n");
}

static void test_consumed_event_not_emitted(void) {
    printf("Test: consumed event is not emitted... ");
    reset_test_single();
    key_cfg = test_ctx.driver_cfg;
    reset_event_log();

    /* Press key */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);

    /* Manually mark as consumed (simulating combo trigger) */
    test_ctx.key_states[0].consumed = 1;

    /* Release */
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);

    assert(event_count == 0);

    printf("PASSED\n");
}

/* --- Linkage init/deinit tests --- */

static void test_linkage_init_valid(void) {
    printf("Test: linkage init with valid parameters... ");
    reset_test_single();
    elib_key_linkage_cfg_t lnk = make_linkage_cfg();
    elib_key_err_t err = elib_key_linkage_init(&test_ctx, &lnk);
    assert(err == ELIB_KEY_OK);
    assert(test_ctx.linkage_initialized == 1);
    printf("PASSED\n");
}

static void test_linkage_init_null_ctx(void) {
    printf("Test: linkage init with null ctx... ");
    elib_key_linkage_cfg_t lnk = make_linkage_cfg();
    assert(elib_key_linkage_init(NULL, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

static void test_linkage_init_not_initialized(void) {
    printf("Test: linkage init before driver init... ");
    memset(&test_ctx, 0, sizeof(test_ctx));
    elib_key_linkage_cfg_t lnk = make_linkage_cfg();
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_NOT_INITIALIZED);
    printf("PASSED\n");
}

static void test_linkage_deinit_valid(void) {
    printf("Test: linkage deinit... ");
    reset_test_single();
    elib_key_linkage_cfg_t lnk = make_linkage_cfg();
    elib_key_linkage_init(&test_ctx, &lnk);
    elib_key_err_t err = elib_key_linkage_deinit(&test_ctx);
    assert(err == ELIB_KEY_OK);
    assert(test_ctx.linkage_initialized == 0);
    printf("PASSED\n");
}

static void test_linkage_deinit_null(void) {
    printf("Test: linkage deinit with null ctx... ");
    assert(elib_key_linkage_deinit(NULL) == ELIB_KEY_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

/* --- SHORT_COMBO tests --- */

static void test_short_combo_triggered(void) {
    printf("Test: SHORT_COMBO A+B triggered... ");
    reset_test_two_combo();
    reset_event_log();
    reset_combo_log();

    /* Press A and release */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    /* Press B and release (within window) */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(last_combo_id == 0);
    assert(last_combo_mode == ELIB_KEY_COMBO_SHORT);
    /* Driver events should be consumed */
    assert(event_count == 0);

    printf("PASSED\n");
}

static void test_short_combo_no_match_single_key(void) {
    printf("Test: SHORT_COMBO, only one key pressed -> no combo... ");
    reset_test_two_combo();
    reset_event_log();
    reset_combo_log();

    /* Only press A */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 0);
    assert(event_count == 1); /* A fires as independent event */

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
    test_single_click_short_press();
    test_single_click_long_press();
    test_single_click_exceed_max_threshold();
    test_double_click_within_window();
    test_triple_click_immediate_emit();
    test_single_click_timeout();
    test_multi_click_second_press_exceeds_threshold();
    test_consumed_event_not_emitted();
    test_linkage_init_valid();
    test_linkage_init_null_ctx();
    test_linkage_init_not_initialized();
    test_linkage_deinit_valid();
    test_linkage_deinit_null();
    test_short_combo_triggered();
    test_short_combo_no_match_single_key();

    printf("\n=== All tests passed ===\n");
    return 0;
}

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

/* Raw edge / long-hold callback logs */
static int press_events[ELIB_KEY_MAX_KEYS];
static int release_events[ELIB_KEY_MAX_KEYS];
static int long_hold_count;
static uint8_t last_long_hold_key;
static uint8_t last_long_hold_idx;

static void reset_event_log(void) {
    event_count = 0;
    last_event_key = 0xFF;
    memset(press_events, 0, sizeof(press_events));
    memset(release_events, 0, sizeof(release_events));
    long_hold_count = 0;
    last_long_hold_key = 0xFF;
    last_long_hold_idx = 0xFF;
}

static void mock_on_event(uint8_t key_id, elib_key_event_t event,
                          uint8_t click_count, uint8_t threshold_idx) {
    last_event_key = key_id;
    last_event_type = event;
    last_event_clicks = click_count;
    last_event_threshold = threshold_idx;
    event_count++;
}

static void mock_on_press(uint8_t key_id) {
    press_events[key_id]++;
}

static void mock_on_release(uint8_t key_id) {
    release_events[key_id]++;
}

static void mock_on_long_hold(uint8_t key_id, uint8_t threshold_idx) {
    long_hold_count++;
    last_long_hold_key = key_id;
    last_long_hold_idx = threshold_idx;
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

/* Two thresholds so LONG_PRESS has a valid range [1000, 2000);
 * hold >= last threshold (2000) is invalid per design spec 4.3 */
static const uint16_t thresholds_single[] = {1000, 2000};
static const elib_key_item_cfg_t single_key_cfg[] = {
    { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_single, .threshold_count = 2 },
};

static elib_key_driver_cfg_t make_single_key_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = single_key_cfg,
        .key_count = 1,
        .read_level = mock_read_level,
        .on_event = mock_on_event,
        .on_press = mock_on_press,
        .on_release = mock_on_release,
        .on_long_hold = mock_on_long_hold,
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

static const elib_key_driver_cfg_t *key_cfg;

/* Two thresholds so the multi-click key's long press has a valid range
 * [2000, 3000); hold >= 3000 is invalid per design spec 4.3 */
static const uint16_t thresholds_multi[] = {2000, 3000};
static const elib_key_item_cfg_t multi_key_cfg[] = {
    { .key_id = 0, .max_click = 3, .multi_click_window_ms = 300,
      .long_press_thresholds = thresholds_multi, .threshold_count = 2 },
};

static elib_key_driver_cfg_t make_multi_key_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = multi_key_cfg,
        .key_count = 1,
        .read_level = mock_read_level,
        .on_event = mock_on_event,
        .on_press = mock_on_press,
        .on_release = mock_on_release,
        .on_long_hold = mock_on_long_hold,
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

/* Short thresholds keep combo sequences inside the 500ms link window:
 * LONG range is [200, 500), hold >= 500 is invalid per design spec 4.3 */
static const uint16_t thresholds_two[] = {200, 500};
static const elib_key_item_cfg_t two_key_cfg[] = {
    { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_two, .threshold_count = 2 },
    { .key_id = 1, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_two, .threshold_count = 2 },
};

static elib_key_driver_cfg_t make_two_key_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = two_key_cfg,
        .key_count = 2,
        .read_level = mock_read_level,
        .on_event = mock_on_event,
        .on_press = mock_on_press,
        .on_release = mock_on_release,
        .on_long_hold = mock_on_long_hold,
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

static const uint8_t combo_hold_a[] = {0};
static const uint8_t combo_press_b[] = {1};
static const elib_key_combo_cfg_t linkage_combos_all[] = {
    { .mode = ELIB_KEY_COMBO_SHORT, .combo_keys = combo_keys_ab,
      .combo_count = 2, .window_ms = 500, .combo_id = 0 },
    { .mode = ELIB_KEY_COMBO_LONG, .combo_keys = combo_keys_ab,
      .combo_count = 2, .window_ms = 500, .combo_id = 1 },
    { .mode = ELIB_KEY_COMBO_HOLD_PRESS, .hold_keys = combo_hold_a,
      .hold_count = 1, .press_keys = combo_press_b, .press_count = 1,
      .combo_id = 2 },
};

static elib_key_linkage_cfg_t make_linkage_cfg_all(void) {
    elib_key_linkage_cfg_t cfg = {
        .combos = linkage_combos_all,
        .combo_count = 3,
        .on_combo = mock_on_combo,
    };
    return cfg;
}

static void reset_test_two_combo_all(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    elib_key_driver_cfg_t drv = make_two_key_driver();
    elib_key_init(&test_ctx, &drv);
    elib_key_linkage_cfg_t lnk = make_linkage_cfg_all();
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
    assert(test_ctx.bit_flags.initialized == 1);
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

static void test_init_null_thresholds(void) {
    printf("Test: init with null thresholds... ");
    memset(&test_ctx, 0, sizeof(test_ctx));
    elib_key_item_cfg_t bad_cfg[] = {
        { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
          .long_press_thresholds = NULL, .threshold_count = 1 },
    };
    elib_key_driver_cfg_t drv = {
        .keys = bad_cfg, .key_count = 1,
        .read_level = mock_read_level, .on_event = mock_on_event,
    };
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_ERR_INVALID_PARAM);
    assert(test_ctx.driver_cfg == NULL); /* ctx must stay untouched on failure */
    printf("PASSED\n");
}

static void test_init_zero_thresholds(void) {
    printf("Test: init with zero threshold count... ");
    memset(&test_ctx, 0, sizeof(test_ctx));
    elib_key_item_cfg_t bad_cfg[] = {
        { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
          .long_press_thresholds = thresholds_single, .threshold_count = 0 },
    };
    elib_key_driver_cfg_t drv = {
        .keys = bad_cfg, .key_count = 1,
        .read_level = mock_read_level, .on_event = mock_on_event,
    };
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

/* --- Deinit tests --- */

static void test_deinit_valid(void) {
    printf("Test: deinit with valid ctx... ");
    reset_test_single();
    elib_key_err_t err = elib_key_deinit(&test_ctx);
    assert(err == ELIB_KEY_OK);
    assert(test_ctx.bit_flags.initialized == 0);
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
    printf("Test: double click within window -> SHORT_CLICK(2) on timeout... ");
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

    /* max_click=3: sequence still open, nothing emitted yet */
    assert(event_count == 0);

    /* Window timeout emits the accumulated double click */
    elib_key_scan(&test_ctx, 300);
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

    /* Second click, hold past max threshold (3000ms) */
    elib_key_scan(&test_ctx, 100);
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 3100);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    assert(event_count == 0);

    printf("PASSED\n");
}

static void test_multi_click_key_long_press(void) {
    printf("Test: multi-click key long press -> LONG_PRESS... ");
    reset_test_multi();
    reset_event_log();

    /* Press and hold into long range [2000, 3000) */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 2000);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    /* hold = 2020ms */

    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_threshold == 0);

    printf("PASSED\n");
}

static void test_double_click_then_long_press_discards_clicks(void) {
    printf("Test: click then long press -> LONG_PRESS only, clicks discarded... ");
    reset_test_multi();
    reset_event_log();

    /* Click once (released, sequence open) */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    /* Second press held into long range: long press terminates the sequence */
    elib_key_scan(&test_ctx, 100);
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 2000);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    /* hold = 2010ms; the pending first click is discarded */

    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_threshold == 0);

    printf("PASSED\n");
}

static void test_long_press_resets_sequence(void) {
    printf("Test: long press terminates sequence, next click starts fresh... ");
    reset_test_multi();
    reset_event_log();

    /* Long press: fires LONG_PRESS and resets the sequence */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 2000);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    /* hold = 2020ms */
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);

    /* Next quick click must count from 1, not continue the old sequence */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 1); /* waiting in MULTI_CLICK_WAIT, level still 1 */

    elib_key_scan(&test_ctx, 300); /* window timeout emits */
    assert(event_count == 2);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);
    assert(last_event_clicks == 1);

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
    test_ctx.key_states[0].bit_flags.consumed = 1;

    /* Release */
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);

    assert(event_count == 0);

    printf("PASSED\n");
}

/* --- Linkage init/deinit tests --- */

static void test_linkage_init_valid(void) {
    printf("Test: linkage init with valid parameters... ");
    reset_test_two_combo();
    elib_key_linkage_cfg_t lnk = make_linkage_cfg();
    elib_key_err_t err = elib_key_linkage_init(&test_ctx, &lnk);
    assert(err == ELIB_KEY_OK);
    assert(test_ctx.bit_flags.linkage_initialized == 1);
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
    reset_test_two_combo();
    elib_key_linkage_cfg_t lnk = make_linkage_cfg();
    elib_key_linkage_init(&test_ctx, &lnk);
    elib_key_err_t err = elib_key_linkage_deinit(&test_ctx);
    assert(err == ELIB_KEY_OK);
    assert(test_ctx.bit_flags.linkage_initialized == 0);
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
    /* Key A's event fired independently in its own scan (known cross-scan
     * race); key B's event was consumed by the combo */
    assert(event_count == 1);

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

/* --- LONG_COMBO tests --- */

static void test_long_combo_triggered(void) {
    printf("Test: LONG_COMBO A+B triggered... ");
    reset_test_two_combo_all();
    reset_event_log();
    reset_combo_log();

    /* Press A, hold into long-press range [200, 500) */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 200);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    /* A fires independently in its own scan (known cross-scan race) */
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_key == 0);

    /* Press B, hold into long-press range, release within A's window */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 200);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(last_combo_id == 1);
    assert(last_combo_mode == ELIB_KEY_COMBO_LONG);
    assert(event_count == 1); /* B's event consumed by the combo */

    printf("PASSED\n");
}

/* --- HOLD_PRESS tests --- */

static void test_hold_press_triggered(void) {
    printf("Test: HOLD_PRESS A held + B pressed... ");
    reset_test_two_combo_all();
    reset_event_log();
    reset_combo_log();

    /* Press A and hold */
    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1000); /* hold past threshold */

    /* Press and release B */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(last_combo_id == 2);
    assert(last_combo_mode == ELIB_KEY_COMBO_HOLD_PRESS);
    assert(event_count == 0); /* consumed */

    /* Release A */
    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);

    printf("PASSED\n");
}

static void test_hold_press_a_not_held_enough(void) {
    printf("Test: HOLD_PRESS, A not held long enough -> no combo... ");
    reset_test_two_combo_all();
    reset_event_log();
    reset_combo_log();

    /* Press A briefly */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    /* Press and release B immediately */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    /* A not held past threshold, no combo */
    assert(combo_count == 0);

    /* Release A */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    printf("PASSED\n");
}

static void test_short_combo_timeout(void) {
    printf("Test: SHORT_COMBO timeout -> no combo... ");
    reset_test_two_combo();
    reset_event_log();
    reset_combo_log();

    /* Press A */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    /* Wait past window (500ms) */
    elib_key_scan(&test_ctx, 500);

    /* Press B */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 0);
    /* B fires as independent event */
    assert(event_count >= 1);

    printf("PASSED\n");
}

static void test_short_combo_same_scan_release(void) {
    printf("Test: SHORT_COMBO same-scan release -> combo only, no driver event... ");
    reset_test_two_combo();
    reset_event_log();
    reset_combo_log();

    /* Press both keys in one scan, release both in one scan */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(last_combo_id == 0);
    assert(last_combo_mode == ELIB_KEY_COMBO_SHORT);
    assert(event_count == 0); /* both driver events consumed by the combo */

    printf("PASSED\n");
}

static void test_short_combo_retriggers(void) {
    printf("Test: SHORT_COMBO can trigger again after firing... ");
    reset_test_two_combo();
    reset_event_log();
    reset_combo_log();

    /* Fire the combo once (same-scan release for clean event counts) */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 1);

    /* Fire it a second time */
    reset_event_log();
    reset_combo_log();
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1); /* retriggered, not one-shot */
    assert(last_combo_id == 0);
    assert(event_count == 0);

    printf("PASSED\n");
}

/* --- Configuration helper macros --- */

/* File-scope config built entirely through helper macros */
static const uint8_t macro_keys_ab[] = {0, 1};
static const uint8_t macro_keys_a[]  = {0};
static const uint8_t macro_keys_b[]  = {1};

static const elib_key_item_cfg_t macro_keys[] = {
    ELIB_KEY_KEY(0, 2, 300, 1000, 3000),   /* double click + long press */
    ELIB_KEY_KEY_CLICKS_ONLY(1, 1, 0),     /* pure single-click, no long press */
};

static const elib_key_combo_cfg_t macro_combos[] = {
    ELIB_KEY_COMBO_SHORT(macro_keys_ab, 500, 0),
    ELIB_KEY_COMBO_HOLD_PRESS(macro_keys_a, macro_keys_b, 1),
};

static const elib_key_driver_cfg_t macro_driver =
    ELIB_KEY_DRIVER_CFG_FULL(macro_keys, mock_read_level, mock_on_event,
                             mock_on_press, mock_on_release,
                             mock_on_long_hold);

static const elib_key_linkage_cfg_t macro_link =
    ELIB_KEY_LINKAGE_CFG(macro_combos, mock_on_combo);

/* Single-click variant for combo flow tests: participants with max_click == 1
 * emit SHORT_CLICK in the release scan, so same-scan combos fire cleanly */
static const elib_key_item_cfg_t macro_keys_1click[] = {
    ELIB_KEY_KEY(0, 1, 0, 1000),
    ELIB_KEY_KEY_CLICKS_ONLY(1, 1, 0),
};

static const elib_key_driver_cfg_t macro_driver_1click =
    ELIB_KEY_DRIVER_CFG(macro_keys_1click, mock_read_level, mock_on_event);

/* Driver-only two-key reset (no linkage): for validation tests that build
 * their own (invalid) linkage configs */
static void reset_init_two_key_driver(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    elib_key_driver_cfg_t drv = make_two_key_driver();
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_OK);
}

static void test_macros_build_valid_config(void) {
    printf("Test: helper macros build a valid config (derived counts)... ");
    reset_event_log();
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));

    assert(elib_key_init(&test_ctx, &macro_driver) == ELIB_KEY_OK);
    assert(test_ctx.driver_cfg->key_count == 2);

    /* ELIB_KEY_KEY: derived threshold count and values */
    assert(test_ctx.driver_cfg->keys[0].key_id == 0);
    assert(test_ctx.driver_cfg->keys[0].max_click == 2);
    assert(test_ctx.driver_cfg->keys[0].multi_click_window_ms == 300);
    assert(test_ctx.driver_cfg->keys[0].threshold_count == 2);
    assert(test_ctx.driver_cfg->keys[0].long_press_thresholds[0] == 1000);
    assert(test_ctx.driver_cfg->keys[0].long_press_thresholds[1] == 3000);

    /* ELIB_KEY_KEY_CLICKS_ONLY: unreachable sentinel cap */
    assert(test_ctx.driver_cfg->keys[1].max_click == 1);
    assert(test_ctx.driver_cfg->keys[1].threshold_count == 1);
    assert(test_ctx.driver_cfg->keys[1].long_press_thresholds[0] ==
           ELIB_KEY_CLICK_ONLY_THRESHOLD);

    /* ELIB_KEY_DRIVER_CFG_FULL wired the optional callbacks */
    assert(test_ctx.driver_cfg->on_press == mock_on_press);
    assert(test_ctx.driver_cfg->on_release == mock_on_release);
    assert(test_ctx.driver_cfg->on_long_hold == mock_on_long_hold);

    /* ELIB_KEY_LINKAGE_CFG: derived combo count */
    assert(elib_key_linkage_init(&test_ctx, &macro_link) == ELIB_KEY_OK);
    assert(test_ctx.linkage_cfg->combo_count == 2);

    printf("PASSED\n");
}

static void test_macros_combo_flow(void) {
    printf("Test: macro-built config detects SHORT combo and consumes... ");
    reset_event_log();
    reset_combo_log();
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));

    assert(elib_key_init(&test_ctx, &macro_driver_1click) == ELIB_KEY_OK);
    assert(elib_key_linkage_init(&test_ctx, &macro_link) == ELIB_KEY_OK);

    /* Same-scan press+release of both keys; both are max_click == 1 so
     * SHORT_CLICK pends in the release scan and the combo fires cleanly */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(last_combo_id == 0);
    assert(last_combo_mode == ELIB_KEY_COMBO_SHORT);
    assert(event_count == 0); /* both classified events consumed */
    assert(release_events[0] == 1); /* raw release still fires */
    assert(release_events[1] == 1);

    printf("PASSED\n");
}

static void test_macros_multiclick_race(void) {
    printf("Test: macro double-click key + combo cross-scan race... ");
    reset_event_log();
    reset_combo_log();
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));

    /* macro_driver: key 0 is a double-click key (window 300ms) */
    assert(elib_key_init(&test_ctx, &macro_driver) == ELIB_KEY_OK);
    assert(elib_key_linkage_init(&test_ctx, &macro_link) == ELIB_KEY_OK);

    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    /* Key 1 (single click) pends SHORT_CLICK -> emitted independently this
     * scan. Key 0 entered its 300ms multi-click wait: no event yet, and the
     * combo cannot match without it. */
    assert(combo_count == 0);
    assert(event_count == 1);

    /* Idle scans: key 0's click window expires at 300ms while the 500ms
     * combo window is still armed, so its single click feeds the combo */
    elib_key_scan(&test_ctx, 100); /* idle 100 */
    elib_key_scan(&test_ctx, 100); /* idle 200 */
    elib_key_scan(&test_ctx, 100); /* idle 300 -> click emitted, combo fires */

    assert(combo_count == 1);
    assert(last_combo_id == 0);
    /* Key 0's event was consumed by the combo; key 1's had already fired */
    assert(event_count == 1);

    printf("PASSED\n");
}

/* --- Linkage init validation --- */

static void test_linkage_rejects_unknown_key(void) {
    printf("Test: linkage init rejects unknown combo key id... ");
    reset_init_two_key_driver();

    static const uint8_t bad_ids[] = {99};
    elib_key_combo_cfg_t bad = {
        .mode = ELIB_KEY_COMBO_SHORT, .combo_keys = bad_ids,
        .combo_count = 1, .window_ms = 500, .combo_id = 9,
    };
    elib_key_linkage_cfg_t lnk = {
        .combos = &bad, .combo_count = 1, .on_combo = mock_on_combo,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);
    assert(test_ctx.bit_flags.linkage_initialized == 0); /* ctx untouched */

    printf("PASSED\n");
}

static void test_linkage_rejects_too_many_combo_keys(void) {
    printf("Test: linkage init rejects combo with too many keys... ");
    reset_init_two_key_driver();

    static const uint8_t many_ids[] = {0, 1, 0, 1, 0}; /* 5 > ELIB_KEY_MAX_COMBO_KEYS */
    elib_key_combo_cfg_t bad = {
        .mode = ELIB_KEY_COMBO_SHORT, .combo_keys = many_ids,
        .combo_count = 5, .window_ms = 500, .combo_id = 9,
    };
    elib_key_linkage_cfg_t lnk = {
        .combos = &bad, .combo_count = 1, .on_combo = mock_on_combo,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_linkage_rejects_null_keys(void) {
    printf("Test: linkage init rejects null/empty combo key lists... ");
    reset_init_two_key_driver();

    elib_key_combo_cfg_t bad = {
        .mode = ELIB_KEY_COMBO_SHORT, .combo_keys = NULL,
        .combo_count = 1, .window_ms = 500, .combo_id = 9,
    };
    elib_key_linkage_cfg_t lnk = {
        .combos = &bad, .combo_count = 1, .on_combo = mock_on_combo,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);

    /* zero count is also rejected */
    static const uint8_t ids[] = {0};
    bad.combo_keys = ids;
    bad.combo_count = 0;
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_linkage_rejects_empty_hold_press(void) {
    printf("Test: linkage init rejects HOLD_PRESS with empty press list... ");
    reset_init_two_key_driver();

    static const uint8_t hold_ids[] = {0};
    elib_key_combo_cfg_t bad = {
        .mode = ELIB_KEY_COMBO_HOLD_PRESS,
        .hold_keys = hold_ids, .hold_count = 1,
        .press_keys = NULL, .press_count = 0,
        .combo_id = 2,
    };
    elib_key_linkage_cfg_t lnk = {
        .combos = &bad, .combo_count = 1, .on_combo = mock_on_combo,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_press_release_callbacks(void) {
    printf("Test: press/release callbacks fire once per edge... ");
    reset_test_single();
    reset_event_log();

    mock_levels[0] = 1;
    elib_key_scan(&test_ctx, 10);
    /* press callback fires immediately, no classified event yet */
    assert(press_events[0] == 1);
    assert(release_events[0] == 0);
    assert(event_count == 0);

    mock_levels[0] = 0;
    elib_key_scan(&test_ctx, 10);
    /* short click classified event: press/release raw edges prior */
    assert(press_events[0] == 1);
    assert(release_events[0] == 1);
    assert(event_count == 1);

    printf("PASSED\n");
}

static void test_press_release_per_multi_click(void) {
    printf("Test: press/release callbacks fire per click in sequence... ");
    reset_test_multi();
    reset_event_log();

    /* Two clicks */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 100);
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    assert(press_events[0] == 2);
    assert(release_events[0] == 2);

    printf("PASSED\n");
}

static void test_null_callbacks_no_crash(void) {
    printf("Test: NULL press/release/long-hold callbacks are allowed... ");
    reset_event_log();
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));

    elib_key_item_cfg_t cfgs[] = {
        { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
          .long_press_thresholds = thresholds_single, .threshold_count = 2 },
    };
    elib_key_driver_cfg_t drv = {
        .keys = cfgs, .key_count = 1,
        .read_level = mock_read_level,
        .on_event = mock_on_event,
        .on_press = NULL,
        .on_release = NULL,
        .on_long_hold = NULL,
    };
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_OK);

    /* Full click cycle with all optional callbacks NULL */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);

    printf("PASSED\n");
}

static void test_release_cb_on_invalid(void) {
    printf("Test: on_release fires even for invalid classification... ");
    reset_test_single();
    reset_event_log();

    /* Press and hold past max threshold (2000ms) -> invalid, no event */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 2100);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    assert(press_events[0] == 1);
    assert(release_events[0] == 1); /* raw release still reported */
    assert(event_count == 0);       /* classified event suppressed */

    printf("PASSED\n");
}

static void test_release_cb_on_consumed(void) {
    printf("Test: on_release fires even when combo consumed the event... ");
    reset_test_two_combo();
    reset_event_log();
    reset_combo_log();

    /* Same-scan combo: both releases consumed */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(event_count == 0); /* classified events consumed by combo */
    assert(release_events[0] == 1);
    assert(release_events[1] == 1); /* raw releases still reported */

    printf("PASSED\n");
}

static void test_long_hold_callbacks(void) {
    printf("Test: long-hold callback fires at each threshold crossing... ");
    reset_test_single();
    reset_event_log();

    /* thresholds_single = {1000, 2000}: hold into [1000, 2000) */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    assert(long_hold_count == 0);

    elib_key_scan(&test_ctx, 1000); /* hold = 1010 >= 1000 */
    assert(long_hold_count == 1);
    assert(last_long_hold_key == 0);
    assert(last_long_hold_idx == 0);

    elib_key_scan(&test_ctx, 400); /* hold = 1410, no new level */
    assert(long_hold_count == 1);

    /* Release within long range: crossing cb already fired; classified
     * LONG_PRESS event follows on release */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(long_hold_count == 1);
    assert(release_events[0] == 1);
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_threshold == 0);

    printf("PASSED\n");
}

static void test_long_hold_same_scan_multi_level(void) {
    printf("Test: long-hold fires both levels within one long scan... ");
    reset_test_single();
    reset_event_log();

    /* One long scan crosses both thresholds {1000, 2000} */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 3000); /* hold = 3010, crossed 1000 and 2000 */
    assert(long_hold_count == 2);
    assert(last_long_hold_idx == 1); /* idx 0 then idx 1 */

    /* Holding beyond last threshold: release classifies invalid, but the
     * crossing at the last level was still notified */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(long_hold_count == 2);
    assert(release_events[0] == 1);
    assert(event_count == 0);

    printf("PASSED\n");
}

static void test_long_hold_resets_on_next_press(void) {
    printf("Test: long-hold notification level resets per press... ");
    reset_test_single();
    reset_event_log();

    /* Long press: crossing notified at idx 0 */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1010);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(long_hold_count == 1);

    /* New press, hold below first threshold: no new notification */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 500);
    assert(long_hold_count == 1);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(long_hold_count == 1);

    printf("PASSED\n");
}

int main(void) {
    printf("=== elib-key tests ===\n\n");

    test_init_valid();
    test_init_null_ctx();
    test_init_null_cfg();
    test_init_null_callback();
    test_init_zero_keys();
    test_init_null_thresholds();
    test_init_zero_thresholds();
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
    test_multi_click_key_long_press();
    test_double_click_then_long_press_discards_clicks();
    test_long_press_resets_sequence();
    test_consumed_event_not_emitted();
    test_linkage_init_valid();
    test_linkage_init_null_ctx();
    test_linkage_init_not_initialized();
    test_linkage_deinit_valid();
    test_linkage_deinit_null();
    test_short_combo_triggered();
    test_short_combo_no_match_single_key();
    test_long_combo_triggered();
    test_hold_press_triggered();
    test_hold_press_a_not_held_enough();
    test_short_combo_timeout();
    test_short_combo_same_scan_release();
    test_short_combo_retriggers();
    test_macros_build_valid_config();
    test_macros_combo_flow();
    test_macros_multiclick_race();
    test_linkage_rejects_unknown_key();
    test_linkage_rejects_too_many_combo_keys();
    test_linkage_rejects_null_keys();
    test_linkage_rejects_empty_hold_press();
    test_press_release_callbacks();
    test_press_release_per_multi_click();
    test_null_callbacks_no_crash();
    test_release_cb_on_invalid();
    test_release_cb_on_consumed();
    test_long_hold_callbacks();
    test_long_hold_same_scan_multi_level();
    test_long_hold_resets_on_next_press();

    printf("\n=== All tests passed ===\n");
    return 0;
}

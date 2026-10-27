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
static int combo_hold_count;
static uint8_t last_combo_hold_id;
static uint8_t last_combo_hold_level;

static void reset_combo_log(void) {
    combo_count = 0;
    last_combo_id = 0xFF;
    combo_hold_count = 0;
    last_combo_hold_id = 0xFF;
    last_combo_hold_level = 0xFF;
}

static void mock_on_combo(uint8_t combo_id, elib_key_combo_mode_t mode) {
    last_combo_id = combo_id;
    last_combo_mode = mode;
    combo_count++;
}

static void mock_on_combo_hold(uint8_t combo_id, uint8_t threshold_idx) {
    last_combo_hold_id = combo_id;
    last_combo_hold_level = threshold_idx;
    combo_hold_count++;
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
    /* static: elib_key_init stores a pointer into cfg; a helper-local would
     * dangle once this helper returns and the caller keeps scanning */
    static elib_key_driver_cfg_t drv;
    drv = make_single_key_driver();
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
    static elib_key_driver_cfg_t drv;   /* survives helper return */
    drv = make_multi_key_driver();
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
    static elib_key_driver_cfg_t drv;   /* survives helper return */
    drv = make_two_key_driver();
    elib_key_init(&test_ctx, &drv);
    static elib_key_linkage_cfg_t lnk;
    lnk = make_linkage_cfg();
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
    static elib_key_driver_cfg_t drv;   /* survives helper return */
    drv = make_two_key_driver();
    elib_key_init(&test_ctx, &drv);
    static elib_key_linkage_cfg_t lnk;
    lnk = make_linkage_cfg_all();
    elib_key_linkage_init(&test_ctx, &lnk);
    key_cfg = test_ctx.driver_cfg;
}

/* --- Single-gesture channel semantics (behavior matrix) ---
 * A(id0) long = reset machine, B(id1) long = reset filter,
 * AB long = factory mode (LONG combo), C(id2) belongs to no combo */
static const uint16_t thresholds_three[] = {1000, 3000};
static const elib_key_item_cfg_t three_key_cfg[] = {
    { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_three, .threshold_count = 2 },
    { .key_id = 1, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_three, .threshold_count = 2 },
    { .key_id = 2, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds_three, .threshold_count = 2 },
};

static elib_key_driver_cfg_t make_three_key_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = three_key_cfg, .key_count = 3,
        .read_level = mock_read_level, .on_event = mock_on_event,
        .on_press = mock_on_press, .on_release = mock_on_release,
        .on_long_hold = mock_on_long_hold,
    };
    return cfg;
}

static const uint8_t three_ids_ab[] = {0, 1};
/* Overall gesture window 6000: the legacy claim happens when the LAST
 * participant crosses its own first threshold (>= 1000 hold), so the
 * window must comfortably exceed that point and the release phase */
static const elib_key_combo_cfg_t three_combos[] = {
    ELIB_KEY_COMBO_LONG_BINARY(10, three_ids_ab, 6000),
};

static void reset_three(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    static elib_key_driver_cfg_t drv;   /* survives helper return */
    drv = make_three_key_driver();
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_OK);
    static elib_key_linkage_cfg_t lnk;
    lnk = (elib_key_linkage_cfg_t){
        .combos = three_combos, .combo_count = 1, .on_combo = mock_on_combo,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_OK);
    key_cfg = test_ctx.driver_cfg;
}

/* --- Windowed COMBO_LONG (co-held windows, mirror of per-key long press):
 * AB combo windows {3000, 5000} — claim the instant both are down, each
 * window crossed fires on_combo_hold, releasing inside [3000, 5000) fires
 * the combo, releasing past 5000 classifies invalid. window_ms is the
 * OVERALL gesture window (all press + all release must sit inside it,
 * measured from the first press); tuned per test via reset_windowed_ms */
static uint16_t combo_windows[] = {3000, 5000};
static elib_key_combo_cfg_t windowed_combos[] = {
    ELIB_KEY_COMBO_LONG_SIMPLE(10, three_ids_ab, 500, combo_windows),
};

static void reset_windowed_ms(uint16_t window_ms) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    static elib_key_driver_cfg_t drv;   /* survives helper return */
    drv = make_three_key_driver();
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_OK);
    static elib_key_linkage_cfg_t lnk;
    windowed_combos[0].window_ms = window_ms;
    lnk = (elib_key_linkage_cfg_t){
        .combos = windowed_combos, .combo_count = 1,
        .on_combo = mock_on_combo, .on_combo_hold = mock_on_combo_hold,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_OK);
    key_cfg = test_ctx.driver_cfg;
}

/* Default 8000: every legacy windowed-behavior timeline fits inside */
static void reset_windowed(void) {
    reset_windowed_ms(8000);
}

/* --- Overall-window SHORT combo: parked clicks beyond the window block
 * the combo match; members keep their own classification (A solo click
 * still fires on its WAIT timeout, B dies as an ignored follower) --- */
static const uint16_t span_key_thresholds[] = {1000, 3000};
static const elib_key_item_cfg_t span_key_cfg[] = {
    { .key_id = 0, .max_click = 2, .multi_click_window_ms = 300,
      .long_press_thresholds = span_key_thresholds, .threshold_count = 2 },
    { .key_id = 1, .max_click = 2, .multi_click_window_ms = 300,
      .long_press_thresholds = span_key_thresholds, .threshold_count = 2 },
};
static const uint8_t span_keys_ab[] = {0, 1};
static const elib_key_combo_cfg_t span_window_combos[] = {
    { .mode = ELIB_KEY_COMBO_SHORT, .combo_keys = span_keys_ab,
      .combo_count = 2, .window_ms = 100, .combo_id = 3 },
};

static elib_key_driver_cfg_t make_span_window_driver(void) {
    elib_key_driver_cfg_t cfg = {
        .keys = span_key_cfg, .key_count = 2,
        .read_level = mock_read_level, .on_event = mock_on_event,
        .on_press = mock_on_press, .on_release = mock_on_release,
        .on_long_hold = mock_on_long_hold,
    };
    return cfg;
}

static void reset_span_window(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));
    reset_event_log();
    reset_combo_log();
    static elib_key_driver_cfg_t drv;   /* survives helper return */
    drv = make_span_window_driver();
    assert(elib_key_init(&test_ctx, &drv) == ELIB_KEY_OK);
    static elib_key_linkage_cfg_t lnk;
    lnk = (elib_key_linkage_cfg_t){
        .combos = span_window_combos, .combo_count = 1,
        .on_combo = mock_on_combo,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_OK);
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

static void test_sequential_clicks_independent(void) {
    printf("Test: sequential single clicks never coexist (one at a time)... ");
    reset_test_two_combo();
    reset_event_log();
    reset_combo_log();

    /* A clicks and completes at its own release scan; the channel frees */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);

    /* B presses afterwards: starts its own gesture, no combo */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 0);
    /* Both fire as independent events: exactly one gesture at a time */
    assert(event_count == 2);

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
    printf("Test: LONG_COMBO claims on hold, fires when all released... ");
    reset_test_two_combo_all();
    reset_event_log();
    reset_combo_log();

    /* Press both in the same scan, hold past the first threshold: the
     * claim engages and cancels every participant's single-key long press */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 200);  /* hold 210: both past thr[0] */

    /* Release staggered: nothing fires until the LAST participant releases */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 0);
    assert(event_count == 0); /* claimed: no independent long press */

    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 1);
    assert(last_combo_id == 1);
    assert(last_combo_mode == ELIB_KEY_COMBO_LONG);
    assert(event_count == 0);
    /* Escalation callbacks were held back for the claimed participants */
    assert(long_hold_count == 0);

    printf("PASSED\n");
}

static void test_long_combo_claim_after_staggered_press(void) {
    printf("Test: LONG_COMBO claims once both participants crossed thr... ");
    reset_test_two_combo_all();
    reset_event_log();
    reset_combo_log();

    /* A pressed alone first: its own escalation still works */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 200);  /* A hold 210: level 0 notified */
    assert(long_hold_count == 1);
    assert(last_long_hold_key == 0);

    /* B joins later: becomes a follower; the combo claims once B is also
     * past its first threshold — while both press ages still fit inside
     * the 500 ms overall window (A is 415 ms in, budget left: 85 ms). */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 195);
    elib_key_scan(&test_ctx, 10);   /* A 415, B 205: claim engages */

    /* B's own escalation is suppressed for the rest of the gesture */
    elib_key_scan(&test_ctx, 10);   /* B hold ~215: still nothing */
    assert(long_hold_count == 1);   /* no further callbacks */

    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(last_combo_id == 1);
    assert(event_count == 0);

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
    ELIB_KEY_COMBO_SHORT(0, macro_keys_ab, 500),
    ELIB_KEY_COMBO_HOLD_PRESS(1, macro_keys_a, macro_keys_b),
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
    static elib_key_driver_cfg_t drv;   /* survives helper return */
    drv = make_two_key_driver();
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
    printf("Test: combo claims multi-click accumulation immediately... ");
    reset_event_log();
    reset_combo_log();
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(mock_levels, 0, sizeof(mock_levels));

    /* macro_driver: key 0 is a double-click key (window 300ms) */
    assert(elib_key_init(&test_ctx, &macro_driver) == ELIB_KEY_OK);
    assert(elib_key_linkage_init(&test_ctx, &macro_link) == ELIB_KEY_OK);

    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    /* Both clicks are captured in the same scan (key 0 parks in its
     * multi-click wait, key 1 pends its click): the SHORT combo claims and
     * consumes both accumulations immediately */
    assert(combo_count == 1);
    assert(last_combo_id == 0);
    assert(event_count == 0);

    /* Idle scans produce nothing afterwards */
    elib_key_scan(&test_ctx, 100);
    elib_key_scan(&test_ctx, 100);
    assert(combo_count == 1);
    assert(event_count == 0);

    /* The channel is free again: key 0's own double click completes */
    reset_event_log();
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 100);
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 1);
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);
    assert(last_event_clicks == 2);

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

static void test_linkage_rejects_bad_combo_windows(void) {
    printf("Test: linkage init rejects malformed COMBO_LONG windows... ");
    reset_init_two_key_driver();

    static const uint8_t ids_ab[] = {0, 1};
    static const uint16_t five_wins[] = {10, 20, 30, 40, 50}; /* > MAX_THRESHOLDS */
    elib_key_combo_cfg_t bad = {
        .mode = ELIB_KEY_COMBO_LONG, .combo_keys = ids_ab,
        .combo_count = 2, .combo_id = 9,
        .long_press_thresholds = five_wins, .threshold_count = 5,
    };
    elib_key_linkage_cfg_t lnk = {
        .combos = &bad, .combo_count = 1, .on_combo = mock_on_combo,
    };
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);

    /* non-NULL pointer with zero count is inconsistent: reject */
    bad.threshold_count = 0;
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);

    /* NULL array with windows pointing nowhere is rejected via count > 0 */
    bad.long_press_thresholds = NULL;
    bad.threshold_count = 2;
    assert(elib_key_linkage_init(&test_ctx, &lnk) == ELIB_KEY_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

/* --- Behavior matrix: single-gesture channel --- */

static void test_claim_cancels_single_longs(void) {
    printf("Test: claimed AB long cancels single A/B long actions... ");
    reset_three();

    /* Press both together, hold well past the first threshold */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1100);    /* hold 1110: claim engages */
    assert(long_hold_count == 0);      /* escalation fully silenced */

    /* Release A, keep B held long: still nothing individual */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1500);    /* B hold ~2620: silent */
    assert(event_count == 0);
    assert(long_hold_count == 0);

    /* Release B: the combo fires exactly once */
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 1);
    assert(last_combo_id == 10);
    assert(last_combo_mode == ELIB_KEY_COMBO_LONG);
    assert(event_count == 0);

    /* Raw edges are unaffected by the rules */
    assert(press_events[0] == 1 && press_events[1] == 1);
    assert(release_events[0] == 1 && release_events[1] == 1);

    /* Individual A long press works again on the freed channel */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1100);    /* A alone: level 0 escalation fires */
    assert(long_hold_count == 1);
    assert(last_long_hold_key == 0);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 1);          /* no second combo */
    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);

    printf("PASSED\n");
}

static void test_owner_survives_bump(void) {
    printf("Test: running gesture never disturbed by a non-matching key... ");
    reset_three();

    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);   /* A owns the channel */
    elib_key_scan(&test_ctx, 500);                      /* A hold 510 */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);   /* B bump: follower */
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);   /* B click discarded */
    assert(event_count == 0);
    assert(combo_count == 0);

    elib_key_scan(&test_ctx, 600);                      /* A hold 1130: level 0 */
    assert(long_hold_count == 1);
    elib_key_scan(&test_ctx, 1000);                     /* A hold ~2140: level 1 */
    assert(long_hold_count == 2);
    assert(last_long_hold_key == 0);

    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    /* A's own long press completed: the "reset machine" action fired */
    assert(combo_count == 0);          /* accidental bump made no combo */
    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_threshold == 1); /* hold in [1000, 3000) */
    assert(last_event_clicks == 0);
    assert(press_events[1] == 1);      /* B raw edges unaffected */
    assert(release_events[1] == 1);

    printf("PASSED\n");
}

static void test_same_scan_rise_without_combo_invalid(void) {
    printf("Test: same-scan multi-press without combo -> all invalid... ");
    reset_three();

    /* A and C rise in the same scan; no combo covers them */
    mock_levels[0] = 1; mock_levels[2] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1500);    /* both held 1510: fully silent */
    assert(event_count == 0);
    assert(long_hold_count == 0);
    assert(combo_count == 0);

    mock_levels[2] = 0; elib_key_scan(&test_ctx, 10);   /* C: discarded */
    assert(event_count == 0);

    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);   /* A: also discarded */
    assert(event_count == 0);
    assert(combo_count == 0);
    assert(release_events[0] == 1 && release_events[2] == 1);

    printf("PASSED\n");
}

static void test_channel_busy_without_linkage(void) {
    printf("Test: gesture exclusivity holds without combos configured... ");
    reset_init_two_key_driver();

    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);   /* A owns */
    elib_key_scan(&test_ctx, 200);                      /* A hold 210: level 0 */
    assert(long_hold_count == 1);

    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);   /* B bump: follower */
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);   /* B click discarded */
    assert(event_count == 0);

    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);   /* A long press valid */
    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_threshold == 0);

    printf("PASSED\n");
}

/* --- Windowed COMBO_LONG behavior --- */

static void test_combo_long_windows_escalation_release(void) {
    printf("Test: windowed AB long escalates per window, fires in-window... ");
    reset_windowed();

    /* Both pressed the same scan: claim engages at scan end and the co-held
     * origin is that moment; both participants go silent immediately */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);

    /* Co-held 1510: below the first window; participants are already
     * claimed, so their own escalations never fire */
    elib_key_scan(&test_ctx, 1500);
    assert(combo_hold_count == 0);
    assert(long_hold_count == 0);
    assert(combo_count == 0);
    assert(event_count == 0);

    /* Co-held 3110: first window crossed -> escalation level 0 */
    elib_key_scan(&test_ctx, 1600);
    assert(combo_hold_count == 1);
    assert(last_combo_hold_id == 10);
    assert(last_combo_hold_level == 0);

    /* A releases first; the co-held timer keeps running until the last
     * participant releases (release time measured from the origin) */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1200);    /* co-held 4320: inside [3000, 5000) */
    assert(combo_hold_count == 1);
    assert(combo_count == 0);

    /* B releases while inside a window: combo fires exactly once */
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 1);
    assert(last_combo_id == 10);
    assert(last_combo_mode == ELIB_KEY_COMBO_LONG);
    assert(event_count == 0);          /* no leaked individual events */
    assert(long_hold_count == 0);
    assert(combo_hold_count == 1);
    assert(press_events[0] == 1 && press_events[1] == 1);
    assert(release_events[0] == 1 && release_events[1] == 1);

    printf("PASSED\n");
}

static void test_combo_long_windows_staggered_origin_reset(void) {
    printf("Test: windowed AB long origin resets when the last key joins... ");
    reset_windowed();

    /* A pressed alone: its own escalation still works while unclaimed */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1300);    /* A hold 1310 >= 1000: level 0 */
    assert(long_hold_count == 1);
    assert(last_long_hold_key == 0);
    assert(combo_hold_count == 0);

    /* B joins: claim at scan end cancels further individual output, and
     * the co-held origin resets to this moment (A's earlier hold ignored) */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1800);    /* co-held 1800 < 3000 */
    assert(combo_hold_count == 0);
    assert(long_hold_count == 1);      /* A's level 1 silenced by the claim */
    assert(combo_count == 0);

    elib_key_scan(&test_ctx, 1300);    /* co-held 3110: window 0 arrives */
    assert(combo_hold_count == 1);
    assert(last_combo_hold_level == 0);

    /* A releases first; B keeps holding inside [3000, 5000) */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1100);    /* co-held 4220 */
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);

    assert(combo_count == 1);
    assert(last_combo_id == 10);
    assert(event_count == 0);
    assert(long_hold_count == 1);      /* no post-claim replay */

    printf("PASSED\n");
}

static void test_combo_long_release_past_cap_invalid(void) {
    printf("Test: releasing past the last window classifies invalid... ");
    reset_windowed();

    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 3200);    /* co-held 3200: window 0 */
    assert(combo_hold_count == 1);
    elib_key_scan(&test_ctx, 2000);    /* co-held 5200: window 1 = the cap */
    assert(combo_hold_count == 2);
    assert(last_combo_hold_level == 1);

    /* Hold past the cap at release: invalid, nothing fires (claims stay
     * cancelled — a dangerous per-key action can never leak) */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 0);
    assert(event_count == 0);
    assert(long_hold_count == 0);

    /* The channel is free again: A's own long press works */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1100);    /* A hold 1110: level 0 */
    assert(long_hold_count == 1);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(combo_count == 0);          /* no combo without B */

    printf("PASSED\n");
}

static void test_combo_long_under_first_window_silent(void) {
    printf("Test: co-held press released before the first window is silent... ");
    reset_windowed();

    /* Brief accidental co-press: claim silences everything, then release
     * before the first window fires nothing at all */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 2500);    /* co-held 2500 < 3000 */
    assert(long_hold_count == 0);      /* A hold 2510 is in its own valid
                                        * long range, yet suppressed */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 0);
    assert(event_count == 0);
    assert(combo_hold_count == 0);

    /* Channel free: a plain A click works again */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);

    printf("PASSED\n");
}

static void test_all_released_resets_everything(void) {
    printf("Test: once every key is released, the channel fully resets... ");
    reset_windowed();

    /* A+B claim the windowed combo and hold past the cap */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 5200);    /* co-held 5200: windows 0+1 both fired */
    assert(combo_hold_count == 2);

    /* C bumps mid-gesture and keeps hanging on after the combo ends */
    mock_levels[2] = 1; elib_key_scan(&test_ctx, 10);

    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    /* Invalid end (past the cap): nothing reports, claims are cleared */
    assert(combo_count == 0);
    assert(event_count == 0);
    assert(long_hold_count == 0);
    assert(test_ctx.key_states[0].bit_flags.claimed == 0);
    assert(test_ctx.key_states[1].bit_flags.claimed == 0);
    assert(test_ctx.gesture.bit_flags.locked == 0);

    /* While C still hangs, a fresh press is still an ignored follower */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 0);
    assert(test_ctx.key_states[0].bit_flags.follower == 0); /* discarded */

    /* C finally releases: from this scan every key is up */
    mock_levels[2] = 0; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 10);      /* one clean idle scan */

    for (uint8_t i = 0; i < 3; i++) {
        const elib_key_state_t *ks = &test_ctx.key_states[i];
        assert(ks->state == ELIB_KEY_STATE_IDLE);
        assert(ks->bit_flags.claimed == 0 && ks->bit_flags.follower == 0);
        assert(ks->bit_flags.owner == 0);
        assert(ks->bit_flags.fresh == 0 && ks->bit_flags.fell == 0);
    }
    assert(test_ctx.gesture.bit_flags.locked == 0);
    assert(test_ctx.gesture.hold_ms == 0);
    assert(test_ctx.gesture.notified_idx == 0);
    assert(test_ctx.gesture.locked_combo == 0);
    assert(test_ctx.pending_count == 0);

    /* The freed channel behaves normally again */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);
    assert(combo_count == 0);

    printf("PASSED\n");
}

static void test_combo_long_window_expired_invalid(void) {
    printf("Test: overall window expired -> condemned combo, release silent... ");
    reset_windowed_ms(3500);    /* overall window < the 5000 release cap */

    /* Both pressed the same scan: press span ~0 -> the claim engages and
     * the release deadline is the first press + 3500 ms */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 3200);    /* co-held 3210: window 0 escalates */
    assert(combo_hold_count == 1);

    elib_key_scan(&test_ctx, 500);     /* the overall window runs out */
    assert(combo_hold_count == 1);     /* no further escalation */

    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    /* Releasing inside the co-held window [3000, 5000) would normally
     * fire — but the overall window condemned the gesture: nothing */
    assert(combo_count == 0);
    assert(event_count == 0);
    assert(long_hold_count == 0);

    /* The channel is free again */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(event_count == 1);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);

    printf("PASSED\n");
}

static void test_combo_long_late_join_ignored(void) {
    printf("Test: B joining after the window cannot hijack A's gesture... ");
    reset_windowed_ms(500);

    /* A pressed alone: its solo long press proceeds normally */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 1100);    /* A hold 1110 >= 1000: level 0 */
    assert(long_hold_count == 1);
    assert(combo_hold_count == 0);

    /* B joins 1120 ms after A's press: outside the 500 ms overall window,
     * so the combo cannot claim; B is an ignored follower and A's own
     * gesture runs to completion */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);  /* B click discarded */
    assert(combo_count == 0);
    assert(event_count == 0);
    assert(long_hold_count == 1);

    elib_key_scan(&test_ctx, 1000);    /* A hold ~2130, still held */
    assert(long_hold_count == 1);      /* no combo escalation fired */

    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 0);
    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_LONG_PRESS);
    assert(last_event_threshold == 0);

    printf("PASSED\n");
}

static void test_short_combo_press_span_window(void) {
    printf("Test: SHORT combo needs all presses inside the window... ");
    reset_span_window();

    /* A clicks: its press is parked (max_click 2), the channel is owned */
    mock_levels[0] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 10);
    elib_key_scan(&test_ctx, 190);     /* A parked, idle 190 < 300 */

    /* B clicks 210 ms after A's press: inside A's park window but beyond
     * the 100 ms combo window. B is an ignored follower; its click dies */
    mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 0);
    assert(event_count == 0);

    /* A's WAIT times out: its solo click is emitted normally */
    mock_levels[0] = 0; elib_key_scan(&test_ctx, 100);  /* idle 300 */
    assert(event_count == 1);
    assert(last_event_key == 0);
    assert(last_event_type == ELIB_KEY_EVENT_SHORT_CLICK);
    assert(last_event_clicks == 1);
    assert(combo_count == 0);

    /* Control: same-scan clicks (span far below the 100 ms window) still
     * match the combo */
    mock_levels[0] = 1; mock_levels[1] = 1; elib_key_scan(&test_ctx, 10);
    mock_levels[0] = 0; mock_levels[1] = 0; elib_key_scan(&test_ctx, 10);
    assert(combo_count == 1);
    assert(last_combo_id == 3);
    assert(event_count == 1);          /* the two clicks stay consumed */

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
    test_sequential_clicks_independent();
    test_short_combo_no_match_single_key();
    test_long_combo_triggered();
    test_long_combo_claim_after_staggered_press();
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
    test_linkage_rejects_bad_combo_windows();
    test_claim_cancels_single_longs();
    test_owner_survives_bump();
    test_same_scan_rise_without_combo_invalid();
    test_channel_busy_without_linkage();
    test_combo_long_windows_escalation_release();
    test_combo_long_windows_staggered_origin_reset();
    test_combo_long_release_past_cap_invalid();
    test_combo_long_under_first_window_silent();
    test_all_released_resets_everything();
    test_combo_long_window_expired_invalid();
    test_combo_long_late_join_ignored();
    test_short_combo_press_span_window();
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

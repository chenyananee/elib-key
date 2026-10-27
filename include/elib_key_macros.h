/* elib_key_macros.h - Configuration helper macros
 *
 * Convenience macros for building configuration tables with minimal
 * boilerplate. Element counts are derived from array sizes, so users cannot
 * miscount thresholds or combo keys.
 *
 * Note: configs built with these macros must be defined at file scope
 * (static/global constants). ELIB_KEY_KEY embeds a C99 file-scope compound
 * literal whose address must outlive elib_key_scan() calls.
 */
#ifndef ELIB_KEY_MACROS_H
#define ELIB_KEY_MACROS_H

#include "elib_key_types.h"

/* Element count of a compile-time constant array */
#define ELIB_KEY_ARRAY_SIZE(a)     (sizeof(a) / sizeof((a)[0]))

/* Sentinel threshold for click-only keys: hold >= 65535ms is invalid, so
 * LONG_PRESS can never fire */
#define ELIB_KEY_CLICK_ONLY_THRESHOLD 65535u

/* Per-key item with thresholds written inline.
 * ELIB_KEY_KEY(id, clicks, window_ms, ...thresholds_ms)
 *   clicks: 1 = single click, 2 = double click, ...
 *   thresholds_ms: one or more ascending long-press thresholds in ms.
 *   The last one is the cap (hold >= last is invalid); use a large value
 *   (e.g. 60000) so no realistic hold falls past it.
 * Note: parameter names differ on purpose from struct field names, since the
 * preprocessor would otherwise substitute tokens like `.max_click`. */
/* Compute the count inline: ELIB_KEY_ARRAY_SIZE cannot be used here because
 * the preprocessor splits macro arguments on commas even inside braces of a
 * compound literal, which would misparse {1000, 3000} as two arguments. */
#define ELIB_KEY_KEY(id, clicks, window_ms, ...)                             \
    { .key_id = (id), .max_click = (clicks),                               \
      .multi_click_window_ms = (window_ms),                                  \
      .long_press_thresholds = (const uint16_t[]){__VA_ARGS__},              \
      .threshold_count =                                                  \
          sizeof((const uint16_t[]){__VA_ARGS__}) / sizeof(uint16_t) }

/* Per-key item reusing an existing, separately declared threshold array */
#define ELIB_KEY_KEY_SIMPLE(id, thr_arr)                                     \
    { .key_id = (id), .max_click = 1, .multi_click_window_ms = 0,          \
      .long_press_thresholds = (thr_arr),                                   \
      .threshold_count = ELIB_KEY_ARRAY_SIZE(thr_arr) }

#define ELIB_KEY_KEY_CLICKS(id, clicks, window_ms, thr_arr)                  \
    { .key_id = (id), .max_click = (clicks),                               \
      .multi_click_window_ms = (window_ms),                                  \
      .long_press_thresholds = (thr_arr),                                   \
      .threshold_count = ELIB_KEY_ARRAY_SIZE(thr_arr) }

/* Multi-click key without long press: unreachable cap plus the configured
 * clicks/window */
#define ELIB_KEY_KEY_CLICKS_ONLY(id, clicks, window_ms)                      \
    { .key_id = (id), .max_click = (clicks),                               \
      .multi_click_window_ms = (window_ms),                                  \
      .long_press_thresholds =                                             \
          (const uint16_t[]){ELIB_KEY_CLICK_ONLY_THRESHOLD},                  \
      .threshold_count = 1 }

/* Combo helpers. The combo id is always the FIRST parameter, matching the
 * per-key macros. Macro names equal the mode enum values; the function-like
 * macro only expands when called with parentheses, so the bare enum value
 * (e.g. in `.mode = ELIB_KEY_COMBO_SHORT`) stays usable.
 * "window" = the OVERALL gesture window in ms, measured from the first
 * participant's press: all presses (and, for LONG, all releases) must fit
 * inside it; 0 disables the constraint. */
#define ELIB_KEY_COMBO_SHORT(cid, keys_list, window)                         \
    { .mode = ELIB_KEY_COMBO_SHORT,                                        \
      .combo_keys = (keys_list),                                             \
      .combo_count = ELIB_KEY_ARRAY_SIZE(keys_list),                         \
      .window_ms = (window),                                                 \
      .combo_id = (cid) }

/* COMBO_LONG with configurable co-held windows (mirror of per-key long
 * press thresholds): the gesture claims the instant all participants are
 * pressed and the press span fits the overall window; each window crossed
 * while they stay held (and the overall window is alive) fires the optional
 * on_combo_hold callback, and releasing while the co-held time falls in a
 * window fires the combo. The LAST window is the cap: releasing past it
 * classifies invalid (nothing fires).
 * ELIB_KEY_COMBO_LONG(cid, keys_list, window, ...coheld_windows_ms) */
#define ELIB_KEY_COMBO_LONG(cid, keys_list, window, ...)                     \
    { .mode = ELIB_KEY_COMBO_LONG,                                         \
      .combo_keys = (keys_list),                                             \
      .combo_count = ELIB_KEY_ARRAY_SIZE(keys_list),                         \
      .long_press_thresholds = (const uint16_t[]){__VA_ARGS__},              \
      .threshold_count =                                                  \
          sizeof((const uint16_t[]){__VA_ARGS__}) / sizeof(uint16_t),        \
      .window_ms = (window),                                                 \
      .combo_id = (cid) }

/* COMBO_LONG reusing an existing, separately declared window array */
#define ELIB_KEY_COMBO_LONG_SIMPLE(cid, keys_list, window, win_arr)          \
    { .mode = ELIB_KEY_COMBO_LONG,                                         \
      .combo_keys = (keys_list),                                             \
      .combo_count = ELIB_KEY_ARRAY_SIZE(keys_list),                         \
      .long_press_thresholds = (win_arr),                                    \
      .threshold_count = ELIB_KEY_ARRAY_SIZE(win_arr),                       \
      .window_ms = (window),                                                 \
      .combo_id = (cid) }

/* Legacy binary COMBO_LONG: no windows (claim when all participants passed
 * their own first threshold; fire once when all released) */
#define ELIB_KEY_COMBO_LONG_BINARY(cid, keys_list, window)                   \
    { .mode = ELIB_KEY_COMBO_LONG,                                         \
      .combo_keys = (keys_list),                                             \
      .combo_count = ELIB_KEY_ARRAY_SIZE(keys_list),                         \
      .window_ms = (window),                                                 \
      .combo_id = (cid) }

/* HOLD_PRESS does not use the overall window (hold keys precede the press
 * keys by definition), so the macro omits it */
#define ELIB_KEY_COMBO_HOLD_PRESS(cid, hold_list, press_list)                \
    { .mode = ELIB_KEY_COMBO_HOLD_PRESS,                                   \
      .hold_keys = (hold_list),                                              \
      .hold_count = ELIB_KEY_ARRAY_SIZE(hold_list),                          \
      .press_keys = (press_list),                                            \
      .press_count = ELIB_KEY_ARRAY_SIZE(press_list),                        \
      .combo_id = (cid) }

/* Driver layer config: required callbacks only (optional ones stay NULL) */
#define ELIB_KEY_DRIVER_CFG(keys_arr, read_fn, event_cb)                     \
    { .keys = (keys_arr),                                                    \
      .key_count = ELIB_KEY_ARRAY_SIZE(keys_arr),                            \
      .read_level = (read_fn),                                               \
      .on_event = (event_cb) }

/* Driver layer config with all optional callbacks registered */
#define ELIB_KEY_DRIVER_CFG_FULL(keys_arr, read_fn, event_cb,               \
                                 press_cb, release_cb, hold_cb)              \
    { .keys = (keys_arr),                                                    \
      .key_count = ELIB_KEY_ARRAY_SIZE(keys_arr),                            \
      .read_level = (read_fn),                                               \
      .on_event = (event_cb),                                                \
      .on_press = (press_cb),                                                \
      .on_release = (release_cb),                                            \
      .on_long_hold = (hold_cb) }

/* Linkage layer config */
#define ELIB_KEY_LINKAGE_CFG(combos_arr, combo_cb)                           \
    { .combos = (combos_arr),                                                \
      .combo_count = ELIB_KEY_ARRAY_SIZE(combos_arr),                        \
      .on_combo = (combo_cb) }

#endif /* ELIB_KEY_MACROS_H */

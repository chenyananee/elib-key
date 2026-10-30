[简体中文](README.md) | **English**

# elib-key

An embedded key-detection library with zero dynamic memory allocation, built on a two-layer state machine architecture.

## Features

- **Short-press detection** - emits a short-press event after the key is released
- **Consecutive clicks** - configurable maximum click count (double-click, triple-click, etc.) with a timeout window
- **Long-press detection** - multiple long-press thresholds; triggered when the release falls inside a threshold interval
- **Multi-click and long press coexist** - the same key can be configured for both multi-click and long press; a long press terminates the sequence
- **Key combinations** - supports three modes: short-press combination, long-press combination, and long-press hold plus short press
- **Raw press/release callbacks** - optional (`NULL` allowed) immediate edge callbacks that are not suppressed by consumption or invalid classification
- **Long-press threshold callbacks** - fired each time a threshold level is crossed while held (`NULL` allowed), enabling staged interactions
- **Event consumption** - when a combination triggers, the driver events of the participating keys are automatically suppressed
- **Zero dynamic allocation** - purely static memory, suitable for resource-constrained embedded systems
- **C99 standard** - no external dependencies

## Quick Start

Minimal runnable single-click detection using only the required callback.

```c
#include "elib_key.h"

static elib_key_ctx_t ctx;

/* Hardware abstraction: returns the pin level, 1 = pressed, 0 = released */
static int read_pin(uint8_t key_id) {
    return HAL_GPIO_ReadPin(key_pins[key_id]);
}

/* Classified event callback (required): short presses and long presses are both reported here on release */
static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK) {
        printf("key %d short click (x%d)\n", key_id, click_count);
    } else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        printf("key %d long press (level %d)\n", key_id, threshold_idx);
    }
}

/* Single threshold {1000}: hold <1000ms = short press; >=1000ms = invalid (no long press).
 * See the long-press scenarios below if you need a long press */
static const uint16_t thresholds[] = {1000};
static const elib_key_item_cfg_t keys[] = {
    { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds, .threshold_count = 1 },
};

static elib_key_driver_cfg_t cfg = {
    .keys = keys, .key_count = 1,
    .read_level = read_pin,
    .on_event = on_event,
    /* .on_press / .on_release / .on_long_hold may be omitted (zero-initialized to NULL) */
};

int main(void) {
    elib_key_init(&ctx, &cfg);

    while (1) {
        elib_key_scan(&ctx, 10);   /* polling at a fixed ~10ms interval is recommended */
        HAL_Delay(10);
    }
}
```

## Configuration Shortcuts

`elib_key_macros.h` provides a set of configuration template macros: **all array element counts are derived automatically** (no miscounting thresholds or keys), and each configuration item takes one line. Use them at file scope (`static const` global tables), because `ELIB_KEY_KEY` embeds a C99 file-scope compound literal:

```c
#include "elib_key.h"   /* already includes elib_key_macros.h */

/* ---- Single-key entries ---- */
static const elib_key_item_cfg_t keys[] = {
    ELIB_KEY_KEY(0, 2, 300, 1000, 3000),   /* id=0, double-click, 300ms window, long-press levels */
    ELIB_KEY_KEY(1, 1, 0,   500),          /* id=1, single click + one long-press level */
    ELIB_KEY_KEY_CLICKS_ONLY(2, 3, 300),   /* id=2, pure triple-click, no long press (large fallback level) */
    ELIB_KEY_KEY_SIMPLE(3, thr_arr3),      /* reuse an already declared threshold array */
    ELIB_KEY_KEY_CLICKS(4, 2, 300, thr_arr4),
};

/* ---- Combination entries ---- */
static const uint8_t keys_ab[] = {0, 1};
static const uint8_t keys_a[]  = {0};
static const uint8_t keys_b[]  = {1};
static const elib_key_combo_cfg_t combos[] = {
    ELIB_KEY_COMBO_SHORT(0, keys_ab, 500),
    /* AB long-press combination: total gesture window 12000 (both keys pressed and released
     * within 12000ms of the first key press); shared-hold windows {1000, 3000}, last level 3000
     * as the cap (holding longer invalid); use _LONG_BINARY when no shared-hold window is needed */
    ELIB_KEY_COMBO_LONG(1, keys_ab, 12000, 1000, 3000),
    /* Windows can also reuse an already declared array (window count derived automatically) */
    /* ELIB_KEY_COMBO_LONG_SIMPLE(1, keys_ab, 12000, ab_windows), */
    ELIB_KEY_COMBO_HOLD_PRESS(2, keys_a, keys_b),  /* no window_ms parameter */
};

/* ---- Top-level configuration ---- */
static const elib_key_driver_cfg_t drv =
    ELIB_KEY_DRIVER_CFG(keys, read_pin, on_event);          /* required callbacks only */
    /* Full-callback variant: ELIB_KEY_DRIVER_CFG_FULL(keys, read_pin, on_event,
     *                                                on_press, on_release, on_long_hold) */
static const elib_key_linkage_cfg_t link =
    ELIB_KEY_LINKAGE_CFG(combos, on_combo);
```

| Macro | Parameters | Description |
|----|------|------|
| `ELIB_KEY_KEY` | `(id, clicks, window_ms, ...thresholds)` | Inline variadic thresholds; the last level is the cap, so use a large value (for example `60000`) for pure multi-click |
| `ELIB_KEY_KEY_CLICKS_ONLY` | `(id, clicks, window_ms)` | No long-press intent; automatically uses an unreachable fallback level `{65535}` |
| `ELIB_KEY_KEY_SIMPLE` | `(id, thr_arr)` | Single-click key; reuses an already declared threshold array |
| `ELIB_KEY_KEY_CLICKS` | `(id, clicks, window_ms, thr_arr)` | Multi-click key; reuses an already declared threshold array |
| `ELIB_KEY_COMBO_SHORT` | `(id, keys_arr, total_gesture_window)` | `combo_count` is derived automatically |
| `ELIB_KEY_COMBO_LONG` | `(id, keys_arr, total_gesture_window, ...shared_hold_window)` | Inline variadic windows; each window fires `on_combo_hold` when reached; the last level is the cap |
| `ELIB_KEY_COMBO_LONG_SIMPLE` | `(id, keys_arr, total_gesture_window, win_arr)` | Reuses an already declared window array |
| `ELIB_KEY_COMBO_LONG_BINARY` | `(id, keys_arr, total_gesture_window)` | Binary long-press combination without shared-hold windows (see below) |
| `ELIB_KEY_COMBO_HOLD_PRESS` | `(id, hold_arr, press_arr)` | Counts for both tables are derived automatically |
| `ELIB_KEY_DRIVER_CFG(_FULL)` | ... | Top-level driver configuration; `key_count` is derived automatically |
| `ELIB_KEY_LINKAGE_CFG` | `(combos_arr, combo_cb)` | `combo_count` is derived automatically |

Limitations and caveats:

- **File scope only** (global/static tables); they cannot be built dynamically inside a function (a compound literal at block scope has stack lifetime)
- Variadic thresholds passed to `ELIB_KEY_KEY` must be **ascending**, the same constraint as the raw form
- The combination macros share names with enum values (`ELIB_KEY_COMBO_SHORT` is both an enum value and a function-like macro): a function-like macro expands only when called with parentheses, so `.mode = ELIB_KEY_COMBO_SHORT` remains an enum value without interference
- The single-key scenarios section keeps the raw struct form to make each field's meaning clear; the full example and real projects should use the macros directly

## Events and Callbacks

| Callback | Required | Trigger time |
|------|:----:|----------|
| `on_event(key_id, event, clicks, level)` | Yes | Classification result after release: `SHORT_CLICK` / `LONG_PRESS` |
| `on_press(key_id)` | No | Press edge, fired as soon as it is seen by a scan (raw edge) |
| `on_release(key_id)` | No | Release edge (raw edge; also fired for invalid releases or when consumed by a combination) |
| `on_long_hold(key_id, level)` | No | Each threshold level crossed while held (on-time, staged); silent during a declared combination |
| `on_combo(combo_id, mode)` | No | Combination matched/released (linkage layer) |
| `on_combo_hold(combo_id, level)` | No | Each shared-hold window crossed for a windowed long-press combination (on-time, staged); structures generated by `ELIB_KEY_LINKAGE_CFG` need `.on_combo_hold = ...` added manually |

All callbacks may be `NULL`; calling this library again from inside a callback is **forbidden** (non-reentrant).

## Single-Key Scenarios

### Short Press + Multi-Click (Double-Click/Triple-Click...)

```c
/* 10000ms is only a fallback: a pure multi-click key usually needs no long press, so just use a large threshold */
static const uint16_t thresholds[] = {10000};
static const elib_key_item_cfg_t keys[] = {
    { .key_id = 0, .max_click = 3, .multi_click_window_ms = 300,
      .long_press_thresholds = thresholds, .threshold_count = 1 },
};

static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK) {
        switch (click_count) {
        case 1: do_open_menu(); break;   /* single click */
        case 2: do_select();    break;   /* double click */
        case 3: do_back();      break;   /* triple click */
        }
    }
}
```

Behavior: after each click is released, a 300ms wait window collects further clicks; once `max_click` (3) clicks are collected the event fires immediately; when the window times out, the event fires with the actual click count (stop after a double-click -> `SHORT_CLICK(2)`).

### Multi-Click and Long Press Coexist (Same Key)

```c
/* Long-press interval [1000, 3000); 3000ms and above is invalid (the last level is the cap) */
static const uint16_t thresholds[] = {1000, 3000};
static const elib_key_item_cfg_t keys[] = {
    { .key_id = 0, .max_click = 2, .multi_click_window_ms = 300,
      .long_press_thresholds = thresholds, .threshold_count = 2 },
};

static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK) {
        if (click_count == 1) toggle_light();
        else if (click_count == 2) next_color();
    } else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        enter_setting_mode();      /* held for 1-3 seconds */
    }
}
```

Rule: if any release falls inside a long-press interval, `LONG_PRESS` is emitted immediately and **any clicks still awaiting classification are discarded** (tap once -> hold -> only a long press is reported, never a short press first). A hold longer than the last level (3000ms) is invalid and produces no event; if you want "any duration counts as a long press", set the last level to a large value such as `{1000, 3000, 60000}`.

### Multi-Window Long Press (Release-Classified, One Report per Press)

```c
/* <1000 short press; [1000,3000) level 0; [3000,5000) level 1; >=5000 invalid */
static const uint16_t thresholds[] = {1000, 3000, 5000};

/* on release, threshold_idx tells you which level was reached */
static void on_event(...
    else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        if (threshold_idx == 0) open_quick_menu();
        else if (threshold_idx == 1) open_full_menu();
    }
```

### Long Press Threshold Callbacks (Hold Without Releasing, Staged Interaction)

`on_long_hold` fires at the moment the hold time crosses each threshold; there is no need to wait for release:

```c
 thresholds {1000, 3000, 5000}

 /* An interaction such as "hold for 10 seconds to force power-off", where the action escalates the longer you hold */
static void on_long_hold(uint8_t key_id, uint8_t threshold_idx) {
    switch (threshold_idx) {
    case 0: show_progress_1s();   break;   /* 1s: start the prompt */
    case 1: show_progress_3s();   break;   /* 3s: strengthen the prompt */
    case 2: show_progress_5s();   break;   /* 5s: start the countdown */
    }
    /* You can even implement staged long-press speed control on an e-bike, escalating with each additional level */
}
```

Note: crossing the **last level** also fires the callback (even if the release is subsequently judged invalid); it coexists with the release-time `LONG_PRESS` event, and you can use either or both as needed.

### Raw Press/Release Callbacks

Immediate notifications for press/release, commonly used for backlights, indicator LEDs, or gestures that pair the interval between a long press and its release:

```c
static void on_press(uint8_t key_id) {
    backlight_on(key_id);          /* light up as soon as the finger touches, without waiting for classification */
}

static void on_release(uint8_t key_id) {
    backlight_off(key_id);
    /* Note: on_release fires regardless of whether this release ends up as a short press, a long press, or invalid */
}

cfg.on_press = on_press;
cfg.on_release = on_release;
```

## Key Combinations (Optional Linkage Layer)

Combinations require an extra linkage-layer initialization; without it the keys still work (including single-gesture mutual-exclusion arbitration, at the cost of only a few bit tests per scan):

```c
static const uint8_t combo_keys_ab[] = {0, 1};
static const uint8_t combo_keys_a[]  = {0};
static const uint8_t combo_keys_b[]  = {1};

static const elib_key_combo_cfg_t combos[] = {
    /* Priority = configuration array order; first match wins */
    { .mode = ELIB_KEY_COMBO_SHORT,       /* both A and B are a "short press" */
      .combo_keys = combo_keys_ab, .combo_count = 2,
      .window_ms = 500, .combo_id = 0 },
    { .mode = ELIB_KEY_COMBO_HOLD_PRESS,  /* hold A + short-press B */
      .hold_keys = combo_keys_a,  .hold_count = 1,
      .press_keys = combo_keys_b, .press_count = 1,
      .combo_id = 1 },
};

static void on_combo(uint8_t combo_id, elib_key_combo_mode_t mode) {
    if (combo_id == 0) lock_device();         /* A+B pressed together */
    else if (combo_id == 1) capture_screenshot();  /* hold A + tap B */
}

static const elib_key_linkage_cfg_t link = {
    .combos = combos, .combo_count = 2, .on_combo = on_combo,
};

elib_key_init(&ctx, &cfg);                 /* see "Quick Start" for cfg */
elib_key_linkage_init(&ctx, &link);        /* call after elib_key_init */
```

Three modes and their trigger conditions (**gesture mutual-exclusion semantics** are described below):

| Mode | Trigger condition | Usage notes |
|------|----------|----------|
| `COMBO_SHORT` | All participating keys have a **pending short press present at the same time**: everyone releases in the same scan, or a key is already inside its own multi-click wait window; and **all presses occurred within the total gesture window** (a timeout only makes this combination fail to match; each key's own gesture continues as usual) | To tolerate presses in sequence, configure the participating keys with `multi_click_window_ms > 0` (100-300 recommended) |
| `COMBO_LONG` (with windows) | Declared at the moment **all keys are pressed** (and the press span is within the total gesture window); that moment is both the shared-hold timing origin and the total-window countdown origin. Each shared-hold window crossed -> `on_combo_hold(combo_id, level)`. If **all keys release** while still inside the total window, classify by shared-hold duration: inside a window -> trigger the combination; **at or beyond the last level, or total window expired** -> invalid, emit nothing | Exactly isomorphic to single-key long press (multiple windows + on-time callbacks + release classification); after declaration, the single-key output of every participating key is cancelled for the whole gesture (no replay) |
| `COMBO_LONG` (without windows) | All participating keys are held and each has passed its first threshold -> declared; triggers when **all keys release** while still inside the total window (binary, no levels) | Legacy compatibility behavior; the declaration point is later than the press point, so the total gesture window must be larger than the first threshold (when you only want to constrain release, not the press span, the simplest option is `window_ms = 0` to disable the total window) |
| `COMBO_HOLD_PRESS` | All `hold_keys` are held past their first thresholds and a short press on `press_keys` is captured; **the total gesture window does not apply** (hold keys are pressed before press keys by definition) | After triggering, the hold keys stay silent until all keys are released (their long/short presses cannot fire accidentally) |

Note: the `window_ms` field is the **total gesture window**: timing starts when the first participating key is pressed, and all participating keys must both press and (for `COMBO_LONG`) release within this window; `window_ms == 0` disables the total-window constraint. A combination that exceeds the window merely **loses match eligibility**, and gestures in the earlier key region continue as usual; priority = configuration array order, first match locks first.

### Windowed Long-Press Combination Details

Fully isomorphic to single-key long press (`on_long_hold` + release classification), except that the timed quantity changes from a single key's hold time to the **shared-hold duration**; in addition, `window_ms` defines the **total gesture window** (timed from the first participating key press):

- **Press window**: all keys must be pressed inside the total gesture window (the preceding span is checked at the moment the last participating key is pressed); on timeout -> the combination is not declared, the single-key gesture of the earlier key **is not hijacked**, and the late key is ignored like an invalid key
- **Origin**: the instant the last participating key is pressed (the moment all keys are down). How long the earlier keys were held alone **does not count** toward the shared-hold time; when the last key is pressed, shared timing starts from 0
- **On-time**: each shared-hold window crossed (the `long_press_thresholds` array) fires `on_combo_hold(combo_id, level)` once (level starts at 0); add the optional `.on_combo_hold` to `linkage_cfg`
- **Release window**: all keys must also be released inside the total gesture window (measured from the first participating key press); once the window is exceeded the gesture is **condemned**: all on-time callbacks go silent, the declaration is held until every key is released, and **nothing is emitted** on settlement
- **Release classification**: when all keys are released and the total window is still alive, classify by **shared-hold duration**: inside `[window k, window k+1)` -> `on_combo(combo_id, COMBO_LONG)`; **at or beyond the last window -> invalid** (neither the combination nor any participating key's single-key output is emitted)
- **Participating-key cancellation** is the same as in the binary variant: from the moment of declaration, every participating key's single-key long-press classification and `on_long_hold` escalation go silent, with **no replay** (to get a single-key action you must release everything and press again)

Example (A long press resets the machine, AB long press enters factory mode, total gesture window 12000, shared-hold windows {3000, 5000}):

```
A press ───────► B press ● origin starts here (shared hold = 0)
               │
               ├ shared hold 3000 → on_combo_hold(combo, 0)
               │
A releases first ──────┤ (timing continues until the last person releases)
               │
B releases ────────┤ shared hold falls in [3000, 5000) → on_combo triggers factory mode
```

The phase where A is held alone (before B joins): A's own on-time callbacks fire as usual; after B joins and the combination is entered, A produces no further single-key output.

## Gesture Mutual Exclusion (Single-Channel Semantics)

The library enforces that **only one gesture exists at a time** and only one output occurs at any moment (the result of a single key, or one combination); the two never happen simultaneously:

- **Mutual exclusion**: while a gesture is in progress (a key held / multi-click waiting / a combination declared), a newly pressed key has only two paths: it can form a combination with the current key set **and its press span is within that combination's total gesture window** -> the gesture **upgrades to that combination**, after which all participating keys' single-key results (short press / multi-click / long press / escalation callbacks) are cancelled; no combination matches (or the window is exceeded) -> the key is **ignored** (its release produces no classification event either, but the gesture in progress is unaffected)
- **Keys pressed in the same scan with no matching combination**: all are invalid (better to emit nothing)
- **Changing your mind after declaration**: to get a single-key action, release everything and press the key again on its own
- The raw `on_press`/`on_release` callbacks are **unclassified observers**; they fire normally at all times and are not suppressed by the rules above

Safety example (A long press resets the machine, B long press resets the filter, A+B long press enters factory mode): if your hand slips and touches B while holding A -> B is ignored and A's reset executes as usual; if both keys are held and both pass their first threshold -> the combination is declared, A/B single-key actions are fully cancelled, and releasing everything reports factory mode exactly once.

Additional notes:

- After a combination is declared/triggered, participating keys' **classification events are consumed** (not reported again); the raw `on_press`/`on_release` callbacks are unaffected
- Combinations can **trigger repeatedly**: the channel is released after a trigger and can match again
- **Ignored keys**: classification events and their `on_long_hold` escalation callbacks stay silent until the key is released; only the next press can begin a new gesture

## Complete Example (Two Keys, with Combinations and All Callbacks)

```c
#include "elib_key.h"
#include <stdio.h>

static elib_key_ctx_t ctx;

static int read_pin(uint8_t key_id) {
    /* replace with the project's actual HAL read */
    return HAL_GPIO_ReadPin(KEY_A_PIN + key_id);
}

static void on_press(uint8_t key_id)  { backlight_on(key_id);  }
static void on_release(uint8_t key_id){ backlight_off(key_id); }

static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK && key_id == 0) {
        if (click_count == 1)      toggle_led();
        else if (click_count >= 2) run_selftest();
    } else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        save_settings();
    }
}

static void on_long_hold(uint8_t key_id, uint8_t threshold_idx) {
    if (key_id == 0 && threshold_idx >= 1) {
        show_force_reset_warning();      /* prompt for reset after holding for more than 3s */
    }
}

static void on_combo(uint8_t combo_id, elib_key_combo_mode_t mode) {
    if (combo_id == 0) enter_factory_mode();
    else if (combo_id == 1) restore_defaults();
}

/* Key A: multi-click and long press coexist; key B: single click only */
static const elib_key_item_cfg_t keys[] = {
    ELIB_KEY_KEY(0, 2, 300, 1000, 3000),
    ELIB_KEY_KEY(1, 1, 0,   1000),
};

static const uint8_t combo_keys_ab[] = {0, 1};
static const uint8_t combo_keys_a[]  = {0};
static const uint8_t combo_keys_b[]  = {1};
static const elib_key_combo_cfg_t combos[] = {
    ELIB_KEY_COMBO_SHORT(0, combo_keys_ab, 500),
    ELIB_KEY_COMBO_HOLD_PRESS(1, combo_keys_a, combo_keys_b),
};

static const elib_key_driver_cfg_t drv =
    ELIB_KEY_DRIVER_CFG_FULL(keys, read_pin, on_event,
                             on_press, on_release, on_long_hold);

static const elib_key_linkage_cfg_t link =
    ELIB_KEY_LINKAGE_CFG(combos, on_combo);

int main(void) {
    elib_key_init(&ctx, &drv);
    elib_key_linkage_init(&ctx, &link);

    while (1) {
        elib_key_scan(&ctx, 10);
        HAL_Delay(10);       /* platform-specific delay */
    }
}
```

## Behavior Details and Caveats

- **Event model**: short-press/long-press/all classification events fire **on release**; `on_press`/`on_release`/`on_long_hold` are the only immediate callbacks
- **Long-press cap at the last level**: a hold >= `thresholds[threshold_count-1]` is invalid. To get unbounded escalation without the last-level cutoff, use `on_long_hold` or configure a large fallback level
- **No debouncing**: this library does not handle mechanical bounce. Bounce (typically 5-20ms) produces extra press/release pairs and even false multi-clicks; debounce at the hardware level (RC filtering) or in the read callback
- **Gesture mutual exclusion is a hard constraint**: only one gesture can exist at a time (see the "Gesture Mutual Exclusion" section); applications that rely on concurrent independent keys are not a good fit for this library
- **All keys released = full reset**: regardless of what happened before (combination declared, declaration failed, invalid result, key ignored), once all keys are released and one scan has run, the library state is completely clean (claimed/follower/owner/gesture slots all zeroed); while a key is mechanically stuck, other keys stay mutually excluded, and the whole system self-heals as soon as that key is released
- **Clicks during a declaration do not leak out**: rapid clicks on participating keys while a gesture is declared are discarded wholesale and never turn into `SHORT_CLICK` events in a later multi-click wait window
- **Setting the last window level to `65535` = release classification never triggers**: `on_combo_hold` on-time callbacks still work, but release always falls into the "beyond the last level, invalid" branch, equivalent to pure on-time usage
- **Timing capacity**: single-key timing uses `uint16_t`, so hold/interval limits are about 65.5 seconds; a single `elapsed_ms` is at most 65535, so keep the polling period well below that
- **Configuration arrays**: all configuration can live in `const`/flash; threshold arrays must be ascending and contain 1 to `ELIB_KEY_MAX_THRESHOLDS` entries
- **Detailed behavior spec**: `docs/superpowers/specs/2026-05-12-elib-key-design.md`

## Compile-Time Limits

Define the following macros before including the header to adjust the limits:

```c
#define ELIB_KEY_MAX_KEYS        16  /* maximum number of keys */
#define ELIB_KEY_MAX_COMBOS      8   /* maximum number of key combinations */
#define ELIB_KEY_MAX_THRESHOLDS  4   /* maximum long-press thresholds per key */
#define ELIB_KEY_MAX_COMBO_KEYS  4   /* maximum participating keys per combination */
```

## Directory Layout

```
elib-key/
├── include/
│   ├── elib_key.h          # public API header (umbrella)
│   ├── elib_key_err.h      # error code definitions
│   ├── elib_key_macros.h   # configuration shortcut macros
│   └── elib_key_types.h    # type definitions and compile-time limits
├── src/
│   ├── elib_key_core.h     # internal header
│   ├── elib_key_core.c     # driver-layer implementation (single-key state machine)
│   └── elib_key_linkage.c  # linkage-layer implementation (combination detection)
├── test/
│   └── test_elib_key.c     # unit tests
├── docs/                   # design specs and implementation plans
├── scripts/                # build and deployment scripts
├── LICENSE
└── README.md
```

## Building and Testing

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o test_elib_key test/test_elib_key.c src/elib_key_core.c src/elib_key_linkage.c && ./test_elib_key
```

To integrate into a project, copy the 7 files under `include/` and `src/`; there are no external dependencies.

## License

MIT License

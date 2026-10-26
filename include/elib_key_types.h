/* elib_key_types.h - Key Detection Type Definitions */
#ifndef ELIB_KEY_TYPES_H
#define ELIB_KEY_TYPES_H

#include <stdint.h>
#include "elib_key_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compile-time limits (user can override before include) */
#ifndef ELIB_KEY_MAX_KEYS
#define ELIB_KEY_MAX_KEYS           16
#endif

#ifndef ELIB_KEY_MAX_COMBOS
#define ELIB_KEY_MAX_COMBOS         8
#endif

#ifndef ELIB_KEY_MAX_THRESHOLDS
#define ELIB_KEY_MAX_THRESHOLDS     4
#endif

#ifndef ELIB_KEY_MAX_COMBO_KEYS
#define ELIB_KEY_MAX_COMBO_KEYS     4
#endif

/* Driver layer state machine states */
typedef enum {
    ELIB_KEY_STATE_IDLE = 0,
    ELIB_KEY_STATE_PRESSED,
    ELIB_KEY_STATE_MULTI_CLICK_WAIT,
} elib_key_state_e;

/* Event types (all fire on release) */
typedef enum {
    ELIB_KEY_EVENT_SHORT_CLICK = 0,
    ELIB_KEY_EVENT_LONG_PRESS,
} elib_key_event_t;

/* Combo modes */
typedef enum {
    ELIB_KEY_COMBO_SHORT = 0,
    ELIB_KEY_COMBO_LONG,
    ELIB_KEY_COMBO_HOLD_PRESS,
} elib_key_combo_mode_t;

/* Callbacks (raw edges and long-hold levels are user-registrable, may be NULL) */
typedef int  (*elib_key_read_level_fn)(uint8_t key_id);
typedef void (*elib_key_event_cb)(uint8_t key_id, elib_key_event_t event,
                                  uint8_t click_count, uint8_t threshold_idx);
typedef void (*elib_key_combo_cb)(uint8_t combo_id, elib_key_combo_mode_t mode);
/* Raw rising edge (key pressed), fires before classified events */
typedef void (*elib_key_press_cb)(uint8_t key_id);
/* Raw falling edge (key released), fires before classified events; not
 * suppressed by consumption or invalid classification */
typedef void (*elib_key_release_cb)(uint8_t key_id);
/* Fires each time held duration crosses a long-press threshold while the
 * key remains pressed (escalation notifications) */
typedef void (*elib_key_long_hold_fn)(uint8_t key_id, uint8_t threshold_idx);
/* Windowed COMBO_LONG escalation: fires each time the co-held time (since
 * all participants were pressed) crosses a combo window while they stay held */
typedef void (*elib_key_combo_hold_fn)(uint8_t combo_id, uint8_t threshold_idx);

/* Per-key internal state */
typedef struct {
    elib_key_state_e state;
    uint8_t  click_count;
    uint8_t  hold_notified_idx; /* long-hold levels already notified */
    uint16_t hold_ms;
    uint16_t idle_ms;
    struct {
        uint16_t last_level : 1;
        uint16_t consumed   : 1;
        uint16_t owner      : 1; /* owns the single gesture channel */
        uint16_t follower   : 1; /* joined while another gesture owned the
                                      channel; classification discarded at
                                      release unless a combo claims it */
        uint16_t claimed    : 1; /* member of a locked combo */
        uint16_t fresh      : 1; /* rose in the current scan */
        uint16_t fell       : 1; /* fell in the current scan */
    } bit_flags;
} elib_key_state_t;

/* Pending driver event (before linkage filtering) */
typedef struct {
    uint8_t  key_id;
    uint8_t  click_count;
    uint8_t  threshold_idx;
    struct {
        uint8_t event : 1;
        uint8_t valid : 1;
    } bit_flags;
} elib_key_pending_event_t;

/* Per-key configuration */
typedef struct {
    uint8_t  key_id;
    uint8_t  max_click;
    uint16_t multi_click_window_ms;
    const uint16_t *long_press_thresholds;
    uint8_t  threshold_count;
} elib_key_item_cfg_t;

/* Driver layer configuration */
typedef struct {
    const elib_key_item_cfg_t *keys;
    uint8_t                    key_count;
    elib_key_read_level_fn     read_level;
    elib_key_event_cb          on_event;       /* required */
    elib_key_press_cb          on_press;       /* optional, may be NULL */
    elib_key_release_cb        on_release;     /* optional, may be NULL */
    elib_key_long_hold_fn      on_long_hold;   /* optional, may be NULL */
} elib_key_driver_cfg_t;

/* Combo configuration */
typedef struct {
    elib_key_combo_mode_t mode;
    const uint8_t *hold_keys;
    uint8_t        hold_count;
    const uint8_t *press_keys;
    uint8_t        press_count;
    const uint8_t *combo_keys;
    uint8_t        combo_count;
    /* COMBO_LONG windows: the gesture claims the instant every participant
     * is pressed (that moment is the timing origin); each window crossed
     * while all stay held fires the escalation callback, and the release
     * classifies by which window the co-held time fell into. The last
     * window is the cap: releasing past it classifies the gesture invalid
     * (nothing fires). NULL/0 = legacy binary behavior: claim when every
     * participant has passed its own first threshold, fire once when all
     * released, no windows */
    const uint16_t *long_press_thresholds;
    uint8_t         threshold_count;
    uint16_t       window_ms;      /* deprecated: not used by matching */
    uint8_t        combo_id;
} elib_key_combo_cfg_t;

/* Linkage layer configuration */
typedef struct {
    const elib_key_combo_cfg_t *combos;
    uint8_t                     combo_count;
    elib_key_combo_cb           on_combo;      /* required */
    elib_key_combo_hold_fn      on_combo_hold; /* optional, may be NULL; used
                                                * by windowed COMBO_LONG */
} elib_key_linkage_cfg_t;

/* Single active gesture state: by design at most one gesture exists in the
 * library at any time (single-gesture channel model) */
typedef struct {
    uint8_t locked_combo;      /* index into linkage combos while locked */
    uint16_t hold_ms;          /* locked windowed LONG: co-held time since
                                    all participants were pressed */
    uint8_t  notified_idx;     /* combo-hold escalation levels already
                                    reported for this gesture */
    struct {
        uint8_t locked : 1;    /* a combo owns the gesture channel */
        uint8_t fired  : 1;    /* combo already reported; HOLD_PRESS fires at
                                    match, LONG fires when all released */
    } bit_flags;
} elib_key_gesture_t;

/* Main context (user-allocated) */
typedef struct {
    /* Driver layer */
    elib_key_state_t         key_states[ELIB_KEY_MAX_KEYS];
    elib_key_pending_event_t pending[ELIB_KEY_MAX_KEYS];
    uint8_t                  pending_count;
    const elib_key_driver_cfg_t *driver_cfg;

    /* Linkage layer */
    elib_key_gesture_t       gesture;
    const elib_key_linkage_cfg_t *linkage_cfg;

    struct {
        uint8_t initialized         : 1;
        uint8_t linkage_initialized : 1;
    } bit_flags;
} elib_key_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_KEY_TYPES_H */

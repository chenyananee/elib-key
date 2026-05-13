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

/* Callbacks */
typedef int  (*elib_key_read_level_fn)(uint8_t key_id);
typedef void (*elib_key_event_cb)(uint8_t key_id, elib_key_event_t event,
                                  uint8_t click_count, uint8_t threshold_idx);
typedef void (*elib_key_combo_cb)(uint8_t combo_id, elib_key_combo_mode_t mode);

/* Per-key internal state */
typedef struct {
    elib_key_state_e state;
    uint8_t  click_count;
    uint16_t hold_ms;
    uint16_t idle_ms;
    uint8_t  last_level;
    uint8_t  consumed;
} elib_key_state_t;

/* Pending driver event (before linkage filtering) */
typedef struct {
    uint8_t  key_id;
    elib_key_event_t event;
    uint8_t  click_count;
    uint8_t  threshold_idx;
    uint8_t  valid;
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
    elib_key_event_cb          on_event;
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
    uint16_t       window_ms;
    uint8_t        combo_id;
} elib_key_combo_cfg_t;

/* Linkage layer configuration */
typedef struct {
    const elib_key_combo_cfg_t *combos;
    uint8_t                     combo_count;
    elib_key_combo_cb           on_combo;
} elib_key_linkage_cfg_t;

/* Per-combo runtime state */
typedef struct {
    uint8_t  triggered;
    uint16_t first_event_ms;
    uint8_t  key_satisfied[ELIB_KEY_MAX_COMBO_KEYS];
} elib_key_combo_state_t;

/* Main context (user-allocated) */
typedef struct {
    /* Driver layer */
    elib_key_state_t         key_states[ELIB_KEY_MAX_KEYS];
    elib_key_pending_event_t pending[ELIB_KEY_MAX_KEYS];
    uint8_t                  pending_count;
    const elib_key_driver_cfg_t *driver_cfg;
    int                      initialized;

    /* Linkage layer */
    elib_key_combo_state_t   combo_states[ELIB_KEY_MAX_COMBOS];
    const elib_key_linkage_cfg_t *linkage_cfg;
    int                      linkage_initialized;
} elib_key_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_KEY_TYPES_H */

/* elib_key_core.c - Key Detection Core Implementation */
#include "elib_key_core.h"
#include <string.h>

/* Initialize key detection driver layer */
elib_key_err_t elib_key_init(elib_key_ctx_t *ctx, const elib_key_driver_cfg_t *cfg) {
    if (ctx == NULL || cfg == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (cfg->keys == NULL || cfg->read_level == NULL || cfg->on_event == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (cfg->key_count == 0 || cfg->key_count > ELIB_KEY_MAX_KEYS) {
        return ELIB_KEY_ERR_TOO_MANY_KEYS;
    }

    /* Validate per-key config before touching ctx so failures leave it intact */
    for (uint8_t i = 0; i < cfg->key_count; i++) {
        if (cfg->keys[i].long_press_thresholds == NULL ||
            cfg->keys[i].threshold_count == 0 ||
            cfg->keys[i].threshold_count > ELIB_KEY_MAX_THRESHOLDS) {
            return ELIB_KEY_ERR_INVALID_PARAM;
        }
    }

    memset(ctx, 0, sizeof(elib_key_ctx_t));

    ctx->driver_cfg = cfg;
    for (uint8_t i = 0; i < cfg->key_count; i++) {
        ctx->key_states[i].state = ELIB_KEY_STATE_IDLE;
        ctx->key_states[i].bit_flags.last_level = (uint8_t)cfg->read_level(cfg->keys[i].key_id);
    }
    ctx->bit_flags.initialized = 1;

    return ELIB_KEY_OK;
}

/* Deinitialize key detection */
elib_key_err_t elib_key_deinit(elib_key_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    ctx->bit_flags.initialized = 0;
    ctx->bit_flags.linkage_initialized = 0;
    return ELIB_KEY_OK;
}

/* Find key config by key_id, return index or -1 */
int elib_key_find_key_index(const elib_key_driver_cfg_t *cfg, uint8_t key_id) {
    for (uint8_t i = 0; i < cfg->key_count; i++) {
        if (cfg->keys[i].key_id == key_id) {
            return (int)i;
        }
    }
    return -1;
}

/* Queue a pending event for the linkage layer; consumed events are filtered
 * at emission time */
static void elib_key_push_pending(elib_key_ctx_t *ctx, uint8_t key_id,
                                  uint8_t event, uint8_t click_count,
                                  uint8_t threshold_idx) {
    elib_key_pending_event_t *pe = &ctx->pending[ctx->pending_count];
    pe->key_id = key_id;
    pe->bit_flags.event = event;
    pe->bit_flags.valid = 1;
    pe->click_count = click_count;
    pe->threshold_idx = threshold_idx;
    ctx->pending_count++;
}

/* Scan all keys and process linkage layer */
elib_key_err_t elib_key_scan(elib_key_ctx_t *ctx, uint32_t elapsed_ms) {
    if (ctx == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_KEY_ERR_NOT_INITIALIZED;
    }

    const elib_key_driver_cfg_t *drv = ctx->driver_cfg;
    ctx->pending_count = 0;

    for (uint8_t i = 0; i < drv->key_count; i++) {
        const elib_key_item_cfg_t *cfg = &drv->keys[i];
        elib_key_state_t *ks = &ctx->key_states[i];

        uint8_t level = (uint8_t)drv->read_level(cfg->key_id);
        uint8_t rising  = (level == 1 && ks->bit_flags.last_level == 0);
        uint8_t falling = (level == 0 && ks->bit_flags.last_level == 1);
        ks->bit_flags.last_level = level;

        /* Raw edge callbacks (unclassified, fire immediately) */
        if (rising && drv->on_press != NULL) {
            drv->on_press(cfg->key_id);
        }
        if (falling && drv->on_release != NULL) {
            drv->on_release(cfg->key_id);
        }

        switch (ks->state) {
        case ELIB_KEY_STATE_IDLE:
            ks->idle_ms = elib_key_add_ms(ks->idle_ms, elapsed_ms);
            if (rising) {
                ks->hold_ms = 0;
                ks->hold_notified_idx = 0;
                ks->bit_flags.consumed = 0;
                ks->state = ELIB_KEY_STATE_PRESSED;
            }
            break;

        case ELIB_KEY_STATE_PRESSED:
            ks->hold_ms = elib_key_add_ms(ks->hold_ms, elapsed_ms);

            /* Long-hold notifications for thresholds crossed while held */
            if (drv->on_long_hold != NULL) {
                uint8_t crossed = 0;
                for (uint8_t t = 0; t < cfg->threshold_count; t++) {
                    if (ks->hold_ms < cfg->long_press_thresholds[t]) {
                        break;
                    }
                    crossed = (uint8_t)(t + 1);
                }
                if (crossed > ks->hold_notified_idx) {
                    for (uint8_t n = (uint8_t)(ks->hold_notified_idx + 1);
                         n <= crossed;
                         n++) {
                        drv->on_long_hold(cfg->key_id, (uint8_t)(n - 1));
                    }
                    ks->hold_notified_idx = crossed;
                }
            }

            if (falling) {
                /* Three-tier release judgment (design spec 4.3):
                 * 1. hold >= max threshold: invalid, discard sequence
                 * 2. hold in [thr[0], max): LONG_PRESS, terminates sequence
                 * 3. hold < thr[0]: SHORT_CLICK, per max_click behavior */
                uint16_t max_thr = cfg->long_press_thresholds[cfg->threshold_count - 1];
                if (ks->hold_ms >= max_thr) {
                    /* Invalid: exceeded max threshold, discard sequence */
                    ks->click_count = 0;
                    ks->state = ELIB_KEY_STATE_IDLE;
                } else if (ks->hold_ms >= cfg->long_press_thresholds[0]) {
                    /* Long press: emit its threshold level; terminates any
                     * click sequence and discards accumulated clicks */
                    uint8_t threshold_idx = 0;
                    for (uint8_t t = 1; t < cfg->threshold_count; t++) {
                        if (ks->hold_ms < cfg->long_press_thresholds[t]) {
                            break;
                        }
                        threshold_idx = t;
                    }
                    elib_key_push_pending(ctx, cfg->key_id,
                                          (uint8_t)ELIB_KEY_EVENT_LONG_PRESS,
                                          0, threshold_idx);
                    ks->click_count = 0;
                    ks->state = ELIB_KEY_STATE_IDLE;
                } else if (cfg->max_click == 1) {
                    /* Single-click key: emit SHORT_CLICK immediately */
                    elib_key_push_pending(ctx, cfg->key_id,
                                          (uint8_t)ELIB_KEY_EVENT_SHORT_CLICK,
                                          1, 0);
                    ks->state = ELIB_KEY_STATE_IDLE;
                } else {
                    /* Multi-click key: enter MULTI_CLICK_WAIT */
                    ks->click_count++;
                    ks->idle_ms = 0;
                    if (ks->click_count >= cfg->max_click) {
                        /* Reached max clicks: emit immediately */
                        elib_key_push_pending(ctx, cfg->key_id,
                                              (uint8_t)ELIB_KEY_EVENT_SHORT_CLICK,
                                              ks->click_count, 0);
                        ks->click_count = 0;
                        ks->state = ELIB_KEY_STATE_IDLE;
                    } else {
                        ks->state = ELIB_KEY_STATE_MULTI_CLICK_WAIT;
                    }
                }
            }
            break;

        case ELIB_KEY_STATE_MULTI_CLICK_WAIT:
            ks->idle_ms = elib_key_add_ms(ks->idle_ms, elapsed_ms);
            if (rising) {
                /* Another press within window */
                ks->hold_ms = 0;
                ks->hold_notified_idx = 0;
                ks->bit_flags.consumed = 0;
                ks->state = ELIB_KEY_STATE_PRESSED;
            } else if (ks->idle_ms >= cfg->multi_click_window_ms) {
                /* Timeout: emit accumulated clicks */
                elib_key_push_pending(ctx, cfg->key_id,
                                      (uint8_t)ELIB_KEY_EVENT_SHORT_CLICK,
                                      ks->click_count, 0);
                ks->click_count = 0;
                ks->state = ELIB_KEY_STATE_IDLE;
            }
            break;
        }
    }

    /* Process linkage layer if initialized */
    if (ctx->bit_flags.linkage_initialized) {
        elib_linkage_process(ctx, elapsed_ms);
    }

    /* Emit unconsumed pending events */
    for (uint8_t i = 0; i < ctx->pending_count; i++) {
        if (!ctx->pending[i].bit_flags.valid) {
            continue;
        }
        int idx = elib_key_find_key_index(drv, ctx->pending[i].key_id);
        if (idx >= 0 && !ctx->key_states[idx].bit_flags.consumed) {
            drv->on_event(ctx->pending[i].key_id, ctx->pending[i].bit_flags.event,
                          ctx->pending[i].click_count, ctx->pending[i].threshold_idx);
        }
    }

    return ELIB_KEY_OK;
}

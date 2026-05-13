/* elib_key_core.c - Key Detection Core Implementation */
#include "elib_key_core.h"
#include <string.h>

extern void elib_linkage_process(elib_key_ctx_t *ctx, uint32_t elapsed_ms);

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

    memset(ctx, 0, sizeof(elib_key_ctx_t));

    ctx->driver_cfg = cfg;
    for (uint8_t i = 0; i < cfg->key_count; i++) {
        ctx->key_states[i].state = ELIB_KEY_STATE_IDLE;
        ctx->key_states[i].last_level = (uint8_t)cfg->read_level(cfg->keys[i].key_id);
    }
    ctx->initialized = 1;

    return ELIB_KEY_OK;
}

/* Deinitialize key detection */
elib_key_err_t elib_key_deinit(elib_key_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    ctx->initialized = 0;
    ctx->linkage_initialized = 0;
    return ELIB_KEY_OK;
}

/* Find key config by key_id, return index or -1 */
static int find_key_index(const elib_key_driver_cfg_t *cfg, uint8_t key_id) {
    for (uint8_t i = 0; i < cfg->key_count; i++) {
        if (cfg->keys[i].key_id == key_id) {
            return (int)i;
        }
    }
    return -1;
}

/* Scan all keys and process linkage layer */
elib_key_err_t elib_key_scan(elib_key_ctx_t *ctx, uint32_t elapsed_ms) {
    if (ctx == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_KEY_ERR_NOT_INITIALIZED;
    }

    const elib_key_driver_cfg_t *drv = ctx->driver_cfg;
    ctx->pending_count = 0;

    for (uint8_t i = 0; i < drv->key_count; i++) {
        const elib_key_item_cfg_t *cfg = &drv->keys[i];
        elib_key_state_t *ks = &ctx->key_states[i];

        uint8_t level = (uint8_t)drv->read_level(cfg->key_id);
        uint8_t rising  = (level == 1 && ks->last_level == 0);
        uint8_t falling = (level == 0 && ks->last_level == 1);
        ks->last_level = level;

        switch (ks->state) {
        case ELIB_KEY_STATE_IDLE:
            ks->idle_ms += (uint16_t)elapsed_ms;
            if (rising) {
                ks->hold_ms = 0;
                ks->consumed = 0;
                ks->state = ELIB_KEY_STATE_PRESSED;
            }
            break;

        case ELIB_KEY_STATE_PRESSED:
            ks->hold_ms += (uint16_t)elapsed_ms;
            if (falling) {
                /* Check if hold exceeds max threshold */
                uint16_t max_thr = cfg->long_press_thresholds[cfg->threshold_count - 1];
                if (ks->hold_ms >= max_thr) {
                    /* Invalid: exceeded max threshold */
                    ks->state = ELIB_KEY_STATE_IDLE;
                } else if (cfg->max_click == 1) {
                    /* Single-click key: emit immediately */
                    elib_key_pending_event_t *pe = &ctx->pending[ctx->pending_count];
                    pe->key_id = cfg->key_id;
                    pe->valid = 1;

                    /* Determine event type */
                    if (cfg->threshold_count > 0 && ks->hold_ms >= cfg->long_press_thresholds[0]) {
                        pe->event = ELIB_KEY_EVENT_LONG_PRESS;
                        pe->click_count = 0;
                        /* Find which threshold range */
                        pe->threshold_idx = 0;
                        for (uint8_t t = 1; t < cfg->threshold_count; t++) {
                            if (ks->hold_ms < cfg->long_press_thresholds[t]) {
                                break;
                            }
                            pe->threshold_idx = t;
                        }
                    } else {
                        pe->event = ELIB_KEY_EVENT_SHORT_CLICK;
                        pe->click_count = 1;
                        pe->threshold_idx = 0;
                    }
                    ctx->pending_count++;
                    ks->state = ELIB_KEY_STATE_IDLE;
                } else {
                    /* Multi-click key: enter MULTI_CLICK_WAIT */
                    ks->click_count++;
                    ks->idle_ms = 0;
                    if (ks->click_count >= cfg->max_click) {
                        /* Reached max clicks: emit immediately */
                        elib_key_pending_event_t *pe = &ctx->pending[ctx->pending_count];
                        pe->key_id = cfg->key_id;
                        pe->valid = 1;
                        pe->event = ELIB_KEY_EVENT_SHORT_CLICK;
                        pe->click_count = ks->click_count;
                        pe->threshold_idx = 0;
                        ctx->pending_count++;
                        ks->click_count = 0;
                        ks->state = ELIB_KEY_STATE_IDLE;
                    } else {
                        ks->state = ELIB_KEY_STATE_MULTI_CLICK_WAIT;
                    }
                }
            }
            break;

        case ELIB_KEY_STATE_MULTI_CLICK_WAIT:
            ks->idle_ms += (uint16_t)elapsed_ms;
            if (rising) {
                /* Another press within window */
                ks->hold_ms = 0;
                ks->consumed = 0;
                ks->state = ELIB_KEY_STATE_PRESSED;
            } else if (ks->idle_ms >= cfg->multi_click_window_ms) {
                /* Timeout: emit accumulated clicks */
                elib_key_pending_event_t *pe = &ctx->pending[ctx->pending_count];
                pe->key_id = cfg->key_id;
                pe->valid = 1;
                pe->event = ELIB_KEY_EVENT_SHORT_CLICK;
                pe->click_count = ks->click_count;
                pe->threshold_idx = 0;
                ctx->pending_count++;
                ks->click_count = 0;
                ks->state = ELIB_KEY_STATE_IDLE;
            }
            break;
        }
    }

    /* Process linkage layer if initialized */
    if (ctx->linkage_initialized) {
        elib_linkage_process(ctx, elapsed_ms);
    }

    /* Emit unconsumed pending events */
    for (uint8_t i = 0; i < ctx->pending_count; i++) {
        if (!ctx->pending[i].valid) {
            continue;
        }
        int idx = find_key_index(drv, ctx->pending[i].key_id);
        if (idx >= 0 && !ctx->key_states[idx].consumed) {
            drv->on_event(ctx->pending[i].key_id, ctx->pending[i].event,
                          ctx->pending[i].click_count, ctx->pending[i].threshold_idx);
        }
    }

    return ELIB_KEY_OK;
}

/* elib_key_linkage.c - Linkage Layer Implementation */
#include "elib_key_core.h"
#include <string.h>

/* Validate one combo's key id list: array non-NULL, count in range, every
 * referenced key_id must exist in the driver config */
static elib_key_err_t validate_combo_keys(const elib_key_driver_cfg_t *drv,
                                          const uint8_t *ids, uint8_t count) {
    if (ids == NULL || count == 0 || count > ELIB_KEY_MAX_COMBO_KEYS) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    for (uint8_t k = 0; k < count; k++) {
        if (elib_key_find_key_index(drv, ids[k]) < 0) {
            return ELIB_KEY_ERR_INVALID_PARAM;
        }
    }
    return ELIB_KEY_OK;
}

/* Validate a combo description against the driver config */
static elib_key_err_t validate_combo(const elib_key_driver_cfg_t *drv,
                                     const elib_key_combo_cfg_t *cc) {
    if (cc->mode == ELIB_KEY_COMBO_SHORT || cc->mode == ELIB_KEY_COMBO_LONG) {
        return validate_combo_keys(drv, cc->combo_keys, cc->combo_count);
    }
    if (cc->mode == ELIB_KEY_COMBO_HOLD_PRESS) {
        elib_key_err_t err = validate_combo_keys(drv, cc->hold_keys, cc->hold_count);
        if (err != ELIB_KEY_OK) {
            return err;
        }
        return validate_combo_keys(drv, cc->press_keys, cc->press_count);
    }
    return ELIB_KEY_ERR_INVALID_PARAM;
}

/* Initialize linkage layer */
elib_key_err_t elib_key_linkage_init(elib_key_ctx_t *ctx, const elib_key_linkage_cfg_t *cfg) {
    if (ctx == NULL || cfg == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_KEY_ERR_NOT_INITIALIZED;
    }
    if (cfg->combos == NULL || cfg->on_combo == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (cfg->combo_count > ELIB_KEY_MAX_COMBOS) {
        return ELIB_KEY_ERR_TOO_MANY_COMBOS;
    }

    /* Validate every combo before touching ctx so failures leave it intact */
    for (uint8_t c = 0; c < cfg->combo_count; c++) {
        if (validate_combo(ctx->driver_cfg, &cfg->combos[c]) != ELIB_KEY_OK) {
            return ELIB_KEY_ERR_INVALID_PARAM;
        }
    }

    ctx->linkage_cfg = cfg;
    memset(ctx->combo_states, 0, sizeof(elib_key_combo_state_t) * cfg->combo_count);
    ctx->bit_flags.linkage_initialized = 1;

    return ELIB_KEY_OK;
}

/* Deinitialize linkage layer */
elib_key_err_t elib_key_linkage_deinit(elib_key_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    ctx->bit_flags.linkage_initialized = 0;
    ctx->linkage_cfg = NULL;
    return ELIB_KEY_OK;
}

/* Mark a key consumed (suppress later driver emission) and invalidate any
 * pending events for it in the current scan */
static void consume_key_events(elib_key_ctx_t *ctx,
                               const elib_key_driver_cfg_t *drv,
                               uint8_t key_id) {
    int idx = elib_key_find_key_index(drv, key_id);
    if (idx >= 0) {
        ctx->key_states[idx].bit_flags.consumed = 1;
    }
    for (uint8_t p = 0; p < ctx->pending_count; p++) {
        if (ctx->pending[p].key_id == key_id) {
            ctx->pending[p].bit_flags.valid = 0;
        }
    }
}

/* Process linkage layer: check all combos against pending events.
 *
 * Window model: armed when the first participating key's event arrives; the
 * window accumulates elapsed_ms across scans. Satisfaction per key persists
 * across scans, so participants may release in different scans. See the
 * design spec section 5 ("Linkage Layer").
 *
 * Known race: a participant's driver event is emitted immediately unless the
 * combo completes in the same scan, so in staggered (cross-scan) combos the
 * earlier participant's independent event fires before consumption. */
void elib_linkage_process(elib_key_ctx_t *ctx, uint32_t elapsed_ms) {
    if (!ctx->bit_flags.linkage_initialized) {
        return;
    }

    const elib_key_linkage_cfg_t *lnk = ctx->linkage_cfg;
    const elib_key_driver_cfg_t *drv = ctx->driver_cfg;

    /* 1) Advance armed windows; reset combos past their window */
    for (uint8_t c = 0; c < lnk->combo_count; c++) {
        elib_key_combo_state_t *cs = &ctx->combo_states[c];
        if (cs->first_event_ms == 0) {
            continue; /* window not armed */
        }
        cs->first_event_ms = elib_key_add_ms(cs->first_event_ms, elapsed_ms);
        if (cs->first_event_ms > lnk->combos[c].window_ms) {
            /* Timeout: reset combo tracking */
            memset(cs, 0, sizeof(*cs));
        }
    }

    /* 2) Accumulate per-key satisfaction from the current scan's events.
     * HOLD_PRESS checks live key state instead, so it is skipped here. */
    for (uint8_t c = 0; c < lnk->combo_count; c++) {
        const elib_key_combo_cfg_t *cc = &lnk->combos[c];
        if (cc->mode == ELIB_KEY_COMBO_HOLD_PRESS) {
            continue;
        }

        elib_key_combo_state_t *cs = &ctx->combo_states[c];
        uint8_t want = (cc->mode == ELIB_KEY_COMBO_SHORT)
                           ? (uint8_t)ELIB_KEY_EVENT_SHORT_CLICK
                           : (uint8_t)ELIB_KEY_EVENT_LONG_PRESS;

        for (uint8_t k = 0; k < cc->combo_count; k++) {
            uint8_t kid = cc->combo_keys[k];
            if (elib_key_combo_is_satisfied(cs, k)) {
                continue;
            }
            for (uint8_t p = 0; p < ctx->pending_count; p++) {
                const elib_key_pending_event_t *pe = &ctx->pending[p];
                if (pe->bit_flags.valid && pe->key_id == kid &&
                    pe->bit_flags.event == want) {
                    elib_key_combo_set_satisfied(cs, k, 1);
                    if (cs->first_event_ms == 0) {
                        cs->first_event_ms = 1; /* arm the window */
                    }
                    break;
                }
            }
        }
    }

    /* 3) Trigger combos in priority order (first match wins) */
    for (uint8_t c = 0; c < lnk->combo_count; c++) {
        const elib_key_combo_cfg_t *cc = &lnk->combos[c];
        elib_key_combo_state_t *cs = &ctx->combo_states[c];

        switch (cc->mode) {
        case ELIB_KEY_COMBO_SHORT:
        case ELIB_KEY_COMBO_LONG: {
            uint8_t all_satisfied = (cc->combo_count > 0);
            for (uint8_t k = 0; k < cc->combo_count; k++) {
                if (!elib_key_combo_is_satisfied(cs, k)) {
                    all_satisfied = 0;
                    break;
                }
            }

            if (all_satisfied) {
                for (uint8_t k = 0; k < cc->combo_count; k++) {
                    consume_key_events(ctx, drv, cc->combo_keys[k]);
                }
                lnk->on_combo(cc->combo_id, cc->mode);
                /* Reset so the combo can trigger again */
                memset(cs, 0, sizeof(*cs));
            }
            break;
        }

        case ELIB_KEY_COMBO_HOLD_PRESS: {
            /* All hold_keys in PRESSED state with hold_ms >= threshold[0] */
            uint8_t all_held = 1;
            for (uint8_t k = 0; k < cc->hold_count; k++) {
                int idx = elib_key_find_key_index(drv, cc->hold_keys[k]);
                if (idx < 0 ||
                    ctx->key_states[idx].state != ELIB_KEY_STATE_PRESSED) {
                    all_held = 0;
                    break;
                }
                if (drv->keys[idx].threshold_count > 0 &&
                    ctx->key_states[idx].hold_ms <
                        drv->keys[idx].long_press_thresholds[0]) {
                    all_held = 0;
                    break;
                }
            }

            if (!all_held) {
                break;
            }

            /* All press_keys have SHORT_CLICK in pending */
            uint8_t all_pressed = 1;
            uint8_t found_any = 0;
            for (uint8_t k = 0; k < cc->press_count; k++) {
                uint8_t kid = cc->press_keys[k];
                uint8_t found = 0;
                for (uint8_t p = 0; p < ctx->pending_count; p++) {
                    const elib_key_pending_event_t *pe = &ctx->pending[p];
                    if (pe->bit_flags.valid && pe->key_id == kid &&
                        pe->bit_flags.event == ELIB_KEY_EVENT_SHORT_CLICK) {
                        found = 1;
                        found_any = 1;
                        break;
                    }
                }
                if (!found) {
                    all_pressed = 0;
                }
            }

            if (all_pressed && found_any) {
                for (uint8_t k = 0; k < cc->hold_count; k++) {
                    consume_key_events(ctx, drv, cc->hold_keys[k]);
                }
                for (uint8_t k = 0; k < cc->press_count; k++) {
                    consume_key_events(ctx, drv, cc->press_keys[k]);
                }
                lnk->on_combo(cc->combo_id, cc->mode);
                /* Reset so the combo can trigger again */
                memset(cs, 0, sizeof(*cs));
            }
            break;
        }
        }
    }
}

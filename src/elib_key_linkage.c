/* elib_key_linkage.c - Linkage Layer Implementation */
#include "elib_key_core.h"
#include <string.h>

/* Initialize linkage layer */
elib_key_err_t elib_key_linkage_init(elib_key_ctx_t *ctx, const elib_key_linkage_cfg_t *cfg) {
    if (ctx == NULL || cfg == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_KEY_ERR_NOT_INITIALIZED;
    }
    if (cfg->combos == NULL || cfg->on_combo == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    if (cfg->combo_count > ELIB_KEY_MAX_COMBOS) {
        return ELIB_KEY_ERR_TOO_MANY_COMBOS;
    }

    ctx->linkage_cfg = cfg;
    memset(ctx->combo_states, 0, sizeof(elib_key_combo_state_t) * cfg->combo_count);
    ctx->linkage_initialized = 1;

    return ELIB_KEY_OK;
}

/* Deinitialize linkage layer */
elib_key_err_t elib_key_linkage_deinit(elib_key_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    ctx->linkage_initialized = 0;
    ctx->linkage_cfg = NULL;
    return ELIB_KEY_OK;
}

/* Find key index in driver config by key_id */
static int find_key_idx(const elib_key_driver_cfg_t *drv, uint8_t key_id) {
    for (uint8_t i = 0; i < drv->key_count; i++) {
        if (drv->keys[i].key_id == key_id) {
            return (int)i;
        }
    }
    return -1;
}

/* Process linkage layer: check all combos against pending events */
void elib_linkage_process(elib_key_ctx_t *ctx, uint32_t elapsed_ms) {
    if (!ctx->linkage_initialized) {
        return;
    }

    const elib_key_linkage_cfg_t *lnk = ctx->linkage_cfg;
    const elib_key_driver_cfg_t *drv = ctx->driver_cfg;

    /* Update combo timers */
    for (uint8_t c = 0; c < lnk->combo_count; c++) {
        elib_key_combo_state_t *cs = &ctx->combo_states[c];
        if (cs->first_event_ms > 0 && !cs->triggered) {
            cs->first_event_ms += (uint16_t)elapsed_ms;
            const elib_key_combo_cfg_t *cc = &lnk->combos[c];
            if (cs->first_event_ms > cc->window_ms) {
                /* Timeout: reset combo tracking */
                memset(cs, 0, sizeof(elib_key_combo_state_t));
            }
        }
    }

    /* Check combos in priority order */
    for (uint8_t c = 0; c < lnk->combo_count; c++) {
        const elib_key_combo_cfg_t *cc = &lnk->combos[c];
        elib_key_combo_state_t *cs = &ctx->combo_states[c];

        if (cs->triggered) {
            continue;
        }

        switch (cc->mode) {
        case ELIB_KEY_COMBO_SHORT: {
            /* Find SHORT_CLICK events for all combo keys within window */
            uint8_t all_satisfied = 1;
            uint8_t found_any = 0;

            for (uint8_t k = 0; k < cc->combo_count; k++) {
                uint8_t kid = cc->combo_keys[k];
                uint8_t found = 0;

                for (uint8_t p = 0; p < ctx->pending_count; p++) {
                    if (ctx->pending[p].key_id == kid &&
                        ctx->pending[p].event == ELIB_KEY_EVENT_SHORT_CLICK &&
                        ctx->pending[p].valid) {
                        found = 1;
                        found_any = 1;
                        break;
                    }
                }

                if (!found) {
                    all_satisfied = 0;
                }
            }

            if (all_satisfied && found_any) {
                /* Trigger combo: consume all participating keys */
                for (uint8_t k = 0; k < cc->combo_count; k++) {
                    int idx = find_key_idx(drv, cc->combo_keys[k]);
                    if (idx >= 0) {
                        ctx->key_states[idx].consumed = 1;
                    }
                    /* Also invalidate pending events */
                    for (uint8_t p = 0; p < ctx->pending_count; p++) {
                        if (ctx->pending[p].key_id == cc->combo_keys[k]) {
                            ctx->pending[p].valid = 0;
                        }
                    }
                }
                cs->triggered = 1;
                lnk->on_combo(cc->combo_id, cc->mode);
            }
            break;
        }

        case ELIB_KEY_COMBO_LONG:
            /* Will be implemented in next task */
            break;

        case ELIB_KEY_COMBO_HOLD_PRESS:
            /* Will be implemented in next task */
            break;
        }
    }
}

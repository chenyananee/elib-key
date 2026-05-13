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
            /* Will be implemented in next task */
            break;

        case ELIB_KEY_STATE_MULTI_CLICK_WAIT:
            /* Will be implemented in next task */
            break;
        }
    }

    /* Emit unconsumed pending events */
    for (uint8_t i = 0; i < ctx->pending_count; i++) {
        if (ctx->pending[i].valid && !ctx->key_states[find_key_index(drv, ctx->pending[i].key_id)].consumed) {
            drv->on_event(ctx->pending[i].key_id, ctx->pending[i].event,
                          ctx->pending[i].click_count, ctx->pending[i].threshold_idx);
        }
    }

    return ELIB_KEY_OK;
}

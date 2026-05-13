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

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

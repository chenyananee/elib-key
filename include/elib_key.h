/* elib_key.h - Key Detection Library Main Header */
#ifndef ELIB_KEY_H
#define ELIB_KEY_H

#include "elib_key_err.h"
#include "elib_key_types.h"
#include "elib_key_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize key detection driver layer
 * @param ctx User-allocated context pointer
 * @param cfg Driver configuration
 * @return elib_key_err_t error code
 */
elib_key_err_t elib_key_init(elib_key_ctx_t *ctx, const elib_key_driver_cfg_t *cfg);

/**
 * @brief Deinitialize key detection
 * @param ctx Context pointer
 * @return elib_key_err_t error code
 */
elib_key_err_t elib_key_deinit(elib_key_ctx_t *ctx);

/**
 * @brief Scan all keys and process linkage layer
 * @param ctx Context pointer
 * @param elapsed_ms Milliseconds since last scan
 * @return elib_key_err_t error code
 */
elib_key_err_t elib_key_scan(elib_key_ctx_t *ctx, uint32_t elapsed_ms);

/**
 * @brief Initialize linkage layer (optional, call after elib_key_init)
 * @param ctx Context pointer
 * @param cfg Linkage configuration
 * @return elib_key_err_t error code
 */
elib_key_err_t elib_key_linkage_init(elib_key_ctx_t *ctx, const elib_key_linkage_cfg_t *cfg);

/**
 * @brief Deinitialize linkage layer
 * @param ctx Context pointer
 * @return elib_key_err_t error code
 */
elib_key_err_t elib_key_linkage_deinit(elib_key_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_KEY_H */

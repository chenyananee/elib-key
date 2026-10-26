/* elib_key_core.h - Key Detection Internal Header */
#ifndef ELIB_KEY_CORE_H
#define ELIB_KEY_CORE_H

#include "../include/elib_key.h"

/* Saturating accumulation for millisecond timers (prevents 16-bit wraparound) */
static inline uint16_t elib_key_add_ms(uint16_t base, uint32_t delta) {
    uint32_t sum = (uint32_t)base + delta;
    return (sum > 0xFFFFu) ? 0xFFFFu : (uint16_t)sum;
}

/* Internal cross-module API */
void elib_linkage_process(elib_key_ctx_t *ctx, uint32_t elapsed_ms);
int elib_key_find_key_index(const elib_key_driver_cfg_t *cfg, uint8_t key_id);

#endif /* ELIB_KEY_CORE_H */

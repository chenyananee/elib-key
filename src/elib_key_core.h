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
void elib_key_arbitrate(elib_key_ctx_t *ctx);
/* Returns 0 when the key's on_long_hold escalation must stay silent this
 * scan (combo-claimed, follower, or a combo is about to claim it); withheld
 * levels do not advance the notified index, and unclaimed keys flush their
 * backlog naturally on a later scan */
uint8_t elib_key_hold_notify_allowed(const elib_key_ctx_t *ctx, uint8_t key_idx);
int elib_key_find_key_index(const elib_key_driver_cfg_t *cfg, uint8_t key_id);

#endif /* ELIB_KEY_CORE_H */

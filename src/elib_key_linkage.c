/* elib_key_linkage.c - Single-Gesture Arbitration and Combo Linkage
 *
 * The library enforces one invariant: at most ONE gesture exists at any
 * time. A gesture is the active state of a key under arbitration — presses,
 * releases, pending classifications and claimed combos never overlap.
 *
 * Channel rules (R1/R2/R3, design spec section 5):
 *   R1 Exclusivity: while a gesture is in progress a newly pressed key
 *      either upgrades the gesture into a matching combo, or is ignored
 *      (all of its classified output is discarded); the running gesture is
 *      never affected.
 *   R2 Single output: a gesture produces at most one result — the owner
 *      key's classification or one combo trigger, never both.
 *   R3 Complete then free: once every participant released and the result
 *      fired, the channel is free for the next gesture.
 *
 * Combo claim model:
 *   Overall gesture window (window_ms, shared by COMBO_SHORT/LONG): counted
 *   from the FIRST participant's press. All participants must press (and,
 *   for LONG, all participants must release) inside that window; window_ms
 *   == 0 disables the constraint. A windowed claim therefore requires the
 *   press span (fist press -> last press) < window_ms, and a claim arms an
 *   expiry countdown — an expired gesture keeps its claims (silence, no
 *   replay) until everyone released, then fires nothing.
 *   COMBO_LONG     claims while all participants are pressed past their
 *                  first threshold; fires once when all released. The
 *                  claim permanently cancels every participant's LONG_PRESS
 *                  classification and on_long_hold escalation for the
 *                  gesture. With windows configured (threshold_count > 0)
 *                  it instead claims the instant every participant is
 *                  pressed — that moment is the co-held timing origin; each
 *                  window crossed while they stay held fires the
 *                  on_combo_hold escalation, and releasing inside a window
 *                  fires the combo while releasing past the last window
 *                  classifies the whole gesture invalid.
 *   COMBO_HOLD_PRESS fires when hold keys are pressed past their first
 *                  threshold AND press keys' clicks are captured; hold keys
 *                  stay claimed (silent) until everything is released.
 *   COMBO_SHORT    fires when all participants' clicks are captured
 *                  simultaneously (valid pending SHORT_CLICK or armed in
 *                  MULTI_CLICK_WAIT); OWNER:
 *                  pending events are consumed, claims are cleared, the
 *                  gesture ends.
 *
 * Raw press/release edge callbacks are unclassified observers and fire
 * regardless of any rule above.
 */
#include "elib_key_core.h"
#include <string.h>

/* ---- state queries over the driver config ---- */

static uint8_t key_idx_of(const elib_key_driver_cfg_t *drv, uint8_t key_id) {
    int idx = elib_key_find_key_index(drv, key_id);
    return (idx >= 0) ? (uint8_t)idx : 0xFF;
}

static uint8_t is_pressed_at(const elib_key_ctx_t *ctx, uint8_t idx) {
    return (idx != 0xFF &&
            ctx->key_states[idx].state == ELIB_KEY_STATE_PRESSED) ? 1 : 0;
}

/* Pressed and past the first long-press threshold */
static uint8_t hold_past_first(const elib_key_ctx_t *ctx,
                               const elib_key_driver_cfg_t *drv,
                               uint8_t key_id) {
    uint8_t idx = key_idx_of(drv, key_id);
    if (idx == 0xFF || drv->keys[idx].threshold_count == 0) {
        return 0;
    }
    return (ctx->key_states[idx].state == ELIB_KEY_STATE_PRESSED &&
            ctx->key_states[idx].hold_ms >=
                drv->keys[idx].long_press_thresholds[0]) ? 1 : 0;
}

/* Click captured: the key has an unresolved classified click this gesture —
 * a valid pending SHORT_CLICK now, or it is parked in MULTI_CLICK_WAIT */
static uint8_t click_captured(const elib_key_ctx_t *ctx,
                              const elib_key_driver_cfg_t *drv,
                              uint8_t key_id) {
    uint8_t idx = key_idx_of(drv, key_id);
    if (idx != 0xFF &&
        ctx->key_states[idx].state == ELIB_KEY_STATE_MULTI_CLICK_WAIT) {
        return 1;
    }
    for (uint8_t p = 0; p < ctx->pending_count; p++) {
        const elib_key_pending_event_t *pe = &ctx->pending[p];
        if (pe->bit_flags.valid && pe->key_id == key_id &&
            pe->bit_flags.event == (uint8_t)ELIB_KEY_EVENT_SHORT_CLICK) {
            return 1;
        }
    }
    return 0;
}

static uint8_t is_member(const uint8_t *ids, uint8_t count, uint8_t key_id) {
    for (uint8_t k = 0; k < count; k++) {
        if (ids[k] == key_id) {
            return 1;
        }
    }
    return 0;
}

static uint8_t all_hold_past_first(const elib_key_ctx_t *ctx,
                                   const elib_key_driver_cfg_t *drv,
                                   const uint8_t *ids, uint8_t count) {
    for (uint8_t k = 0; k < count; k++) {
        if (!hold_past_first(ctx, drv, ids[k])) {
            return 0;
        }
    }
    return 1;
}

/* Every listed key currently pressed */
static uint8_t all_pressed(const elib_key_ctx_t *ctx,
                           const elib_key_driver_cfg_t *drv,
                           const uint8_t *ids, uint8_t count) {
    for (uint8_t k = 0; k < count; k++) {
        if (!is_pressed_at(ctx, key_idx_of(drv, ids[k]))) {
            return 0;
        }
    }
    return 1;
}

/* All hold keys except key_id (used to test claim imminence for one key) */
static uint8_t all_hold_past_first_others(const elib_key_ctx_t *ctx,
                                          const elib_key_driver_cfg_t *drv,
                                          const elib_key_combo_cfg_t *cc,
                                          uint8_t key_id) {
    for (uint8_t k = 0; k < cc->hold_count; k++) {
        if (cc->hold_keys[k] == key_id) {
            continue;
        }
        if (!hold_past_first(ctx, drv, cc->hold_keys[k])) {
            return 0;
        }
    }
    return 1;
}

/* Members of a combo in cfg order; HOLD_PRESS participants are the hold
 * list followed by the press list */
static uint8_t combo_member_id(const elib_key_combo_cfg_t *cc, uint8_t k,
                               uint8_t *out_id) {
    if (cc->mode == ELIB_KEY_COMBO_HOLD_PRESS) {
        if (k < cc->hold_count) {
            *out_id = cc->hold_keys[k];
            return 1;
        }
        k = (uint8_t)(k - cc->hold_count);
        if (k < cc->press_count) {
            *out_id = cc->press_keys[k];
            return 1;
        }
        return 0;
    }
    if (k < cc->combo_count) {
        *out_id = cc->combo_keys[k];
        return 1;
    }
    return 0;
}

static uint8_t combo_total_members(const elib_key_combo_cfg_t *cc) {
    return (cc->mode == ELIB_KEY_COMBO_HOLD_PRESS)
               ? (uint8_t)(cc->hold_count + cc->press_count)
               : cc->combo_count;
}

/* All members (HOLD_PRESS: hold list then press list) released */
static uint8_t combo_all_released(const elib_key_ctx_t *ctx,
                                  const elib_key_driver_cfg_t *drv,
                                  const elib_key_combo_cfg_t *cc) {
    uint8_t n = combo_total_members(cc);
    for (uint8_t k = 0; k < n; k++) {
        uint8_t id;
        if (combo_member_id(cc, k, &id) &&
            is_pressed_at(ctx, key_idx_of(drv, id))) {
            return 0;
        }
    }
    return 1;
}

/* Age of a key's current press: held time so far while PRESSED; for a
 * parked click (MULTI_CLICK_WAIT: released, click not yet classified) the
 * hold duration plus the time already spent waiting */
static uint32_t press_age_ms(const elib_key_ctx_t *ctx,
                             const elib_key_driver_cfg_t *drv,
                             uint8_t key_id) {
    uint8_t idx = key_idx_of(drv, key_id);
    if (idx == 0xFF) {
        return 0;
    }
    const elib_key_state_t *ks = &ctx->key_states[idx];
    uint32_t age = (uint32_t)ks->hold_ms;
    if (ks->state == ELIB_KEY_STATE_MULTI_CLICK_WAIT) {
        age += (uint32_t)ks->idle_ms;
    }
    return age;
}

/* Press span (first -> last participant press) inside the gesture window:
 * measured conservatively as the current age of the earliest-pressed
 * member (== exact span at the moment the last participant just pressed;
 * >= span later). window_ms == 0 disables the constraint */
static uint32_t max_press_age_ms(const elib_key_ctx_t *ctx,
                                 const elib_key_driver_cfg_t *drv,
                                 const uint8_t *ids, uint8_t count) {
    uint32_t first_press_age = 0;
    for (uint8_t k = 0; k < count; k++) {
        uint32_t age = press_age_ms(ctx, drv, ids[k]);
        if (age > first_press_age) {
            first_press_age = age;
        }
    }
    return first_press_age;
}

static uint8_t press_span_in_window(const elib_key_ctx_t *ctx,
                                    const elib_key_driver_cfg_t *drv,
                                    const uint8_t *ids, uint8_t count,
                                    uint16_t window_ms) {
    if (window_ms == 0) {
        return 1;
    }
    /* strictly inside: the release phase keeps at least one scan */
    return max_press_age_ms(ctx, drv, ids, count) <
                   (uint32_t)window_ms ? 1 : 0;
}

static uint8_t all_click_captured(const elib_key_ctx_t *ctx,
                                  const elib_key_driver_cfg_t *drv,
                                  const uint8_t *ids, uint8_t count) {
    for (uint8_t k = 0; k < count; k++) {
        if (!click_captured(ctx, drv, ids[k])) {
            return 0;
        }
    }
    return 1;
}

/* ---- mutation helpers ---- */

static void discard_pending_for(elib_key_ctx_t *ctx, uint8_t key_id) {
    for (uint8_t p = 0; p < ctx->pending_count; p++) {
        if (ctx->pending[p].key_id == key_id) {
            ctx->pending[p].bit_flags.valid = 0;
        }
    }
}

/* End a key's click sequence without emitting anything */
static void abort_click_sequence(elib_key_ctx_t *ctx, uint8_t key_id) {
    uint8_t idx = key_idx_of(ctx->driver_cfg, key_id);
    if (idx == 0xFF) {
        return;
    }
    discard_pending_for(ctx, key_id);
    ctx->key_states[idx].click_count = 0;
    if (ctx->key_states[idx].state == ELIB_KEY_STATE_MULTI_CLICK_WAIT) {
        ctx->key_states[idx].state = ELIB_KEY_STATE_IDLE;
    }
}

static void set_claimed(elib_key_ctx_t *ctx, const elib_key_driver_cfg_t *drv,
                        const uint8_t *ids, uint8_t count) {
    for (uint8_t k = 0; k < count; k++) {
        uint8_t idx = key_idx_of(drv, ids[k]);
        if (idx != 0xFF) {
            ctx->key_states[idx].bit_flags.claimed = 1;
            ctx->key_states[idx].bit_flags.owner = 0;
            ctx->key_states[idx].bit_flags.follower = 0;
        }
    }
}

static void clear_claimed(elib_key_ctx_t *ctx,
                          const elib_key_driver_cfg_t *drv,
                          const uint8_t *ids, uint8_t count) {
    for (uint8_t k = 0; k < count; k++) {
        uint8_t idx = key_idx_of(drv, ids[k]);
        if (idx != 0xFF) {
            ctx->key_states[idx].bit_flags.claimed = 0;
        }
    }
}

static void for_each_member_claim(elib_key_ctx_t *ctx,
                                  const elib_key_driver_cfg_t *drv,
                                  const elib_key_combo_cfg_t *cc,
                                  uint8_t value) {
    uint8_t n = combo_total_members(cc);
    for (uint8_t k = 0; k < n; k++) {
        uint8_t id;
        if (combo_member_id(cc, k, &id)) {
            if (value) {
                set_claimed(ctx, drv, &id, 1);
            } else {
                clear_claimed(ctx, drv, &id, 1);
            }
        }
    }
}

/* ---- init / deinit ---- */

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
        const elib_key_combo_cfg_t *cc = &cfg->combos[c];
        if (cc->mode == ELIB_KEY_COMBO_SHORT ||
            cc->mode == ELIB_KEY_COMBO_LONG) {
            if (cc->combo_keys == NULL || cc->combo_count == 0 ||
                cc->combo_count > ELIB_KEY_MAX_COMBO_KEYS) {
                return ELIB_KEY_ERR_INVALID_PARAM;
            }
            /* COMBO_LONG windows (optional): when present the array pointer
             * must be set and the count bounded like per-key thresholds */
            if (cc->mode == ELIB_KEY_COMBO_LONG &&
                (cc->threshold_count > ELIB_KEY_MAX_THRESHOLDS ||
                 (cc->threshold_count > 0 &&
                  cc->long_press_thresholds == NULL))) {
                return ELIB_KEY_ERR_INVALID_PARAM;
            }
            for (uint8_t k = 0; k < cc->combo_count; k++) {
                if (elib_key_find_key_index(ctx->driver_cfg,
                                            cc->combo_keys[k]) < 0) {
                    return ELIB_KEY_ERR_INVALID_PARAM;
                }
            }
        } else if (cc->mode == ELIB_KEY_COMBO_HOLD_PRESS) {
            if (cc->hold_keys == NULL || cc->hold_count == 0 ||
                cc->press_keys == NULL || cc->press_count == 0 ||
                cc->hold_count > ELIB_KEY_MAX_COMBO_KEYS ||
                cc->press_count > ELIB_KEY_MAX_COMBO_KEYS) {
                return ELIB_KEY_ERR_INVALID_PARAM;
            }
            for (uint8_t k = 0; k < cc->hold_count; k++) {
                if (elib_key_find_key_index(ctx->driver_cfg,
                                            cc->hold_keys[k]) < 0) {
                    return ELIB_KEY_ERR_INVALID_PARAM;
                }
            }
            for (uint8_t k = 0; k < cc->press_count; k++) {
                if (elib_key_find_key_index(ctx->driver_cfg,
                                            cc->press_keys[k]) < 0) {
                    return ELIB_KEY_ERR_INVALID_PARAM;
                }
            }
        } else {
            return ELIB_KEY_ERR_INVALID_PARAM;
        }
    }

    ctx->linkage_cfg = cfg;
    memset(&ctx->gesture, 0, sizeof(ctx->gesture));
    /* Safety: no key may stay claimed across a re-init */
    for (uint8_t i = 0; i < ctx->driver_cfg->key_count; i++) {
        ctx->key_states[i].bit_flags.claimed = 0;
    }
    ctx->bit_flags.linkage_initialized = 1;

    return ELIB_KEY_OK;
}

elib_key_err_t elib_key_linkage_deinit(elib_key_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_KEY_ERR_INVALID_PARAM;
    }
    ctx->bit_flags.linkage_initialized = 0;
    ctx->linkage_cfg = NULL;
    memset(&ctx->gesture, 0, sizeof(ctx->gesture));
    if (ctx->driver_cfg != NULL) {
        for (uint8_t i = 0; i < ctx->driver_cfg->key_count; i++) {
            ctx->key_states[i].bit_flags.claimed = 0;
        }
    }
    return ELIB_KEY_OK;
}

/* ---- escalation gating (called from the driver per pressed key) ---- */

uint8_t elib_key_hold_notify_allowed(const elib_key_ctx_t *ctx, uint8_t key_idx) {
    const elib_key_driver_cfg_t *drv = ctx->driver_cfg;
    const elib_key_state_t *ks = &ctx->key_states[key_idx];
    const uint8_t key_id = drv->keys[key_idx].key_id;

    /* Followers and claimed members stay silent; claimed is permanent for
     * the gesture (no replay — single-key actions must never leak) */
    if (ks->bit_flags.claimed || ks->bit_flags.follower) {
        return 0;
    }
    if (!ctx->bit_flags.linkage_initialized) {
        return 1;
    }

    const elib_key_linkage_cfg_t *lnk = ctx->linkage_cfg;
    for (uint8_t c = 0; c < lnk->combo_count; c++) {
        const elib_key_combo_cfg_t *cc = &lnk->combos[c];
        if (cc->mode == ELIB_KEY_COMBO_LONG) {
            /* Claim imminent check differs by configuration:
             * windowed LONG claims as soon as every participant is down
             * (one scan ordering margin); legacy binary claims once every
             * other participant is past its own first threshold */
            uint8_t others_ok = 1;
            for (uint8_t k = 0; k < cc->combo_count; k++) {
                if (cc->combo_keys[k] == key_id) {
                    continue;
                }
                if (cc->threshold_count > 0) {
                    if (!is_pressed_at(ctx, key_idx_of(drv,
                                                       cc->combo_keys[k]))) {
                        others_ok = 0;
                        break;
                    }
                } else if (!hold_past_first(ctx, drv, cc->combo_keys[k])) {
                    others_ok = 0;
                    break;
                }
            }
            if (others_ok && is_member(cc->combo_keys, cc->combo_count, key_id)) {
                return 0;
            }
        } else if (cc->mode == ELIB_KEY_COMBO_HOLD_PRESS) {
            if (is_member(cc->hold_keys, cc->hold_count, key_id) &&
                all_hold_past_first_others(ctx, drv, cc, key_id) &&
                all_click_captured(ctx, drv, cc->press_keys, cc->press_count)) {
                return 0;
            }
        }
    }
    return 1;
}

/* ---- arbitration (runs at the end of every scan) ---- */

void elib_key_arbitrate(elib_key_ctx_t *ctx, uint32_t elapsed_ms) {
    const elib_key_driver_cfg_t *drv = ctx->driver_cfg;
    const elib_key_linkage_cfg_t *lnk =
        ctx->bit_flags.linkage_initialized ? ctx->linkage_cfg : NULL;

    /* 1) Resolve a locked gesture */
    if (ctx->gesture.bit_flags.locked && lnk != NULL) {
        const elib_key_combo_cfg_t *cc =
            &lnk->combos[ctx->gesture.locked_combo];
        uint8_t all_rel = combo_all_released(ctx, drv, cc);

        if (!all_rel) {
            /* Overall gesture window countdown (from the first press) */
            if (ctx->gesture.window_left != 0xFFFFu) {
                ctx->gesture.window_left =
                    (elapsed_ms >= ctx->gesture.window_left)
                        ? 0u
                        : (uint16_t)(ctx->gesture.window_left - elapsed_ms);
            }
            if (cc->mode == ELIB_KEY_COMBO_LONG && cc->threshold_count > 0) {
                /* Windowed LONG: accumulate the co-held time while the
                 * gesture runs and report every window crossing while the
                 * overall window is alive; the claim stays permanent (no
                 * replay) until every participant released */
                uint8_t crossed = 0;
                ctx->gesture.hold_ms =
                    elib_key_add_ms(ctx->gesture.hold_ms, elapsed_ms);
                for (uint8_t t = 0; t < cc->threshold_count; t++) {
                    if (ctx->gesture.hold_ms <
                        cc->long_press_thresholds[t]) {
                        break;
                    }
                    crossed = (uint8_t)(t + 1);
                }
                if (crossed > ctx->gesture.notified_idx) {
                    if (lnk->on_combo_hold != NULL &&
                        ctx->gesture.window_left != 0u) {
                        for (uint8_t n =
                                 (uint8_t)(ctx->gesture.notified_idx + 1);
                             n <= crossed;
                             n++) {
                            lnk->on_combo_hold(cc->combo_id,
                                               (uint8_t)(n - 1));
                        }
                    }
                    ctx->gesture.notified_idx = crossed;
                }
            }
        } else {
            /* All released: settle by the recorded timing */
            const uint8_t window_alive = (ctx->gesture.window_left != 0u);
            if (cc->mode == ELIB_KEY_COMBO_LONG &&
                cc->threshold_count > 0) {
                /* Windowed LONG: releasing inside a co-held window fires,
                 * past the cap or after the overall window expired fires
                 * nothing (claims were held to here — nothing leaks) */
                const uint16_t cap =
                    cc->long_press_thresholds[cc->threshold_count - 1];
                if (window_alive &&
                    ctx->gesture.hold_ms >=
                        cc->long_press_thresholds[0] &&
                    ctx->gesture.hold_ms < cap) {
                    lnk->on_combo(cc->combo_id, cc->mode);
                }
            } else if (!ctx->gesture.bit_flags.fired && window_alive) {
                /* Legacy LONG: fire once when the last participant
                 * released. HOLD_PRESS reported its trigger at match time
                 * (fired == 1) and only terminates its claims here */
                lnk->on_combo(cc->combo_id, cc->mode);
            }
            for_each_member_claim(ctx, drv, cc, 0);
            memset(&ctx->gesture, 0, sizeof(ctx->gesture));
        }
    }

    /* 2) Unlocked: try to match a combo (configuration order = priority) */
    if (!ctx->gesture.bit_flags.locked && lnk != NULL) {
        for (uint8_t c = 0; c < lnk->combo_count; c++) {
            const elib_key_combo_cfg_t *cc = &lnk->combos[c];
            if (cc->mode == ELIB_KEY_COMBO_LONG) {
                if (cc->threshold_count > 0) {
                    /* Windowed LONG claims the moment every participant is
                     * pressed; that instant is the co-held timing origin */
                    if (!all_pressed(ctx, drv, cc->combo_keys,
                                     cc->combo_count)) {
                        continue;
                    }
                } else if (!all_hold_past_first(ctx, drv, cc->combo_keys,
                                                cc->combo_count)) {
                    continue;
                }
                /* Overall window: all participants must press inside the
                 * gesture window measured from the first press */
                if (!press_span_in_window(ctx, drv, cc->combo_keys,
                                          cc->combo_count, cc->window_ms)) {
                    continue; /* presses stretched too far: not this combo;
                               * earlier keys keep their solo gesture */
                }
                ctx->gesture.locked_combo = c;
                ctx->gesture.bit_flags.fired = 0;
                ctx->gesture.hold_ms = 0;
                ctx->gesture.notified_idx = 0;
                {
                    /* Remaining release deadline, measured from the claim
                     * instant so the total (first press -> last release)
                     * stays inside the window */
                    uint32_t spent =
                        max_press_age_ms(ctx, drv, cc->combo_keys,
                                         cc->combo_count);
                    ctx->gesture.window_left =
                        (cc->window_ms == 0)
                            ? 0xFFFFu
                            : (uint16_t)(cc->window_ms - spent);
                }
                ctx->gesture.bit_flags.locked = 1;
                for_each_member_claim(ctx, drv, cc, 1);
                break;
            } else if (cc->mode == ELIB_KEY_COMBO_HOLD_PRESS) {
                if (all_hold_past_first(ctx, drv, cc->hold_keys,
                                        cc->hold_count) &&
                    all_click_captured(ctx, drv, cc->press_keys,
                                       cc->press_count)) {
                    lnk->on_combo(cc->combo_id, cc->mode);
                    /* Hold keys stay claimed until released */
                    set_claimed(ctx, drv, cc->hold_keys, cc->hold_count);
                    for (uint8_t k = 0; k < cc->press_count; k++) {
                        abort_click_sequence(ctx, cc->press_keys[k]);
                    }
                    ctx->gesture.locked_combo = c;
                    ctx->gesture.bit_flags.fired = 1;
                    ctx->gesture.bit_flags.locked = 1;
                    /* HOLD_PRESS does not participate in the overall
                     * gesture window: its hold keys were pressed long
                     * before the press keys (that is the gesture itself),
                     * so a press-span check would never pass. Unbounded */
                    ctx->gesture.window_left = 0xFFFFu;
                    break;
                }
            } else { /* ELIB_KEY_COMBO_SHORT: pure overlap, fires instantly */
                if (all_click_captured(ctx, drv, cc->combo_keys,
                                       cc->combo_count)) {
                    /* Overall window: the whole press span (first -> last
                     * captured press) must sit inside it. Out of window:
                     * skip this combo but keep trying others — the members
                     * keep their own classification (the first arriver's
                     * gesture proceeds on its own; latecomers drop as
                     * ignored followers). NOT a break: a later combo may
                     * still match */
                    if (!press_span_in_window(ctx, drv, cc->combo_keys,
                                              cc->combo_count,
                                              cc->window_ms)) {
                        continue;
                    }
                    lnk->on_combo(cc->combo_id, cc->mode);
                    for (uint8_t k = 0; k < cc->combo_count; k++) {
                        abort_click_sequence(ctx, cc->combo_keys[k]);
                    }
                    break;
                }
            }
        }
    }

    /* 3) Follower resolution: follower released this scan and no combo took
     * it -> its classification is discarded entirely */
    for (uint8_t i = 0; i < drv->key_count; i++) {
        elib_key_state_t *ks = &ctx->key_states[i];
        if (ks->bit_flags.follower && ks->bit_flags.fell &&
            !ks->bit_flags.claimed) {
            /* Claiming this scan already cleared the follower bit; anything
             * still flagged is unmatched */
            const uint8_t key_id = drv->keys[i].key_id;
            discard_pending_for(ctx, key_id);
            abort_click_sequence(ctx, key_id);
            ks->bit_flags.follower = 0;
        }
    }

    /* 4) Same-scan multi-rise: no owner emerges; every rising key becomes a
     * follower (all invalid unless a combo claims them later) */
    uint8_t fresh_count = 0;
    for (uint8_t i = 0; i < drv->key_count; i++) {
        if (ctx->key_states[i].bit_flags.fresh) {
            fresh_count++;
        }
    }
    if (fresh_count >= 2) {
        for (uint8_t i = 0; i < drv->key_count; i++) {
            elib_key_state_t *ks = &ctx->key_states[i];
            if (ks->bit_flags.fresh && !ks->bit_flags.claimed) {
                ks->bit_flags.owner = 0;
                ks->bit_flags.follower = 1;
            }
        }
    }

    /* 5) Cleanup per-scan flags; classification is complete once a key
     * returns to IDLE, so its ownership of the channel ends there */
    for (uint8_t i = 0; i < drv->key_count; i++) {
        elib_key_state_t *ks = &ctx->key_states[i];
        ks->bit_flags.fresh = 0;
        ks->bit_flags.fell = 0;
        if (ks->state == ELIB_KEY_STATE_IDLE) {
            ks->bit_flags.owner = 0;
        }
    }
}


#include "countdown.h"
#include "soc/timer_group_reg.h"

// ── Timer configuration ───────────────────────────────────────────
// Timer Group 0, Timer 1 — separate from ultrasonic's Timer 0
// Same divider: 80 MHz / 8000 = 10,000 ticks/sec → 1 tick = 0.1 ms
#define TIMER_DIVIDER_VALUE  8000
#define TIMER_INCREMENT_MODE (1 << 30)
#define TIMER_ENABLE         (1 << 31)
#define MS_TO_TICKS(ms)      ((ms) * 10UL)

// ── Private state ─────────────────────────────────────────────────
static bool     _active        = false;
static bool     _expired       = false;
static uint32_t _startTick     = 0;
static uint32_t _durationTicks = 0;

// ── Private helper ────────────────────────────────────────────────
static uint32_t _readTimer(void) {
    *((volatile uint32_t *)TIMG_T1UPDATE_REG(0)) = 1;
    return *((volatile uint32_t *)TIMG_T1LO_REG(0));
}

void Countdown_init(void) {
    uint32_t timer_config  = (TIMER_DIVIDER_VALUE << 13);
    timer_config          |= TIMER_INCREMENT_MODE;
    timer_config          |= TIMER_ENABLE;
    *((volatile uint32_t *)TIMG_T1CONFIG_REG(0)) = timer_config;
    *((volatile uint32_t *)TIMG_T1UPDATE_REG(0)) = 1;
}

void Countdown_start(uint32_t durationMs) {
    _startTick     = _readTimer();
    _durationTicks = MS_TO_TICKS(durationMs);
    _expired       = false;
    _active        = true;
}

void Countdown_cancel(void) {
    _active  = false;
    _expired = false;
}

uint32_t Countdown_getSecondsRemaining(void) {
    if (!_active) return 0;

    uint32_t elapsed = _readTimer() - _startTick;
    if (elapsed >= _durationTicks) return 0;

    // Convert remaining ticks back to whole seconds (10,000 ticks/sec)
    return (_durationTicks - elapsed) / 10000UL;
}

bool Countdown_hasExpired(void) {
    if (!_active) return false;

    uint32_t elapsed = _readTimer() - _startTick;
    if (elapsed >= _durationTicks) {
        _active  = false;
        _expired = true;
    }

    if (_expired) {
        _expired = false; // clear flag after read
        return true;
    }
    return false;
}

bool Countdown_isActive(void) {
    return _active;
}
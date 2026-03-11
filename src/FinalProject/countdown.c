/**
 * @file countdown.c
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-01
 * @brief Implementation of the hardware-timer-based countdown driver.
 *
 * Uses Timer Group 0, Timer 1 with an 8000 divider (10,000 ticks/sec).
 * Elapsed time is computed by comparing the current tick count against a
 * recorded start tick, avoiding any need for interrupts or timer resets.
 */

// ========================== Includes ===============================

#include "countdown.h"
#include "soc/timer_group_reg.h"

// ========================== Macros =================================

#define TIMER_DIVIDER_VALUE  8000       /**< 80 MHz / 8000 = 10,000 ticks/sec (0.1 ms/tick). */
#define TIMER_INCREMENT_MODE (1 << 30)  /**< Timer count-up mode bit in TCONFIG register. */
#define TIMER_ENABLE         (1 << 31)  /**< Timer enable bit in TCONFIG register. */
#define MS_TO_TICKS(ms)      ((ms) * 10UL) /**< Convert milliseconds to timer ticks. */

// ======================== Global Variables =========================

static bool     _active        = false; /**< Whether a countdown is currently running. */
static bool     _expired       = false; /**< Latched expiry flag, cleared after one read. */
static uint32_t _startTick     = 0;     /**< Timer tick recorded when the countdown started. */
static uint32_t _durationTicks = 0;     /**< Total countdown duration expressed in ticks. */

// ====================== Function Prototypes ========================

static uint32_t _readTimer(void);

// ====================== Function Implementations ===================

// See countdown.h for full interface documentation.
void Countdown_init(void) {
    uint32_t timer_config  = (TIMER_DIVIDER_VALUE << 13);
    timer_config          |= TIMER_INCREMENT_MODE;
    timer_config          |= TIMER_ENABLE;
    *((volatile uint32_t *)TIMG_T1CONFIG_REG(0)) = timer_config;
    *((volatile uint32_t *)TIMG_T1UPDATE_REG(0)) = 1;
}

// See countdown.h for full interface documentation.
void Countdown_start(uint32_t durationMs) {
    _startTick     = _readTimer();
    _durationTicks = MS_TO_TICKS(durationMs);
    _expired       = false;
    _active        = true;
}

// See countdown.h for full interface documentation.
void Countdown_cancel(void) {
    _active  = false;
    _expired = false;
}

// See countdown.h for full interface documentation.
uint32_t Countdown_getSecondsRemaining(void) {
    if (!_active) return 0;

    uint32_t elapsed = _readTimer() - _startTick;
    if (elapsed >= _durationTicks) return 0;

    // 10,000 ticks/sec — integer divide gives whole seconds remaining
    return (_durationTicks - elapsed) / 10000UL;
}

// See countdown.h for full interface documentation.
bool Countdown_hasExpired(void) {
    if (!_active) return false;

    uint32_t elapsed = _readTimer() - _startTick;
    if (elapsed >= _durationTicks) {
        _active  = false;
        _expired = true;
    }

    if (_expired) {
        _expired = false;
        return true;
    }
    return false;
}

// See countdown.h for full interface documentation.
bool Countdown_isActive(void) {
    return _active;
}

/**
 * @brief Latches and reads the low 32 bits of Timer Group 0, Timer 1.
 *
 * Writing 1 to TIMG_T1UPDATE_REG snapshots the live counter into the
 * readable lo/hi registers before the read.
 *
 * @return Current timer tick count (low 32 bits).
 */
static uint32_t _readTimer(void) {
    *((volatile uint32_t *)TIMG_T1UPDATE_REG(0)) = 1;
    return *((volatile uint32_t *)TIMG_T1LO_REG(0));
}
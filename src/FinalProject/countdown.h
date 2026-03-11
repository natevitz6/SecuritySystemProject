/**
 * @file countdown.h
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Public interface for the hardware-timer-based countdown driver.
 *
 * Provides a single countdown timer using Timer Group 0, Timer 1
 * (separate from the ultrasonic driver which uses Timer 0). Resolution
 * is 0.1 ms per tick (80 MHz / 8000 divider = 10,000 ticks/sec).
 * Intended to be driven from Countdown_Task.
 */

#ifndef COUNTDOWN_H
#define COUNTDOWN_H

// ========================== Includes ===============================

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// ====================== Function Prototypes ========================

/**
 * @brief Initializes Timer Group 0, Timer 1 for countdown use.
 *
 * Must be called once in @c setup() before any other Countdown functions.
 */
void Countdown_init(void);

/**
 * @brief Starts a countdown from the given duration.
 *
 * Overwrites any countdown currently in progress.
 *
 * @param durationMs  Countdown duration in milliseconds.
 */
void Countdown_start(uint32_t durationMs);

/**
 * @brief Cancels the active countdown without triggering expiry.
 */
void Countdown_cancel(void);

/**
 * @brief Returns the number of whole seconds remaining in the countdown.
 *
 * @return Seconds remaining, or 0 if the countdown is inactive or expired.
 */
uint32_t Countdown_getSecondsRemaining(void);

/**
 * @brief Checks whether the countdown has expired.
 *
 * Clears the expiry flag on read, so this returns @c true only once
 * per expiry event.
 *
 * @return @c true  if the countdown expired since the last call.
 * @return @c false otherwise.
 */
bool Countdown_hasExpired(void);

/**
 * @brief Returns whether a countdown is currently running.
 *
 * @return @c true  if a countdown is active.
 * @return @c false otherwise.
 */
bool Countdown_isActive(void);

#ifdef __cplusplus
}
#endif

#endif // COUNTDOWN_H
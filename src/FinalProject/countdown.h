#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Call once in setup() — initializes Timer Group 0, Timer 1
void Countdown_init(void);

// Start a countdown from durationMs milliseconds
void Countdown_start(uint32_t durationMs);

// Cancel an active countdown
void Countdown_cancel(void);

// Returns whole seconds remaining (0 if not active)
uint32_t Countdown_getSecondsRemaining(void);

// Returns true if the countdown has expired since last call (clears the flag)
bool Countdown_hasExpired(void);

// Returns true if a countdown is currently running
bool Countdown_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
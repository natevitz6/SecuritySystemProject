/**
 * @file pir.c
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-01
 * @brief Implementation of the PIR motion sensor driver with software debouncing.
 *
 * Implements a consecutive-sample debounce algorithm to filter the noisy
 * digital output of a PIR sensor. The sensor output must remain at the same
 * logic level for DEBOUNCE_COUNT consecutive calls to PIR_update() before the
 * stable state is updated. At ~32 Hz this means a state change requires ~93 ms
 * of consistent signal before it is committed.
 */

// ========================== Includes ===============================

#include "pir.h"

// ========================== Macros =================================

/** @brief Number of consecutive identical reads required to commit a state change.
 *
 *  At a ~32 Hz polling rate this corresponds to roughly 93 ms of stable signal,
 *  which is long enough to reject the brief glitches common on PIR outputs while
 *  still responding quickly to genuine motion events.
 */
#define DEBOUNCE_COUNT 3

// ======================== Global Variables =========================

static uint8_t _inputPin;       /**< GPIO pin connected to the PIR sensor output. */
static uint8_t _ledPin;         /**< GPIO pin connected to the motion-indicator LED. */
static bool    _stableState    = false; /**< Debounced, confirmed motion state reported to callers. */
static bool    _candidateState = false; /**< New signal level currently being evaluated for stability. */
static uint8_t _confirmCount   = 0;    /**< Consecutive reads that match _candidateState so far. */

// ====================== Function Implementations ===================

// See pir.h for full interface documentation.
void PIR_init(uint8_t inputPin, uint8_t ledPin) {
    _inputPin = inputPin;
    _ledPin   = ledPin;
    pinMode(_ledPin,   OUTPUT);
    pinMode(_inputPin, INPUT);
}

// See pir.h for full interface documentation.
void PIR_update(void) {
    bool rawVal = (digitalRead(_inputPin) == HIGH);

    if (rawVal == _candidateState) {
        // Signal is holding steady — advance the confirmation counter,
        // capping it at DEBOUNCE_COUNT to avoid overflow on long stable reads.
        if (_confirmCount < DEBOUNCE_COUNT) {
            _confirmCount++;
        }

        // Only commit to the new stable state once we have seen enough
        // consecutive matching samples.
        if (_confirmCount >= DEBOUNCE_COUNT) {
            _stableState = _candidateState;
        }
    } else {
        // Signal changed direction — discard accumulated confirmation count
        // and start tracking the new candidate level from scratch.
        _candidateState = rawVal;
        _confirmCount   = 1;
    }

    // Mirror the debounced stable state on the indicator LED so the user
    // gets immediate physical feedback without any extra logic in the task.
    digitalWrite(_ledPin, _stableState ? HIGH : LOW);
}

// See pir.h for full interface documentation.
bool PIR_isMotionDetected(void) {
    return _stableState;
}
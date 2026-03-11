/**
 * @file ir_remote.h
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Public interface for the IR remote PIN entry driver.
 *
 * Decodes NEC IR signals from a remote control and manages a PIN entry buffer.
 * Includes cooldown debouncing to suppress repeated codes from a held button.
 * Intended to be polled from IR_Task at ~64 Hz.
 */

#ifndef IR_REMOTE_H
#define IR_REMOTE_H

// ========================== Includes ===============================

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// ====================== Function Prototypes ========================

/**
 * @brief Initializes the IR receiver.
 *
 * Must be called once in @c setup() before any calls to @c IRRemote_update().
 *
 * @param receivePin  GPIO pin connected to the IR receiver data line.
 */
void IRRemote_init(uint8_t receivePin);

/**
 * @brief Decodes the latest IR signal and updates the PIN entry buffer.
 *
 * Handles digit accumulation, clear, and OK (submit) commands. Repeated
 * identical codes within IR_COOLDOWN_MS are ignored. After this returns
 * @c true, call @c IRRemote_isPINCorrect() to evaluate the entered PIN.
 *
 * @return @c true  if the OK button was pressed and a PIN is ready to evaluate.
 * @return @c false otherwise.
 */
bool IRRemote_update(void);

/**
 * @brief Checks whether the entered PIN matches the stored PIN.
 *
 * Resets the digit buffer regardless of the result. Only valid immediately
 * after @c IRRemote_update() returns @c true.
 *
 * @return @c true  if the entered PIN matches CORRECT_PIN.
 * @return @c false if it does not match or fewer than PIN_LENGTH digits were entered.
 */
bool IRRemote_isPINCorrect(void);

/**
 * @brief Returns whether the clear button was pressed during the last update.
 *
 * @return @c true if the clear button was pressed this cycle.
 * @return @c false otherwise.
 */
bool IRRemote_wasClearPressed(void);

/**
 * @brief Returns whether the disarm button was pressed during the last update.
 *
 * @return @c true if the disarm button was pressed this cycle.
 * @return @c false otherwise.
 */
bool IRRemote_wasDisarmPressed(void);

/**
 * @brief Returns the number of digits currently in the PIN entry buffer.
 *
 * Useful for driving a masked PIN progress display on the LCD.
 *
 * @return Number of digits entered so far (0 to PIN_LENGTH).
 */
uint8_t IRRemote_getDigitCount(void);

/**
 * @brief Copies the current PIN entry buffer into a caller-supplied array.
 *
 * @param buf  Pointer to the destination array.
 * @param len  Number of elements to copy (capped at PIN_LENGTH).
 */
void IRRemote_getEnteredPIN(uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif // IR_REMOTE_H
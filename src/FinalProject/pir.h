/**
 * @file pir.h
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-01
 * @brief Public interface for the PIR (Passive Infrared) motion sensor driver.
 *
 * This module provides initialization and polling functions for a PIR motion
 * sensor. It implements software debouncing to filter out spurious transitions
 * and drives an indicator LED that mirrors the debounced motion state.
 * Intended to be called from a dedicated FreeRTOS task (PIR_Task) at ~32 Hz.
 */

#ifndef PIR_H
#define PIR_H

// ========================== Includes ===============================

#ifdef __cplusplus
extern "C" {
#endif

#include <Arduino.h>

// ========================= Function Prototypes =====================

/**
 * @brief Initializes the PIR sensor and its associated indicator LED.
 *
 * Configures the given GPIO pins and sets the internal debounce state to
 * "no motion detected". Must be called once in @c setup() before any calls
 * to @c PIR_update().
 *
 * @param inputPin  GPIO pin number connected to the PIR sensor output.
 * @param ledPin    GPIO pin number connected to the motion-indicator LED.
 */
void PIR_init(uint8_t inputPin, uint8_t ledPin);

/**
 * @brief Samples the PIR sensor and updates the debounced motion state.
 *
 * Reads the raw digital value from the PIR input pin and runs a
 * consecutive-sample debounce algorithm. The stable motion state is only
 * updated after @c DEBOUNCE_COUNT identical consecutive reads. The indicator
 * LED is driven HIGH when motion is confirmed and LOW otherwise.
 *
 * Should be called periodically from @c PIR_Task at ~32 Hz (every ~31 ms).
 */
void PIR_update(void);

/**
 * @brief Returns the current debounced motion detection state.
 *
 * Reports the last stable state committed by @c PIR_update(). This value
 * does not change between calls to @c PIR_update().
 *
 * @return @c true  if motion is currently detected (debounced HIGH signal).
 * @return @c false if no motion is detected.
 */
bool PIR_isMotionDetected(void);

#ifdef __cplusplus
}
#endif

#endif // PIR_H
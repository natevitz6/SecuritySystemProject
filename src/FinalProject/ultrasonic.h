/**
 * @file ultrasonic.h
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Public interface for the HC-SR04 ultrasonic distance sensor driver.
 *
 * Provides distance measurement and loitering detection using a hardware timer
 * (Timer Group 0, Timer 0) for tick-based timing. Intended to be polled from
 * Ultrasonic_Task at 50 Hz.
 */

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

// ========================== Includes ===============================

#ifdef __cplusplus
extern "C" {
#endif

#include <Arduino.h>
#include <stdint.h>

// ====================== Function Prototypes ========================

/**
 * @brief Initializes the ultrasonic sensor pins and hardware timer.
 *
 * Configures the trigger pin as OUTPUT and echo pin as INPUT, then starts
 * Timer Group 0, Timer 0 with an 8000 divider (10,000 ticks/sec).
 *
 * @param trigPin  GPIO pin connected to the sensor TRIG line.
 * @param echoPin  GPIO pin connected to the sensor ECHO line.
 */
void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin);

/**
 * @brief Triggers a distance measurement and updates the internal result.
 *
 * Sends a 10 µs trigger pulse and measures the echo pulse width via
 * @c pulseIn(). Should be called at 50 Hz from Ultrasonic_Task.
 */
void Ultrasonic_update(void);

/**
 * @brief Returns the most recent distance measurement.
 *
 * @return Distance in centimeters from the last call to @c Ultrasonic_update().
 */
int Ultrasonic_getDistance(void);

/**
 * @brief Detects whether an object has been within range for too long.
 *
 * Uses the hardware timer to track how long the measured distance has
 * continuously stayed at or below @p distanceThresholdCm. Resets the timer
 * whenever the object moves out of range.
 *
 * @param distanceThresholdCm  Maximum distance (cm) that counts as "close".
 * @param timeLimitMs          Duration (ms) before loitering is declared.
 * @return @c true if an object has been within range for longer than @p timeLimitMs.
 * @return @c false otherwise.
 */
bool Ultrasonic_isLoitering(int distanceThresholdCm, unsigned long timeLimitMs);

#ifdef __cplusplus
}
#endif

#endif // ULTRASONIC_H
#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <Arduino.h>

// Initialize ultrasonic sensor pins
void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin);

// Update sensor reading (call in loop)
void Ultrasonic_update(void);

// Get the latest distance reading in cm
int Ultrasonic_getDistance(void);

// Check if someone is loitering (distance < threshold for longer than time limit)
bool Ultrasonic_isLoitering(int distanceThresholdCm, unsigned long timeLimitMs);

#endif

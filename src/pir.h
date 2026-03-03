#ifndef PIR_H
#define PIR_H

#include <Arduino.h>

// Initialize PIR sensor
void PIR_init(uint8_t inputPin, uint8_t ledPin);

// Call repeatedly in loop()
void PIR_update(void);

// Optional: check current motion state
bool PIR_isMotionDetected(void);

#endif

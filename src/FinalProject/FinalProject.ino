#include "pir.h"

// Define pins
#define PIR_PIN 2
#define LED_PIN 13

void setup() {
    Serial.begin(9600);        // Initialize serial monitor
    PIR_init(PIR_PIN, LED_PIN); // Initialize PIR module
}

void loop() {
    PIR_update();              // Update PIR sensor state

    // Optional: check state
    if (PIR_isMotionDetected()) {
        // For testing, print repeatedly
        Serial.println("Motion is currently detected!");
    }

    delay(500);  // Half-second delay to slow serial output for readability
}


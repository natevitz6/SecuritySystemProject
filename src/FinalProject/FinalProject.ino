#include "pir.h"
#include "ultrasonic.h"

// Define pins
#define TRIG_PIN 2
#define ECHO_PIN 1
#define LOITER_DISTANCE_CM 100   // distance threshold for approach
#define LOITER_TIME_MS 5000      // 5 seconds to detect loitering
#define PIR_PIN 2
#define LED_PIN 13

extern "C" {
  void PIR_init(uint8_t inputPin, uint8_t ledPin);
  void PIR_update(void);
  bool PIR_isMotionDetected(void);
  void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin);
  void Ultrasonic_update(void);
  int Ultrasonic_getDistance(void);

}

void setup() {
    Serial.begin(9600);        // Initialize serial monitor
    PIR_init(PIR_PIN, LED_PIN); // Initialize PIR module
    Ultrasonic_init(TRIG_PIN, ECHO_PIN);
}

void loop() {
    PIR_update();              // Update PIR sensor state

    // Optional: check state
    if (PIR_isMotionDetected()) {
        // For testing, print repeatedly
        Serial.println("Motion is currently detected!");
    }

    delay(500);  // Half-second delay to slow serial output for readability

    Ultrasonic_update();

    int dist = Ultrasonic_getDistance();
    Serial.print("Current distance: ");
    Serial.println(dist);

    if (Ultrasonic_isLoitering(LOITER_DISTANCE_CM, LOITER_TIME_MS))
    {
        Serial.println("Loitering detected! Show 'Enter PIN' on display.");
        // TODO: trigger display message in your security system
    }

    delay(50); // small delay for sensor stability
}


#include "pir.h"

// Static variables (private to this file)
static uint8_t _inputPin;
static uint8_t _ledPin;
static int _pirState = LOW;
static int _val = 0;

void PIR_init(uint8_t inputPin, uint8_t ledPin) {
    _inputPin = inputPin;
    _ledPin = ledPin;

    pinMode(_ledPin, OUTPUT);
    pinMode(_inputPin, INPUT);

    Serial.begin(9600);
}

void PIR_update(void) {
    _val = digitalRead(_inputPin);

    if (_val == HIGH) {
        digitalWrite(_ledPin, HIGH);

        if (_pirState == LOW) {
            Serial.println("Motion detected!");
            _pirState = HIGH;
        }
    } else {
        digitalWrite(_ledPin, LOW);
        if (_pirState == HIGH) {
            Serial.println("Motion ended!");
            _pirState = LOW;
        }
    }
}

bool PIR_isMotionDetected(void) {
    return (_pirState == HIGH);
}

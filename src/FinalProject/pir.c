#include "pir.h"

#define DEBOUNCE_COUNT 3  // must see same value 3 times in a row (~93ms at 32Hz)

static uint8_t _inputPin;
static uint8_t _ledPin;
static bool    _stableState    = false;  // the confirmed, debounced state
static bool    _candidateState = false;  // what we're currently seeing
static uint8_t _confirmCount   = 0;      // how many consecutive reads match candidate

void PIR_init(uint8_t inputPin, uint8_t ledPin) {
    _inputPin = inputPin;
    _ledPin   = ledPin;
    pinMode(_ledPin,   OUTPUT);
    pinMode(_inputPin, INPUT);
}

void PIR_update(void) {
    bool rawVal = (digitalRead(_inputPin) == HIGH);

    if (rawVal == _candidateState) {
        // Signal is holding — increment confirmation counter
        if (_confirmCount < DEBOUNCE_COUNT) {
            _confirmCount++;
        }
        // Once we've seen it stable long enough, commit to stable state
        if (_confirmCount >= DEBOUNCE_COUNT) {
            _stableState = _candidateState;
        }
    } else {
        // Signal changed — reset and start tracking the new value
        _candidateState = rawVal;
        _confirmCount   = 1;
    }

    digitalWrite(_ledPin, _stableState ? HIGH : LOW);
}

bool PIR_isMotionDetected(void) {
    return _stableState;
}
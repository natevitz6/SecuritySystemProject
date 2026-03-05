#include "ir_remote.h"
#include <IRremote.h>

// ── Configuration ────────────────────────────────────────────────
#define PIN_LENGTH 4
static const uint8_t CORRECT_PIN[PIN_LENGTH] = {1, 2, 3, 4}; // change to your PIN

// ── Replace these with your actual hex codes from IRTest.ino ─────
#define IR_CODE_0    0x19
#define IR_CODE_1    0x45 
#define IR_CODE_2    0x46
#define IR_CODE_3    0x47 
#define IR_CODE_4    0x44 
#define IR_CODE_5    0x40
#define IR_CODE_6    0x43 
#define IR_CODE_7    0x07 
#define IR_CODE_8    0x15
#define IR_CODE_9    0x09 
#define IR_CODE_OK   0x1C 
#define IR_CODE_CLR  0x16  
#define IR_CODE_DISARM 0x0D 

// ── Private state ─────────────────────────────────────────────────
static uint8_t  _receivePin;
static uint8_t  _enteredPIN[PIN_LENGTH];
static uint8_t  _digitCount    = 0;
static bool     _pinReady      = false;
static bool     _clearPressed  = false;

// ── Helper: map IR hex code to digit 0-9, returns 255 if not a digit
static uint8_t decodeDigit(uint8_t code) {
    switch (code) {
        case IR_CODE_0: return 0;
        case IR_CODE_1: return 1;
        case IR_CODE_2: return 2;
        case IR_CODE_3: return 3;
        case IR_CODE_4: return 4;
        case IR_CODE_5: return 5;
        case IR_CODE_6: return 6;
        case IR_CODE_7: return 7;
        case IR_CODE_8: return 8;
        case IR_CODE_9: return 9;
        default:        return 255; // not a digit
    }
}

void IRRemote_init(uint8_t receivePin) {
    _receivePin = receivePin;
    IrReceiver.begin(_receivePin, DISABLE_LED_FEEDBACK);
}

bool IRRemote_update(void) {
    _pinReady     = false;
    _clearPressed = false;

    if (!IrReceiver.decode()) {
        return false; // nothing received this cycle
    }

    uint8_t code = IrReceiver.decodedIRData.command;
    IrReceiver.resume();

    if (code == IR_CODE_CLR) {
        // Clear button — reset entry
        _digitCount   = 0;
        _clearPressed = true;
        return false;
    }

    if (code == IR_CODE_OK) {
        // Confirm button — mark PIN as ready for evaluation
        _pinReady = true;
        return true;
    }

    // Try to decode as a digit
    uint8_t digit = decodeDigit(code);
    if (digit != 255 && _digitCount < PIN_LENGTH) {
        _enteredPIN[_digitCount++] = digit;
    }

    return false;
}

bool IRRemote_isPINCorrect(void) {
    if (_digitCount != PIN_LENGTH) return false;
    for (uint8_t i = 0; i < PIN_LENGTH; i++) {
        if (_enteredPIN[i] != CORRECT_PIN[i]) {
            _digitCount = 0; // reset after failed attempt
            return false;
        }
    }
    _digitCount = 0; // reset after successful attempt
    return true;
}

bool IRRemote_wasClearPressed(void) {
    return _clearPressed;
}

uint8_t IRRemote_getDigitCount(void) {
    return _digitCount;
}

void IRRemote_getEnteredPIN(uint8_t *buf, uint8_t len) {
    for (uint8_t i = 0; i < len && i < PIN_LENGTH; i++) {
        buf[i] = _enteredPIN[i];
    }
}
/**
 * @file ir_remote.cpp
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Implementation of the IR remote PIN entry driver.
 *
 * IR hex codes are mapped to digits 0–9 and control keys (OK, CLR) via a
 * switch statement. To remap buttons, update the IR_CODE_* macros and
 * CORRECT_PIN to match your remote and desired PIN, then recompile.
 */

// ========================== Includes ===============================

#include "ir_remote.h"
#include <IRremote.hpp>
#include "esp_timer.h"

// ========================== Macros =================================

#define PIN_LENGTH      4    /**< Number of digits in the entry PIN. */
#define IR_COOLDOWN_MS  300  /**< Minimum ms between accepted identical IR codes. */

// Raw hex command codes for each button on the remote.
// Run IRTest.ino to discover the codes for your specific remote.
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
#define IR_CODE_OK   0x1C  /**< Submit / confirm PIN. */
#define IR_CODE_CLR  0x16  /**< Clear the current PIN entry buffer. */
#define IR_CODE_DISARM 0x0D

// ======================== Global Variables =========================

/** @brief Correct PIN to match against. Change digits to set your PIN. */
static const uint8_t CORRECT_PIN[PIN_LENGTH] = {1, 2, 3, 4};

static uint8_t  _receivePin;
static uint8_t  _enteredPIN[PIN_LENGTH]; /**< Buffer holding digits entered so far. */
static uint8_t  _digitCount    = 0;
static bool     _pinReady      = false;
static bool     _clearPressed  = false;
static uint32_t _lastCommandTime = 0;    /**< Timestamp (ms) of last accepted IR command. */
static uint8_t  _lastCode        = 0xFF; /**< Command code of the last accepted IR signal. */
static bool _disarmPressed = false;

// ====================== Function Prototypes ========================

static uint8_t decodeDigit(uint8_t code);

// ====================== Function Implementations ===================

void accessGranted() {
    _digitCount = 0;
}

// See ir_remote.h for full interface documentation.
void IRRemote_init(uint8_t receivePin) {
    _receivePin = receivePin;
    IrReceiver.begin(_receivePin, DISABLE_LED_FEEDBACK);
}

// See ir_remote.h for full interface documentation.
bool IRRemote_update(void) {
    _pinReady     = false;
    _clearPressed = false;
    _disarmPressed = false;

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    if (!IrReceiver.decode()) {
        if (((now - _lastCommandTime) > 3000) && _digitCount != 0) {
            _digitCount = 0;
            _clearPressed = true;
        } 
        return false;
    }

    uint8_t code = IrReceiver.decodedIRData.command;
    IrReceiver.resume();

    // Suppress repeated identical codes held within the cooldown window
    
    if ((code == _lastCode) && ((now - _lastCommandTime) < IR_COOLDOWN_MS)) {
        return false;
    }
    _lastCode        = code;
    _lastCommandTime = now;

    if (code == IR_CODE_CLR && _digitCount != 0) {
        _digitCount   = 0;
        _clearPressed = true;
        return false;
    }
    if (code == IR_CODE_OK) {
        _pinReady = true;
        return true;
    }
    if (code == IR_CODE_DISARM) {
        _disarmPressed = true;
        return true;
    }

    uint8_t digit = decodeDigit(code);
    if (digit != 255 && _digitCount < PIN_LENGTH) {
        _enteredPIN[_digitCount++] = digit;
    }
    return false;
}

// See ir_remote.h for full interface documentation.
bool IRRemote_isPINCorrect(void) {
    if (_digitCount != PIN_LENGTH) return false;
    for (uint8_t i = 0; i < PIN_LENGTH; i++) {
        if (_enteredPIN[i] != CORRECT_PIN[i]) {
            _digitCount = 0;
            return false;
        }
    }
    _digitCount = 0;
    return true;
}

// See ir_remote.h for full interface documentation.
bool IRRemote_wasClearPressed(void) {
    return _clearPressed;
}

bool IRRemote_wasDisarmPressed(void) {
    return _disarmPressed;
}

// See ir_remote.h for full interface documentation.
uint8_t IRRemote_getDigitCount(void) {
    return _digitCount;
}

// See ir_remote.h for full interface documentation.
void IRRemote_getEnteredPIN(uint8_t *buf, uint8_t len) {
    for (uint8_t i = 0; i < len && i < PIN_LENGTH; i++) {
        buf[i] = _enteredPIN[i];
    }
}

/**
 * @brief Maps a raw IR command byte to a decimal digit 0–9.
 *
 * @param code  Raw IR command byte from the decoded signal.
 * @return Digit value 0–9, or 255 if the code does not map to a digit.
 */
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
        default:        return 255;
    }
}
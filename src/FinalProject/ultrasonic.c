/**
 * @file ultrasonic.c
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Implementation of the HC-SR04 ultrasonic distance sensor driver.
 *
 * Uses Timer Group 0, Timer 0 (80 MHz / 8000 divider = 10,000 ticks/sec)
 * for loitering detection timing. Distance is computed from the echo pulse
 * duration using the standard speed-of-sound formula.
 */

// ========================== Includes ===============================

#include "ultrasonic.h"
#include "soc/timer_group_reg.h"
#include "esp_timer.h"
#include "ultrasonic.h"
#include "soc/timer_group_reg.h"
// ========================== Macros =================================
// Use Timer GROUP 1 (not 0 — that's used by FreeRTOS)
#define TIMER_GROUP      1
#define TIMER_DIVIDER    8000   // 80MHz / 8000 = 10,000 ticks/sec (0.1ms per tick)
// ======================== Global Variables =========================

static uint8_t  _trigPin;
static uint8_t  _echoPin;
static long     _duration;
static int      _distance;
static bool     _wasClose          = false; /**< Whether the object was in range last update. */
static uint64_t _approachStartUs = 0;
static uint64_t _seenLastUs      = 0;
// ====================== Function Prototypes ========================


// ====================== Function Implementations ===================

// See ultrasonic.h for full interface documentation.




void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin) {
    _trigPin = trigPin;
    _echoPin = echoPin;
    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);

    // Reset the timer register first
    *((volatile uint32_t *)TIMG_T0CONFIG_REG(TIMER_GROUP)) = 0;

    // Set divider [28:13], count-up [30], enable [31]
    uint32_t cfg = 0;
    cfg |= ((uint32_t)TIMER_DIVIDER << 13);
    cfg |= (1UL << 30);  // count up
    cfg |= (1UL << 31);  // enable
    *((volatile uint32_t *)TIMG_T0CONFIG_REG(TIMER_GROUP)) = cfg;

    // Load 0 into the counter
    *((volatile uint32_t *)TIMG_T0LOADLO_REG(TIMER_GROUP)) = 0;
    *((volatile uint32_t *)TIMG_T0LOADHI_REG(TIMER_GROUP)) = 0;
    *((volatile uint32_t *)TIMG_T0LOAD_REG(TIMER_GROUP))   = 1;

    // Latch initial value
    *((volatile uint32_t *)TIMG_T0UPDATE_REG(TIMER_GROUP)) = 1;
}

// See ultrasonic.h for full interface documentation.

void Ultrasonic_update(void) {
    // Send 10 µs trigger pulse
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(5);
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);

    _duration = pulseIn(_echoPin, HIGH, 15000);
    if (_duration == 0) {
        _distance = 999;
    } else {
        _distance = _duration * 0.034 / 2; // speed of sound: 0.034 cm/µs, divide by 2 for round-trip
    }
}

// See ultrasonic.h for full interface documentation.
int Ultrasonic_getDistance(void) {
    return _distance;
}


bool Ultrasonic_isLoitering(int distanceThresholdCm, uint32_t timeLimitMs) {
    uint64_t nowUs     = readTimer(); // microseconds since boot
    uint64_t limitUs   = (uint64_t)timeLimitMs * 10ULL;

    if (_distance <= distanceThresholdCm) {
        if (!_wasClose) {
            _approachStartUs = nowUs;
            _wasClose = true;
        }
        _seenLastUs = nowUs;

        if ((nowUs - _approachStartUs) >= limitUs) {
            _wasClose = false;
            return true;
        }
    } else if ((nowUs - _seenLastUs) > 50000ULL) { // 500ms grace period
        _wasClose = false;
        _approachStartUs = 0;
    }

    return false;
}

/**
 * @brief Latches and reads the low 32 bits of Timer Group 0, Timer 0.
 *
 * Writing 1 to TIMG_T0UPDATE_REG snapshots the live counter into the
 * readable lo/hi registers before the read.
 *
 * @return Current timer tick count (low 32 bits).
 */
uint32_t readTimer(void) {
    *((volatile uint32_t *)TIMG_T0UPDATE_REG(TIMER_GROUP)) = 1;
    return *((volatile uint32_t *)TIMG_T0LO_REG(TIMER_GROUP));
}
/**
 * @file ultrasonic.c
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-01
 * @brief Implementation of the HC-SR04 ultrasonic distance sensor driver.
 *
 * Uses Timer Group 0, Timer 0 (80 MHz / 8000 divider = 10,000 ticks/sec)
 * for loitering detection timing. Distance is computed from the echo pulse
 * duration using the standard speed-of-sound formula.
 */

// ========================== Includes ===============================

#include "ultrasonic.h"
#include "soc/timer_group_reg.h"

// ========================== Macros =================================

#define TIMER_DIVIDER_VALUE   8000      /**< 80 MHz / 8000 = 10,000 ticks/sec (0.1 ms/tick). */
#define TIMER_INCREMENT_MODE  (1 << 30) /**< Timer count-up mode bit in TCONFIG register. */
#define TIMER_ENABLE          (1 << 31) /**< Timer enable bit in TCONFIG register. */
#define MS_TO_TICKS(ms)       ((ms) * 10UL) /**< Convert milliseconds to timer ticks. */

// ======================== Global Variables =========================

static uint8_t  _trigPin;
static uint8_t  _echoPin;
static long     _duration;
static int      _distance;
static bool     _wasClose          = false; /**< Whether the object was in range last update. */
static uint32_t _approachStartTick = 0;     /**< Timer tick when object first entered range. */

// ====================== Function Prototypes ========================

static uint32_t _readTimer(void);

// ====================== Function Implementations ===================

// See ultrasonic.h for full interface documentation.
void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin) {
    _trigPin = trigPin;
    _echoPin = echoPin;

    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);

    // Configure Timer Group 0, Timer 0
    uint32_t timer_config = (TIMER_DIVIDER_VALUE << 13);
    timer_config |= TIMER_INCREMENT_MODE;
    timer_config |= TIMER_ENABLE;
    *((volatile uint32_t *)TIMG_T0CONFIG_REG(0)) = timer_config;
    *((volatile uint32_t *)TIMG_T0UPDATE_REG(0)) = 1;
}

// See ultrasonic.h for full interface documentation.
void Ultrasonic_update(void) {
    // Send 10 µs trigger pulse
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(5);
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);

    _duration = pulseIn(_echoPin, HIGH, 1000000);
    _distance = _duration * 0.034 / 2; // speed of sound: 0.034 cm/µs, divide by 2 for round-trip
}

// See ultrasonic.h for full interface documentation.
int Ultrasonic_getDistance(void) {
    return _distance;
}

// See ultrasonic.h for full interface documentation.
bool Ultrasonic_isLoitering(int distanceThresholdCm, unsigned long timeLimitMs) {
    uint32_t currentTick = _readTimer();
    uint32_t limitTicks  = (uint32_t)MS_TO_TICKS(timeLimitMs);

    if (_distance <= distanceThresholdCm) {
        if (!_wasClose) {
            _approachStartTick = currentTick;
            _wasClose = true;
        }
        if ((currentTick - _approachStartTick) >= limitTicks) {
            return true;
        }
    } else {
        _wasClose = false;
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
static uint32_t _readTimer(void) {
    *((volatile uint32_t *)TIMG_T0UPDATE_REG(0)) = 1;
    return *((volatile uint32_t *)TIMG_T0LO_REG(0));
}
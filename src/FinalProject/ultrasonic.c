#include "ultrasonic.h"
#include "soc/timer_group_reg.h"

#define TIMER_DIVIDER_VALUE   8000
#define TIMER_INCREMENT_MODE  (1 << 30)
#define TIMER_ENABLE          (1 << 31)
#define MS_TO_TICKS(ms)       ((ms) * 10UL)

// Private static variables
static uint8_t _trigPin;
static uint8_t _echoPin;
static long _duration;
static int _distance;
static bool _wasClose = false;
static uint32_t _approachStartTick = 0;
static uint32_t _readTimer(void); // forward declaration

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
    *((volatile uint32_t *)TIMG_T0UPDATE_REG(0)) = 1; // load settings
}

void Ultrasonic_update(void) {
    // Trigger sensor
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(5);
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);

    // Read echo
    _duration = pulseIn(_echoPin, HIGH, 1000000);

    // Calculate distance in cm
    _distance = _duration * 0.034 / 2;
}

int Ultrasonic_getDistance(void) {
    return _distance;
}

bool Ultrasonic_isLoitering(int distanceThresholdCm, unsigned long timeLimitMs) {
    uint32_t currentTick  = _readTimer();
    uint32_t limitTicks   = (uint32_t)MS_TO_TICKS(timeLimitMs);

    if (_distance <= distanceThresholdCm) {
        if (!_wasClose) {
            _approachStartTick = currentTick; // start timer
            _wasClose = true;
        }

        if ((currentTick - _approachStartTick) >= limitTicks) {
            return true; // loitering detected
        }
    }
    else {
        _wasClose = false; // reset
    }

    return false;
}

static uint32_t _readTimer(void) {
    *((volatile uint32_t *)TIMG_T0UPDATE_REG(0)) = 1;
    return *((volatile uint32_t *)TIMG_T0LO_REG(0));
}
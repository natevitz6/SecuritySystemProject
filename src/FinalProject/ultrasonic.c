#include "ultrasonic.h"

// Private static variables
static uint8_t _trigPin;
static uint8_t _echoPin;
static long _duration;
static int _distance;
static unsigned long _approachStartTime = 0;
static bool _wasClose = false;

void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin)
{
    _trigPin = trigPin;
    _echoPin = echoPin;

    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);

}

void Ultrasonic_update(void)
{
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

int Ultrasonic_getDistance(void)
{
    return _distance;
}

bool Ultrasonic_isLoitering(int distanceThresholdCm, unsigned long timeLimitMs)
{
    unsigned long currentTime = millis();

    if (_distance <= distanceThresholdCm)
    {
        if (!_wasClose)
        {
            _approachStartTime = currentTime; // start timer
            _wasClose = true;
        }

        if ((currentTime - _approachStartTime) >= timeLimitMs)
        {
            return true; // loitering detected
        }
    }
    else
    {
        _wasClose = false; // reset timer
    }

    return false;
}

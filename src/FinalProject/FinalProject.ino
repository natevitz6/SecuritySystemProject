//
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "pir.h"
#include "ultrasonic.h"

// Define pins
#define TRIG_PIN 2
#define ECHO_PIN 1
#define LOITER_DISTANCE_CM 100   // distance threshold for approach
#define LOITER_TIME_MS 5000      // 5 seconds to detect loitering
#define PIR_PIN 2
#define LED_PIN 13

const uint8_t LCD_ADDR   = 0x27;
const uint8_t RS_BIT     = 0x01;
const uint8_t RW_BIT     = 0x02;
const uint8_t E_BIT      = 0x04;
const uint8_t BL_BIT     = 0x08;


LiquidCrystal_I2C lcd(0x27, 16, 2);

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
    Wire.setPins(47, 48);
    Wire.begin();
    lcd.init();
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


/**
* @brief Sends a raw byte to the LCD over I2C.
* @param value Byte to transmit.
* @return None
*/
void lcd_write_raw(uint8_t value) {
 Wire.beginTransmission(LCD_ADDR);
 Wire.write(value);
 Wire.endTransmission();
 delayMicroseconds(40);
}

/**
* @brief Pulses a 4-bit nibble to the LCD.
* @param nibble Upper 4 bits of data.
* @param control Control bits (RS, RW, BL).
*/
void lcd_pulse_nibble(uint8_t nibble, uint8_t control) {
 uint8_t out = (nibble & 0xF0) | control;
 lcd_write_raw(out | E_BIT);
 lcd_write_raw(out & ~E_BIT);
}

/**
* @brief Sends a command byte to the LCD.
* @param cmd Command byte.
*/
void lcd_command(uint8_t cmd) {
 uint8_t control = BL_BIT;
 lcd_pulse_nibble(cmd & 0xF0, control);
 lcd_pulse_nibble((cmd << 4) & 0xF0, control);
 delayMicroseconds(1600);
}

/**
* @brief Sends character data to the LCD.
* @param data ASCII character to display.
*/
void lcd_data(uint8_t data) {
 uint8_t control = BL_BIT | RS_BIT;
 lcd_pulse_nibble(data & 0xF0, control);
 lcd_pulse_nibble((data << 4) & 0xF0, control);
}

/**
* @brief Clears the LCD display.
*/
void lcd_clear() {
 lcd_command(0x01);
 delay(2);
}

/**
* @brief Moves cursor to first LCD line.
*/
void lcd_home_first_line() {
 lcd_command(0x80);
}

/**
* @brief Moves cursor to second LCD line.
*/
void lcd_second_line() {
 lcd_command(0xC0);
}
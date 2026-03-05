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

extern "C" {
  void PIR_init(uint8_t inputPin, uint8_t ledPin);
  void PIR_update(void);
  bool PIR_isMotionDetected(void);
  void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin);
  void Ultrasonic_update(void);
  int Ultrasonic_getDistance(void);

}

LiquidCrystal_I2C lcd(0x27, 16, 2);

// System Event Types
typedef enum {
    EVENT_PIR_MOTION,
    EVENT_PIR_CLEAR,
    EVENT_DISTANCE_UPDATE,
    EVENT_LOITERING,
    EVENT_DISPLAY_UPDATE
} system_event_t;

typedef struct {
    system_event_t type;
    int value;
    char displayMsg[32]; // For display messages (if needed)
} system_message_t;

// Queues
QueueHandle_t sensorQueue;
QueueHandle_t uiQueue;

void PIR_Task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(31); // ~32Hz

    while (1) {
        PIR_update();
        system_message_t msg;

        if (PIR_isMotionDetected()) {
            msg.type = EVENT_PIR_MOTION;
            msg.value = 1;
            xQueueSend(sensorQueue, &msg, 0);
        } else {
            msg.type = EVENT_PIR_CLEAR;
            msg.value = 0;
            xQueueSend(sensorQueue, &msg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void Ultrasonic_Task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz

    while (1) {
        Ultrasonic_update();

        int dist = Ultrasonic_getDistance();

        system_message_t msg;

        // Send distance update
        msg.type = EVENT_DISTANCE_UPDATE;
        msg.value = dist;
        xQueueSend(sensorQueue, &msg, 0);

        // Check loitering
        if (Ultrasonic_isLoitering(LOITER_DISTANCE_CM, LOITER_TIME_MS)) {
            msg.type = EVENT_LOITERING;
            msg.value = dist;
            xQueueSend(sensorQueue, &msg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void SecurityController_Task(void *pvParameters) {
    system_message_t msg;
    system_message_t uiMsg;

    while (1) {
        if (xQueueReceive(sensorQueue, &msg, portMAX_DELAY)) {
            switch (msg.type) {
                case EVENT_PIR_MOTION:
                    // Example: Show "Enter PIN" when motion detected
                    uiMsg.type = EVENT_DISPLAY_UPDATE;
                    snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Enter PIN:");
                    xQueueSend(uiQueue, &uiMsg, 0);
                    break;

                case EVENT_PIR_CLEAR:
                    // Example: Show "Idle" when no motion
                    uiMsg.type = EVENT_DISPLAY_UPDATE;
                    snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Idle");
                    xQueueSend(uiQueue, &uiMsg, 0);
                    break;

                case EVENT_DISTANCE_UPDATE:
                    break;

                case EVENT_LOITERING: // add alarm being set off with LED
                    uiMsg.type = EVENT_DISPLAY_UPDATE;
                    snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Loitering Detected");
                    xQueueSend(uiQueue, &uiMsg, 0);
                    break;

                default:
                    break;
            }
        }
    }
}

// LCD/UI Task: Handles all display updates
void LCD_Task(void *pvParameters) {
    system_message_t uiMsg;
    char lastMsg[32] = "";
    while (1) {
        if (xQueueReceive(uiQueue, &uiMsg, portMAX_DELAY)) {
            if (uiMsg.type == EVENT_DISPLAY_UPDATE) {
                if (strcmp(lastMsg, uiMsg.displayMsg) != 0) {
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print(uiMsg.displayMsg);
                    strncpy(lastMsg, uiMsg.displayMsg, sizeof(lastMsg));
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    Wire.begin();
    lcd.init();
    lcd.backlight();

    // Initialize modules
    PIR_init(PIR_PIN, LED_PIN);
    Ultrasonic_init(TRIG_PIN, ECHO_PIN);

    // Create queues
    sensorQueue = xQueueCreate(10, sizeof(system_message_t));
    uiQueue     = xQueueCreate(10, sizeof(system_message_t));

    // Create tasks pinned to cores
    xTaskCreatePinnedToCore(
        PIR_Task,
        "PIR Task",
        4096,
        NULL,
        2,
        NULL,
        0); // Core 0

    xTaskCreatePinnedToCore(
        Ultrasonic_Task,
        "Ultrasonic Task",
        4096,
        NULL,
        3,
        NULL,
        0); // Core 0

    xTaskCreatePinnedToCore(
        SecurityController_Task,
        "Security Controller",
        4096,
        NULL,
        1,
        NULL,
        1); // Core 1

    xTaskCreatePinnedToCore(
        LCD_Task,
        "LCD Task",
        4096,
        NULL,
        1,
        NULL,
        1); // Core 1
}

void loop() {}

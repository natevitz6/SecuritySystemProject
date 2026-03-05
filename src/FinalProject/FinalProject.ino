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
#define PIR_PIN 4
#define PIN_LENGTH 4
#define LED_PIN 13
#define RED_LED_PIN   5    //solid = locked, blinking = alarm
#define GREEN_LED_PIN 6 
#define IR_RECEIVE_PIN 15
#define RFID_SS_PIN  9
#define RFID_RST_PIN 10
#define SCK_PIN 12
#define MOSI_PIN 11
#define MISO_PIN 13

extern "C" {
  void PIR_init(uint8_t inputPin, uint8_t ledPin);
  void PIR_update(void);
  bool PIR_isMotionDetected(void);
  void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin);
  void Ultrasonic_update(void);
  int Ultrasonic_getDistance(void);
  bool Ultrasonic_isLoitering(int distanceThresholdCm, unsigned long timeLimitMs);
  void    IRRemote_init(uint8_t receivePin);
  bool    IRRemote_update(void);
  bool    IRRemote_isPINCorrect(void);
  bool    IRRemote_wasClearPressed(void);
  uint8_t IRRemote_getDigitCount(void);
  void IRRemote_getEnteredPIN(uint8_t *buf, uint8_t len);
  void    RFID_init(uint8_t ssPin, uint8_t rstPin);
  bool    RFID_update(void);
  bool    RFID_isAuthorized(void);
}

LiquidCrystal_I2C lcd(0x27, 16, 2);

// System Event Types
typedef enum {
    EVENT_PIR_MOTION,
    EVENT_PIR_CLEAR,
    EVENT_DISTANCE_UPDATE,
    EVENT_LOITERING,
    EVENT_DISPLAY_UPDATE,
    EVENT_ALARM_TRIGGER,
    EVENT_ALARM_CLEAR,
    EVENT_ACCESS_GRANTED, 
    EVENT_ACCESS_DENIED
} system_event_t;

// System state machine
typedef enum {
    STATE_IDLE,
    STATE_MOTION_DETECTED,
    STATE_DISARMED,
    STATE_ALARM
} security_state_t;

typedef struct {
    system_event_t type;
    int value;
    char displayMsg[32]; // For display messages (if needed)
} system_message_t;

// Queues
QueueHandle_t sensorQueue;
QueueHandle_t uiQueue;
QueueHandle_t alarmQueue;

void SecurityController_Task(void *pvParameters) {
    system_message_t msg;
    system_message_t uiMsg;
    system_message_t alarmMsg;
    security_state_t state = STATE_IDLE;

    while (1) {
        if (xQueueReceive(sensorQueue, &msg, portMAX_DELAY)) {
            switch (state) {

                case STATE_IDLE:
                    if (msg.type == EVENT_PIR_MOTION) {
                        state = STATE_MOTION_DETECTED;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Motion Detected");
                        xQueueSend(uiQueue, &uiMsg, 0);
                    }
                    break;

                case STATE_MOTION_DETECTED:
                    if (msg.type == EVENT_LOITERING) {
                        state = STATE_ALARM;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Loitering Detected");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ALARM_TRIGGER;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    } else if (msg.type == EVENT_PIR_CLEAR) {
                        state = STATE_IDLE;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Idle");
                        xQueueSend(uiQueue, &uiMsg, 0);
                    } else if (msg.type == EVENT_ACCESS_GRANTED) {
                        state = STATE_DISARMED;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Access Granted!");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    } else if (msg.type == EVENT_ACCESS_DENIED) {
                        state = STATE_ALARM;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Access Denied!");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ALARM_TRIGGER;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    }
                    break;

                case STATE_AWAITING_AUTH:
                    // Placeholder — RFID task will post ACCESS_GRANTED or ACCESS_DENIED
                    // which will arrive here and be handled identically to MOTION_DETECTED
                    if (msg.type == EVENT_ACCESS_GRANTED) {
                        state = STATE_DISARMED;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Access Granted!");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    } else if (msg.type == EVENT_ACCESS_DENIED) {
                        state = STATE_ALARM;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Access Denied!");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ALARM_TRIGGER;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    }
                    break;

                case STATE_DISARMED:
                    // System is unlocked — only re-arm if motion clears
                    if (msg.type == EVENT_PIR_CLEAR) {
                        state = STATE_IDLE;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Idle");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_DENIED; // turn off green LED
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    }
                    break;

                case STATE_ALARM:
                    // Correct PIN disarms the alarm
                    if (msg.type == EVENT_ACCESS_GRANTED) {
                        state = STATE_IDLE;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "System Disarmed");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ALARM_CLEAR;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    }
                    // Dedicated admin override button also disarms
                    else if (msg.type == EVENT_ALARM_CLEAR) {
                        state = STATE_IDLE;
                        uiMsg.type = EVENT_DISPLAY_UPDATE;
                        snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Admin Override");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ALARM_CLEAR;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    }
                    break;
            }
        }
    }
}

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

void IR_Task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(16); // ~64Hz

    while (1) {
        bool pinSubmitted = IRRemote_update();

        system_message_t msg;
        system_message_t uiMsg;

        if (IRRemote_wasClearPressed()) {
            // Give LCD feedback that entry was cleared
            uiMsg.type = EVENT_DISPLAY_UPDATE;
            snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Cleared");
            xQueueSend(uiQueue, &uiMsg, 0);
        }

        if (pinSubmitted) {
            if (IRRemote_isPINCorrect()) {
                msg.type  = EVENT_ACCESS_GRANTED;
                msg.value = 0;
            } else {
                msg.type  = EVENT_ACCESS_DENIED;
                msg.value = 0;
            }
            xQueueSend(sensorQueue, &msg, 0);
        }

        // Show digit count progress on LCD (e.g. "PIN: ** " )
        uint8_t digits = IRRemote_getDigitCount();
        if (digits > 0) {
            uint8_t pinBuf[PIN_LENGTH];
            IRRemote_getEnteredPIN(pinBuf, digits);
            uiMsg.type = EVENT_DISPLAY_UPDATE;
            char pinDisplay[PIN_LENGTH + 1];
            for (uint8_t i = 0; i < digits; i++) {
                pinDisplay[i] = '0' + pinBuf[i];
            }
            pinDisplay[digits] = '\0';
            snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "PIN: %s", pinDisplay);
            xQueueSend(uiQueue, &uiMsg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void RFID_Task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10Hz — cards don't need fast polling

    while (1) {
        bool cardScanned = RFID_update();

        if (cardScanned) {
            system_message_t msg;
            system_message_t uiMsg;

            if (RFID_isAuthorized()) {
                msg.type  = EVENT_ACCESS_GRANTED;
                msg.value = 0;
                uiMsg.type = EVENT_DISPLAY_UPDATE;
                snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Access Granted!");
            } else {
                msg.type  = EVENT_ACCESS_DENIED;
                msg.value = 0;
                uiMsg.type = EVENT_DISPLAY_UPDATE;
                snprintf(uiMsg.displayMsg, sizeof(uiMsg.displayMsg), "Access Denied!");
            }

            xQueueSend(sensorQueue, &msg, 0);
            xQueueSend(uiQueue, &uiMsg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
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

void Alarm_Task(void *pvParameters) {
    bool alarmActive   = false;
    bool accessGranted = false;
    bool ledState      = false;
    bool disarmPending = false;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(250); // 4 Hz blink

    while (1) {
        // Check for new events (non-blocking)
        system_message_t msg;
        while (xQueueReceive(alarmQueue, &msg, 0) == pdTRUE) {
            switch (msg.type) {
                case EVENT_ALARM_TRIGGER:
                    alarmActive   = true;
                    accessGranted = false;
                    disarmPending  = false;
                    break;
                case EVENT_ALARM_CLEAR:
                    if (alarmActive) {
                        alarmActive  = false;
                        ledState     = false;
                        digitalWrite(RED_LED_PIN, HIGH);
                    }
                    break;
                case EVENT_ACCESS_GRANTED:
                    accessGranted = true;
                    alarmActive   = false;
                    digitalWrite(GREEN_LED_PIN, HIGH);
                    digitalWrite(RED_LED_PIN,   LOW);
                    break;
                case EVENT_ACCESS_DENIED:
                    accessGranted = false;
                    digitalWrite(GREEN_LED_PIN, LOW);
                    break;
                default:
                    break;
            }
        }

        // LED output logic
        if (alarmActive) {
            // Blink red LED
            ledState = !ledState;
            digitalWrite(RED_LED_PIN,   ledState ? HIGH : LOW);
            digitalWrite(GREEN_LED_PIN, LOW);
        } else if (!accessGranted) {
            // Idle / locked state: solid red
            digitalWrite(RED_LED_PIN,   HIGH);
            digitalWrite(GREEN_LED_PIN, LOW);
        }
        // If accessGranted: GREEN is already set above on the event edge

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void setup() {
    Serial.begin(115200);

    Wire.begin();
    lcd.init();
    lcd.backlight();
    
    // Set alarm led pins
    pinMode(RED_LED_PIN,   OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN,   HIGH); // Start locked (red on)
    digitalWrite(GREEN_LED_PIN, LOW);

    // Initialize modules
    PIR_init(PIR_PIN, LED_PIN);
    Ultrasonic_init(TRIG_PIN, ECHO_PIN);
    IRRemote_init(IR_RECEIVE_PIN);
    RFID_init(RFID_SS_PIN, RFID_RST_PIN);

    // Create queues
    sensorQueue = xQueueCreate(10, sizeof(system_message_t));
    uiQueue     = xQueueCreate(10, sizeof(system_message_t));
    alarmQueue = xQueueCreate(10, sizeof(system_message_t));

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
        IR_Task, 
        "IR Task", 
        4096, 
        NULL, 
        2, 
        NULL, 
        0);
    
    xTaskCreatePinnedToCore(
        RFID_Task,
        "RFID Task",
        4096,
        NULL,
        2,
        NULL,
        0);

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

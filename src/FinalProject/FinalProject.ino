/**
 * @file FinalProject.ino
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Top-level FreeRTOS application for the Enhanced Multi-Mode Security System.
 *
 * Implements a multi-task security system on the ESP32 using FreeRTOS queues
 * for inter-task communication. Sensor tasks run on Core 0; the security
 * controller, LCD, and alarm tasks run on Core 1.
 *
 * Task summary:
 *  - PIR_Task             : Polls PIR sensor at 32 Hz, sends motion events
 *  - Ultrasonic_Task      : Polls distance sensor at 50 Hz, sends loiter events
 *  - IR_Task              : Polls IR remote at 64 Hz, sends PIN auth events
 *  - RFID_Task            : Polls RFID reader at 10 Hz, sends card auth events
 *  - Countdown_Task       : Manages grace-period countdown, sends expiry events
 *  - SecurityController_Task : State machine consuming sensor events
 *  - LCD_Task             : Consumes UI messages and updates the LCD display
 *  - Alarm_Task           : Drives red/green LEDs based on alarm state
 *
 * Queue summary:
 *  - sensorQueue    : Sensor/auth tasks  → SecurityController_Task
 *  - uiQueue        : Any task           → LCD_Task
 *  - alarmQueue     : Controller         → Alarm_Task
 *  - countdownQueue : Controller         → Countdown_Task
 */

// ========================== Includes ===============================

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "pir.h"
#include "ultrasonic.h"
#include "rfid.h"
#include "ir_remote.h"
#include "countdown.h"

// ========================== Macros =================================

#define TRIG_PIN             2    /**< Ultrasonic sensor TRIG pin. */
#define ECHO_PIN             1    /**< Ultrasonic sensor ECHO pin. */
#define LOITER_DISTANCE_CM   10   /**< Distance threshold (cm) for loitering detection. */
#define LOITER_TIME_MS       10000 /**< Time (ms) within range before loitering is declared. */
#define PIR_PIN              3    /**< PIR sensor signal pin. */
#define PIN_LENGTH           4    /**< IR remote PIN length. */
#define LED_PIN              7    /**< PIR activity indicator LED pin. */
#define RED_LED_PIN          5    /**< Red LED: solid = locked, blinking = alarm. */
#define GREEN_LED_PIN        6    /**< Green LED: lit = access granted / disarmed. */
#define IR_RECEIVE_PIN       4   /**< IR receiver data pin. */
#define RFID_SS_PIN          10    /**< RFID reader SDA/SS pin. */
#define RFID_RST_PIN         9   /**< RFID reader RST pin. */
#define SCK_PIN              12   /**< SPI clock pin. */
#define MOSI_PIN             11   /**< SPI MOSI pin. */
#define MISO_PIN             13   /**< SPI MISO pin. */
#define ALARM_GRACE_PERIOD   9000 /**< Grace period (ms) before alarm triggers after denied entry. */
#define EXIT_COOLDOWN        3000   /**< Time (ms) after exit before system re-arms. */

// ======================== Global Variables =========================

LiquidCrystal_I2C lcd(0x27, 16, 2); /**< I2C LCD at address 0x27, 16 columns x 2 rows. */

QueueHandle_t sensorQueue;    /**< Sensor/auth events to SecurityController_Task. */
QueueHandle_t uiQueue;        /**< Display update messages to LCD_Task. */
QueueHandle_t alarmQueue;     /**< Alarm state changes to Alarm_Task. */
QueueHandle_t countdownQueue; /**< Countdown commands to Countdown_Task. */

// ========================== Enums ==================================

/**
 * @brief System-wide event types passed between tasks via queues.
 */
typedef enum {
    EVENT_PIR_MOTION,        /**< PIR sensor detected motion. */
    EVENT_PIR_CLEAR,         /**< PIR sensor cleared (no motion). */
    EVENT_DISTANCE_UPDATE,   /**< New ultrasonic distance reading available. */
    EVENT_LOITER_MOTION,     /**< Object within range of door. */
    EVENT_LOITERING,         /**< Object within range longer than LOITER_TIME_MS. */
    EVENT_LOITER_CLEAR,      /**< Object moved out of range. */
    EVENT_DISPLAY_UPDATE,    /**< New content ready for the LCD display. */
    EVENT_ALARM_TRIGGER,     /**< Alarm should activate. */
    EVENT_ALARM_CLEAR,       /**< Alarm should deactivate. */
    EVENT_ACCESS_GRANTED,    /**< Valid PIN or RFID credential presented. */
    EVENT_ACCESS_DENIED,     /**< Invalid PIN or RFID credential presented. */
    EVENT_COUNTDOWN_EXPIRED  /**< Grace-period countdown reached zero. */
} system_event_t;

/**
 * @brief Top-level security system states.
 */
typedef enum {
    STATE_IDLE,             /**< System armed, no motion detected. */
    STATE_MOTION_DETECTED,  /**< Motion detected, awaiting authentication. */
    STATE_DISARMED,         /**< Valid credential accepted, system disarmed. */
    STATE_ALARM_PENDING,    /**< Invalid credential; grace-period countdown running. */
    STATE_ALARM             /**< Alarm active. */
} security_state_t;

/**
 * @brief Commands sent to Countdown_Task via countdownQueue.
 */
typedef enum {
    CMD_COUNTDOWN_START,  /**< Start the grace-period countdown. */
    CMD_COUNTDOWN_CANCEL  /**< Cancel an in-progress countdown. */
} countdown_cmd_t;

// ========================== Structs ================================

/**
 * @brief Message payload passed through sensorQueue and uiQueue.
 *
 * For sensor events, @p type and @p value are used.
 * For display events (@c EVENT_DISPLAY_UPDATE), @p displayLine0 and
 * @p displayLine1 carry the two LCD row strings.
 */
typedef struct {
    system_event_t type;
    int            value;
    char           displayLine0[17]; /**< LCD row 0 content (16 chars + null). */
    char           displayLine1[17]; /**< LCD row 1 content (16 chars + null). */
} system_message_t;

// ========================== Macros =================================

/**
 * @brief Populates a system_message_t for an LCD display update.
 *
 * Sets the message type to EVENT_DISPLAY_UPDATE and safely copies
 * the two provided strings into the display line buffers.
 *
 * @param msg  The system_message_t variable to populate.
 * @param l0   String for LCD row 0 (max 16 chars).
 * @param l1   String for LCD row 1 (max 16 chars).
 */
#define LCD_MSG(msg, l0, l1) do { \
    (msg).type = EVENT_DISPLAY_UPDATE; \
    strncpy((msg).displayLine0, (l0), 16); \
    strncpy((msg).displayLine1, (l1), 16); \
    (msg).displayLine0[16] = '\0'; \
    (msg).displayLine1[16] = '\0'; \
} while(0)

/**
 * @brief Prints a two-line LCD message to the Serial monitor.
 *
 * Mirrors every LCD display update to the serial console so that system
 * state changes can be observed without a physical LCD attached. Output
 * format is a divider line followed by the two display rows.
 *
 * @param l0  String for LCD row 0 (the same value passed to LCD_MSG).
 * @param l1  String for LCD row 1 (the same value passed to LCD_MSG).
 */
#define SERIAL_MSG(l0, l1) do { \
    Serial.println("----------------"); \
    Serial.println(l0); \
    Serial.println(l1); \
} while(0)

// Forward declarations for C-linkage driver functions
extern "C" {
    void PIR_init(uint8_t inputPin, uint8_t ledPin);
    void PIR_update(void);
    bool PIR_isMotionDetected(void);
    void Ultrasonic_init(uint8_t trigPin, uint8_t echoPin);
    void Ultrasonic_update(void);
    int  Ultrasonic_getDistance(void);
    bool Ultrasonic_isLoitering(int distanceThresholdCm, unsigned long timeLimitMs);
    void     IRRemote_init(uint8_t receivePin);
    bool     IRRemote_update(void);
    bool     IRRemote_isPINCorrect(void);
    bool     IRRemote_wasClearPressed(void);
    uint8_t  IRRemote_getDigitCount(void);
    void     IRRemote_getEnteredPIN(uint8_t *buf, uint8_t len);
    void     RFID_init(uint8_t ssPin, uint8_t rstPin);
    bool     RFID_update(void);
    bool     RFID_isAuthorized(void);
    void     Countdown_init(void);
    void     Countdown_start(uint32_t durationMs);
    void     Countdown_cancel(void);
    uint32_t Countdown_getSecondsRemaining(void);
    bool     Countdown_hasExpired(void);
    bool     Countdown_isActive(void);
    bool IRRemote_wasDisarmPressed(void);
}

// ====================== Function Implementations ===================

/**
 * @brief Central security state machine task.
 *
 * Consumes events from sensorQueue and drives state transitions. Sends
 * display messages to uiQueue, alarm commands to alarmQueue, and countdown
 * commands to countdownQueue. Also manages the exit cooldown timer that
 * re-arms the system after an authorized user leaves.
 *
 * Pinned to Core 1. Priority 1.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void SecurityController_Task(void *pvParameters) {
    
    system_message_t msg;
    system_message_t uiMsg;
    system_message_t alarmMsg;
    security_state_t state = STATE_IDLE;
    countdown_cmd_t  cdCmd;
    bool     exitCooldownActive  = false;
    uint32_t exitCooldownStartMs = 0;

    // --- add these ---
    #define MIN_DISPLAY_MS 2000   // minimum ms to hold a feedback state
    bool     holdingDisplay      = false;
    uint32_t displayHoldStartMs  = 0;
    // -----------------

    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);

        // Don't process any new sensor events while holding a display state
        if (holdingDisplay) {
            if ((now - displayHoldStartMs) >= MIN_DISPLAY_MS) {
                holdingDisplay = false;
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;  // skip queue receive entirely until hold expires
            }
        }

        // Re-arm after exit cooldown
        if (exitCooldownActive && state == STATE_DISARMED) {
            if ((now - exitCooldownStartMs) >= EXIT_COOLDOWN) {
                exitCooldownActive = false;
                state = STATE_IDLE;
                LCD_MSG(uiMsg, "  SYSTEM ARMED  ", "");
                SERIAL_MSG("  SYSTEM ARMED  ", "");
                xQueueSend(uiQueue, &uiMsg, 0);
                alarmMsg.type = EVENT_ACCESS_DENIED;
                xQueueSend(alarmQueue, &alarmMsg, 0);
            }
        }

        if (xQueueReceive(sensorQueue, &msg, pdMS_TO_TICKS(100))) {
            Serial.println(state);
            switch (state) {

                case STATE_IDLE:
                    if (msg.type == EVENT_PIR_MOTION) {
                        state = STATE_DISARMED;
                        LCD_MSG(uiMsg, "Goodbye!", "");
                        SERIAL_MSG("Goodbye!", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    } else if (msg.type == EVENT_LOITER_MOTION) {
                        state = STATE_MOTION_DETECTED;
                        LCD_MSG(uiMsg, "Person Detected", "Scan/Enter Pin");
                        SERIAL_MSG("Person Detected", "Scan/Enter Pin");
                        xQueueSend(uiQueue, &uiMsg, 0);
                    }
                    break;

                case STATE_MOTION_DETECTED:
                    if (msg.type == EVENT_LOITERING) {
                        state = STATE_ALARM_PENDING;
                        LCD_MSG(uiMsg, " Loiter Detected!", "");
                        SERIAL_MSG(" Loiter Detected!", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        cdCmd = CMD_COUNTDOWN_START;
                        xQueueSend(countdownQueue, &cdCmd, 0);
                    } else if (msg.type == EVENT_PIR_MOTION) {
                        state = STATE_DISARMED;
                        LCD_MSG(uiMsg, "Goodbye!", "");
                        SERIAL_MSG("Goodbye!", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    } else if (msg.type == EVENT_ACCESS_GRANTED) {
                        state = STATE_DISARMED;
                        exitCooldownActive = false;
                        LCD_MSG(uiMsg, " Access Granted!", "");
                        SERIAL_MSG(" Access Granted!", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                        // hold so the user can see the green LED and message
                        holdingDisplay     = true;
                        displayHoldStartMs = now;
                    } else if (msg.type == EVENT_ACCESS_DENIED) {
                        state = STATE_ALARM_PENDING;
                        LCD_MSG(uiMsg, " Access Denied!", "");
                        SERIAL_MSG(" Access Denied!", "");
                        cdCmd = CMD_COUNTDOWN_START;
                        xQueueSend(countdownQueue, &cdCmd, 0);
                    } else if (msg.type == EVENT_LOITER_CLEAR) {
                        state = STATE_IDLE;
                        LCD_MSG(uiMsg, "  SYSTEM ARMED  ", "");
                        SERIAL_MSG("  SYSTEM ARMED  Loiter clear", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                    }
                    break;

                case STATE_DISARMED:
                    if (!exitCooldownActive) {
                        exitCooldownActive  = true;
                        exitCooldownStartMs = now;
                    }
                    if (msg.type == EVENT_PIR_MOTION) {
                        LCD_MSG(uiMsg, "Goodbye!", "");
                        SERIAL_MSG("Goodbye!", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                    }
                    break;

                case STATE_ALARM:
                    if (msg.type == EVENT_ACCESS_GRANTED) {
                        state = STATE_DISARMED;
                        exitCooldownActive = false;
                        LCD_MSG(uiMsg, " Access Granted!", "");
                        SERIAL_MSG(" Access Granted!", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                        holdingDisplay     = true;
                        displayHoldStartMs = now;
                    }
                    break;

                case STATE_ALARM_PENDING:
                    if (msg.type == EVENT_ACCESS_GRANTED) {
                        state = STATE_DISARMED;
                        exitCooldownActive = false;
                        cdCmd = CMD_COUNTDOWN_CANCEL;
                        xQueueSend(countdownQueue, &cdCmd, 0);
                        LCD_MSG(uiMsg, "Access Granted! ", "");
                        SERIAL_MSG("Access Granted! ", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                        holdingDisplay     = true;
                        displayHoldStartMs = now;
                    } else if (msg.type == EVENT_COUNTDOWN_EXPIRED) {
                        state = STATE_ALARM;
                        LCD_MSG(uiMsg, "!!! ALARM !!! ", "");
                        SERIAL_MSG("  !!! ALARM !!! ", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ALARM_TRIGGER;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    } else if (msg.type == EVENT_PIR_MOTION) {
                        state = STATE_DISARMED;
                        exitCooldownActive = false;
                        cdCmd = CMD_COUNTDOWN_CANCEL;
                        xQueueSend(countdownQueue, &cdCmd, 0);
                        LCD_MSG(uiMsg, "Goodbye!", "");
                        SERIAL_MSG("Goodbye!", "");
                        xQueueSend(uiQueue, &uiMsg, 0);
                        alarmMsg.type = EVENT_ACCESS_GRANTED;
                        xQueueSend(alarmQueue, &alarmMsg, 0);
                    }
                    break;
            }
        }
    }
}

/**
 * @brief Polls the PIR sensor and sends motion state change events.
 *
 * Only enqueues a message when the debounced state transitions, avoiding
 * redundant events. Runs at 32 Hz on Core 0. Priority 2.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void PIR_Task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(31);
    bool lastState = false;

    while (1) {
        //Serial.println("pir task");
        PIR_update();
        bool currentState = PIR_isMotionDetected();

        if (currentState != lastState) {
            lastState = currentState;
            system_message_t msg;
            msg.type  = currentState ? EVENT_PIR_MOTION : EVENT_PIR_CLEAR;
            msg.value = currentState ? 1 : 0;
            xQueueSend(sensorQueue, &msg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Polls the ultrasonic sensor and sends loitering events.
 *
 * Runs at 50 Hz on Core 0. Priority 3.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void Ultrasonic_Task(void *pvParameters) {
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    while (1) {
        //Serial.println("ultra task");
        Ultrasonic_update();
        int dist = Ultrasonic_getDistance();
        Serial.print("Dist ");
        Serial.println(dist);
        if (dist < LOITER_DISTANCE_CM) {
            system_message_t msg;
            msg.type  = EVENT_LOITER_MOTION;
            msg.value = dist;
            xQueueSend(sensorQueue, &msg, 0);
        } else if (Ultrasonic_isLoitering(LOITER_DISTANCE_CM, LOITER_TIME_MS)) {
            system_message_t msg;
            msg.type  = EVENT_LOITERING;
            msg.value = dist;
            xQueueSend(sensorQueue, &msg, 0);
        } else if (dist > LOITER_DISTANCE_CM) {
            system_message_t msg;
            msg.type  = EVENT_LOITER_CLEAR;
            msg.value = dist;
            xQueueSend(sensorQueue, &msg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Polls the IR remote and sends PIN authentication events.
 *
 * Updates the LCD with masked PIN progress on each digit press and sends
 * EVENT_ACCESS_GRANTED or EVENT_ACCESS_DENIED when OK is pressed.
 * Runs at 64 Hz on Core 0. Priority 2.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void IR_Task(void *pvParameters) {
   
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(16);

    while (1) {
        //Serial.println("ir task");
        bool pinSubmitted = IRRemote_update();

        system_message_t msg;
        system_message_t uiMsg;

        if (pinSubmitted) {
            bool granted = IRRemote_isPINCorrect() || IRRemote_wasDisarmPressed();
            msg.type  = granted ? EVENT_ACCESS_GRANTED : EVENT_ACCESS_DENIED;
            msg.value = 0;
            xQueueSend(sensorQueue, &msg, 0);
        }

        // Update LCD with masked digit progress (e.g. "PIN: **  ")
        uint8_t digits = IRRemote_getDigitCount();
        if (digits > 0) {
            char pinDisplay[17] = "Enter PIN:      ";
            for (uint8_t i = 0; i < digits; i++) {
                pinDisplay[10 + i] = '*';
            }
            LCD_MSG(uiMsg, pinDisplay, "");
            SERIAL_MSG(pinDisplay, "");
            xQueueSend(uiQueue, &uiMsg, 0);
        }

        if (IRRemote_wasClearPressed()) {
            LCD_MSG(uiMsg, "PIN: Cleared    ", "");
            SERIAL_MSG("PIN: Cleared    ", "");
            xQueueSend(uiQueue, &uiMsg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Polls the RFID reader and sends card authentication events.
 *
 * Runs at 10 Hz on Core 0. Priority 2.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void RFID_Task(void *pvParameters) {
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100);

    while (1) {
        bool cardScanned = RFID_update();
        //Serial.println("rfid task");
        if (cardScanned) {
            //Serial.print("Scanned");
            system_message_t msg;

            msg.type = RFID_isAuthorized() ? EVENT_ACCESS_GRANTED : EVENT_ACCESS_DENIED;

            xQueueSend(sensorQueue, &msg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Receives display messages and updates the I2C LCD.
 *
 * Only redraws when the content has actually changed to minimize I2C traffic.
 * Mirrors every accepted display update to the Serial monitor as well.
 * Blocks indefinitely on uiQueue when idle. Pinned to Core 1. Priority 1.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void LCD_Task(void *pvParameters) {
    
    system_message_t uiMsg;
    char lastLine0[17] = "";
    char lastLine1[17] = "";

    lcd.init();        // reinitialize from within the task
    //lcd.backlight();

    while (1) {
        //Serial.println("lcd task");
        if (xQueueReceive(uiQueue, &uiMsg, portMAX_DELAY)) {
            if (uiMsg.type == EVENT_DISPLAY_UPDATE) {
                if (strcmp(lastLine0, uiMsg.displayLine0) != 0 ||
                    strcmp(lastLine1, uiMsg.displayLine1) != 0) {

                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print(uiMsg.displayLine0);
                    lcd.setCursor(0, 1);
                    lcd.print(uiMsg.displayLine1);

                    // Mirror the accepted LCD update to the serial monitor so
                    // display changes are visible without a physical LCD attached.
                    Serial.println("[LCD]");
                    Serial.println(uiMsg.displayLine0);
                    Serial.println(uiMsg.displayLine1);

                    strncpy(lastLine0, uiMsg.displayLine0, sizeof(lastLine0));
                    strncpy(lastLine1, uiMsg.displayLine1, sizeof(lastLine1));
                }
            }
        }
    }
}

/**
 * @brief Drives the red and green LEDs based on alarm and access state.
 *
 * Red LED: solid when locked, blinking at 4 Hz when alarm is active.
 * Green LED: lit when access is granted / system is disarmed.
 * Pinned to Core 1. Priority 2.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void Alarm_Task(void *pvParameters) {
    
    bool alarmActive   = false;
    bool accessGranted = false;
    bool ledState      = false;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(250); // 4 Hz blink rate

    while (1) {
        //Serial.println("Alarm task");
        system_message_t msg;
        while (xQueueReceive(alarmQueue, &msg, 0) == pdTRUE) {
            switch (msg.type) {
                case EVENT_ALARM_TRIGGER:
                    alarmActive   = true;
                    accessGranted = false;
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

        if (alarmActive) {
            ledState = !ledState;
            digitalWrite(RED_LED_PIN,   ledState ? HIGH : LOW);
            digitalWrite(GREEN_LED_PIN, LOW);
        } else if (!accessGranted) {
            digitalWrite(RED_LED_PIN,   HIGH);
            digitalWrite(GREEN_LED_PIN, LOW);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Manages the grace-period countdown and notifies the controller on expiry.
 *
 * Receives CMD_COUNTDOWN_START / CMD_COUNTDOWN_CANCEL commands from
 * SecurityController_Task via countdownQueue. While counting, updates the
 * LCD with the remaining seconds once per second and sends
 * EVENT_COUNTDOWN_EXPIRED to sensorQueue when time runs out.
 * Pinned to Core 1. Priority 2.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
void Countdown_Task(void *pvParameters) {
    countdown_cmd_t  cmd;
    system_message_t expiredMsg;
    uint32_t         lastSecond = 0;
    bool             counting   = false;

    expiredMsg.type = EVENT_COUNTDOWN_EXPIRED;

    while (1) {
        // Block when idle; non-blocking poll when counting
        BaseType_t got = xQueueReceive(countdownQueue, &cmd,
                                       counting ? 0 : portMAX_DELAY);
        if (got == pdTRUE) {
            if (cmd == CMD_COUNTDOWN_START) {
                Countdown_start(ALARM_GRACE_PERIOD);
                lastSecond = Countdown_getSecondsRemaining();
                counting   = true;
            } else if (cmd == CMD_COUNTDOWN_CANCEL) {
                Countdown_cancel();
                counting = false;
            }
        }

        if (counting) {
            uint32_t secsLeft = Countdown_getSecondsRemaining();

            if (secsLeft != lastSecond) {
                lastSecond = secsLeft;
                system_message_t uiMsg;
                LCD_MSG(uiMsg, "!! DISARM NOW !!", "                ");
                snprintf(uiMsg.displayLine1, sizeof(uiMsg.displayLine1),
                         "Scan/PIN %1lus left", (unsigned long)secsLeft);
                // Print the fully-formatted countdown line to serial as well
                SERIAL_MSG("!! DISARM NOW !!", uiMsg.displayLine1);
                xQueueSend(uiQueue, &uiMsg, 0);
            }

            if (Countdown_hasExpired()) {
                counting = false;
                xQueueSend(sensorQueue, &expiredMsg, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz poll — sufficient for 1-second LCD updates
    }
}

/**
 * @brief Arduino setup — initializes all peripherals, queues, and FreeRTOS tasks.
 */
void setup() {
    Serial.begin(115200);
    Wire.setPins(47,48);
    Wire.begin();
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("  SYSTEM ARMED  ");

    // Mirror the startup message to the serial monitor
    Serial.println("----------------");
    Serial.println("  SYSTEM ARMED  ");

    pinMode(RED_LED_PIN,   OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN,   HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);

    PIR_init(PIR_PIN, LED_PIN);
    Ultrasonic_init(TRIG_PIN, ECHO_PIN);
    IRRemote_init(IR_RECEIVE_PIN);
    RFID_init(RFID_SS_PIN, RFID_RST_PIN);
    Countdown_init();

    sensorQueue    = xQueueCreate(10, sizeof(system_message_t));
    uiQueue        = xQueueCreate(10, sizeof(system_message_t));
    alarmQueue     = xQueueCreate(10, sizeof(system_message_t));
    countdownQueue = xQueueCreate(5,  sizeof(countdown_cmd_t));

    xTaskCreatePinnedToCore(PIR_Task,                "PIR Task",            4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(Ultrasonic_Task,         "Ultrasonic Task",     4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(IR_Task,                 "IR Task",             4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(RFID_Task,               "RFID Task",           4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(Countdown_Task,          "Countdown Task",      4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(SecurityController_Task, "Security Controller", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(LCD_Task,                "LCD Task",            4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(Alarm_Task,              "Alarm Task",          4096, NULL, 2, NULL, 1);
}

/**
 * @brief Arduino main loop — unused; all work is done in FreeRTOS tasks.
 */
void loop() {}

/**
 * @file rfid.cpp
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Implementation of the RC522 RFID reader driver.
 *
 * Authorized UIDs are stored as compile-time constants. To add or change
 * credentials, update AUTHORIZED_UID / AUTHORIZED_UID2 and recompile.
 * Cooldown debouncing uses esp_timer_get_time() to prevent a held card
 * from firing repeatedly.
 */

// ========================== Includes ===============================

#include "rfid.h"
#include <MFRC522.h>
#include <SPI.h>

// ========================== Macros =================================

#define RFID_COOLDOWN_MS 2000 /**< Minimum ms between accepted scans of the same card. */

// ======================== Global Variables =========================

/** @brief UID of the first authorized RFID credential. */
static const uint8_t AUTHORIZED_UID[]  = {0x73, 0xC6, 0xDA, 0x26};

/** @brief UID of the second authorized RFID credential. */
static const uint8_t AUTHORIZED_UID2[] = {0x63, 0xA9, 0xCB, 0x13};

static const uint8_t AUTHORIZED_UID_LEN = 4; /**< Expected UID length in bytes. */

static MFRC522 _mfrc522;                          /**< MFRC522 driver instance. */
static bool    _authorized             = false;   /**< Result of the last card scan. */
static uint8_t _lastUID[4]            = {0};      /**< UID seen in the most recent accepted scan. */
static uint32_t _lastScanTime         = 0;        /**< Timestamp (ms) of the most recent accepted scan. */

// ====================== Function Implementations ===================

// See rfid.h for full interface documentation.
void RFID_init(uint8_t ssPin, uint8_t rstPin) {
    SPI.begin(12, 13, 11, 10);
    _mfrc522 = MFRC522(ssPin, rstPin);
    _mfrc522.PCD_Init();
    delay(50);
    // Debug — prints firmware version, should be 0x91 or 0x92
    // If it prints 0x00 or 0xFF, wiring is wrong
    byte v = _mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.print("RFID Firmware Version: 0x");
    Serial.println(v, HEX);
}

// See rfid.h for full interface documentation.
bool RFID_update(void) {
    _authorized = false;

    if (!_mfrc522.PICC_IsNewCardPresent()) return false;
    if (!_mfrc522.PICC_ReadCardSerial())   return false;

    // Reject repeat scans of the same card within the cooldown window
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    bool sameCard = (_mfrc522.uid.size == AUTHORIZED_UID_LEN);
    if (sameCard) {
        for (uint8_t i = 0; i < AUTHORIZED_UID_LEN; i++) {
            if (_mfrc522.uid.uidByte[i] != _lastUID[i]) {
                sameCard = false;
                break;
            }
        }
    }
    if (sameCard && (now - _lastScanTime) < RFID_COOLDOWN_MS) {
        _mfrc522.PICC_HaltA();
        _mfrc522.PCD_StopCrypto1();
        return false;
    }

    // Record this scan as the new reference point for cooldown tracking
    _lastScanTime = now;
    if (_mfrc522.uid.size == AUTHORIZED_UID_LEN) {
        for (uint8_t i = 0; i < AUTHORIZED_UID_LEN; i++) {
            _lastUID[i] = _mfrc522.uid.uidByte[i];
        }
    }

    // Check scanned UID against both authorized credentials
    if (_mfrc522.uid.size == AUTHORIZED_UID_LEN) {
        bool match1 = true, match2 = true;
        for (uint8_t i = 0; i < AUTHORIZED_UID_LEN; i++) {
            if (_mfrc522.uid.uidByte[i] != AUTHORIZED_UID[i])  match1 = false;
            if (_mfrc522.uid.uidByte[i] != AUTHORIZED_UID2[i]) match2 = false;
        }
        _authorized = match1 || match2;
    }

    _mfrc522.PICC_HaltA();
    _mfrc522.PCD_StopCrypto1();
    return true;
}

// See rfid.h for full interface documentation.
bool RFID_isAuthorized(void) {
    return _authorized;
}
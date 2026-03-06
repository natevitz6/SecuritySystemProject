#include "rfid.h"
#include <MFRC522.h>
#include <SPI.h>

// ── Configuration ────────────────────────────────────────────────
// Replace these bytes with your actual keyfob's UID
// You can find your UID by running a simple MFRC522 dump sketch
static const uint8_t AUTHORIZED_UID[]  = {0x73, 0xC6, 0xDA, 0x26};
static const uint8_t AUTHORIZED_UID2[]  = {0x63, 0xA9, 0xCB, 0x13};
static const uint8_t AUTHORIZED_UID_LEN = 4;

// ── Private state ─────────────────────────────────────────────────
static MFRC522 _mfrc522;
static bool    _authorized = false;

void RFID_init(uint8_t ssPin, uint8_t rstPin) {
    SPI.begin(12, 13, 11, 10);
    _mfrc522 = MFRC522(ssPin, rstPin);
    _mfrc522.PCD_Init();
}

bool RFID_update(void) {
    _authorized = false;

    if (!_mfrc522.PICC_IsNewCardPresent()) return false;
    if (!_mfrc522.PICC_ReadCardSerial())   return false;

    if (_mfrc522.uid.size == AUTHORIZED_UID_LEN) {
        bool match1 = true;
        bool match2 = true;

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

bool RFID_isAuthorized(void) {
    return _authorized;
}
#ifndef RFID_H
#define RFID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Call once in setup()
void RFID_init(uint8_t ssPin, uint8_t rstPin);

// Call from RFID_Task — checks for a new card scan
// Returns true if a card was scanned this cycle
bool RFID_update(void);

// Call after RFID_update() returns true
// Returns true if the scanned UID matches the authorized UID
bool RFID_isAuthorized(void);

#ifdef __cplusplus
}
#endif

#endif
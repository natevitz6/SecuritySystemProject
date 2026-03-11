/**
 * @file rfid.h
 * @authors Nathan Vitzthum (natevitz), Peter Golden (petergo6)
 * @date 2025-03-19
 * @brief Public interface for the RC522 RFID reader driver.
 *
 * Provides card scanning and UID authorization using the MFRC522 library over SPI.
 * Includes a cooldown debounce to prevent repeated triggers from a held card.
 * Intended to be polled from RFID_Task at 10 Hz.
 */

#ifndef RFID_H
#define RFID_H

// ========================== Includes ===============================

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// ====================== Function Prototypes ========================

/**
 * @brief Initializes the MFRC522 RFID reader over SPI.
 *
 * Must be called once in @c setup() before any calls to @c RFID_update().
 *
 * @param ssPin   GPIO pin connected to the RC522 SDA/SS line.
 * @param rstPin  GPIO pin connected to the RC522 RST line.
 */
void RFID_init(uint8_t ssPin, uint8_t rstPin);

/**
 * @brief Checks for a new card scan and updates the authorization state.
 *
 * Reads the UID of any present card and compares it against the stored
 * authorized UIDs. Repeated scans of the same card within RFID_COOLDOWN_MS
 * are ignored. Call @c RFID_isAuthorized() after this returns @c true.
 *
 * @return @c true  if a new card was scanned this cycle.
 * @return @c false if no card was present or the scan was a cooldown repeat.
 */
bool RFID_update(void);

/**
 * @brief Returns whether the last scanned card is authorized.
 *
 * Only valid immediately after @c RFID_update() returns @c true.
 *
 * @return @c true  if the scanned UID matches an authorized UID.
 * @return @c false otherwise.
 */
bool RFID_isAuthorized(void);

#ifdef __cplusplus
}
#endif

#endif // RFID_H
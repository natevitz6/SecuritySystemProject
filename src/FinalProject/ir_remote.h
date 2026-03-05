#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Call once in setup()
void IRRemote_init(uint8_t receivePin);

// Call from IR_Task — decodes latest signal and updates internal PIN buffer
// Returns true if a completed PIN entry (OK pressed) is ready to evaluate
bool IRRemote_update(void);

// Call after IRRemote_update() returns true
// Returns true if the entered PIN matches the stored PIN
bool IRRemote_isPINCorrect(void);

// Returns true if the clear button was pressed this update cycle
bool IRRemote_wasClearPressed(void);

// Returns the number of digits entered so far (for LCD feedback)
uint8_t IRRemote_getDigitCount(void);

void IRRemote_getEnteredPIN(uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif
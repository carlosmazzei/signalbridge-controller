#ifndef INPUT_DELIVERY_H
#define INPUT_DELIVERY_H

#include <stdbool.h>
#include <stdint.h>
#include "data_event.h"

#define INPUT_SNAPSHOT_COUNT 80U
#define INPUT_SNAPSHOT_NONE 255U

/* Task context only. Absolute inputs retain their latest value until the USB
 * writer accepts the complete packet. A new session republishes all known
 * values. Relative encoder events remain bounded, best-effort queue entries. */
void input_delivery_reset(void);
void input_delivery_session(uint32_t session);
void input_delivery_key(uint8_t row, uint8_t column, bool pressed);
void input_delivery_adc(uint8_t channel, uint16_t value);
bool input_delivery_next(data_events_t *event, uint8_t *slot, uint32_t *version);
void input_delivery_complete(uint8_t slot, uint32_t version, uint32_t session, bool success);

#endif

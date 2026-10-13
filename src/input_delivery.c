/**
 * @file input_delivery.c
 * @brief Retain and schedule versioned keypad and ADC snapshots for USB delivery.
 *
 * Absolute input state is preserved across queue or USB-session failures.
 * Each slot tracks its current, delivered, and pending versions so failed
 * attempts can be retried without losing a newer value.
 */

#include "input_delivery.h"

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "commands.h"

/**
 * @brief Latest event and delivery state for one absolute input slot.
 */
typedef struct {
	data_events_t event; /**< Most recently observed event for this slot. */
	uint32_t version;    /**< Nonzero version of the current event. */
	uint32_t delivered;  /**< Version accepted by the writer in the current session. */
	uint32_t pending;    /**< Version currently selected for delivery, or zero. */
} input_snapshot_t;

static input_snapshot_t snapshots[INPUT_SNAPSHOT_COUNT]; /**< Key and ADC snapshot table. */
static uint32_t current_session; /**< CDC session generation currently tracked. */
static uint8_t cursor; /**< Next slot scanned by the round-robin selector. */

void input_delivery_reset(void)
{
	taskENTER_CRITICAL();
	(void)memset(snapshots, 0, sizeof(snapshots));
	cursor = 0U;
	current_session = 0U;
	taskEXIT_CRITICAL();
}

void input_delivery_session(uint32_t session)
{
	taskENTER_CRITICAL();
	if (session != current_session)
	{
		current_session = session;
		for (size_t i = 0U; i < INPUT_SNAPSHOT_COUNT; i++)
		{
			snapshots[i].delivered = 0U;
			snapshots[i].pending = 0U;
		}
	}
	taskEXIT_CRITICAL();
}

/**
 * @brief Replace a slot's event and advance its version when its value changes.
 *
 * @param[in] slot Snapshot slot to update.
 * @param[in] event New event value.
 */
static void update(uint8_t slot, const data_events_t *event)
{
	taskENTER_CRITICAL();
	input_snapshot_t *snapshot = &snapshots[slot];
	if ((snapshot->version == 0U) ||
	    (memcmp(snapshot->event.data, event->data, event->data_length) != 0))
	{
		snapshot->event = *event;
		snapshot->version++;
		if (snapshot->version == 0U)
		{
			snapshot->version = 1U;
		}
	}
	taskEXIT_CRITICAL();
}

void input_delivery_key(uint8_t row, uint8_t column, bool pressed)
{
	if ((row < 8U) && (column < 8U))
	{
		data_events_t event = {0};
		event.command = PC_KEY_CMD;
		event.data_length = 1U;
		event.data[0] = (uint8_t)((column << 4U) | (row << 1U) | (pressed ? 1U : 0U));
		update((uint8_t)((column * 8U) + row), &event);
	}
}

void input_delivery_adc(uint8_t channel, uint16_t value)
{
	if (channel < 16U)
	{
		data_events_t event = {0};
		event.command = PC_AD_CMD;
		event.data_length = 3U;
		event.data[0] = channel;
		event.data[1] = (uint8_t)(value >> 8U);
		event.data[2] = (uint8_t)value;
		update((uint8_t)(64U + channel), &event);
	}
}

bool input_delivery_next(data_events_t *event, uint8_t *slot, uint32_t *version)
{
	bool found = false;
	taskENTER_CRITICAL();
	for (size_t i = 0U; i < INPUT_SNAPSHOT_COUNT; i++)
	{
		uint8_t index = cursor;
		cursor = (uint8_t)((cursor + 1U) % INPUT_SNAPSHOT_COUNT);
		input_snapshot_t *snapshot = &snapshots[index];
		if ((snapshot->version != snapshot->delivered) && (snapshot->pending == 0U))
		{
			*event = snapshot->event;
			*slot = index;
			*version = snapshot->version;
			snapshot->pending = snapshot->version;
			found = true;
			break;
		}
	}
	taskEXIT_CRITICAL();
	return found;
}

void input_delivery_complete(uint8_t slot, uint32_t version, uint32_t session, bool success)
{
	if (slot < INPUT_SNAPSHOT_COUNT)
	{
		taskENTER_CRITICAL();
		input_snapshot_t *snapshot = &snapshots[slot];
		if ((session == current_session) && (snapshot->pending == version))
		{
			if (success)
			{
				snapshot->delivered = version;
			}
			snapshot->pending = 0U;
		}
		taskEXIT_CRITICAL();
	}
}

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "pico/unique_id.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]       MIDI | HID | MSC | CDC          [LSB]
 */
#define USB_PID           (0x4000U | ((uint16_t)CFG_TUD_CDC << 0U) | ((uint16_t)CFG_TUD_MSC << 1U) | \
			   ((uint16_t)CFG_TUD_HID << 2U) | ((uint16_t)CFG_TUD_MIDI << 3U) | ((uint16_t)CFG_TUD_VENDOR << 4U))

#define USB_VID   0xCAFEU
#define USB_BCD   0x0200U

/** USB language ID for English (United States), string descriptor index 0. */
#define USB_LANGID_ENGLISH_US 0x0409U

/** Maximum characters in a string descriptor (buffer holds the header plus 31 UTF-16 units). */
#define USB_STRING_MAX_CHARS 31U

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
// MISRA Deviation D2 (Rule 8.7): TinyUSB callback referenced by the stack in lib/
uint8_t const * tud_descriptor_device_cb(void)
{
	static tusb_desc_device_t const desc_device =
	{
		.bLength            = sizeof(tusb_desc_device_t),
		.bDescriptorType    = TUSB_DESC_DEVICE,
		.bcdUSB             = USB_BCD,

		// Use Interface Association Descriptor (IAD) for CDC
		// As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
		.bDeviceClass       = TUSB_CLASS_MISC,
		.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
		.bDeviceProtocol    = MISC_PROTOCOL_IAD,
		.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

		.idVendor           = USB_VID,
		.idProduct          = USB_PID,
		.bcdDevice          = 0x0100U,

		.iManufacturer      = 0x01,
		.iProduct           = 0x02,
		.iSerialNumber      = 0x03,

		.bNumConfigurations = 0x01
	};

	return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum
{
	ITF_NUM_CDC_0 = 0,
	ITF_NUM_CDC_0_DATA,
	ITF_NUM_TOTAL
};

_Static_assert(CFG_TUD_CDC == 1, "Application supports exactly one CDC interface");

/* Configuration + one CDC (IAD, 2 interfaces, functional descriptors, 3 endpoints).
 * The static assertion on the descriptor array keeps this in sync with TinyUSB. */
#define CONFIG_TOTAL_LEN    75U

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
// LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
// 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
  #define EPNUM_CDC_0_NOTIF   0x81
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x82

  #define EPNUM_CDC_1_NOTIF   0x84
  #define EPNUM_CDC_1_OUT     0x05
  #define EPNUM_CDC_1_IN      0x85

#elif CFG_TUSB_MCU == OPT_MCU_SAMG || CFG_TUSB_MCU ==  OPT_MCU_SAMX7X
// SAMG & SAME70 don't support a same endpoint number with different direction IN and OUT
//    e.g EP1 OUT & EP1 IN cannot exist together
  #define EPNUM_CDC_0_NOTIF   0x81
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x83

  #define EPNUM_CDC_1_NOTIF   0x84
  #define EPNUM_CDC_1_OUT     0x05
  #define EPNUM_CDC_1_IN      0x86

#elif CFG_TUSB_MCU == OPT_MCU_FT90X || CFG_TUSB_MCU == OPT_MCU_FT93X
// FT9XX doesn't support a same endpoint number with different direction IN and OUT
//    e.g EP1 OUT & EP1 IN cannot exist together
  #define EPNUM_CDC_0_NOTIF   0x81
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x83

  #define EPNUM_CDC_1_NOTIF   0x84
  #define EPNUM_CDC_1_OUT     0x05
  #define EPNUM_CDC_1_IN      0x86

#else
  #define EPNUM_CDC_0_NOTIF   0x81
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x82

  #define EPNUM_CDC_1_NOTIF   0x83
  #define EPNUM_CDC_1_OUT     0x04
  #define EPNUM_CDC_1_IN      0x84
#endif

/**
 * @brief Full-speed configuration descriptor.
 *
 * @return Pointer to the static descriptor bytes.
 */
static uint8_t const *fs_configuration(void)
{
	static uint8_t const desc_fs_configuration[] =
	{
		// Config number, interface count, string index, total length, attribute, power in mA
		// cppcheck-suppress [misra-c2012-10.1,misra-c2012-10.4,misra-c2012-10.8,misra-c2012-12.1,misra-c2012-12.2] ; Deviation D5: TinyUSB descriptor macro
		TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

		// 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
		// cppcheck-suppress [misra-c2012-10.1,misra-c2012-10.4,misra-c2012-10.8,misra-c2012-12.1,misra-c2012-12.2] ; Deviation D5: TinyUSB descriptor macro
		TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),

	};

	_Static_assert(sizeof(desc_fs_configuration) == CONFIG_TOTAL_LEN, "USB descriptor length mismatch");

	return desc_fs_configuration;
}

#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

static uint8_t const desc_hs_configuration[] =
{
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

	// 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 512),

};

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
static tusb_desc_device_qualifier_t const desc_device_qualifier =
{
	.bLength            = sizeof(tusb_desc_device_t),
	.bDescriptorType    = TUSB_DESC_DEVICE,
	.bcdUSB             = USB_BCD,

	.bDeviceClass       = TUSB_CLASS_MISC,
	.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol    = MISC_PROTOCOL_IAD,

	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
	.bNumConfigurations = 0x01,
	.bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
// MISRA Deviation D2 (Rule 8.7): TinyUSB callback referenced by the stack in lib/
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
	return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
// MISRA Deviation D2 (Rule 8.7): TinyUSB callback referenced by the stack in lib/
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
	(void) index; // for multiple configurations

	// if link speed is high return fullspeed config, and vice versa
	return (tud_speed_get() == TUSB_SPEED_HIGH) ?  fs_configuration() : desc_hs_configuration;
}

#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
// MISRA Deviation D2 (Rule 8.7): TinyUSB callback referenced by the stack in lib/
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
	(void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
	// Although we are highspeed, host may be fullspeed.
	uint8_t const *descriptor = (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : fs_configuration();
#else
	uint8_t const *descriptor = fs_configuration();
#endif
	return descriptor;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+


/**
 * @brief Return the ASCII text for a string descriptor index.
 *
 * @param[in] index String descriptor index (1-4).
 *
 * @return Pointer to a NUL-terminated string, or NULL for an unknown index.
 */
static char const *string_descriptor_text(uint8_t index)
{
	// array of pointer to string descriptors
	static char const *const string_desc_arr[] =
	{
		NULL,                           // 0: supported language (USB_LANGID_ENGLISH_US, set in the callback)
		"Signalbridge",                 // 1: Manufacturer
		"Signalbridge Device",          // 2: Product
		NULL,                           // 3: Serial number (chip unique ID, set at runtime)
		"Signalbridge CDC",             // 4: CDC Interface
	};
	static char serial[(2U * (uint32_t)PICO_UNIQUE_BOARD_ID_SIZE_BYTES) + 1U];
	char const *str = NULL;

	// Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
	// https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
	if (index == 3U)
	{
		if (serial[0] == '\0')
		{
			pico_get_unique_board_id_string(serial, sizeof(serial));
		}
		str = serial;
	}
	else if ((size_t)index < (sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
	{
		str = string_desc_arr[index];
	}
	else
	{
		// Unknown index: the request is stalled
	}
	return str;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// MISRA Deviation D2 (Rule 8.7): TinyUSB callback referenced by the stack in lib/
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	static uint16_t desc_str[USB_STRING_MAX_CHARS + 1U];
	uint16_t const *result = desc_str;
	uint8_t chr_count = 0U;

	(void) langid;

	if (index == 0U)
	{
		desc_str[1] = USB_LANGID_ENGLISH_US;
		chr_count = 1U;
	}
	else
	{
		char const *str = string_descriptor_text(index);

		if (str == NULL)
		{
			result = NULL;
		}
		else
		{
			// Cap at max char
			size_t length = strlen(str);
			if (length > USB_STRING_MAX_CHARS)
			{
				length = USB_STRING_MAX_CHARS;
			}
			chr_count = (uint8_t)length;

			// Convert ASCII string into UTF-16
			for (uint8_t i = 0U; i < chr_count; i++)
			{
				desc_str[1U + i] = (uint16_t)(uint8_t)str[i];
			}
		}
	}

	if (result != NULL)
	{
		// first byte is length (including header), second byte is string type
		desc_str[0] = (uint16_t)(((uint16_t)TUSB_DESC_STRING << 8U) | ((2U * (uint16_t)chr_count) + 2U));
	}

	return result;
}

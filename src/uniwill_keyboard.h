/* SPDX-License-Identifier: GPL-2.0+ */
/*!
 * Copyright (c) 2020-2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-drivers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef UNIWILL_KEYBOARD_H
#define UNIWILL_KEYBOARD_H

#include "tuxedo_keyboard_common.h"
#include <linux/acpi.h>
#include <linux/wmi.h>
#include <linux/workqueue.h>
#include <linux/keyboard.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/led-class-multicolor.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/efi.h>
#include <linux/slab.h>
#include <linux/i8042.h>
#include <linux/serio.h>
#include <acpi/battery.h>
#include "uniwill_interfaces.h"
#include "uniwill_leds.h"
#include "uniwill_mechrevo_quirks.h"
#if defined(CONFIG_ACPI_PLATFORM_PROFILE) || defined(CONFIG_ACPI_PLATFORM_PROFILE_MODULE)
#include <linux/platform_profile.h>
#endif

#define FAN_ON_MIN_SPEED_PERCENT 25

#define UNIWILL_OSD_RADIOON				0x01A
#define UNIWILL_OSD_RADIOOFF				0x01B
#define UNIWILL_OSD_KB_LED_LEVEL0			0x03B
#define UNIWILL_OSD_KB_LED_LEVEL1			0x03C
#define UNIWILL_OSD_KB_LED_LEVEL2			0x03D
#define UNIWILL_OSD_KB_LED_LEVEL3			0x03E
#define UNIWILL_OSD_KB_LED_LEVEL4			0x03F
#define UNIWILL_OSD_DC_ADAPTER_CHANGE			0x0AB
#define UNIWILL_OSD_MODE_CHANGE_KEY_EVENT		0x0B0

#define UNIWILL_KEY_RFKILL				0x0A4
#define UNIWILL_KEY_KBDILLUMDOWN			0x0B1
#define UNIWILL_KEY_KBDILLUMUP				0x0B2
#define UNIWILL_KEY_FN_LOCK				0x0B8
#define UNIWILL_KEY_KBDILLUMTOGGLE			0x0B9

#define UNIWILL_OSD_TOUCHPADWORKAROUND			0xFFF

#define UNIWILL_FN_LOCK_MASK				0x10

#define UW_MEMORY_OVERCLOCKING_SWITCH			0x33
#define UW_MEMORY_OVERCLOCKING_SUPPORT			0x60
#define UW_CPU_PERFORMANCE_AND_OVERCLOCKING_SUPPORT	0x6E
#define UW_CPU_PERFORMANCE_AND_OVERCLOCKING_SWITCH	0x6f

static void uw_charging_priority_write_state(void);
static void uw_charging_profile_write_state(void);
static void uniwill_set_custom_profile_mode(bool zero_bit_initially);

struct tuxedo_keyboard_driver uniwill_keyboard_driver;

struct uniwill_device_features_t uniwill_device_features;

static bool uw_feats_loaded = false;

static u8 uniwill_kbd_bl_enable_state_on_start = 0xff;

static struct key_entry uniwill_wmi_keymap[] = {
	// { KE_KEY,	UNIWILL_OSD_RADIOON,		{ KEY_RFKILL } },
	// { KE_KEY,	UNIWILL_OSD_RADIOOFF,		{ KEY_RFKILL } },
	// { KE_KEY,	0xb0,				{ KEY_F13 } },
	// Manual mode rfkill
	{ KE_KEY,	UNIWILL_KEY_RFKILL,		{ KEY_RFKILL }},
	{ KE_KEY,	UNIWILL_OSD_TOUCHPADWORKAROUND,	{ KEY_F21 } },
	// Keyboard brightness
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMDOWN,	{ KEY_KBDILLUMDOWN } },
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMUP,		{ KEY_KBDILLUMUP } },
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMTOGGLE,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL0,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL1,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL2,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL3,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL4,	{ KEY_KBDILLUMTOGGLE } },
	// Send FN_ESC to user space as input-event-codes.h does not define Fn-Lock
	{ KE_KEY,	UNIWILL_KEY_FN_LOCK,		{ KEY_FN_ESC } },
	// Only used to put ev bits
	{ KE_KEY,	0xffff,				{ KEY_F6 } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTALT } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTMETA } },
	{ KE_END,	0 }
};

static struct uniwill_interfaces_t {
	struct uniwill_interface_t *wmi;
} uniwill_interfaces = { .wmi = NULL };

uniwill_event_callb_t uniwill_event_callb;

int uniwill_read_ec_ram(u16 address, u8 *data)
{
	int status;

	if (!IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		status = uniwill_interfaces.wmi->read_ec_ram(address, data);
	else {
		pr_err("no active interface while read addr 0x%04x\n", address);
		status = -EIO;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_read_ec_ram);

int uniwill_read_ec_ram_with_retry(u16 address, u8 *data, int retries)
{
	int status, i;

	for (i = 0; i < retries; ++i) {
		status = uniwill_read_ec_ram(address, data);
		if (status != 0)
			pr_debug("uniwill_read_ec_ram(...) failed.\n");
		else
			break;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_read_ec_ram_with_retry);

static int uniwill_read_ec_ram_u16(u16 hibyte_address, u16 lobyte_address, u16 *data) {
	int result;
	u8 hi, lo;
	result = uniwill_read_ec_ram(hibyte_address, &hi);
	if (result)
		return result;
	result = uniwill_read_ec_ram(lobyte_address, &lo);
	if (result)
		return result;
	*data = (hi << 8) | lo;
	return result;
}

int uniwill_write_ec_ram(u16 address, u8 data)
{
	int status;

	if (!IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		status = uniwill_interfaces.wmi->write_ec_ram(address, data);
	else {
		pr_err("no active interface while write addr 0x%04x data 0x%02x\n", address, data);
		status = -EIO;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_write_ec_ram);

int uniwill_write_ec_ram_with_retry(u16 address, u8 data, int retries)
{
	int status, i;
	u8 control_data;

	for (i = 0; i < retries; ++i) {
		status = uniwill_write_ec_ram(address, data);
		if (status != 0) {
			msleep(50);
			continue;
		}
		else {
			status = uniwill_read_ec_ram(address, &control_data);
			if (status != 0 || data != control_data) {
				msleep(50);
				continue;
			}
			break;
		}
	}

	return status;
}
EXPORT_SYMBOL(uniwill_write_ec_ram_with_retry);


int uniwill_wmi_evaluate(u8 function, u32 arg, u32 *return_buffer)
{
    int status;

    if (!IS_ERR_OR_NULL(uniwill_interfaces.wmi) &&
        !IS_ERR_OR_NULL(uniwill_interfaces.wmi->wmi_evaluate)) {
        status = uniwill_interfaces.wmi->wmi_evaluate(function, arg, return_buffer);
    } else {
        pr_err("no active interface or ec_evaluate while calling function %u\n", function);
        status = -EIO;
    }

    return status;
}
EXPORT_SYMBOL(uniwill_wmi_evaluate);

static DEFINE_MUTEX(uniwill_interface_modification_lock);

int uniwill_add_interface(struct uniwill_interface_t *interface)
{
	mutex_lock(&uniwill_interface_modification_lock);

	if (strcmp(interface->string_id, UNIWILL_INTERFACE_WMI_STRID) == 0)
		uniwill_interfaces.wmi = interface;
	else {
		TUXEDO_DEBUG("trying to add unknown interface\n");
		mutex_unlock(&uniwill_interface_modification_lock);
		return -EINVAL;
	}
	interface->event_callb = uniwill_event_callb;

	mutex_unlock(&uniwill_interface_modification_lock);

	// Initialize driver if not already present
	tuxedo_keyboard_init_driver(&uniwill_keyboard_driver);

	return 0;
}
EXPORT_SYMBOL(uniwill_add_interface);

int uniwill_remove_interface(struct uniwill_interface_t *interface)
{
	mutex_lock(&uniwill_interface_modification_lock);

	if (strcmp(interface->string_id, UNIWILL_INTERFACE_WMI_STRID) == 0) {
		// Remove driver if last interface is removed
		tuxedo_keyboard_remove_driver(&uniwill_keyboard_driver);

		uniwill_interfaces.wmi = NULL;
	} else {
		mutex_unlock(&uniwill_interface_modification_lock);
		return -EINVAL;
	}

	mutex_unlock(&uniwill_interface_modification_lock);

	return 0;
}
EXPORT_SYMBOL(uniwill_remove_interface);

int uniwill_get_active_interface_id(char **id_str)
{
	if (IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		return -ENODEV;

	if (!IS_ERR_OR_NULL(id_str))
		*id_str = uniwill_interfaces.wmi->string_id;

	return 0;
}
EXPORT_SYMBOL(uniwill_get_active_interface_id);

static void key_event_work(struct work_struct *work)
{
	// Delay sometimes needed to make userspace reliably separate
	// the touchpadtoggle key events from the custom key events
	// coming from firmware
	msleep(50);
	sparse_keymap_report_known_event(
		uniwill_keyboard_driver.input_device,
		UNIWILL_OSD_TOUCHPADWORKAROUND,
		1,
		true
	);
}
static DECLARE_WORK(uniwill_key_event_work, key_event_work);

static void uniwill_write_kbd_bl_enable(u8 enable)
{
	u8 backlight_data;
	enable = enable & 0x01;

	uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &backlight_data);
	backlight_data = backlight_data & ~(1 << 1);
	backlight_data |= (!enable << 1);
	uniwill_write_ec_ram(UW_EC_REG_KBD_BL_STATUS, backlight_data);
}

void uniwill_event_callb(u32 code)
{
	switch (code) {
		case UNIWILL_OSD_MODE_CHANGE_KEY_EVENT:
			// Special key combination when mode change key is pressed (the one next to
			// the power key). Opens TCC by default when installed.
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTMETA, 1);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTALT, 1);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_F6, 1);
			input_sync(uniwill_keyboard_driver.input_device);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_F6, 0);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTALT, 0);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTMETA, 0);
			input_sync(uniwill_keyboard_driver.input_device);
			break;
		case UNIWILL_OSD_DC_ADAPTER_CHANGE:
			// Refresh keyboard state and charging settings on cable switch event and make sure that the custom
			// profile mode is still applied in case it's needed.
			uniwill_set_custom_profile_mode(false);
			uniwill_leds_restore_state_extern();
			msleep(50);
			uw_charging_priority_write_state();
			uw_charging_profile_write_state();
			break;
		case UNIWILL_KEY_KBDILLUMTOGGLE:
		case UNIWILL_OSD_KB_LED_LEVEL0:
		case UNIWILL_OSD_KB_LED_LEVEL1:
		case UNIWILL_OSD_KB_LED_LEVEL2:
		case UNIWILL_OSD_KB_LED_LEVEL3:
		case UNIWILL_OSD_KB_LED_LEVEL4:
			// Notify userspace/UPower that the firmware changed the keyboard backlight
			// brightness on white only keyboards. Fallthrough on other keyboards to
			// emit KEY_KBDILLUMTOGGLE.
			if (uniwill_leds_notify_brightness_change_extern())
				return;
			fallthrough;
		default:
			if (uniwill_keyboard_driver.input_device != NULL)
				if (!sparse_keymap_report_known_event(uniwill_keyboard_driver.input_device, code, 1, true))
					TUXEDO_DEBUG("Unknown code - %d (%0#6x)\n", code, code);
	}
}

static void uniwill_set_custom_profile_mode(bool zero_bit_initially)
{
	// Set custom profile mode if needed
	struct uniwill_device_features_t *uw_feats = uniwill_get_device_features();
	if (uw_feats->uniwill_custom_profile_mode_needed) {
		u8 data;
		if (uniwill_read_ec_ram(UW_EC_REG_CUSTOM_PROFILE, &data))
			return;
		if (zero_bit_initially) {
			// Certain devices seem to need this first reset to zero on boot to have it properly applied
			data &= ~(1 << 6);
			uniwill_write_ec_ram(UW_EC_REG_CUSTOM_PROFILE, data);
			msleep(50);
		}
		data |= (1 << 6);
		uniwill_write_ec_ram(UW_EC_REG_CUSTOM_PROFILE, data);
	}
}

#define UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS	0x24
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_RED	"lightbar_rgb:1:status"
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN	"lightbar_rgb:2:status"
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE	"lightbar_rgb:3:status"
#define UNIWILL_LIGHTBAR_LED_NAME_ANIMATION	"lightbar_animation::status"

static void uniwill_write_lightbar_rgb(u8 red, u8 green, u8 blue)
{
	if (red <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x0749, red);
	}
	if (green <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x074a, green);
	}
	if (blue <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x074b, blue);
	}
}

static void uniwill_read_lightbar_rgb(u8 *red, u8 *green, u8 *blue)
{
	uniwill_read_ec_ram(0x0749, red);
	uniwill_read_ec_ram(0x074a, green);
	uniwill_read_ec_ram(0x074b, blue);
}

static void uniwill_write_lightbar_animation(bool animation_status)
{
	u8 value;

	uniwill_read_ec_ram(0x0748, &value);
	if (animation_status) {
		value |= 0x80;
	} else {
		value &= ~0x80;
	}
	uniwill_write_ec_ram(0x0748, value);
}

static void uniwill_read_lightbar_animation(bool *animation_status)
{
	u8 lightbar_animation_data;
	uniwill_read_ec_ram(0x0748, &lightbar_animation_data);
	*animation_status = (lightbar_animation_data & 0x80) > 0;
}

static int lightbar_set_blocking(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	u8 red = 0xff, green = 0xff, blue = 0xff;
	bool led_red = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE) != NULL;
	bool led_animation = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_ANIMATION) != NULL;

	if (led_red || led_green || led_blue) {
		if (led_red) {
			red = brightness;
		} else if (led_green) {
			green = brightness;
		} else if (led_blue) {
			blue = brightness;
		}
		uniwill_write_lightbar_rgb(red, green, blue);
		// Also make sure the animation is off
		uniwill_write_lightbar_animation(false);
	} else if (led_animation) {
		if (brightness == 1) {
			uniwill_write_lightbar_animation(true);
		} else {
			uniwill_write_lightbar_animation(false);
		}
	}
	return 0;
}

static enum led_brightness lightbar_get(struct led_classdev *led_cdev)
{
	u8 red, green, blue;
	bool animation_status;
	bool led_red = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE) != NULL;
	bool led_animation = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_ANIMATION) != NULL;

	if (led_red || led_green || led_blue) {
		uniwill_read_lightbar_rgb(&red, &green, &blue);
		if (led_red) {
			return red;
		} else if (led_green) {
			return green;
		} else if (led_blue) {
			return blue;
		}
	} else if (led_animation) {
		uniwill_read_lightbar_animation(&animation_status);
		return animation_status ? 1 : 0;
	}

	return 0;
}

static bool uw_lightbar_loaded;
static struct led_classdev lightbar_led_classdevs[] = {
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_RED,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_ANIMATION,
		.max_brightness = 1,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	}
};

static int uw_lightbar_init(struct platform_device *dev)
{
	int i, j, status;

	bool lightbar_supported = false
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		|| dmi_match(DMI_BOARD_NAME, "TRINITY1501I")
		|| dmi_match(DMI_BOARD_NAME, "TRINITY1701I")
		|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI04")
		|| dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04")

#endif
		;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
	TUXEDO_ERROR(
		"Warning: Kernel version less that 4.18, lightbar might not be properly recognized.");
#endif

	if (!lightbar_supported)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(lightbar_led_classdevs); ++i) {
		status = led_classdev_register(&dev->dev, &lightbar_led_classdevs[i]);
		if (status < 0) {
			for (j = 0; j < i; j++)
				led_classdev_unregister(&lightbar_led_classdevs[j]);
			return status;
		}
	}

	// Init default state
	uniwill_write_lightbar_animation(false);
	uniwill_write_lightbar_rgb(0, 0, 0);

	return 0;
}

static int uw_lightbar_remove(struct platform_device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(lightbar_led_classdevs); ++i) {
		led_classdev_unregister(&lightbar_led_classdevs[i]);
	}
	return 0;
}

static bool uw_charging_prio_loaded = false;
static bool uw_charging_prio_last_written_value;

static ssize_t uw_charging_prios_available_show(struct device *child,
						struct device_attribute *attr,
						char *buffer);
static ssize_t uw_charging_prio_show(struct device *child,
				     struct device_attribute *attr, char *buffer);
static ssize_t uw_charging_prio_store(struct device *child,
				      struct device_attribute *attr,
				      const char *buffer, size_t size);

struct uw_charging_prio_attrs_t {
	struct device_attribute charging_prios_available;
	struct device_attribute charging_prio;
} uw_charging_prio_attrs = {
	.charging_prios_available = __ATTR(charging_prios_available, 0444, uw_charging_prios_available_show, NULL),
	.charging_prio = __ATTR(charging_prio, 0644, uw_charging_prio_show, uw_charging_prio_store)
};

static struct attribute *uw_charging_prio_attrs_list[] = {
	&uw_charging_prio_attrs.charging_prios_available.attr,
	&uw_charging_prio_attrs.charging_prio.attr,
	NULL
};

static struct attribute_group uw_charging_prio_attr_group = {
	.name = "charging_priority",
	.attrs = uw_charging_prio_attrs_list
};

/*
 * charging_prio values
 *     0 => charging priority
 *     1 => performance priority
 */
static int uw_set_charging_priority(u8 charging_priority)
{
	u8 previous_data, next_data;
	int result;

	charging_priority = (charging_priority & 0x01) << 7;

	result = uniwill_read_ec_ram(0x07cc, &previous_data);
	if (result != 0)
		return result;

	next_data = (previous_data & ~(1 << 7)) | charging_priority;
	result = uniwill_write_ec_ram(0x07cc, next_data);
	if (result == 0)
		uw_charging_prio_last_written_value = charging_priority;

	return result;
}

static int uw_get_charging_priority(u8 *charging_priority)
{
	int result = uniwill_read_ec_ram(0x07cc, charging_priority);
	*charging_priority = (*charging_priority >> 7) & 0x01;
	return result;
}

static int uw_has_charging_priority(bool *status)
{
	u8 data;
	int result;

	/*
	 * The ODM dropped this feature for certain reasons by just disabling the feature within their Control Center.
	 * Therefore every device using the control center until version 5.9.49.16 at least theoretically supports the
	 * feature. However, due to the support identification bit, being listed among the following devices does not
	 * automatically mean that this device supports the feature.
	 * After 5.9.50.3 devices may still have the support identification bit set but don't officially support the
	 * feature anymore.
	*/
	bool device_before_feature_drop = false
		|| dmi_match(DMI_BOARD_NAME, "PH4PRX1_PH6PRX1") // IBP Gen8
		|| dmi_match(DMI_BOARD_NAME, "PH6PG01_PH6PG71")
		|| dmi_match(DMI_BOARD_NAME, "PH4PG31")
		|| dmi_match(DMI_BOARD_NAME, "PHxARX1_PHxAQF1") // IBP Gen7
		|| dmi_match(DMI_BOARD_NAME, "PH6AG01_PH6AQ71_PH6AQI1")
		|| dmi_match(DMI_BOARD_NAME, "PHxTxX1") // IBP Gen6
		|| dmi_match(DMI_BOARD_NAME, "GMxXGxx") // Polaris Gen5
		|| dmi_match(DMI_BOARD_NAME, "GMxNGxx") // Polaris Gen3
		|| dmi_match(DMI_BOARD_NAME, "GMxTGxx") // Stellaris/Polaris Gen3
		|| dmi_match(DMI_BOARD_NAME, "GMxZGxx") // Stellaris Gen3
		|| dmi_match(DMI_BOARD_NAME, "GMxMGxx") // Polaris Gen2
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501I1650TI") // Polaris Gen1
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501A1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701A1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701I1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501I2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501A2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701I2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701A2060")
		|| dmi_match(DMI_BOARD_NAME, "PF5LUXG") // Pulse Gen2
		|| dmi_match(DMI_BOARD_NAME, "PULSE1401") // Pulse Gen1
		|| dmi_match(DMI_BOARD_NAME, "PULSE1501")
		;

	if (!device_before_feature_drop) {
		*status = false;
		return 0;
	}

	result = uniwill_read_ec_ram(0x0742, &data);
	if (result != 0)
		return -EIO;

	if (data & (1 << 5))
		*status = true;
	else
		*status = false;

	return 0;
}

static void uw_charging_priority_write_state(void)
{
	if (uw_charging_prio_loaded)
		uw_set_charging_priority(uw_charging_prio_last_written_value);
}

static void uw_charging_priority_init(struct platform_device *dev)
{
	u8 value;
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->uniwill_has_charging_prio)
		uw_charging_prio_loaded = sysfs_create_group(&dev->dev.kobj, &uw_charging_prio_attr_group) == 0;

	// Read for state init
	if (uw_charging_prio_loaded) {
		uw_get_charging_priority(&value);
		uw_charging_prio_last_written_value = value;
	}
}

static bool uw_charging_profile_loaded = false;
static u8 uw_charging_profile_last_written_value;

static ssize_t uw_charging_profiles_available_show(struct device *child,
						   struct device_attribute *attr,
						   char *buffer);
static ssize_t uw_charging_profile_show(struct device *child,
					struct device_attribute *attr, char *buffer);
static ssize_t uw_charging_profile_store(struct device *child,
					 struct device_attribute *attr,
					 const char *buffer, size_t size);

struct uw_charging_profile_attrs_t {
	struct device_attribute charging_profiles_available;
	struct device_attribute charging_profile;
} uw_charging_profile_attrs = {
	.charging_profiles_available = __ATTR(charging_profiles_available, 0444, uw_charging_profiles_available_show, NULL),
	.charging_profile = __ATTR(charging_profile, 0644, uw_charging_profile_show, uw_charging_profile_store)
};

static struct attribute *uw_charging_profile_attrs_list[] = {
	&uw_charging_profile_attrs.charging_profiles_available.attr,
	&uw_charging_profile_attrs.charging_profile.attr,
	NULL
};

static struct attribute_group uw_charging_profile_attr_group = {
	.name = "charging_profile",
	.attrs = uw_charging_profile_attrs_list
};

/*
 * charging_profile values
 *     0 => high capacity
 *     1 => balanced
 *     2 => stationary
 */
static int uw_set_charging_profile(u8 charging_profile)
{
	u8 previous_data, next_data, shifted_profile;
	int result;

	/* Store unshifted value (0-2) for later restoration */
	charging_profile = charging_profile & 0x03;
	shifted_profile = charging_profile << 4;

	result = uniwill_read_ec_ram(0x07a6, &previous_data);
	if (result != 0)
		return result;

	next_data = (previous_data & ~(0x03 << 4)) | shifted_profile;
	result = uniwill_write_ec_ram(0x07a6, next_data);

	if (result == 0)
		uw_charging_profile_last_written_value = charging_profile;

	return result;
}

static int uw_get_charging_profile(u8 *charging_profile)
{
	int result = uniwill_read_ec_ram(0x07a6, charging_profile);
	if (result == 0)
		*charging_profile = (*charging_profile >> 4) & 0x03;
	return result;
}

static int uw_has_charging_profile(bool *status)
{
	u8 data;
	int result;

	bool not_supported_device = false
		|| dmi_match(DMI_BOARD_NAME, "PF5PU1G")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
	;

	if (not_supported_device) {
		*status = false;
		return 0;
	}

	result = uniwill_read_ec_ram(0x078e, &data);
	if (result != 0)
		return -EIO;

	if (data & (1 << 3))
		*status = true;
	else
		*status = false;

	return 0;
}

static void uw_charging_profile_write_state(void)
{
	if (uw_charging_profile_loaded)
		uw_set_charging_profile(uw_charging_profile_last_written_value);
}

static void uw_charging_profile_init(struct platform_device *dev)
{
	u8 value;
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->uniwill_has_charging_profile)
		uw_charging_profile_loaded = sysfs_create_group(&dev->dev.kobj, &uw_charging_profile_attr_group) == 0;

	// Read for state init
	if (uw_charging_profile_loaded) {
		uw_get_charging_profile(&value);
		uw_charging_profile_last_written_value = value;
	}
}

struct char_to_u8_t {
	char* descriptor;
	u8 value;
};

static struct char_to_u8_t charging_profile_options[] = {
	{ .descriptor = "high_capacity", .value = 0x00 },
	{ .descriptor = "balanced",	 .value = 0x01 },
	{ .descriptor = "stationary",	 .value = 0x02 }
};

static ssize_t uw_charging_profiles_available_show(struct device *child,
						   struct device_attribute *attr,
						   char *buffer)
{
	int i, n;
	n = ARRAY_SIZE(charging_profile_options);
	for (i = 0; i < n; ++i) {
		sprintf(buffer + strlen(buffer), "%s",
			charging_profile_options[i].descriptor);
		if (i < n - 1)
			sprintf(buffer + strlen(buffer), " ");
		else
			sprintf(buffer + strlen(buffer), "\n");
	}

	return strlen(buffer);
}

static ssize_t uw_charging_profile_show(struct device *child,
					struct device_attribute *attr, char *buffer)
{
	u8 charging_profile_value;
	int i, result;

	result = uw_get_charging_profile(&charging_profile_value);
	if (result != 0)
		return result;

	for (i = 0; i < ARRAY_SIZE(charging_profile_options); ++i)
		if (charging_profile_options[i].value == charging_profile_value) {
			sprintf(buffer, "%s\n", charging_profile_options[i].descriptor);
			return strlen(buffer);
		}

	pr_err("Read charging profile value not matched to a descriptor\n");

	return -EIO;
}

static ssize_t uw_charging_profile_store(struct device *child,
					 struct device_attribute *attr,
					 const char *buffer, size_t size)
{
	u8 charging_profile_value;
	int i, result;
	char *buffer_copy;
	char *charging_profile_descriptor;
	buffer_copy = kmalloc(size + 1, GFP_KERNEL);
	strcpy(buffer_copy, buffer);
	charging_profile_descriptor = strstrip(buffer_copy);

	for (i = 0; i < ARRAY_SIZE(charging_profile_options); ++i)
		if (strcmp(charging_profile_options[i].descriptor, charging_profile_descriptor) == 0) {
			charging_profile_value = charging_profile_options[i].value;
			break;
		}

	kfree(buffer_copy);

	if (i < ARRAY_SIZE(charging_profile_options)) {
		// Option found try to set
		result = uw_set_charging_profile(charging_profile_value);
		if (result == 0)
			return size;
		else
			return -EIO;
	} else
		// Invalid input, not matched to an option
		return -EINVAL;
}

static struct char_to_u8_t charging_prio_options[] = {
	{ .descriptor = "charge_battery", .value = 0x00 },
	{ .descriptor = "performance",    .value = 0x01 }
};

static ssize_t uw_charging_prios_available_show(struct device *child,
						struct device_attribute *attr,
						char *buffer)
{
	int i, n;
	n = ARRAY_SIZE(charging_prio_options);
	for (i = 0; i < n; ++i) {
		sprintf(buffer + strlen(buffer), "%s",
			charging_prio_options[i].descriptor);
		if (i < n - 1)
			sprintf(buffer + strlen(buffer), " ");
		else
			sprintf(buffer + strlen(buffer), "\n");
	}

	return strlen(buffer);
}

static ssize_t uw_charging_prio_show(struct device *child,
				     struct device_attribute *attr, char *buffer)
{
	u8 charging_prio_value;
	int i, result;

	result = uw_get_charging_priority(&charging_prio_value);
	if (result != 0)
		return result;

	for (i = 0; i < ARRAY_SIZE(charging_prio_options); ++i)
		if (charging_prio_options[i].value == charging_prio_value) {
			sprintf(buffer, "%s\n", charging_prio_options[i].descriptor);
			return strlen(buffer);
		}

	pr_err("Read charging prio value not matched to a descriptor\n");

	return -EIO;
}

static ssize_t uw_charging_prio_store(struct device *child,
				      struct device_attribute *attr,
				      const char *buffer, size_t size)
{
	u8 charging_prio_value;
	int i, result;
	char *buffer_copy;
	char *charging_prio_descriptor;
	buffer_copy = kmalloc(size + 1, GFP_KERNEL);
	strcpy(buffer_copy, buffer);
	charging_prio_descriptor = strstrip(buffer_copy);

	for (i = 0; i < ARRAY_SIZE(charging_prio_options); ++i)
		if (strcmp(charging_prio_options[i].descriptor, charging_prio_descriptor) == 0) {
			charging_prio_value = charging_prio_options[i].value;
			break;
		}

	kfree(buffer_copy);

	if (i < ARRAY_SIZE(charging_prio_options)) {
		// Option found try to set
		result = uw_set_charging_priority(charging_prio_value);
		if (result == 0)
			return size;
		else
			return -EIO;
	} else
		// Invalid input, not matched to an option
		return -EINVAL;
}

/*
 * We didn't find any identification bits to retrieve the information if the
 * device supports the features usb_powershare and ac_auto_boot. The nb02
 * control center reads the support flag either from the registry or from the
 * UniWillVariable efivar. The efivar most likely stores this information at an
 * offset of 92 bytes. However, tests have shown that the control center most
 * likely sets the bit in this variable itself dynamically, so we have to check
 * DMI strings for now.
 */
static int is_auto_boot_and_powershare_supported(bool *status)
{
	*status = false ||
		  // IBP Gen9
		  dmi_match(DMI_BOARD_NAME, "GXxMRXx") ||
		  dmi_match(DMI_BOARD_NAME, "GXxHRXx") ||
		  // IBP Gen10
		  dmi_match(DMI_BOARD_NAME, "XxHP4NAx") ||
		  dmi_match(DMI_BOARD_NAME, "XxKK4NAx_XxSP4NAx") ||
		  dmi_match(DMI_BOARD_NAME, "XxAR4NAx") ||

		  // Stellaris Gen6
		  dmi_match(DMI_BOARD_NAME, "GM6IXxB_MB1") ||
		  dmi_match(DMI_BOARD_NAME, "GM6IXxB_MB2") ||
		  dmi_match(DMI_BOARD_NAME, "GM7IXxN") ||
		  // Stellaris Gen7
		  dmi_match(DMI_BOARD_NAME, "X6AR5xxY") ||
		  dmi_match(DMI_BOARD_NAME, "X6AR5xxY_mLED") ||
		  dmi_match(DMI_BOARD_NAME, "X6FR5xxY") ||

		  // Stellaris Slim Gen6 & Mechrevo family
		  dmi_match(DMI_BOARD_NAME, "GMxHGxx") ||
		  dmi_match(DMI_BOARD_NAME, "GM5HG0A") ||
		  dmi_match(DMI_PRODUCT_NAME, "yilong15 Pro Series GM5HG0A") ||
		  dmi_match(DMI_BOARD_NAME, "GM5IXxA") ||
		  dmi_match(DMI_BOARD_NAME, "GM5IX0A") ||

		  // InfinityBook Max Gen10
		  dmi_match(DMI_BOARD_NAME, "X5KK45xS_X5SP45xS");

	return 0;
}

static bool uw_ac_auto_boot_loaded = false;
static bool uw_ac_auto_boot_last_written_value;

static ssize_t uw_ac_auto_boot_show(struct device *child,
				    struct device_attribute *attr,
				    char *buffer);
static ssize_t uw_ac_auto_boot_store(struct device *child,
				     struct device_attribute *attr,
				     const char *buffer, size_t size);

struct uw_ac_auto_boot_attrs_t
{
	struct device_attribute ac_auto_boot;
} uw_ac_auto_boot_attrs = {
	.ac_auto_boot = __ATTR(ac_auto_boot, 0644, uw_ac_auto_boot_show, uw_ac_auto_boot_store)
};

static struct attribute *uw_ac_auto_boot_attrs_list[] = {
	&uw_ac_auto_boot_attrs.ac_auto_boot.attr,
	NULL};

static struct attribute_group uw_ac_auto_boot_attr_group = {
	.name = "ac_auto_boot",
	.attrs = uw_ac_auto_boot_attrs_list};

static int uw_set_ac_auto_boot(u8 ac_auto_boot)
{
	u8 previous_data, next_data;
	int result;

	ac_auto_boot = (ac_auto_boot & 0x01) << 3;

	result = uniwill_read_ec_ram(UW_EC_REG_AC_AUTO_BOOT_STATUS,
				     &previous_data);
	if (result != 0)
		return result;

	next_data = (previous_data & ~(1 << 3)) | ac_auto_boot;
	result = uniwill_write_ec_ram(UW_EC_REG_AC_AUTO_BOOT_STATUS, next_data);
	if (result == 0)
		uw_ac_auto_boot_last_written_value = ac_auto_boot;

	return result;
}

static int uw_get_ac_auto_boot(u8 *ac_auto_boot)
{
	int result;
	result = uniwill_read_ec_ram(UW_EC_REG_AC_AUTO_BOOT_STATUS,
				     ac_auto_boot);
	if (result == 0)
		*ac_auto_boot = (*ac_auto_boot >> 3) & 0x01;
	return result;
}

static int uw_has_ac_auto_boot(bool *status)
{
	return is_auto_boot_and_powershare_supported(status);
}

static void uw_ac_auto_boot_init(struct platform_device *dev)
{
	u8 value;
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->uniwill_has_ac_auto_boot)
		uw_ac_auto_boot_loaded =
			sysfs_create_group(&dev->dev.kobj,
					   &uw_ac_auto_boot_attr_group) == 0;

	// Read for state init
	if (uw_ac_auto_boot_loaded) {
		uw_get_ac_auto_boot(&value);
		uw_ac_auto_boot_last_written_value = value;
	}
}

static ssize_t uw_ac_auto_boot_show(struct device *child,
				    struct device_attribute *attr,
				    char *buffer)
{
	u8 ac_auto_boot_value;
	int result;

	result = uw_get_ac_auto_boot(&ac_auto_boot_value);
	if (result == 0)
		return sprintf(buffer, "%d\n", ac_auto_boot_value);

	return -EIO;
}

static ssize_t uw_ac_auto_boot_store(struct device *child,
				     struct device_attribute *attr,
				     const char *buffer, size_t size)
{
	u8 ac_auto_boot_value;
	int result;

	if (kstrtou8(buffer, 10, &ac_auto_boot_value) ||
	    ac_auto_boot_value < 0 || ac_auto_boot_value > 1)
		return -EINVAL;

	result = uw_set_ac_auto_boot(ac_auto_boot_value);
	if (result == 0)
		return size;
	else
		return -EIO;
}

static bool uw_usb_powershare_loaded = false;
static bool uw_usb_powershare_last_written_value;

static ssize_t uw_usb_powershare_show(struct device *child,
				      struct device_attribute *attr,
				      char *buffer);
static ssize_t uw_usb_powershare_store(struct device *child,
				       struct device_attribute *attr,
				       const char *buffer, size_t size);

struct uw_usb_powershare_attrs_t
{
	struct device_attribute usb_powershare;
} uw_usb_powershare_attrs = {
	.usb_powershare = __ATTR(usb_powershare, 0644, uw_usb_powershare_show, uw_usb_powershare_store)
};

static struct attribute *uw_usb_powershare_attrs_list[] = {
	&uw_usb_powershare_attrs.usb_powershare.attr,
	NULL};

static struct attribute_group uw_usb_powershare_attr_group = {
	.name = "usb_powershare",
	.attrs = uw_usb_powershare_attrs_list};

static int uw_set_usb_powershare(u8 usb_powershare)
{
	u8 previous_data, next_data;
	int result;
	usb_powershare = (usb_powershare & 0x01) << 4;

	result = uniwill_read_ec_ram(UW_EC_REG_USB_POWERSHARE_STATUS,
				     &previous_data);
	if (result != 0)
		return result;

	next_data = (previous_data & ~(1 << 4)) | usb_powershare;
	// This bit is set to 0 after a cold boot regardless of its original value for some reason.
	result = uniwill_write_ec_ram(UW_EC_REG_USB_POWERSHARE_STATUS,
				      next_data);
	if (result == 0)
		uw_usb_powershare_last_written_value = usb_powershare;

	return result;
}

static int uw_get_usb_powershare(u8 *usb_powershare)
{
	int result;
	result = uniwill_read_ec_ram(UW_EC_REG_USB_POWERSHARE_STATUS,
				     usb_powershare);
	if (result == 0)
		*usb_powershare = (*usb_powershare >> 4) & 0x01;
	return result;
}

static int uw_has_usb_powershare(bool *status)
{
	return is_auto_boot_and_powershare_supported(status);
}

static void uw_usb_powershare_init(struct platform_device *dev)
{
	u8 value;
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->uniwill_has_usb_powershare)
		uw_usb_powershare_loaded =
			sysfs_create_group(&dev->dev.kobj,
					   &uw_usb_powershare_attr_group) == 0;

	// Read for state init
	if (uw_usb_powershare_loaded) {
		uw_get_usb_powershare(&value);
		uw_usb_powershare_last_written_value = value;
	}
}

static ssize_t uw_usb_powershare_show(struct device *child,
				      struct device_attribute *attr,
				      char *buffer)
{
	u8 usb_powershare_value;
	int result;

	result = uw_get_usb_powershare(&usb_powershare_value);
	if (result == 0)
		return sprintf(buffer, "%d\n", usb_powershare_value);

	return -EIO;
}

static ssize_t uw_usb_powershare_store(struct device *child,
				       struct device_attribute *attr,
				       const char *buffer, size_t size)
{
	u8 usb_powershare_value;
	int result;

	if (kstrtou8(buffer, 10, &usb_powershare_value) ||
	    usb_powershare_value < 0 || usb_powershare_value > 1)
		return -EINVAL;

	result = uw_set_usb_powershare(usb_powershare_value);
	if (result == 0)
		return size;
	else
		return -EIO;
}

static ssize_t raw_cycle_count_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	int result;
	u16 cycle_count;
	result = uniwill_read_ec_ram_u16(UW_EC_REG_BATTERY_CYCN_HI, UW_EC_REG_BATTERY_CYCN_LO, &cycle_count);
	if (result)
		return result;
	return snprintf(buf, PAGE_SIZE, "%d\n", cycle_count);
}

static ssize_t raw_xif1_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	int result;
	u16 xif1;
	result = uniwill_read_ec_ram_u16(UW_EC_REG_BATTERY_XIF1_HI, UW_EC_REG_BATTERY_XIF1_LO, &xif1);
	if (result)
		return result;
	return snprintf(buf, PAGE_SIZE, "%d\n", xif1);
}

static ssize_t raw_xif2_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	int result;
	u16 xif2;
	result = uniwill_read_ec_ram_u16(UW_EC_REG_BATTERY_XIF2_HI, UW_EC_REG_BATTERY_XIF2_LO, &xif2);
	if (result)
		return result;
	return snprintf(buf, PAGE_SIZE, "%d\n", xif2);
}

static DEVICE_ATTR_RO(raw_cycle_count);
static DEVICE_ATTR_RO(raw_xif1);
static DEVICE_ATTR_RO(raw_xif2);

static struct attribute *uw_battery_attrs[] = {
	&dev_attr_raw_cycle_count.attr,
	&dev_attr_raw_xif1.attr,
	&dev_attr_raw_xif2.attr,
	NULL,
};

ATTRIBUTE_GROUPS(uw_battery);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int uw_battery_add(struct power_supply *battery)
#else
static int uw_battery_add(struct power_supply *battery, struct acpi_battery_hook *hook)
#endif
{
	TUXEDO_DEBUG("uw_battery_add\n");
	if (device_add_groups(&battery->dev, uw_battery_groups))
		return -ENODEV;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int uw_battery_remove(struct power_supply *battery)
#else
static int uw_battery_remove(struct power_supply *battery, struct acpi_battery_hook *hook)
#endif
{
	TUXEDO_DEBUG("uw_battery_remove\n");
	device_remove_groups(&battery->dev, uw_battery_groups);
	return 0;
}

static struct acpi_battery_hook uw_battery_hook = {
	.add_battery = uw_battery_add,
	.remove_battery = uw_battery_remove,
	.name = "TUXEDO Battery Extension",
};

static bool uw_battery_hook_registered = false;

static void uw_battery_init(void)
{
	battery_hook_register(&uw_battery_hook);
	uw_battery_hook_registered = true;
}

static void uw_battery_uninit(void)
{
	if (uw_battery_hook_registered)
		battery_hook_unregister(&uw_battery_hook);
	else
		TUXEDO_ERROR("attempted to unregister battery hook which was not registered\n");
}


static bool uw_mini_led_local_dimming_loaded = false;
static bool uw_mini_led_local_dimming_last_written_value;

static ssize_t uw_mini_led_local_dimming_show(struct device *child,
				     struct device_attribute *attr, char *buffer);
static ssize_t uw_mini_led_local_dimming_store(struct device *child,
				      struct device_attribute *attr,
				      const char *buffer, size_t size);

struct uw_mini_led_local_dimming_attrs_t {
	struct device_attribute mini_led_local_dimming;
} uw_mini_led_local_dimming_attrs = {
	.mini_led_local_dimming = __ATTR(mini_led_local_dimming, 0644, uw_mini_led_local_dimming_show, uw_mini_led_local_dimming_store)
};

static struct attribute *uw_mini_led_local_dimming_attrs_list[] = {
	&uw_mini_led_local_dimming_attrs.mini_led_local_dimming.attr,
	NULL
};

static struct attribute_group uw_mini_led_local_dimming_attr_group = {
	.name = "mini_led_local_dimming",
	.attrs = uw_mini_led_local_dimming_attrs_list
};

/*
 * mini_led_local_dimming values
 *     0 => local dimming off
 *     1 => local dimming on
 */
static int uw_set_mini_led_local_dimming(u8 mini_led_local_dimming)
{
	int result;
	u32 uw_data[10];

	if (mini_led_local_dimming == 1) {
		result = uniwill_wmi_evaluate(UNIWILL_WMI_FUNCTION_FEATURE_TOGGLE,
					    UNIWILL_WMI_LOCAL_DIMMING_ON,
					    uw_data);
	} else {
		result = uniwill_wmi_evaluate(UNIWILL_WMI_FUNCTION_FEATURE_TOGGLE,
					    UNIWILL_WMI_LOCAL_DIMMING_OFF,
					    uw_data);
	}
	if (result != 0)
		return result;

	uw_mini_led_local_dimming_last_written_value = mini_led_local_dimming;

	return result;
}

static int uw_get_mini_led_local_dimming(u8 *mini_led_local_dimming)
{
	/*
	 * As of now, we do not have any possibility to read out the current state of local dimming. However,
	 * as this feature is set to disabled on boot per default by calling uw_set_mini_led_local_dimming,
	 * uw_mini_led_local_dimming_last_written_value is always initialized and thereby should not cause
	 * any harm.
	 *
	 * A rather hacky solution could be the following, as uniwill_wmi_evaluate writes the current state
	 * into the return buffer before overwriting it:

	 * u32 return_buffer;
	 * bool initial_status;
	 * uniwill_wmi_evaluate(local_dimming, off, return_buffer);
	 * if (return_buffer == UNIWILL_WMI_LOCAL_DIMMING_ON)
	 * 	initial_status = true;
	 * else
	 * 	initial_status = false;
	 * uniwill_wmi_evaluate(local_dimming, initial_status, return_buffer);
	 * *mini_led_local_dimming = initial_status;
	 */
	*mini_led_local_dimming = uw_mini_led_local_dimming_last_written_value;
	return 0;
}

static int uw_has_mini_led_local_dimming(bool *status)
{
	u8 data;
	int result;

	result = uniwill_read_ec_ram(UW_EC_REG_MINI_LED_LOCAL_DIMMING_SUPPORT,
				     &data);
	if (result)
		return result;

	*status = (data != 0xFF) && ((data & 0x01) > 0);
	return 0;
}

static void uw_mini_led_local_dimming_init(struct platform_device *dev)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->uniwill_has_mini_led_local_dimming)
		uw_mini_led_local_dimming_loaded = sysfs_create_group(&dev->dev.kobj, &uw_mini_led_local_dimming_attr_group) == 0;

	// Set default to off
	uw_set_mini_led_local_dimming(false);
}

static ssize_t uw_mini_led_local_dimming_show(struct device *child,
				      struct device_attribute *attr,
				      char *buffer)
{
	u8 mini_led_local_dimming_value;
	int result;

	result = uw_get_mini_led_local_dimming(&mini_led_local_dimming_value);
	if (result == 0)
		return sprintf(buffer, "%d\n", mini_led_local_dimming_value);

	return -EIO;
}

static ssize_t uw_mini_led_local_dimming_store(struct device *child,
				       struct device_attribute *attr,
				       const char *buffer, size_t size)
{
	u8 mini_led_local_dimming_value;
	int result;

	if (kstrtou8(buffer, 10, &mini_led_local_dimming_value) ||
	    mini_led_local_dimming_value < 0 || mini_led_local_dimming_value > 1)
		return -EINVAL;

	result = uw_set_mini_led_local_dimming(mini_led_local_dimming_value);
	if (result == 0)
		return size;
	else
		return -EIO;
}

static efi_guid_t uw_oem_magic_guid =
	EFI_GUID(0x9f33f85c, 0x13ca, 0x4fd1,
	         0x9c, 0x4a, 0x96, 0x21, 0x77, 0x22, 0xc5, 0x93);

static int uw_has_hidden_bios_options(bool *status)
{
	*status = false
		// Stellaris 16 G7
		|| dmi_match(DMI_BOARD_NAME, "X6AR5xxY")
		|| dmi_match(DMI_BOARD_NAME, "X6AR5xxY_mLED")
		// IBM 16 G10
		|| dmi_match(DMI_BOARD_NAME, "X6AR55xU");
	return 0;
}

static void uw_show_hidden_bios_options(void)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	efi_status_t st;
	u32 attr = 0;
	unsigned long size = 0;
	u8 *buf = NULL;
	bool changed = false;
	efi_char16_t name[] = L"OemMagicVariable";
	u8 b1, b2;

	if (!uw_feats->uniwill_has_hidden_bios_options) {
		pr_debug("hidden_bios_options: not supported on this device\n");
		return;
	}

	if (!efi_enabled(EFI_RUNTIME_SERVICES)) {
		pr_warn(
			"hidden_bios_options: EFI runtime services not available\n");
		return;
	}

	st = efi.get_variable(name, &uw_oem_magic_guid, &attr, &size, NULL);
	if (st != EFI_BUFFER_TOO_SMALL) {
		pr_err(
			"hidden_bios_options: get_variable probe failed: st=0x%lx\n",
			(unsigned long)st);
		return;
	}

	if (size <= UW_CPU_PERFORMANCE_AND_OVERCLOCKING_SWITCH) {
		pr_err(
			"hidden_bios_options: EFI variable too small (%lu bytes)\n",
			size);
		return;
	}

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		pr_err("hidden_bios_options: something went wrong during allocating read buffer\n");
		return;
	}

	st = efi.get_variable(name, &uw_oem_magic_guid, &attr, &size, buf);
	if (st != EFI_SUCCESS) {
		pr_err(
			"hidden_bios_options: get_variable read failed: st=0x%lx\n",
			(unsigned long)st);
		kfree(buf);
		return;
	}

	if (buf[UW_MEMORY_OVERCLOCKING_SUPPORT] != 0x01 ||
	    buf[UW_CPU_PERFORMANCE_AND_OVERCLOCKING_SUPPORT] != 0x01) {
		pr_warn(
			"hidden_bios_options: support bits not set (mem=0x%02x cpu=0x%02x) -> skip\n",
			buf[UW_MEMORY_OVERCLOCKING_SUPPORT],
			buf[UW_CPU_PERFORMANCE_AND_OVERCLOCKING_SUPPORT]);
		kfree(buf);
		return;
	}

	b1 = buf[UW_MEMORY_OVERCLOCKING_SWITCH];
	b2 = buf[UW_CPU_PERFORMANCE_AND_OVERCLOCKING_SWITCH];

	if (!((b1 == 0x00 || b1 == 0x01) && (b2 == 0x00 || b2 == 0x01))) {
		pr_err("hidden_bios_options: unexpected byte values off1=0x%02x off2=0x%02x -> skip\n",
			b1, b2);
		kfree(buf);
		return;
	}

	if (b1 == 0x00) {
		buf[UW_MEMORY_OVERCLOCKING_SWITCH] = 0x01;
		changed = true;
	}
	if (b2 == 0x00) {
		buf[UW_CPU_PERFORMANCE_AND_OVERCLOCKING_SWITCH] = 0x01;
		changed = true;
	}

	if (!changed) {
		pr_debug("hidden_bios_options: already enabled\n");
		kfree(buf);
		return;
	}

	st = efi.set_variable(name, &uw_oem_magic_guid, attr, size, buf);
	if (st != EFI_SUCCESS) {
		pr_warn("hidden_bios_options: set_variable failed: st=0x%lx\n",
			(unsigned long)st);
		kfree(buf);
		return;
	}

	pr_info("hidden_bios_options: enabled hidden BIOS options\n");

	kfree(buf);
}

static const u8 uw_romid_PH4PxX[14] = {0x0C, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const u8 uw_romid_PH6PxX[14] = {0x0C, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const struct dmi_system_id uw_sku_romid_table[] = {
	// IBPG8 mk1
	// Logic: If product serial matches 16inch use that, else default to 14inch
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_SKU, "IBP1XI08MK1"),
			DMI_MATCH(DMI_PRODUCT_SERIAL, "PH6PRX"),
		},
		.driver_data = (void *)&uw_romid_PH6PxX
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_SKU, "IBP1XI08MK1"),
		},
		.driver_data = (void *)&uw_romid_PH4PxX
	},
	// IBP16G8 mk2
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_SKU, "IBP1XI08MK2"),
			DMI_MATCH(DMI_PRODUCT_SERIAL, "PH6"),
		},
		.driver_data = (void *)&uw_romid_PH6PxX
	},
	{}
};

static int set_rom_id(void) {
	int i, ret;
	const struct dmi_system_id *uw_sku_romid;
	const u8 *romid;
	u8 data;
	bool romid_false = false;

	uw_sku_romid = dmi_first_match(uw_sku_romid_table);
	if (!uw_sku_romid)
		return 0;

	romid = (const u8 *)uw_sku_romid->driver_data;
	pr_debug("ROMID 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		 romid[0], romid[1], romid[2], romid[3], romid[4], romid[5], romid[6], romid[7],
		 romid[8], romid[9], romid[10], romid[11], romid[12], romid[13]);

	for (i = 0; i < 14; ++i) {
		ret = uniwill_read_ec_ram_with_retry(UW_EC_REG_ROMID_START + i, &data, 3);
		if (ret) {
			pr_debug("uniwill_read_ec_ram_with_retry(...) failed.\n");
			return ret;
		}
		pr_debug("ROMID index: %d, expected value: 0x%02X, actual value: 0x%02X\n", i, romid[i], data);
		if (data != romid[i]) {
			pr_debug("ROMID is false. Correcting...\n");
			romid_false = true;
			break;
		}
	}

	if (romid_false) {
		ret = uniwill_write_ec_ram_with_retry(UW_EC_REG_ROMID_SPECIAL_1, 0xA5, 3);
		if (ret) {
			pr_debug("uniwill_write_ec_ram_with_retry(...) failed.\n");
			return ret;
		}
		ret = uniwill_write_ec_ram_with_retry(UW_EC_REG_ROMID_SPECIAL_2, 0x78, 3);
		if (ret) {
			pr_debug("uniwill_write_ec_ram_with_retry(...) failed.\n");
			return ret;
		}
		for (i = 0; i < 14; ++i) {
			ret = uniwill_write_ec_ram_with_retry(UW_EC_REG_ROMID_START + i, romid[i], 3);
			if (ret) {
				pr_debug("uniwill_write_ec_ram_with_retry(...) failed.\n");
				return ret;
			}
		}
	}
	else
		pr_debug("ROMID is correct.\n");

	return 0;
}

static int has_universal_ec_fan_control(void) {
	int ret;
	u8 data;

	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	bool universal_fan_control_exception = false
		// For some reason, on this particular device, the 2nd fan is not controlled via the
		// "GPU" fan curve when the bit to separate both fancurves is set, but the old fan
		// control works just fine.
		|| uw_feats->model == UW_MODEL_PH4TRX
		// For some devices the "universal fan control" doesn't work for turning the fans
		// off reliably, however, the old fan control works.
		|| dmi_match(DMI_BOARD_NAME, "GXxMRXx")
		|| dmi_match(DMI_BOARD_NAME, "XxAR4NAx")
		|| dmi_match(DMI_BOARD_NAME, "X6FR5xxY")
		|| dmi_match(DMI_BOARD_NAME, "X5AR45xS")
	;

	if (universal_fan_control_exception) {
		return 0;
	}

	ret = uniwill_read_ec_ram(0x078e, &data);
	if (ret < 0) {
		return ret;
	}
	return (data >> 6) & 1;
}

static int has_double_pl4(bool *status)
{
	u8 data;
	int result;

	result = uniwill_read_ec_ram(0x0727, &data);
	if (result)
		return result;

	if (data & (1 << 7))
		*status = true;
	else
		*status = false;

	return 0;
}

struct uniwill_device_features_t *uniwill_get_device_features(void)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u32 status;
	int result;
	bool feats_loaded;

	if (uw_feats_loaded)
		return uw_feats;

	feats_loaded = true;

	status = uniwill_read_ec_ram(0x0740, &uw_feats->model);
	if (status != 0) {
		uw_feats->model = 0;
		feats_loaded = false;
	}

	uw_feats->uniwill_profile_v1_two_profs = false
		|| dmi_match(DMI_BOARD_NAME, "PF5PU1G")
		|| dmi_match(DMI_BOARD_NAME, "PULSE1401")
		|| dmi_match(DMI_BOARD_NAME, "PULSE1501")
	;

	uw_feats->uniwill_profile_v1_three_profs = false
	// Devices with "classic" profile support
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501A1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501A2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501I1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501I2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701A1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701A2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701I1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701I2060")
		|| dmi_match(DMI_BOARD_NAME, "GXxMRXx")
		|| dmi_match(DMI_BOARD_NAME, "GXxHRXx")
		|| dmi_match(DMI_BOARD_NAME, "XxHP4NAx")
		|| dmi_match(DMI_BOARD_NAME, "XxKK4NAx_XxSP4NAx")
		|| dmi_match(DMI_BOARD_NAME, "XxAR4NAx")

		// Note: XMG Fusion removed for now, seem to have
		// neither same power profile control nor TDP set
		//|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		//|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		//|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
	;

	uw_feats->uniwill_profile_v1_three_profs_leds_only = false
	// Devices where profile mainly controls power profile LED status
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XA02")
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XI02")
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI04")
		|| dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04")
#endif
	;

	uw_feats->uniwill_custom_profile_mode_needed = false
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I06")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS17I06")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I07")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS16A07")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLSL15I06")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLSL15A06")
		|| dmi_match(DMI_SYS_VENDOR, "MECHREVO")
		|| dmi_match(DMI_BOARD_NAME, "GM5HG0A")
		|| dmi_match(DMI_PRODUCT_NAME, "yilong15 Pro Series GM5HG0A")
		|| dmi_match(DMI_BOARD_NAME, "GXxMRXx")
		|| dmi_match(DMI_BOARD_NAME, "GXxHRXx")
		|| dmi_match(DMI_BOARD_NAME, "XxHP4NAx")
		|| dmi_match(DMI_BOARD_NAME, "XxKK4NAx_XxSP4NAx")
		|| dmi_match(DMI_BOARD_NAME, "X5KK45xS_X5SP45xS")
		|| dmi_match(DMI_BOARD_NAME, "X6KK45xU_X6SP45xU")
		|| dmi_match(DMI_BOARD_NAME, "X6AR55xU")
		|| dmi_match(DMI_BOARD_NAME, "X5AR45xS")
#endif
	;


	if (has_double_pl4(&uw_feats->uniwill_has_double_pl4) != 0)
		feats_loaded = false;

	uw_feats->uniwill_profile_v1 =
		uw_feats->uniwill_profile_v1_two_profs ||
		uw_feats->uniwill_profile_v1_three_profs;

	if (uw_has_charging_priority(&uw_feats->uniwill_has_charging_prio) != 0)
		feats_loaded = false;
	if (uw_has_charging_profile(&uw_feats->uniwill_has_charging_profile) != 0)
		feats_loaded = false;
	if (uw_has_ac_auto_boot(&uw_feats->uniwill_has_ac_auto_boot) != 0)
		feats_loaded = false;
	if (uw_has_usb_powershare(&uw_feats->uniwill_has_usb_powershare) != 0)
		feats_loaded = false;
	if (uw_has_mini_led_local_dimming(&uw_feats->uniwill_has_mini_led_local_dimming) != 0)
		feats_loaded = false;
	if (uw_has_hidden_bios_options(&uw_feats->uniwill_has_hidden_bios_options) != 0)
		feats_loaded = false;

	result = has_universal_ec_fan_control();
	if (result < 0) {
		feats_loaded = false;
	} else {
		uw_feats->uniwill_has_universal_ec_fan_control = (result == 1);
	}


	uw_feats->regmap = uniwill_match_ec_regmap();
	if (!uw_feats->regmap)
		uw_feats->regmap = &default_uniwill_regmap;

	if (feats_loaded)
		pr_debug("feats loaded\n");
	else
		pr_debug("feats not yet loaded\n");

	uw_feats_loaded = feats_loaded;

	return uw_feats;
}
EXPORT_SYMBOL(uniwill_get_device_features);

/*
 * Retrieve active register map for the identified motherboard.
 * Falls back to default hardcoded addresses for non-target models (zero regression).
 */
static inline const struct uniwill_ec_regmap *uniwill_get_active_regmap(void)
{
	struct uniwill_device_features_t *feats = uniwill_get_device_features();

	if (feats && feats->regmap)
		return feats->regmap;
	return &default_uniwill_regmap;
}

// Fn lock

static int uniwill_wmi_fn_lock_get(int *on)
{
	u8 data;
	int err;

	err = uniwill_read_ec_ram(UW_EC_REG_KBD_FN_LOCK_STATUS_BIT, &data);
	if (err)
		return err;

	if (on)
		*on = (data & UNIWILL_FN_LOCK_MASK) >> 4;

	return 0;
}

static int uniwill_wmi_fn_lock_set(int on)
{
	u8 data;
	int err;

	// possible race condition
	err = uniwill_read_ec_ram(UW_EC_REG_KBD_FN_LOCK_STATUS_BIT, &data);
	if (err)
		return err;

	if (on)
		data = data | UNIWILL_FN_LOCK_MASK;
	else
		data = data & ~UNIWILL_FN_LOCK_MASK;

	err = uniwill_write_ec_ram(UW_EC_REG_KBD_FN_LOCK_STATUS_BIT, data);
	if (err)
		return err;

	return 0;
}

static ssize_t uniwill_fn_lock_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err, on;

	err = uniwill_wmi_fn_lock_get(&on);
	if (err)
		return err;

	return sprintf(buf, "%d\n", on);
}

static ssize_t uniwill_fn_lock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int on, err;

	if (kstrtoint(buf, 10, &on) ||
			on < 0 || on > 1)
		return -EINVAL;

	err = uniwill_wmi_fn_lock_set(on);
	if (err)
		return err;

	return size;
}

static bool uniwill_fn_lock_available(void){
	int err, on;

	// Fn lock does not work for XMG Fusion
	// exclude all versions
	if (dmi_match(DMI_BOARD_NAME, "LAPQC71A")
	    || dmi_match(DMI_BOARD_NAME, "LAPQC71B")
	    || dmi_match(DMI_PRODUCT_NAME, "A60 MUV")) {
		return 0;
	}

	// do a read for test (this may not produce an error)
	err = uniwill_wmi_fn_lock_get(&on);
	if (err)
		return 0;
	else
		return 1;
}

static u8 direct_fan_control_current_value_fan0 = 0;
static u8 direct_fan_control_current_value_fan1 = 0;
static u8 direct_fan_control_current_value_fan0_suspend_save = 0;
static u8 direct_fan_control_current_value_fan1_suspend_save = 0;
static bool fans_initialized = false;
static bool direct_fan_control_started = false;
static bool direct_fan_control_suspend = false;
static void restart_direct_fan_control_work_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(direct_fan_control_restart_delayed_work, restart_direct_fan_control_work_handler);

int set_full_fan_mode(bool enable) {
	u8 mode_data;

	uniwill_read_ec_ram(0x0751, &mode_data);

	if (enable && !(mode_data & 0x40)) {
		// If not "full fan mode" (i.e. 0x40 bit not set) switch to it (required for old fancontrol)
		return uniwill_write_ec_ram(0x0751, mode_data | 0x40);
	}
	else if (mode_data & 0x40){
		// If "full fan mode" (i.e. 0x40 bit set) turn it off (required for new fancontrol)
		return uniwill_write_ec_ram(0x0751, mode_data & ~0x40);
	}

	return 0;
}
EXPORT_SYMBOL(set_full_fan_mode);

int uw_init_fan(void) {
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	int i, temp_offset;

	u16 addr_use_custom_fan_table_0 = 0x07c5; // use different tables for both fans (0x0f00-0x0f2f and 0x0f30-0x0f5f respectivly)
	u16 addr_use_custom_fan_table_1 = 0x07c6; // enable 0x0fxx fantables
	u8 offset_use_custom_fan_table_0 = 7;
	u8 offset_use_custom_fan_table_1 = 2;
	u8 value_use_custom_fan_table_0;
	u8 value_use_custom_fan_table_1;
	u16 addr_cpu_custom_fan_table_end_temp = 0x0f00;
	u16 addr_cpu_custom_fan_table_start_temp = 0x0f10;
	u16 addr_cpu_custom_fan_table_fan_speed = 0x0f20;
	u16 addr_gpu_custom_fan_table_end_temp = 0x0f30;
	u16 addr_gpu_custom_fan_table_start_temp = 0x0f40;
	u16 addr_gpu_custom_fan_table_fan_speed = 0x0f50;

	if (!fans_initialized && uw_feats->uniwill_has_universal_ec_fan_control) {
		set_full_fan_mode(false);

		uniwill_read_ec_ram(addr_use_custom_fan_table_0, &value_use_custom_fan_table_0);
		if (!((value_use_custom_fan_table_0 >> offset_use_custom_fan_table_0) & 1)) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_0, value_use_custom_fan_table_0 + (1 << offset_use_custom_fan_table_0), 3);
		}

		// Setup
		// - one controllable zone 0-115 deg
		// - rest 116-117, 117-118 etc single non reachable dummy zones
		//   with increasing ranges and max fan (same or increasing)
		uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_end_temp, 115, 3);
		uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_start_temp, 0, 3);
		uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_fan_speed, 0x01, 3);
		uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_end_temp, 120, 3);
		uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_start_temp, 0, 3);
		uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_fan_speed, 0x01, 3);
		temp_offset = 115;
		for (i = 0x1; i <= 0xf; ++i) {
			uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_end_temp + i, temp_offset + i + 1, 3);
			uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_start_temp + i, temp_offset + i, 3);
			uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_fan_speed + i, 0xc8, 3);
			uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_end_temp + i, temp_offset + i + 1, 3);
			uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_start_temp + i, temp_offset + i, 3);
			uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_fan_speed + i, 0xc8, 3);
		}

		uniwill_read_ec_ram(addr_use_custom_fan_table_1, &value_use_custom_fan_table_1);
		if (!((value_use_custom_fan_table_1 >> offset_use_custom_fan_table_1) & 1)) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_1, value_use_custom_fan_table_1 + (1 << offset_use_custom_fan_table_1), 3);
		}
	}

	fans_initialized = true;

	return 0;
}
EXPORT_SYMBOL(uw_init_fan);

static void restart_direct_fan_control_work_handler(struct work_struct *work)
{
	int i;
	u16 addr_fan0 = 0x1804;
	u16 addr_fan1 = 0x1809;

	pr_debug("restart fan control\n");

	set_full_fan_mode(false);
	msleep(10);
	set_full_fan_mode(true);

	// Attempt to write both fans as quick as possible before complete ramp-up
	pr_debug("prevent ramp-up start\n");
	for (i = 0; i < 10; ++i) {
		uniwill_write_ec_ram(addr_fan0, direct_fan_control_current_value_fan0 & 0xff);
		uniwill_write_ec_ram(addr_fan1, direct_fan_control_current_value_fan1 & 0xff);
		msleep(10);
	}
	pr_debug("prevent ramp-up done\n");

	schedule_delayed_work(&direct_fan_control_restart_delayed_work, msecs_to_jiffies(50 * 60 * 1000));
}

static int direct_fan_control(u32 fan_index, u8 fan_speed, bool prevent_rampup)
{
	u16 addr_for_fan;
	u16 addr_fan0 = 0x1804;
	u16 addr_fan1 = 0x1809;

	if (fan_index == 0) {
		addr_for_fan = addr_fan0;
		direct_fan_control_current_value_fan0 = fan_speed;
	} else if (fan_index == 1) {
		addr_for_fan = addr_fan1;
		direct_fan_control_current_value_fan1 = fan_speed;
	} else {
		return -EINVAL;
	}

	if (prevent_rampup && !direct_fan_control_started) {
		direct_fan_control_started = true;
		schedule_delayed_work(&direct_fan_control_restart_delayed_work, 0);
	} else {
		uniwill_write_ec_ram(addr_for_fan, fan_speed & 0xff);
	}

	return 0;
}

u32 uw_set_fan(u32 fan_index, u8 fan_speed)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u16 addr_for_fan;

	u16 addr_cpu_custom_fan_table_fan_speed = 0x0f20;
	u16 addr_gpu_custom_fan_table_fan_speed = 0x0f50;

	u8 byte_data;

	if (uw_feats->uniwill_has_universal_ec_fan_control) {
		uniwill_read_ec_ram(0x0751, &byte_data);
		if (!(byte_data & 0x40)) {
			uw_init_fan();

			if (fan_index == 0)
				addr_for_fan = addr_cpu_custom_fan_table_fan_speed;
			else if (fan_index == 1)
				addr_for_fan = addr_gpu_custom_fan_table_fan_speed;
			else
				return -EINVAL;

			if (fan_speed > NB02_FAN_SPEED_MAX)
				return -EINVAL;

			// Don't allow vallues between fan-off and minimum fan-on-speed
			if (fan_speed < FAN_ON_MIN_SPEED_PERCENT * NB02_FAN_SPEED_MAX / 2 / 100)
				fan_speed = 0;
			else if (fan_speed < FAN_ON_MIN_SPEED_PERCENT * NB02_FAN_SPEED_MAX / 100)
				fan_speed = FAN_ON_MIN_SPEED_PERCENT * NB02_FAN_SPEED_MAX / 100;

			if (fan_speed == 0) {
				// Avoid hard coded EC behaviour: Setting fan speed = 0x00 spins the fan up
				// to 0x3c (30%) for 3 minutes before going to 0x00. Setting fan speed = 1
				// also causes the fan to stop since on 2020 or later TF devices the
				// microcontroller in the fan itself is intelligent enough to not try to
				// start up the motor when the speed is to slow. Older devices don't use
				// this fan controll anyway, but the else case below.
				fan_speed = 1;
			}

			uniwill_write_ec_ram(addr_for_fan, fan_speed & 0xff);

			direct_fan_control(fan_index, fan_speed, false);
		}
	}
	else { // old workaround using full fan mode
		direct_fan_control(fan_index, fan_speed, true);
	}

	return 0;
}
EXPORT_SYMBOL(uw_set_fan);

u32 uw_set_fan_auto(void)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u8 mode_data;

	if (uw_feats->uniwill_has_universal_ec_fan_control) {
		u16 addr_use_custom_fan_table_0 = 0x07c5; // use different tables for both fans (0x0f00-0x0f2f and 0x0f30-0x0f5f respectivly)
		u16 addr_use_custom_fan_table_1 = 0x07c6; // enable 0x0fxx fantables
		u8 offset_use_custom_fan_table_0 = 7;
		u8 offset_use_custom_fan_table_1 = 2;
		u8 value_use_custom_fan_table_0;
		u8 value_use_custom_fan_table_1;
		uniwill_read_ec_ram(addr_use_custom_fan_table_1, &value_use_custom_fan_table_1);
		if ((value_use_custom_fan_table_1 >> offset_use_custom_fan_table_1) & 1) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_1, value_use_custom_fan_table_1 - (1 << offset_use_custom_fan_table_1), 3);
		}
		uniwill_read_ec_ram(addr_use_custom_fan_table_0, &value_use_custom_fan_table_0);
		if ((value_use_custom_fan_table_0 >> offset_use_custom_fan_table_0) & 1) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_0, value_use_custom_fan_table_0 - (1 << offset_use_custom_fan_table_0), 3);
		}
		fans_initialized = false;
	}
	else {
		cancel_delayed_work_sync(&direct_fan_control_restart_delayed_work);
		direct_fan_control_started = false;
		// Get current mode
		uniwill_read_ec_ram(0x0751, &mode_data);
		// Switch off "full fan mode" (i.e. unset 0x40 bit)
		uniwill_write_ec_ram(0x0751, mode_data & 0xbf);
	}

	direct_fan_control_current_value_fan0 = 0;
	direct_fan_control_current_value_fan1 = 0;

	return 0;
}
EXPORT_SYMBOL(uw_set_fan_auto);

static u8 uniwill_touchp_toggle_seq[] = {
	0xe0, 0x5b, // Super down
	0x1d,       // Control down
	0x76,       // Zenkaku/Hankaku down
	0xf6,       // Zenkaku/Hankaku up
	0x9d,       // Control up
	0xe0, 0xdb  // Super up
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
static bool uniwill_i8042_filter(unsigned char data, unsigned char str,
				 struct serio *port __always_unused)
#else
static bool uniwill_i8042_filter(unsigned char data, unsigned char str,
				 struct serio *port __always_unused,
				 void *context __always_unused)
#endif
{
	static u8 seq_pos;

	if (unlikely(str & I8042_STR_AUXDATA))
		return false;

	if (unlikely(data == uniwill_touchp_toggle_seq[seq_pos])) {
		++seq_pos;
		if (unlikely(data == 0x76 || data == 0xf6))
			return true;
		else if (unlikely(seq_pos == ARRAY_SIZE(uniwill_touchp_toggle_seq))) {
			schedule_work(&uniwill_key_event_work);
			seq_pos = 0;
		}
		return false;
	}

	seq_pos = 0;
	return false;
}

/*
 * Sysfs performance mode and power LED interfaces.
 * Allows user-space tools and the kernel platform profile subsystem to control
 * mode profiles (office, gaming, turbo, custom) and read-modify-write status LEDs.
 */



#if IS_REACHABLE(CONFIG_ACPI_PLATFORM_PROFILE)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
static struct device *uniwill_pprof_dev;
#else
static struct platform_profile_handler uniwill_pprof_handler;
#endif
#endif

static DEFINE_MUTEX(uniwill_mechrevo_ec_lock);

static int uniwill_checked_ec_write(u16 addr, u8 value)
{
	u8 readback;
	int ret = uniwill_write_ec_ram(addr, value);

	if (!ret)
		ret = uniwill_read_ec_ram(addr, &readback);
	if (!ret && readback != value)
		ret = -EIO;
	if (ret)
		pr_err("EC write/readback failed at 0x%04x (wanted 0x%02x): %d\n",
		       addr, value, ret);
	return ret;
}

static void uniwill_notify_platform_profile(void)
{
#if IS_REACHABLE(CONFIG_ACPI_PLATFORM_PROFILE)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	if (uniwill_pprof_dev)
		platform_profile_notify(uniwill_pprof_dev);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
	platform_profile_notify();
#endif
#endif
}

static int uniwill_get_perf_mode_info(u8 *out_mode, bool *is_custom)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0, cflag = 0;
	int ret;

	if (is_custom) {
		*is_custom = false;
		if (uniwill_read_ec_ram(0x0726, &cflag) == 0 && (cflag & BIT(7)))
			*is_custom = true;
	}

	ret = uniwill_read_ec_ram(regmap->reg_perf_mode, &val);
	if (ret)
		return ret;

	if (out_mode)
		*out_mode = val & 0xBF; /* Mask out bit 6 (FanBoost) */

	return 0;
}

static int uniwill_set_perf_mode_info(u8 base_mode, u8 led_mode, bool custom)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	const u16 addrs[] = { regmap->reg_perf_mode, 0x0726, 0x0727,
			      regmap->reg_power_led };
	u8 previous[ARRAY_SIZE(addrs)], values[ARRAY_SIZE(addrs)];
	int i, ret;

	mutex_lock(&uniwill_mechrevo_ec_lock);
	for (i = 0; i < ARRAY_SIZE(addrs); i++) {
		ret = uniwill_read_ec_ram(addrs[i], &previous[i]);
		if (ret)
			goto out;
	}
	values[0] = (previous[0] & BIT(6)) | (base_mode & ~BIT(6));
	values[1] = custom ? previous[1] | BIT(7) : previous[1] & ~BIT(7);
	values[2] = custom ? previous[2] | BIT(6) : previous[2] & ~BIT(6);
	values[3] = (previous[3] & ~0x03) | (led_mode & 0x03);
	for (i = 0; i < ARRAY_SIZE(addrs); i++) {
		ret = uniwill_checked_ec_write(addrs[i], values[i]);
		if (ret)
			goto restore;
	}
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	uniwill_notify_platform_profile();
	return 0;

restore:
	/* Best effort: failure is always returned, even when rollback succeeds. */
	for (; i >= 0; i--)
		uniwill_checked_ec_write(addrs[i], previous[i]);
out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret;
}

static ssize_t perf_mode_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	u8 mode = 0;
	bool custom = false;
	int ret = uniwill_get_perf_mode_info(&mode, &custom);

	if (ret)
		return ret;

	if (custom)
		return sysfs_emit(buf, "custom\n");

	switch (mode) {
	case 0xa0:
		return sysfs_emit(buf, "office\n");
	case 0x00:
		return sysfs_emit(buf, "gaming\n");
	case 0x10:
		return sysfs_emit(buf, "turbo\n");
	default:
		return sysfs_emit(buf, "unknown\n");
	}
}

static ssize_t perf_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 mode_val, led_mode;
	bool custom = false;
	char str[16];
	int ret;

	if (sscanf(buf, "%15s", str) != 1)
		return -EINVAL;

	if (sysfs_streq(str, "office")) {
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_OFFICE_BIT))
			return -EOPNOTSUPP;
		mode_val = 0xa0;
		led_mode = 0;
	} else if (sysfs_streq(str, "gaming") || sysfs_streq(str, "balanced")) {
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_GAMING_BIT))
			return -EOPNOTSUPP;
		mode_val = 0x00;
		led_mode = 1;
	} else if (sysfs_streq(str, "turbo")) {
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_TURBO_BIT))
			return -EOPNOTSUPP;
		mode_val = 0x10;
		led_mode = 2;
	} else if (sysfs_streq(str, "custom")) {
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_CUSTOM_BIT))
			return -EOPNOTSUPP;
		mode_val = 0x00;
		led_mode = 1;
		custom = true;
	} else {
		return -EINVAL;
	}

	ret = uniwill_set_perf_mode_info(mode_val, led_mode, custom);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(perf_mode);

static ssize_t power_led_mode_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0;
	int ret;

	ret = uniwill_read_ec_ram(regmap->reg_power_led, &val);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", val & 0x03);
}

static ssize_t power_led_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 mode, val = 0, readback = 0;
	int ret;

	if (kstrtou8(buf, 0, &mode) || mode > 3)
		return -EINVAL;

	mutex_lock(&uniwill_mechrevo_ec_lock);
	ret = uniwill_read_ec_ram(regmap->reg_power_led, &val);
	if (ret)
		goto out;

	val = (val & ~0x03) | (mode & 0x03);
	ret = uniwill_write_ec_ram(regmap->reg_power_led, val);
	if (ret)
		goto out;

	ret = uniwill_read_ec_ram(regmap->reg_power_led, &readback);
	if (ret)
		goto out;
	if ((readback & 0x03) != (mode & 0x03))
		ret = -EIO;

	out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(power_led_mode);

#if IS_REACHABLE(CONFIG_ACPI_PLATFORM_PROFILE)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
static int uniwill_pprof_probe(void *drvdata, unsigned long *choices)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	set_bit(PLATFORM_PROFILE_QUIET, choices);
	set_bit(PLATFORM_PROFILE_BALANCED, choices);
	if (regmap->supported_modes_mask & UNIWILL_MODE_TURBO_BIT)
		set_bit(PLATFORM_PROFILE_PERFORMANCE, choices);
	if (regmap->supported_modes_mask & UNIWILL_MODE_CUSTOM_BIT)
		set_bit(PLATFORM_PROFILE_CUSTOM, choices);
	return 0;
}

static int uniwill_pprof_get(struct device *dev, enum platform_profile_option *profile)
{
	u8 mode = 0;
	bool custom = false;
	int ret = uniwill_get_perf_mode_info(&mode, &custom);

	if (ret)
		return ret;

	if (custom) {
		*profile = PLATFORM_PROFILE_CUSTOM;
		return 0;
	}

	switch (mode) {
	case 0xa0:
		*profile = PLATFORM_PROFILE_QUIET;
		break;
	case 0x00:
		*profile = PLATFORM_PROFILE_BALANCED;
		break;
	case 0x10:
		*profile = PLATFORM_PROFILE_PERFORMANCE;
		break;
	default:
		*profile = PLATFORM_PROFILE_BALANCED;
		break;
	}
	return 0;
}

static int uniwill_pprof_set(struct device *dev, enum platform_profile_option profile)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 mode, led;
	bool custom = false;

	switch (profile) {
	case PLATFORM_PROFILE_QUIET:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_OFFICE_BIT))
			return -EOPNOTSUPP;
		mode = 0xa0;
		led = 0;
		break;
	case PLATFORM_PROFILE_BALANCED:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_GAMING_BIT))
			return -EOPNOTSUPP;
		mode = 0x00;
		led = 1;
		break;
	case PLATFORM_PROFILE_PERFORMANCE:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_TURBO_BIT))
			return -EOPNOTSUPP;
		mode = 0x10;
		led = 2;
		break;
	case PLATFORM_PROFILE_CUSTOM:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_CUSTOM_BIT))
			return -EOPNOTSUPP;
		mode = 0x00;
		led = 1;
		custom = true;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return uniwill_set_perf_mode_info(mode, led, custom);
}

static const struct platform_profile_ops uniwill_pprof_ops = {
	.probe = uniwill_pprof_probe,
	.profile_get = uniwill_pprof_get,
	.profile_set = uniwill_pprof_set,
};
#else
static int uniwill_pprof_get(struct platform_profile_handler *pprof,
			     enum platform_profile_option *profile)
{
	u8 mode = 0;
	bool custom = false;
	int ret = uniwill_get_perf_mode_info(&mode, &custom);

	if (ret)
		return ret;

	if (custom) {
		*profile = PLATFORM_PROFILE_CUSTOM;
		return 0;
	}

	switch (mode) {
	case 0xa0:
		*profile = PLATFORM_PROFILE_QUIET;
		break;
	case 0x00:
		*profile = PLATFORM_PROFILE_BALANCED;
		break;
	case 0x10:
		*profile = PLATFORM_PROFILE_PERFORMANCE;
		break;
	default:
		*profile = PLATFORM_PROFILE_BALANCED;
		break;
	}
	return 0;
}

static int uniwill_pprof_set(struct platform_profile_handler *pprof,
			     enum platform_profile_option profile)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 mode, led;
	bool custom = false;

	switch (profile) {
	case PLATFORM_PROFILE_QUIET:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_OFFICE_BIT))
			return -EOPNOTSUPP;
		mode = 0xa0;
		led = 0;
		break;
	case PLATFORM_PROFILE_BALANCED:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_GAMING_BIT))
			return -EOPNOTSUPP;
		mode = 0x00;
		led = 1;
		break;
	case PLATFORM_PROFILE_PERFORMANCE:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_TURBO_BIT))
			return -EOPNOTSUPP;
		mode = 0x10;
		led = 2;
		break;
	case PLATFORM_PROFILE_CUSTOM:
		if (!(regmap->supported_modes_mask & UNIWILL_MODE_CUSTOM_BIT))
			return -EOPNOTSUPP;
		mode = 0x00;
		led = 1;
		custom = true;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return uniwill_set_perf_mode_info(mode, led, custom);
}

static struct platform_profile_handler uniwill_pprof_handler = {
	.name = "uniwill-mechrevo",
	.profile_get = uniwill_pprof_get,
	.profile_set = uniwill_pprof_set,
};
#endif
#endif

static void uniwill_init_platform_profile(struct device *dev)
{
#if IS_REACHABLE(CONFIG_ACPI_PLATFORM_PROFILE)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	uniwill_pprof_dev = platform_profile_register(dev, "uniwill-mechrevo",
						      NULL, &uniwill_pprof_ops);
	if (IS_ERR(uniwill_pprof_dev)) {
		pr_debug("platform_profile_register not available: %ld\n",
			 PTR_ERR(uniwill_pprof_dev));
		uniwill_pprof_dev = NULL;
	}
#else
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	set_bit(PLATFORM_PROFILE_QUIET, uniwill_pprof_handler.choices);
	set_bit(PLATFORM_PROFILE_BALANCED, uniwill_pprof_handler.choices);
	if (regmap->supported_modes_mask & UNIWILL_MODE_TURBO_BIT)
		set_bit(PLATFORM_PROFILE_PERFORMANCE, uniwill_pprof_handler.choices);
	if (regmap->supported_modes_mask & UNIWILL_MODE_CUSTOM_BIT)
		set_bit(PLATFORM_PROFILE_CUSTOM, uniwill_pprof_handler.choices);
	uniwill_pprof_handler.dev = dev;
	if (platform_profile_register(&uniwill_pprof_handler))
		pr_debug("platform_profile_register failed or already registered\n");
#endif
#endif
}

static void uniwill_exit_platform_profile(struct device *dev)
{
#if IS_REACHABLE(CONFIG_ACPI_PLATFORM_PROFILE)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	if (uniwill_pprof_dev) {
		platform_profile_remove(uniwill_pprof_dev);
		uniwill_pprof_dev = NULL;
	}
#else
	platform_profile_remove(&uniwill_pprof_handler);
#endif
#endif
}

static struct attribute *uniwill_perf_attrs[] = {
	&dev_attr_perf_mode.attr,
	&dev_attr_power_led_mode.attr,
	NULL,
};

static const struct attribute_group uniwill_perf_attr_group = {
	.attrs = uniwill_perf_attrs,
};

/* CPU power limits (SPL / sPPT / fPPT) and vendor-specific EC 0x0786.
 * Power writes clamp to model limits; 0x0786 rejects out-of-range values.
 */

static int uniwill_write_power_limit(u16 addr, u8 value)
{
	u8 previous;
	int ret;

	mutex_lock(&uniwill_mechrevo_ec_lock);
	ret = uniwill_read_ec_ram(addr, &previous);
	if (ret)
		goto out;
	ret = uniwill_checked_ec_write(addr, value);
	if (ret && uniwill_checked_ec_write(addr, previous))
		pr_err("Could not restore EC power limit at 0x%04x\n", addr);
out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret;
}

static ssize_t tdp_spl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0;
	int ret = uniwill_read_ec_ram(regmap->reg_tdp_spl, &val);

	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", val);
}

static ssize_t tdp_spl_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	unsigned int val;
	int ret;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	val = clamp_val(val, regmap->tdp_spl_min, regmap->tdp_spl_max);
	ret = uniwill_write_power_limit(regmap->reg_tdp_spl, (u8)val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tdp_spl);

static ssize_t tdp_spl_min_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tdp_spl_min);
}
static DEVICE_ATTR_RO(tdp_spl_min);

static ssize_t tdp_spl_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tdp_spl_max);
}
static DEVICE_ATTR_RO(tdp_spl_max);

static ssize_t tdp_sppt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0;
	int ret = uniwill_read_ec_ram(regmap->reg_tdp_sppt, &val);

	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", val);
}

static ssize_t tdp_sppt_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	unsigned int val;
	int ret;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	val = clamp_val(val, regmap->tdp_sppt_min, regmap->tdp_sppt_max);
	ret = uniwill_write_power_limit(regmap->reg_tdp_sppt, (u8)val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tdp_sppt);

static ssize_t tdp_sppt_min_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tdp_sppt_min);
}
static DEVICE_ATTR_RO(tdp_sppt_min);

static ssize_t tdp_sppt_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tdp_sppt_max);
}
static DEVICE_ATTR_RO(tdp_sppt_max);

static ssize_t tdp_fppt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0;
	bool double_pl4 = false;
	int ret = uniwill_read_ec_ram(regmap->reg_tdp_fppt, &val);

	if (ret)
		return ret;

	has_double_pl4(&double_pl4);
	if (double_pl4)
		return sysfs_emit(buf, "%u\n", (unsigned int)val * 2);
	return sysfs_emit(buf, "%u\n", val);
}

static ssize_t tdp_fppt_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	unsigned int val;
	u8 write_val = 0;
	bool double_pl4 = false;
	int ret;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	val = clamp_val(val, regmap->tdp_fppt_min, regmap->tdp_fppt_max);
	has_double_pl4(&double_pl4);
	if (double_pl4)
		write_val = (u8)(val / 2);
	else
		write_val = (u8)val;

	ret = uniwill_write_power_limit(regmap->reg_tdp_fppt, write_val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tdp_fppt);

static ssize_t tdp_fppt_min_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tdp_fppt_min);
}
static DEVICE_ATTR_RO(tdp_fppt_min);

static ssize_t tdp_fppt_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tdp_fppt_max);
}
static DEVICE_ATTR_RO(tdp_fppt_max);

static ssize_t tcc_offset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0;
	int ret = uniwill_read_ec_ram(regmap->reg_tcc_offset, &val);

	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", val & 0x7f);
}

static ssize_t tcc_offset_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	unsigned int val;
	int ret;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (regmap->cpu_vendor != UNIWILL_CPU_AMD &&
	    regmap->cpu_vendor != UNIWILL_CPU_INTEL)
		return -EOPNOTSUPP;
	if (val < regmap->tcc_offset_min || val > regmap->tcc_offset_max)
		return -ERANGE;
	/* AMD: absolute degrees C; Intel: degrees below TjMax. */
	ret = uniwill_write_power_limit(regmap->reg_tcc_offset, (u8)val | 0x80);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tcc_offset);

static ssize_t tcc_offset_min_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tcc_offset_min);
}
static DEVICE_ATTR_RO(tcc_offset_min);

static ssize_t tcc_offset_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	return sysfs_emit(buf, "%d\n", regmap->tcc_offset_max);
}
static DEVICE_ATTR_RO(tcc_offset_max);

static struct attribute *uniwill_tdp_attrs[] = {
	&dev_attr_tdp_spl.attr,
	&dev_attr_tdp_spl_min.attr,
	&dev_attr_tdp_spl_max.attr,
	&dev_attr_tdp_sppt.attr,
	&dev_attr_tdp_sppt_min.attr,
	&dev_attr_tdp_sppt_max.attr,
	&dev_attr_tdp_fppt.attr,
	&dev_attr_tdp_fppt_min.attr,
	&dev_attr_tdp_fppt_max.attr,
	&dev_attr_tcc_offset.attr,
	&dev_attr_tcc_offset_min.attr,
	&dev_attr_tcc_offset_max.attr,
	NULL,
};

static umode_t uniwill_tdp_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr,
					   int n)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_tdp_control)
		return 0;
	return attr->mode;
}

static const struct attribute_group uniwill_tdp_attr_group = {
	.attrs = uniwill_tdp_attrs,
	.is_visible = uniwill_tdp_attr_is_visible,
};

/* 16-point EC fan curves. The kernel validates complete tables and verifies
 * EC writes; userspace remains responsible for choosing the temperature curve.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#define UW_BIN_ATTR_ARG const struct bin_attribute *
#else
#define UW_BIN_ATTR_ARG struct bin_attribute *
#endif

/* Each physical table is 48 (1p5) or 80 (2p0) bytes.  The logical
 * ABI exposes 16 {up, down, duty} triplets for each independently driven fan.
 * Slot 15 DownT aliases slot 14: the physical DownT[0] is reserved.
 */
static int uniwill_validate_fan_curve(const u8 *curve)
{
	int i, last = -1;
	bool padding = false, cooling = false;

	if (curve[0] != 0 || curve[1] != 0 || curve[43] != curve[46])
		return -EINVAL;
	for (i = 0; i < 16; i++) {
		u8 up = curve[3 * i], down = curve[3 * i + 1];
		u8 duty = curve[3 * i + 2];

		if (duty > 100)
			return -ERANGE;
		if (i == 0)
			continue;
		if (up == 0xff) {
			padding = true;
			if (duty != 100)
				return -EINVAL;
			continue;
		}
		if (padding || up > 110 || up <= last || down >= up ||
		    up - down < 3 || up - down > 15)
			return -EINVAL;
		cooling = up >= 85 && duty >= 80;
		last = up;
	}
	return cooling ? 0 : -EINVAL;
}

static int uniwill_fan_write_byte(u16 addr, u8 value)
{
	return uniwill_checked_ec_write(addr, value);
}

static ssize_t uniwill_fan_curve_read(u16 base,
		const struct uniwill_ec_regmap *regmap,
		char *buf, loff_t off, size_t count)
{
	u8 curve[48] = { 0 };
	u8 val;
	int i, ret;

	if (off < 0)
		return -EINVAL;
	if (off >= sizeof(curve))
		return 0;
	count = min_t(size_t, count, sizeof(curve) - off);
	mutex_lock(&uniwill_mechrevo_ec_lock);
	for (i = 0; i < 16; i++) {
		if (i) {
			ret = uniwill_read_ec_ram(base + i - 1, &curve[i * 3]);
			if (ret)
				goto out;
		}
		ret = uniwill_read_ec_ram(base + regmap->fan_down_temp_offset +
					 min(i + 1, 15), &curve[i * 3 + 1]);
		if (ret)
			goto out;
		ret = uniwill_read_ec_ram(base + regmap->fan_duty_offset + i, &val);
		if (ret)
			goto out;
		curve[i * 3 + 2] = regmap->fan_duty_scale ?
				val / regmap->fan_duty_scale : val;
	}
	memcpy(buf, curve + off, count);
	ret = count;
out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret;
}

static ssize_t uniwill_fan_curve_write(u16 base,
		const struct uniwill_ec_regmap *regmap,
		const char *buf, loff_t off, size_t count)
{
	u8 previous[80], en5, en6, value;
	int i, j, size, ret, restore_ret = 0;

	if (off != 0 || count != 48 || regmap->fan_points != 16 ||
	    (regmap->fan_table_layout != 1 && regmap->fan_table_layout != 2))
		return -EINVAL;
	ret = uniwill_validate_fan_curve(buf);
	if (ret)
		return ret;
	size = regmap->fan_table_layout == 2 ? 80 : 48;
	mutex_lock(&uniwill_mechrevo_ec_lock);
	ret = uniwill_read_ec_ram(0x07C5, &en5);
	if (ret)
		goto out;
	ret = uniwill_read_ec_ram(0x07C6, &en6);
	if (ret)
		goto out;
	for (i = 0; i < size; i++) {
		ret = uniwill_read_ec_ram(base + i, &previous[i]);
		if (ret)
			goto out;
	}
	/* Stop consuming the table while its bytes are being replaced. */
	ret = uniwill_checked_ec_write(0x07C5, en5 & ~BIT(7));
	if (ret)
		goto restore_flags;
	ret = uniwill_checked_ec_write(0x07C6, en6 & ~BIT(2));
	if (ret)
		goto restore_flags;
	/* FanTable2p0 also has two GPU temperature planes per fan. They
	 * are not represented by this CPU-temperature ABI and must remain
	 * untouched rather than being overwritten with CPU thresholds.
	 */
	for (i = 0; i < 48; i++) {
		j = i & 15;
		switch (i / 16) {
		case 0:
			value = j == 15 ? 0xff : buf[(j + 1) * 3];
			break;
		case 1:
			value = j == 0 ? 0 : buf[(j - 1) * 3 + 1];
			break;
		default:
			value = buf[j * 3 + 2] *
				(regmap->fan_duty_scale ? regmap->fan_duty_scale : 1);
		}
		ret = uniwill_fan_write_byte(base + i, value);
		if (ret)
			goto restore;
	}
	ret = uniwill_checked_ec_write(0x07C6, en6 | BIT(2));
	if (!ret)
		ret = uniwill_checked_ec_write(0x07C5, en5 | BIT(7));
	if (!ret) {
		mutex_unlock(&uniwill_mechrevo_ec_lock);
		return count;
	}
restore:
	/* Disable table consumption even if the enable write partly succeeded. */
	if (uniwill_checked_ec_write(0x07C5, en5 & ~BIT(7)))
		restore_ret = -EIO;
	if (uniwill_checked_ec_write(0x07C6, en6 & ~BIT(2)))
		restore_ret = -EIO;
	/* Never reactivate a partially restored table. */
	for (i = 0; i < size; i++)
		if (uniwill_checked_ec_write(base + i, previous[i]))
			restore_ret = -EIO;
	if (restore_ret)
		pr_err("EC fan table rollback failed at 0x%04x; table left disabled\n",
		       base);
restore_flags:
	if (restore_ret)
		goto out;
	if (uniwill_checked_ec_write(0x07C6, en6))
		pr_err("EC fan enable rollback failed at 0x07c6\n");
	if (uniwill_checked_ec_write(0x07C5, en5))
		pr_err("EC fan control rollback failed at 0x07c5\n");
out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret;
}

static ssize_t fan_curve1_read(struct file *filp, struct kobject *kobj,
			       UW_BIN_ATTR_ARG bin_attr,
			       char *buf, loff_t off, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_fan_table ||
	    regmap->fan_channels < 1 || !regmap->reg_fan_curve_cpu)
		return -EOPNOTSUPP;
	return uniwill_fan_curve_read(regmap->reg_fan_curve_cpu,
					regmap, buf, off, count);
}

static ssize_t fan_curve1_write(struct file *filp, struct kobject *kobj,
				UW_BIN_ATTR_ARG bin_attr,
				char *buf, loff_t off, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_fan_table ||
	    regmap->fan_channels < 1 || !regmap->reg_fan_curve_cpu)
		return -EOPNOTSUPP;
	return uniwill_fan_curve_write(regmap->reg_fan_curve_cpu,
					regmap, buf, off, count);
}

static struct bin_attribute bin_attr_fan_curve1 = {
	.attr = { .name = "fan_curve1", .mode = 0644 },
	.size = 48,
	.read = fan_curve1_read,
	.write = fan_curve1_write,
};

static ssize_t fan_curve2_read(struct file *filp, struct kobject *kobj,
			       UW_BIN_ATTR_ARG bin_attr,
			       char *buf, loff_t off, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_fan_table ||
	    regmap->fan_channels < 2 || !regmap->reg_fan_curve_gpu)
		return -EOPNOTSUPP;
	return uniwill_fan_curve_read(regmap->reg_fan_curve_gpu,
					regmap, buf, off, count);
}

static ssize_t fan_curve2_write(struct file *filp, struct kobject *kobj,
				UW_BIN_ATTR_ARG bin_attr,
				char *buf, loff_t off, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_fan_table ||
	    regmap->fan_channels < 2 || !regmap->reg_fan_curve_gpu)
		return -EOPNOTSUPP;
	return uniwill_fan_curve_write(regmap->reg_fan_curve_gpu,
					regmap, buf, off, count);
}

static struct bin_attribute bin_attr_fan_curve2 = {
	.attr = { .name = "fan_curve2", .mode = 0644 },
	.size = 48,
	.read = fan_curve2_read,
	.write = fan_curve2_write,
};

static ssize_t fan_curve3_read(struct file *filp, struct kobject *kobj,
			       UW_BIN_ATTR_ARG bin_attr,
			       char *buf, loff_t off, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_fan_table ||
	    regmap->fan_channels < 3 || !regmap->reg_fan_curve_mid)
		return -EOPNOTSUPP;
	return uniwill_fan_curve_read(regmap->reg_fan_curve_mid,
					regmap, buf, off, count);
}

static ssize_t fan_curve3_write(struct file *filp, struct kobject *kobj,
				UW_BIN_ATTR_ARG bin_attr,
				char *buf, loff_t off, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_fan_table ||
	    regmap->fan_channels < 3 || !regmap->reg_fan_curve_mid)
		return -EOPNOTSUPP;
	return uniwill_fan_curve_write(regmap->reg_fan_curve_mid,
					regmap, buf, off, count);
}

static struct bin_attribute bin_attr_fan_curve3 = {
	.attr = { .name = "fan_curve3", .mode = 0644 },
	.size = 48,
	.read = fan_curve3_read,
	.write = fan_curve3_write,
};

static ssize_t fan_boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0;
	int ret = uniwill_read_ec_ram(regmap->reg_perf_mode, &val);

	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", !!(val & BIT(6)));
}

static ssize_t fan_boost_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	bool enable;
	u8 val = 0, readback = 0;
	int ret;

	if (kstrtobool(buf, &enable))
		return -EINVAL;

	mutex_lock(&uniwill_mechrevo_ec_lock);
	ret = uniwill_read_ec_ram(regmap->reg_perf_mode, &val);
	if (ret)
		goto out;

	if (enable)
		val |= BIT(6);
	else
		val &= ~BIT(6);

	ret = uniwill_write_ec_ram(regmap->reg_perf_mode, val);
	if (ret)
		goto out;

	ret = uniwill_read_ec_ram(regmap->reg_perf_mode, &readback);
	if (ret)
		goto out;
	if (!!(readback & BIT(6)) != enable)
		ret = -EIO;

	out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(fan_boost);

static ssize_t over_boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 val = 0;
	int ret = uniwill_read_ec_ram(regmap->reg_power_led, &val);

	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", !!(val & BIT(4)));
}

static ssize_t over_boost_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	bool enable;
	u8 val = 0, readback = 0;
	int ret;

	if (kstrtobool(buf, &enable))
		return -EINVAL;

	mutex_lock(&uniwill_mechrevo_ec_lock);
	ret = uniwill_read_ec_ram(regmap->reg_power_led, &val);
	if (ret)
		goto out;

	if (enable)
		val |= BIT(4);
	else
		val &= ~BIT(4);

	ret = uniwill_write_ec_ram(regmap->reg_power_led, val);
	if (ret)
		goto out;

	ret = uniwill_read_ec_ram(regmap->reg_power_led, &readback);
	if (ret)
		goto out;
	if (!!(readback & BIT(4)) != enable)
		ret = -EIO;

	out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(over_boost);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static const struct bin_attribute *const uniwill_fan_bin_attrs[] = {
	&bin_attr_fan_curve1,
	&bin_attr_fan_curve2,
	&bin_attr_fan_curve3,
	NULL,
};
#else
static struct bin_attribute *uniwill_fan_bin_attrs[] = {
	&bin_attr_fan_curve1,
	&bin_attr_fan_curve2,
	&bin_attr_fan_curve3,
	NULL,
};
#endif

static struct attribute *uniwill_fan_attrs[] = {
	&dev_attr_fan_boost.attr,
	&dev_attr_over_boost.attr,
	NULL,
};

static umode_t uniwill_fan_bin_attr_is_visible(struct kobject *kobj,
					       UW_BIN_ATTR_ARG attr,
					       int n)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_fan_table)
		return 0;
	if (attr == &bin_attr_fan_curve3 && regmap->fan_channels < 3)
		return 0;
	return attr->attr.mode;
}

static const struct attribute_group uniwill_fan_attr_group = {
	.attrs = uniwill_fan_attrs,
	.bin_attrs = uniwill_fan_bin_attrs,
	.is_bin_visible = uniwill_fan_bin_attr_is_visible,
};

/*
 * OEMG(0x0300, 1) requests firmware GPU eject and requires PEGP._PSC == 3
 * before removing power.  A plain sysfs write cannot establish that Linux
 * ACPI hotplug, DRM and all GPU functions have completed the eject handshake.
 * Reject live power cuts until a verified hotplug orchestration is available.
 */

static ssize_t dgpu_power_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	u8 status = 0;
	int ret;

	/* Query power status: cmd=2, subsystem=0x0300 */
	ret = uniwill_wmi_oemg(2, 0x0300, &status);
	if (ret)
		return ret;

	/* 0x55 indicates power cut / isolated, 0xAA indicates power active */
	if (status == 0x55)
		return sysfs_emit(buf, "0\n");
	else if (status == 0xaa)
		return sysfs_emit(buf, "1\n");

	return sysfs_emit(buf, "unknown (0x%02x)\n", status);
}
static ssize_t dgpu_power_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u8 state;
	bool on;
	int ret;

	if (!regmap || !regmap->has_dgpu_power_cut)
		return -EOPNOTSUPP;
	ret = kstrtobool(buf, &on);
	if (ret)
		return ret;
	mutex_lock(&uniwill_mechrevo_ec_lock);
	ret = uniwill_wmi_oemg(2, 0x0300, &state);
	if (ret)
		goto out;
	if (state != 0x55 && state != 0xaa) {
		ret = -EIO;
		goto out;
	}
	if (!on && state == 0xaa) {
		ret = -EOPNOTSUPP; /* _PSC eject handshake not proven safe. */
		goto out;
	}
	if (on && state == 0x55) {
		ret = uniwill_wmi_oemg(0, 0x0300, &state);
		if (ret)
			goto out;
		ret = uniwill_wmi_oemg(2, 0x0300, &state);
		if (ret || state != 0xaa) {
			ret = -EIO;
			goto out;
		}
	}
	ret = count;
out:
	mutex_unlock(&uniwill_mechrevo_ec_lock);
	return ret;
}
static DEVICE_ATTR_RW(dgpu_power);

static ssize_t mux_scheme_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_mux)
		return sysfs_emit(buf, "none\n");

	switch (regmap->mux_scheme) {
	case UNIWILL_MUX_AMD:
		return sysfs_emit(buf, "amd\n");
	case UNIWILL_MUX_INTEL:
		return sysfs_emit(buf, "intel\n");
	default:
		return sysfs_emit(buf, "none\n");
	}
}
static DEVICE_ATTR_RO(mux_scheme);

static struct attribute *uniwill_dgpu_attrs[] = {
	&dev_attr_dgpu_power.attr,
	&dev_attr_mux_scheme.attr,
	NULL,
};

static umode_t uniwill_dgpu_attr_is_visible(struct kobject *kobj,
					    struct attribute *attr,
					    int n)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	if (!regmap || !regmap->has_dgpu)
		return 0;

	if (attr == &dev_attr_dgpu_power.attr && !regmap->has_dgpu_power_cut)
		return 0;

	if (attr == &dev_attr_mux_scheme.attr && !regmap->has_mux)
		return 0;

	return attr->mode;
}

static const struct attribute_group uniwill_dgpu_attr_group = {
	.attrs = uniwill_dgpu_attrs,
	.is_visible = uniwill_dgpu_attr_is_visible,
};

static int uniwill_keyboard_probe(struct platform_device *dev)
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();
	u32 i;
	u8 data;
	int status;
	struct uniwill_device_features_t *uw_feats;

	set_rom_id();

	uw_feats = uniwill_get_device_features();

	/* Never overwrite an unknown or unreadable firmware mode on probe. */

	if (uw_feats->uniwill_profile_v1 && (!regmap || !regmap->has_fan_table)) {
		// Set manual-mode fan-curve in 0x0743 - 0x0747
		// Some kind of default fan-curve is stored in 0x0786 - 0x078a: Using it to initialize manual-mode fan-curve
		for (i = 0; i < 5; ++i) {
			status = uniwill_read_ec_ram(0x0786 + i, &data);
			if (status)
				return status;
			status = uniwill_write_ec_ram(0x0743 + i, data);
			if (status)
				return status;
		}
	}

	// Make sure custom TDP/custom fan curve mode is set. Using the
	// custom profile mode flag to ID this set of devices.
	uniwill_set_custom_profile_mode(true);

	// Enable manual mode for legacy platforms only
	if (!regmap || !regmap->has_fan_table)
		uniwill_write_ec_ram(0x0741, 0x01);

	// Zero second fan temp for detection
	uniwill_write_ec_ram(0x044f, 0x00);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0)
	TUXEDO_ERROR("Warning: Kernel version less that 5.9, keyboard backlight might not be properly recognized.");
#endif
	uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
	uniwill_kbd_bl_enable_state_on_start = (data >> 1) & 0x01;
	uniwill_leds_init(dev);
	uniwill_write_kbd_bl_enable(1);

	status = uw_lightbar_init(dev);
	uw_lightbar_loaded = (status >= 0);

	uw_charging_priority_init(dev);
	uw_charging_profile_init(dev);
	uw_ac_auto_boot_init(dev);
	uw_usb_powershare_init(dev);
	uw_mini_led_local_dimming_init(dev);
	uw_show_hidden_bios_options();
	uw_battery_init();

	status = sysfs_create_group(&dev->dev.kobj, &uniwill_perf_attr_group);
	if (status)
		pr_warn("Failed to create uniwill perf sysfs group: %d\n", status);
	status = sysfs_create_group(&dev->dev.kobj, &uniwill_tdp_attr_group);
	if (status)
		pr_warn("Failed to create uniwill tdp sysfs group: %d\n", status);
	status = sysfs_create_group(&dev->dev.kobj, &uniwill_fan_attr_group);
	if (status)
		pr_warn("Failed to create uniwill fan sysfs group: %d\n", status);
	status = sysfs_create_group(&dev->dev.kobj, &uniwill_dgpu_attr_group);
	if (status)
		pr_warn("Failed to create uniwill dgpu sysfs group: %d\n", status);
	uniwill_init_platform_profile(&dev->dev);

	// Ignore return value, it just means there is already a filter active
	// which is fine, because it is probably just the upstream patch of this
	// filter.
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
	if (i8042_install_filter(uniwill_i8042_filter))
#else
	if (i8042_install_filter(uniwill_i8042_filter, NULL))
#endif
		pr_info("Could not install i8042 filter.\n");

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int uniwill_keyboard_remove(struct platform_device *dev)
#else
static void uniwill_keyboard_remove(struct platform_device *dev)
#endif
{
	const struct uniwill_ec_regmap *regmap = uniwill_get_active_regmap();

	uniwill_exit_platform_profile(&dev->dev);
	sysfs_remove_group(&dev->dev.kobj, &uniwill_dgpu_attr_group);
	sysfs_remove_group(&dev->dev.kobj, &uniwill_fan_attr_group);
	sysfs_remove_group(&dev->dev.kobj, &uniwill_tdp_attr_group);
	sysfs_remove_group(&dev->dev.kobj, &uniwill_perf_attr_group);

	if (uw_charging_prio_loaded)
		sysfs_remove_group(&dev->dev.kobj, &uw_charging_prio_attr_group);

	if (uw_charging_profile_loaded)
		sysfs_remove_group(&dev->dev.kobj, &uw_charging_profile_attr_group);

	uw_battery_uninit();

	uniwill_leds_remove(dev);

	// Restore previous backlight enable state
	if (uniwill_kbd_bl_enable_state_on_start != 0xff) {
		uniwill_write_kbd_bl_enable(uniwill_kbd_bl_enable_state_on_start);
	}

	if (uw_lightbar_loaded)
		uw_lightbar_remove(dev);

	// Disable manual mode for legacy platforms only
	if (!regmap || !regmap->has_fan_table)
		uniwill_write_ec_ram(0x0741, 0x00);

	// Ignore return value, it just means this filter was not active atm.
	if (i8042_remove_filter(uniwill_i8042_filter))
		pr_info("Could not remove i8042 filter.\n");

	cancel_delayed_work_sync(&direct_fan_control_restart_delayed_work);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static int uniwill_keyboard_suspend(struct platform_device *dev, pm_message_t state)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u8 data;
	if (uw_feats->uniwill_custom_profile_mode_needed) {
		// Unset "customer mode light" before suspend. Otherwise at
		// least one device is known to immediately wake up.
		uniwill_read_ec_ram(0x0727, &data);
		data &= ~(1 << 6);
		uniwill_write_ec_ram(0x0727, data);
	}
	uniwill_write_kbd_bl_enable(0);
	if (direct_fan_control_started) {
		direct_fan_control_suspend = true;
		direct_fan_control_current_value_fan0_suspend_save = direct_fan_control_current_value_fan0;
		direct_fan_control_current_value_fan1_suspend_save = direct_fan_control_current_value_fan1;
		uw_set_fan_auto();
	}
	return 0;
}

static int uniwill_keyboard_resume(struct platform_device *dev)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u8 data;

	if (direct_fan_control_suspend) {
		direct_fan_control_suspend = false;
		uw_set_fan(0, direct_fan_control_current_value_fan0_suspend_save);
		uw_set_fan(1, direct_fan_control_current_value_fan1_suspend_save);
	}

	if (uw_feats->uniwill_custom_profile_mode_needed) {
		// Re-set "customer mode light" on resume
		uniwill_read_ec_ram(0x0727, &data);
		data |= (1 << 6);
		uniwill_write_ec_ram(0x0727, data);
	}
	uniwill_leds_restore_state_extern();
	uniwill_write_kbd_bl_enable(1);
	// Restore charging settings on resume
	uw_charging_priority_write_state();
	uw_charging_profile_write_state();
	return 0;
}

static struct platform_driver platform_driver_uniwill = {
	.remove = uniwill_keyboard_remove,
	.suspend = uniwill_keyboard_suspend,
	.resume = uniwill_keyboard_resume,
	.driver =
		{
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
		},
};

struct tuxedo_keyboard_driver uniwill_keyboard_driver = {
	.platform_driver = &platform_driver_uniwill,
	.probe = uniwill_keyboard_probe,
	.key_map = uniwill_wmi_keymap,
	.fn_lock_available = uniwill_fn_lock_available,
	.fn_lock_show = uniwill_fn_lock_show,
	.fn_lock_store = uniwill_fn_lock_store,
};

#endif // UNIWILL_KEYBOARD_H

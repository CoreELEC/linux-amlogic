/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/kernel.h>

#include "hid-ids.h"

static int firetv_raw_event(struct hid_device *hid, struct hid_report *report,
			   u8 *data, int size)
{
	static bool caps_set = false;
	struct hid_input *hidinput;
	struct input_dev *input = NULL;
	int i;

	const struct {
		u8 code;
		u16 key;
		u32 scan;
	} app_keys[] = {
		{ 0xa1, KEY_A,  0xc0077 }, 	// App1 - next subtitle 
		{ 0xa2, KEY_L,  0xc0078 }, 	// App2 - A/V sync overlay
		{ 0xa3, KEY_O,  0xc0079 }, 	// App3 - video process info
		{ 0xa4, KEY_I,  0xc007a }, 	// App4 - info
	};

	if (size < 2 || data[0] != 0xef)
		return 0;

	hidinput = list_first_entry_or_null(&hid->inputs, struct hid_input, list);
	if (!hidinput || !hidinput->input) {
		pr_warn("firetv: no input device bound!\n");
		return 0;
	}
	input = hidinput->input;

	if (!caps_set) {
		set_bit(EV_MSC, input->evbit);
		set_bit(MSC_SCAN, input->mscbit);
		set_bit(EV_KEY, input->evbit);

		for (i = 0; i < ARRAY_SIZE(app_keys); i++)
			set_bit(app_keys[i].key, input->keybit);

		caps_set = true;
		pr_info("firetv: registered app button keys\n");
	}

	for (i = 0; i < ARRAY_SIZE(app_keys); i++) {
		if (data[1] == app_keys[i].code) {
			input_event(input, EV_MSC, MSC_SCAN, app_keys[i].scan);
			input_event(input, EV_KEY, app_keys[i].key, 1);
			input_sync(input);
			input_event(input, EV_KEY, app_keys[i].key, 0);
			input_sync(input);
			break;
		}
	}

	return 0;
}

static int firetv_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		dev_err(&hdev->dev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		dev_err(&hdev->dev, "hw start failed\n");
		return ret;
	}

	dev_info(&hdev->dev, "Amazon Fire TV remote initialized\n");
	return 0;
}

static void firetv_remove(struct hid_device *hdev)
{
	hid_hw_stop(hdev);
}

static const struct hid_device_id firetv_devices[] = {
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_AR, HID_ANY_ID), .driver_data = 0 },
	{ }
};
MODULE_DEVICE_TABLE(hid, firetv_devices);
MODULE_ALIAS("hid:b0005g*v00000171p000004??");


static struct hid_driver firetv_driver = {
	.name = "hid-firetv",
	.id_table = firetv_devices,
	.raw_event = firetv_raw_event,
	.probe = firetv_probe,
	.remove = firetv_remove,
};
module_hid_driver(firetv_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Add support for Amazon Fire TV Remote app buttons");

/*
 *  Multitouch HID driver for NextWindow Riemann (standalone) Touchscreen
 *
 *  Copyright (c) 2008-2010 Rafi Rubin
 *  Copyright (c) 2009-2010 Stephane Chatty
 *  Copyright (c) 2010 Canonical, Ltd.
 *	Copyright (C) 2009-2011 Daniel Newton (djpnewton@gmail.com)
 *	Copyright (C) 2001-2004 Oliver Thane (othane@nextwindow.com)
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>

//#include "hid-ids.h"	/* our vid is in here but it is hard to link to atm so just hard code for now */

#define TIPSWITCH_BIT 	(1<<0)
#define IN_RANGE_BIT	(1<<1)
#define CONFIDENCE_BIT	(1<<2)

#define info(...) printk(__VA_ARGS__)
#define debug(...) printk(__VA_ARGS__)
#define trace(...) printk(__VA_ARGS__)

struct riemann_data {
	__u8	status;
	__u8	contact_id;
	__u16	x,y;
	__u16	w,h;
};

static int riemann_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct riemann_data *hd = hid_get_drvdata(hdev);
	struct input_dev *input = hi->input;
	trace("%s() - usage:0x%.8X\n", __func__, usage->hid);
#if 1

	/* ignore the digitizer for now */
	if (field->logical != HID_DG_FINGER)
		return -1;

	switch (usage->hid & HID_USAGE_PAGE) {

		case HID_UP_GENDESK:
			switch (usage->hid) {
				case HID_GD_X:
					debug("%s() - x min:%d; x max:%d\n", __func__, field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_X);
					input_set_abs_params(input, ABS_X, field->logical_minimum, field->logical_maximum, 0, 0);
					input_set_abs_params(input, ABS_MT_POSITION_X, field->logical_minimum, field->logical_maximum, 0, 0);
					return 1;
				case HID_GD_Y:
					debug("%s() - y min:%d; y max:%d\n", __func__, field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_Y);
					input_set_abs_params(input, ABS_Y, field->logical_minimum, field->logical_maximum, 0, 0);
					input_set_abs_params(input, ABS_MT_POSITION_Y, field->logical_minimum, field->logical_maximum, 0, 0);
					return 1;
			}
			return 0;

		case HID_UP_DIGITIZER:
			switch (usage->hid) {
				case HID_DG_INRANGE:
				case HID_DG_CONFIDENCE:
				case HID_DG_INPUTMODE:
				case HID_DG_DEVICEINDEX:
				case HID_DG_CONTACTCOUNT:
				case HID_DG_CONTACTMAX:
				case HID_DG_TIPPRESSURE:
				case HID_DG_WIDTH:
				case HID_DG_HEIGHT:
					return -1;
				case HID_DG_TIPSWITCH:
					/* touchscreen emulation */
					debug("%s() - mapping TIPSWITCH to BTN_TOUCH\n", __func__);
					hid_map_usage(hi, usage, bit, max, EV_KEY, BTN_TOUCH);
					input_set_capability(input, EV_KEY, BTN_TOUCH);
					return 1;
				case HID_DG_CONTACTID:
					debug("%s() - mapping CONTACT_ID to ABS_MT_TRACKING_ID\n", __func__);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_TRACKING_ID);
					return 1;
			}
			return 0;

		case 0xff000000:
			/* ignore vendor-specific features */
			return -1;
	}
#endif
	/* pass the rest through */
	return 0;
}

static int riemann_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	trace("%s() - usage:0x%.8X\n", __func__, usage->hid);

	/* this usage has been mapped just keep processing all as though this cb was not hooked up */
	/**@todo I dont really know what I am supposed to do here */
	if (usage->type == EV_KEY || usage->type == EV_ABS)
		clear_bit(usage->code, *bit);
	return 0;
}

/*
 * this function is called by riemann_event when
 * a touch is ready to be sent to the input sub
 * system
 */
static void report_touch(struct riemann_data *hd, struct input_dev *input)
{
	trace("%s()\n", __func__);

	/* filter junk */
	if ((hd->x < 0) || (hd->x > 32767) ||
		(hd->y < 0) || (hd->y > 32767) ||
		(hd->contact_id < 0) || (hd->contact_id > 1))
	{
		debug("%s() - junk report detected\n", __func__);
		return;
	}

	/* multitouch */
	if (hd->status & (TIPSWITCH_BIT | IN_RANGE_BIT | CONFIDENCE_BIT)) {	
		debug("%s() - sending multitouch event to input\n", __func__);
		input_event(input, EV_ABS, ABS_MT_TRACKING_ID, hd->contact_id);
		input_event(input, EV_ABS, ABS_MT_POSITION_X, hd->x);
		input_event(input, EV_ABS, ABS_MT_POSITION_Y, hd->y);
	}
	input_mt_sync(input);
#if 0
	
	/* mouse (only for the first touch point) */
	/**@todo can we use contact_id for this ? */
	if (hd->contact_id == 0) {
		if (hd->status & (TIPSWITCH_BIT | IN_RANGE_BIT | CONFIDENCE_BIT)) {	
			debug("%s() - sending mouse event to input\n", __func__);
			input_event(input, EV_KEY, BTN_TOUCH, 1);
			input_event(input, EV_ABS, ABS_X, hd->x);
			input_event(input, EV_ABS, ABS_Y, hd->y);
		}
		input_sync(input);
	}
#endif
}

/*
 * this function is called upon all reports
 * so that we can filter contact point information,
 * decide whether we are in multi or single touch mode
 * and call input_mt_sync after each point if necessary
 */
static int riemann_event (struct hid_device *hid, struct hid_field *field,
		                        struct hid_usage *usage, __s32 value)
{
	struct input_dev *input = field->hidinput->input;
	struct riemann_data *hd = hid_get_drvdata(hid);
	
	debug("%s() - usage:0x%.8X\n", __func__, usage->hid);
	debug("%s() - reportid:%d\n", __func__, field->report->id);
	debug("%s() - application:0x%.8X\n", __func__, field->application);
	debug("%s() - logical usage: 0x%.8X\n", __func__, field->logical);

	/* ensure this is a touchscreen collection (all are for existing nw devices) */
	if (field->application != HID_DG_TOUCHSCREEN)
		return 0;	/* hid sub system please handle these non finger related stuff */

	/* process finger reports & only if input subsystem connected */
	if ((field->logical == HID_DG_FINGER) &&
		(hid->claimed & HID_CLAIMED_INPUT)) {
		/* interpret report */
		/**@todo do I need to endian correct the values or has hid-core already done it for me ? */
		switch (usage->hid) {
			/* multitouch (finger) report */
			case HID_DG_TIPSWITCH:
				info("%s() - TIPSWITCH:0x%.4X\n", __func__, value);
				hd->status &= ~TIPSWITCH_BIT;
				if (value)
					hd->status |= TIPSWITCH_BIT;
				break;
			case HID_DG_INRANGE:
				info("%s() - INRANGE:0x%.4X\n", __func__, value);
				hd->status &= ~IN_RANGE_BIT;
				if (value)
					hd->status |= IN_RANGE_BIT;
				break;
			case HID_DG_CONFIDENCE:
				info("%s() - CONFIDENCE:0x%.4X\n", __func__, value);
				hd->status &= ~CONFIDENCE_BIT;
				if (value)
					hd->status |= CONFIDENCE_BIT;
				break;
			case HID_DG_CONTACTID:
				info("%s() - CONTACT ID:0x%.4X\n", __func__, value);
				hd->contact_id = value;
				break;
			case HID_GD_X:
				info("%s() - X:0x%.4X\n", __func__, value);
				hd->x = value;
				break;
			case HID_GD_Y:
				info("%s() - Y:0x%.4X\n", __func__, value);
				hd->y = value;
				break;
			case HID_DG_WIDTH:
				info("%s() - W:0x%.4X\n", __func__, value);
				hd->w = value;
				break;
			case HID_DG_HEIGHT:
				info("%s() - H:0x%.4X\n", __func__, value);
				hd->h = value;
				/* last item in riemann's touch report so trigger an update */
				//report_touch(hd, input);
				break;
		}
	}

	/**@todo I dont get what below does (I think it passes the report on
	 * but to who and why I dont get */
	/* we have handled the hidinput part, now remains hiddev */
	if ((hid->claimed & HID_CLAIMED_HIDDEV) && hid->hiddev_hid_event)
		hid->hiddev_hid_event(hid, field, usage, value);

	return 1;	/* we handled it */
}

static int riemann_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct riemann_data *hd;

	trace("%s()\n", __func__); 

	hd = kzalloc(sizeof(struct riemann_data), GFP_KERNEL);
	if (!hd) {
		dev_err(&hdev->dev, "cannot allocate NW riemann data\n");
		return -ENOMEM;
	}

	hid_set_drvdata(hdev, hd);

	ret = hid_parse(hdev);
	if (ret) {
		dev_err(&hdev->dev, "parse failed\n");
		goto error_free;
	}

	/**@todo we must call this, but what is connect_mask about ? */
	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (ret) {
		dev_err(&hdev->dev, "hw start failed\n");
		goto error_free;
	}

	return 0;

error_free:
	kfree(hd);
	return ret;
}

static void riemann_remove(struct hid_device *hdev)
{
	trace("%s()\n", __func__); 
	hid_hw_stop(hdev);
	kfree(hid_get_drvdata(hdev));
	hid_set_drvdata(hdev, NULL);
}

static const struct hid_device_id riemann_devices[] = {
	{HID_USB_DEVICE(0x1926, 0x0008)},
	{ }
};
MODULE_DEVICE_TABLE(hid, riemann_devices);

/* if reports with these usages show up send them to event handler */
static const struct hid_usage_id riemann_grabbed_usages[] = {
	/**@todo add our multitouch usage to this list only */
	{ HID_ANY_ID, HID_ANY_ID, HID_ANY_ID },
	{ HID_ANY_ID - 1, HID_ANY_ID - 1, HID_ANY_ID - 1 }
};

static struct hid_driver riemann_driver = {
	.name = "riemann",
	.id_table = riemann_devices,
	.probe = riemann_probe,
	.remove = riemann_remove,
	.input_mapping = riemann_input_mapping,
	.input_mapped = riemann_input_mapped,
	.usage_table = riemann_grabbed_usages,
	.event = riemann_event,
};

struct hid_dynid {
	struct list_head list;
	struct hid_device_id id;
};

static int __init riemann_init(void)
{
	trace("%s()\n", __func__);
	return hid_register_driver(&riemann_driver);
}

static void __exit riemann_exit(void)
{
	hid_unregister_driver(&riemann_driver);
	trace("%s()\n", __func__);
}

module_init(riemann_init);
module_exit(riemann_exit);
MODULE_LICENSE("GPL");

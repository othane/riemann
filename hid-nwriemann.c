/*
 *  Multitouch HID driver for NextWindow Riemann (standalone) Touchscreen
 *
 *	This code is mostly based on hid-multitouch.c but using older api's to make it more portable:
 *	Note _only_ use this driver if your version of hid-multitouch does not support your Nextwindow panel
 *
 *  Copyright (c) 2008-2010 Rafi Rubin
 *  Copyright (c) 2009-2010 Stephane Chatty
 *  Copyright (c) 2010 Canonical, Ltd.
 *  Copyright (C) 2009-2011 Daniel Newton (djpnewton@gmail.com)
 *  Copyright (C) 2011 Oliver Thane (othane@nextwindow.com)
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

MODULE_AUTHOR("Oliver Thane <othane@gmail.com>");
MODULE_DESCRIPTION("Nextwindow multitouch panels");
MODULE_LICENSE("GPL");

#define TIPSWITCH_BIT 	(1<<0)
#define IN_RANGE_BIT	(1<<1)
#define CONFIDENCE_BIT	(1<<2)

#ifndef MAX_TOUCHES
#define MAX_TOUCHES 5
#endif

#ifdef DEBUG
#define info(...) printk("INFO:"__VA_ARGS__)
#define debug(...) printk("DBG:"__VA_ARGS__)
#define trace(...) printk("TRACE:"__VA_ARGS__)
#else
#define info(...)
#define debug(...)
#define trace(...)
#endif
#define error(...) printk(__VA_ARGS__)

struct riemann_data {
	__u8	touch_index;
	struct {
		__u8	status;
		__s32	contact_id;
		__s32	x,y;
		__s32	w,h;
	} touch[MAX_TOUCHES];
	__u8	contact_count;
};

static int riemann_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	trace("%s() - usage:0x%.8X\n", __func__, usage->hid);

	/* this driver should only operate on touchscreen reports, leave the rest to hid */
	if (field->application != HID_DG_TOUCHSCREEN)
		return -1;

	switch (usage->hid & HID_USAGE_PAGE) {

		case HID_UP_GENDESK:
			switch (usage->hid) {
				case HID_GD_X:
					info("%s() - x min:%d; x max:%d\n", __func__,
						field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_X);
					input_set_abs_params(hi->input, ABS_X,
								field->logical_minimum,
								field->logical_maximum, 0, 0);
					input_set_abs_params(hi->input, ABS_MT_POSITION_X,
								field->logical_minimum,
								field->logical_maximum, 0, 0);
					return 1;
				case HID_GD_Y:
					info("%s() - y min:%d; y max:%d\n", __func__,
						field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_Y);
					input_set_abs_params(hi->input, ABS_Y,
								field->logical_minimum,
								field->logical_maximum, 0, 0);
					input_set_abs_params(hi->input, ABS_MT_POSITION_Y,
								field->logical_minimum,
								field->logical_maximum, 0, 0);
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
					return 1;

				case HID_DG_WIDTH:
					info("%s() - mapping WIDTH to ABS_MT_WIDTH_MAJOR %d, %d\n", __func__, field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_WIDTH_MAJOR);
					input_set_capability(hi->input, EV_KEY, ABS_MT_WIDTH_MAJOR);
					input_set_abs_params(hi->input, ABS_MT_WIDTH_MAJOR, field->logical_minimum, field->logical_maximum, 0, 0);
					return 1;

				case HID_DG_HEIGHT:
					info("%s() - mapping HEIGHT to ABS_MT_WIDTH_MINOR %d, %d\n", __func__, field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_WIDTH_MINOR);
					input_set_capability(hi->input, EV_KEY, ABS_MT_WIDTH_MINOR);
					input_set_abs_params(hi->input, ABS_MT_WIDTH_MINOR, field->logical_minimum, field->logical_maximum, 0, 0);
					return 1;

				case HID_DG_TIPSWITCH:
					info("%s() - mapping TIPSWITCH to ABS_MT_TOUCH_MAJOR %d, %d\n", __func__, field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_KEY, ABS_MT_TOUCH_MAJOR);
					input_set_capability(hi->input, EV_KEY, ABS_MT_TOUCH_MAJOR);
					input_set_abs_params(hi->input, ABS_MT_TOUCH_MAJOR, field->logical_minimum, field->logical_maximum, 0, 0);
					hid_map_usage(hi, usage, bit, max, EV_KEY, BTN_TOUCH);
					input_set_capability(hi->input, EV_KEY, BTN_TOUCH);
					return 1;

				case HID_DG_CONTACTID:
					info("%s() - mapping CONTACT_ID to ABS_MT_TRACKING_ID %d, %d\n", __func__, field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_TRACKING_ID);
					input_set_capability(hi->input, EV_KEY, ABS_MT_TRACKING_ID);
					input_set_abs_params(hi->input, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
					return 1;
			}
			return 0;

		case 0xff000000:
			/* ignore vendor-specific features */
			return -1;
	}
	
	/* pass the rest through */
	return 0;
}

static int riemann_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	trace("%s() - usage:0x%.8X\n", __func__, usage->hid);

	if (usage->type == EV_KEY || usage->type == EV_ABS)
		set_bit(usage->type, hi->input->evbit);

	return 0;
}

/*
 * this function is called by riemann_event when touches are ready to be sent 
 * to the input sub-system
 */
static void report_touch(struct riemann_data *rd, struct input_dev *input)
{
	unsigned int k;
	bool touched = false;
	trace("%s()\n", __func__);

	if (rd->touch_index > MAX_TOUCHES) {
		error("%s() - invalid report\n", __func__);
		return;
	}

	for (k=0; k < rd->touch_index; k++) {
		/* multitouch */
		int x = rd->touch[k].x; 
		int y = rd->touch[k].y;
		/* fix ups for some older fw that reported w.h as 0 */
		int w = (rd->touch[k].w == 0)? 1: rd->touch[k].w;
		int h = (rd->touch[k].h == 0)? 1: rd->touch[k].h;
		/* divided by two to match visual scale of touch */
		int	major = max(w, h) >> 1;
		int minor = min(w, h) >> 1;
	
		/* send touch info to input */
		if (rd->touch[k].status & (TIPSWITCH_BIT | IN_RANGE_BIT | CONFIDENCE_BIT)) {
			input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, rd->touch[k].status);
			input_event(input, EV_ABS, ABS_MT_TRACKING_ID, rd->touch[k].contact_id);
			input_event(input, EV_ABS, ABS_MT_POSITION_X, x);
			input_event(input, EV_ABS, ABS_MT_POSITION_Y, y);
			input_event(input, EV_ABS, ABS_MT_WIDTH_MAJOR, major);
			input_event(input, EV_ABS, ABS_MT_WIDTH_MINOR, minor);
			input_mt_sync(input);
			touched = true;
		}
	}
	
	/* send empty sync report when no touches http://source.android.com/tech/input/touch-devices.html */ 
	if (!touched)
		input_mt_sync(input);

	/* mouse (only for the first touch point) */
	if (rd->touch[0].status & (TIPSWITCH_BIT | IN_RANGE_BIT | CONFIDENCE_BIT)) {
		input_event(input, EV_KEY, BTN_TOUCH, 1);
		input_event(input, EV_ABS, ABS_X, rd->touch[0].x);
		input_event(input, EV_ABS, ABS_Y, rd->touch[0].y);
	}
	else {
		input_event(input, EV_KEY, BTN_TOUCH, 0);
		input_event(input, EV_ABS, ABS_X, rd->touch[0].x);
		input_event(input, EV_ABS, ABS_Y, rd->touch[0].y);
	}

	/* sync multi and signle touch data with input */
	input_sync(input);
}

/*
 * this function is called upon all reports
 * so that we can filter contact point information,
 * decide whether we are in multi or single touch mode
 * and call input_mt_sync after each point if necessary
 */
static int riemann_event (struct hid_device *hdev, struct hid_field *field,
		                        struct hid_usage *usage, __s32 value)
{
	struct riemann_data *rd = hid_get_drvdata(hdev);

	/* I dont know why this is happening but it is bad news */
	if (field->hidinput == NULL)
	{
		error("%s() - oh dear! field->hidinput is NULL\n", __func__);
		return 0;
	}

	/* this driver should only operate on touchscreen reports, leave the rest to hid */
	if (field->application != HID_DG_TOUCHSCREEN)
		return 0;	/* hid can handle these non touch related stuff */

	/* process the touch report */
	if (hdev->claimed & HID_CLAIMED_INPUT) {
		/* interpret report */
		switch (usage->hid) {
			/* multitouch (finger) report */
			case HID_DG_TIPSWITCH:
				rd->touch[rd->touch_index].status &= ~TIPSWITCH_BIT;
				if (value)
					rd->touch[rd->touch_index].status |= TIPSWITCH_BIT;
				break;
			case HID_DG_INRANGE:
				rd->touch[rd->touch_index].status &= ~IN_RANGE_BIT;
				if (value)
					rd->touch[rd->touch_index].status |= IN_RANGE_BIT;
				break;
			case HID_DG_CONFIDENCE:
				rd->touch[rd->touch_index].status &= ~CONFIDENCE_BIT;
				if (value)
					rd->touch[rd->touch_index].status |= CONFIDENCE_BIT;
				break;
			case HID_DG_CONTACTID:
				rd->touch[rd->touch_index].contact_id = value;
				break;
			case HID_GD_X:
				rd->touch[rd->touch_index].x = value;
				break;
			case HID_GD_Y:
				rd->touch[rd->touch_index].y = value;
				break;
			case HID_DG_WIDTH:
				rd->touch[rd->touch_index].w = value;
				break;
			case HID_DG_HEIGHT:
				rd->touch[rd->touch_index].h = value;
				/* last item in the touch report so move to next touch */
				if (rd->touch_index < MAX_TOUCHES)
					rd->touch_index++;
				else
					error("%s() too many touches (%d)!\n", __func__, rd->touch_index);
				break;
			case HID_DG_CONTACTCOUNT:
				rd->contact_count = value;
				/* last item in our touch reports is always the count */
				report_touch(rd, field->hidinput->input);
				rd->touch_index = 0;
				break;
		}
	}

	/* we have handled the hidinput part, now remains hiddev */
	if ((hdev->claimed & HID_CLAIMED_HIDDEV) && hdev->hiddev_hid_event)
		hdev->hiddev_hid_event(hdev, field, usage, value);

	return 1;	/* we handled it */
}

static int riemann_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct riemann_data *rd;

	trace("%s()\n", __func__); 

	rd = kzalloc(sizeof(struct riemann_data), GFP_KERNEL);
	if (!rd) {
		dev_err(&hdev->dev, "cannot allocate NW riemann data\n");
		return -ENOMEM;
	}
	rd->touch_index = 0;
	hid_set_drvdata(hdev, rd);

	ret = hid_parse(hdev);
	if (!ret)
		ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	else
		kfree(rd);

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
	{HID_USB_DEVICE(0x1926, HID_ANY_ID)},
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

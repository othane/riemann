/*
 *  Multitouch HID driver for NextWindow Riemann (standalone) Touchscreen
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

//#include "hid-ids.h"	/* our vid is in here but it is hard to link to atm so just hard code for now */

#define TIPSWITCH_BIT 	(1<<0)
#define IN_RANGE_BIT	(1<<1)
#define CONFIDENCE_BIT	(1<<2)

#define info(...) printk(__VA_ARGS__)
#define debug(...) printk(__VA_ARGS__)
#define trace(...) printk(__VA_ARGS__)
#define error(...) printk(__VA_ARGS__)

struct riemann_data {
	/* report data */
	__u8	touch_index;
	struct {
		__u8	status;
		__u8	contact_id;
		__u16	x,y;
		__u16	w,h;
	} touch[5];
	__u8	contact_count;

	/* report limits */
	int		report_xmin;
	int		report_ymin;
	int		report_xmax;
	int		report_ymax;
	
	/* rescale output to within these dimensions */
	bool scale_set;
	struct
	{
		int xmin; 
		int ymin;
		int xmax; 
		int ymax;
	} scale;
};


/* sysfs interface */

static ssize_t show_attr_scale(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct riemann_data *rd = hid_get_drvdata(hdev);
	
	trace("%s()\n", __func__); 
	return sprintf(buf, "%dx%d, %dx%d\n", 
		rd->scale.xmin, rd->scale.ymin, rd->scale.xmax, rd->scale.ymax);
}

static ssize_t set_attr_scale(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct riemann_data *rd = hid_get_drvdata(hdev);
	int xmin, ymin;
	int xmax, ymax;
	
	trace("%s()\n", __func__); 
	if (sscanf(buf, "%dx%d, %dx%d", &xmin, &ymin, &xmax, &ymax) == 4) {
		rd->scale.xmin = xmin;
		rd->scale.ymin = ymin;
		rd->scale.xmax = xmax;
		rd->scale.ymax = ymax;
		rd->scale_set = true;
		debug("%s() - scale %d x %d\n", __func__, xmax, ymax);
	}
	else {
		error("%s() invalid scale input!\n", __func__);
		return -1;
	}
	return count;
}

static DEVICE_ATTR(scale, S_IWUGO | S_IRUGO, show_attr_scale, set_attr_scale);

static struct attribute *dev_attrs[] = {
	&dev_attr_scale.attr,
	NULL
};

static struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};


/* hid interface */

static int riemann_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct riemann_data *rd = hid_get_drvdata(hdev);
	trace("%s() - usage:0x%.8X\n", __func__, usage->hid);

	/* just touchscreen for now */
	if (field->application != HID_DG_TOUCHSCREEN)
		return -1;

	switch (usage->hid & HID_USAGE_PAGE) {

		case HID_UP_GENDESK:
			switch (usage->hid) {
				case HID_GD_X:
					debug("%s() - x min:%d; x max:%d\n", __func__,
						field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max,
							EV_ABS, ABS_MT_POSITION_X);
					/* touchscreen emulation */
					input_set_abs_params(hi->input, ABS_X,
								field->logical_minimum,
								field->logical_maximum, 0, 0);
					/* scaling values */
					rd->report_xmin = field->logical_minimum;
					rd->report_xmax = field->logical_maximum;
					if (rd->scale_set == false) {
						rd->scale.xmin = field->logical_minimum;
						rd->scale.xmax = field->logical_maximum;
					}
					return 1;
				case HID_GD_Y:
					debug("%s() - y min:%d; y max:%d\n", __func__,
						field->logical_minimum, field->logical_maximum);
					hid_map_usage(hi, usage, bit, max,
							EV_ABS, ABS_MT_POSITION_Y);
					/* touchscreen emulation */
					input_set_abs_params(hi->input, ABS_Y,
								field->logical_minimum,
								field->logical_maximum, 0, 0);
					/* scaling values */
					rd->report_ymin = field->logical_minimum;
					rd->report_ymax = field->logical_maximum;
					if (rd->scale_set == false) {
						rd->scale.ymin = field->logical_minimum;
						rd->scale.ymax = field->logical_maximum;
					}
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
					return -1;

				case HID_DG_WIDTH:
					debug("%s() - mapping WIDTH to ABS_MT_TOUCH_MAJOR\n", __func__);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_TOUCH_MAJOR);
					return 1;

				case HID_DG_HEIGHT:
					debug("%s() - mapping HEIGHT to ABS_MT_TOUCH_MINOR\n", __func__);
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_TOUCH_MINOR);
					return 1;

				case HID_DG_TIPSWITCH:
					/* touchscreen emulation */
					debug("%s() - mapping TIPSWITCH to BTN_TOUCH\n", __func__);
					hid_map_usage(hi, usage, bit, max, EV_KEY, BTN_TOUCH);
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
	
	/* pass the rest through */
	return 0;
}

static int riemann_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	trace("%s() - usage:0x%.8X\n", __func__, usage->hid);

	if (usage->type == EV_KEY || usage->type == EV_ABS)
		clear_bit(usage->code, *bit);

	return 0;
}

/*
 * this function is called by riemann_event when
 * a touch is ready to be sent to the input sub
 * system
 */
#define xMOUSE_HACK
#define xMULTITOUCH_HACK
#define MIN_WIDTH 1
#define MIN_HEIGHT 1
static void report_touch(struct riemann_data *rd, struct input_dev *input)
{
	unsigned int k;
	__u16	x;
	__u16	y;
	__u16	w;
	__u16	h;
	int	scale_xspan = rd->scale.xmax - rd->scale.xmin;
	int	scale_yspan = rd->scale.ymax - rd->scale.ymin;
	int 	report_xspan = rd->report_xmax - rd->report_xmin;
	int 	report_yspan = rd->report_ymax - rd->report_ymin;
	trace("%s()\n", __func__);

	/* sanity checks */
	if (report_xspan == 0)
	    report_xspan = 1;
	if (report_yspan == 0)
	    report_yspan = 1;
	info("%s() - touch_index=%d, contact_count=%d\n", __func__, rd->touch_index, rd->contact_count);
	info("%s() - info=%p\n", __func__, input);
	if (rd->touch_index != 5) {
		info("%s() - invalid report\n", __func__);
		return;
	}

	for (k=0; k < rd->contact_count; k++) {
		/* filter junk */
		if ((rd->touch[k].x < 0) || (rd->touch[k].x > rd->report_xmax) ||
			(rd->touch[k].y < 0) || (rd->touch[k].y > rd->report_ymax) ||
			(rd->touch[k].contact_id < 0) || (rd->touch[k].contact_id > 4)) {
			debug("%s() - junk report detected\n", __func__);
			return;
		}
		/* multitouch */
		if (rd->touch[k].status & (TIPSWITCH_BIT | IN_RANGE_BIT | CONFIDENCE_BIT)) {		    	
			x = (rd->touch[k].x - rd->report_xmin) * scale_xspan/report_xspan + rd->scale.xmin;
			y = (rd->touch[k].y - rd->report_ymin) * scale_yspan/report_yspan + rd->scale.ymin;
			/* ideally for width and heigh we would do something like below (I think):
			 * 		phi = o + pi/2
			 * 		w+ = w/report_xmax * sqrt((xmax * cos(phi))^2 	+ (ymax * sin(phi))^2)
			 * 		h+ = w/report_ymax * sqrt((xmax * cos(o))^2 	+ (ymax * sin(o))^2)
			 * where
			 * 		o: orientation, 
			 * 		xmax, ymax: new spacial dimensions to map the touch too
			 * 		report_xspan, report_yspan: span of the incoming report size ie from 0 to 32767
			 * 		w+, h+: are the new width and height in the xmax, ymax co-ordinates
			 * but we do not have o sent from the device so we assume a square touch with w in x, and h in y
			 */
			w = rd->touch[k].w * scale_xspan/report_xspan;
			h = rd->touch[k].h * scale_yspan/report_yspan;

			/**@note android needs width and needs it to be non zero and some old holly version set w = 0 */
			if (w == 0) 
				w = MIN_WIDTH;
			if (h == 0) 
				h = MIN_HEIGHT;

			debug("%s() - sending multitouch event (x:%d, y:%d) to input\n", __func__, x, y);
			input_event(input, EV_ABS, ABS_MT_TRACKING_ID, rd->touch[k].contact_id);
			input_event(input, EV_ABS, ABS_MT_POSITION_X, x);
			input_event(input, EV_ABS, ABS_MT_POSITION_Y, y);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, w);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MINOR, h);
		}
		input_mt_sync(input);
	}

	x = (rd->touch[0].x - rd->report_xmin) * scale_xspan/report_xspan + rd->scale.xmin;
	y = (rd->touch[0].y - rd->report_ymin) * scale_yspan/report_yspan + rd->scale.ymin;
	/* mouse (only for the first touch point) */
	if (rd->touch[0].status & (TIPSWITCH_BIT | IN_RANGE_BIT | CONFIDENCE_BIT)) {
		debug("%s() - sending mouse event (x:%d, y:%d) to input\n", __func__, x, y);
		input_event(input, EV_KEY, BTN_TOUCH, 1);
		input_event(input, EV_ABS, ABS_X, x);
		input_event(input, EV_ABS, ABS_Y, y);
	}
	else {
		debug("%s() - sending mouse event (x:%d, y:%d) to input\n", __func__, x, y);
		input_event(input, EV_KEY, BTN_TOUCH, 0);
		input_event(input, EV_ABS, ABS_X, x);
		input_event(input, EV_ABS, ABS_Y, y);
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
		info("%s() - oh dear! field->hidinput is NULL\n", __func__);
		return 0;
	}

	debug("%s() - usage:0x%.8X\n", __func__, usage->hid);
	debug("%s() - reportid:%d\n", __func__, field->report->id);
	debug("%s() - application:0x%.8X\n", __func__, field->application);
	debug("%s() - logical usage: 0x%.8X\n", __func__, field->logical);

	/* ensure this is a touchscreen collection (all are for existing nw devices) */
	if (field->application != HID_DG_TOUCHSCREEN)
		return 0;	/* hid sub system please handle these non finger related stuff */

	/* process the touch report */
	if (hdev->claimed & HID_CLAIMED_INPUT) {
		/* interpret report */
		/**@todo do I need to endian correct the values or has hid-core already done it for me ? */
		switch (usage->hid) {
			/* multitouch (finger) report */
			case HID_DG_TIPSWITCH:
				info("%s() - TIPSWITCH:0x%.4X\n", __func__, value);
				rd->touch[rd->touch_index].status &= ~TIPSWITCH_BIT;
				if (value)
					rd->touch[rd->touch_index].status |= TIPSWITCH_BIT;
				break;
			case HID_DG_INRANGE:
				info("%s() - INRANGE:0x%.4X\n", __func__, value);
				rd->touch[rd->touch_index].status &= ~IN_RANGE_BIT;
				if (value)
					rd->touch[rd->touch_index].status |= IN_RANGE_BIT;
				break;
			case HID_DG_CONFIDENCE:
				info("%s() - CONFIDENCE:0x%.4X\n", __func__, value);
				rd->touch[rd->touch_index].status &= ~CONFIDENCE_BIT;
				if (value)
					rd->touch[rd->touch_index].status |= CONFIDENCE_BIT;
				break;
			case HID_DG_CONTACTID:
				info("%s() - CONTACT ID:%d\n", __func__, value);
				rd->touch[rd->touch_index].contact_id = value;
				break;
			case HID_GD_X:
				info("%s() - X:%d\n", __func__, value);
				rd->touch[rd->touch_index].x = value;
				break;
			case HID_GD_Y:
				info("%s() - Y:%d\n", __func__, value);
				rd->touch[rd->touch_index].y = value;
				break;
			case HID_DG_WIDTH:
				info("%s() - W:%d\n", __func__, value);
				rd->touch[rd->touch_index].w = value;
				break;
			case HID_DG_HEIGHT:
				info("%s() - H:%d\n", __func__, value);
				rd->touch[rd->touch_index].h = value;
				/* last item in the touch report so move to next touch */
				rd->touch_index++;
				break;
			case HID_DG_CONTACTCOUNT:
				info("%s() - CONTACTCOUNT:0x%.4X\n", __func__, value);
				rd->contact_count = value;
				/**@todo process report now we have both touches */
				report_touch(rd, field->hidinput->input);
				rd->touch_index = 0;
				break;
		}
	}

	/**@todo I dont get what below does (I think it passes the report on
	 * but to who and why I dont get */
	/* we have handled the hidinput part, now remains hiddev */
	if ((hdev->claimed & HID_CLAIMED_HIDDEV) && hdev->hiddev_hid_event)
		hdev->hiddev_hid_event(hdev, field, usage, value);

	return 1;	/* we handled it */
}


/* general driver stuff */

static int riemann_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct riemann_data *rd;

	trace("%s()\n", __func__); 

	/* init riemann data */
	rd = kzalloc(sizeof(struct riemann_data), GFP_KERNEL);
	if (!rd) {
		dev_err(&hdev->dev, "cannot allocate NW riemann data\n");
		return -ENOMEM;
	}
	rd->touch_index = 0;
	hid_set_drvdata(hdev, rd);
	rd->scale_set = false;
	rd->scale.xmin = 0;
	rd->scale.ymin = 0;
	rd->scale.xmax = 32767;
	rd->scale.ymax = 32767;
	rd->report_xmin = 0;
	rd->report_ymin = 0;
	rd->report_xmax = 32767;
	rd->report_ymax = 32767;

	/* hid init */
	ret = hid_parse(hdev);
	if (!ret)
		ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	else
		kfree(rd);
	
	/* create a sysfs node */
	ret = sysfs_create_group(&hdev->dev.kobj, &dev_attr_grp);
	if (ret) {
		kfree(rd);
		return ret;
	}

	return ret;
}

static void riemann_remove(struct hid_device *hdev)
{
	trace("%s()\n", __func__); 
	sysfs_remove_group(&hdev->dev.kobj, &dev_attr_grp);
	hid_hw_stop(hdev);
	kfree(hid_get_drvdata(hdev));
	hid_set_drvdata(hdev, NULL);
}

static const struct hid_device_id riemann_devices[] = {
	{HID_USB_DEVICE(0x1926, 0x0008)},
	{HID_USB_DEVICE(0x1926, 0x00FF)},
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

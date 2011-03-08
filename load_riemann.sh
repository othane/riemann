#!/bin/sh

GENERIC_USB_HID=/sys/bus/hid/drivers/generic-usb
RIEMANN=/sys/bus/hid/drivers/riemann

# find the holly touch interface
TOUCH_INTERFACE=`ls ${GENERIC_USB_HID} | grep 1926 | head -n 1`

# unbind our touch interface from generic hid
echo -n ${TOUCH_INTERFACE} > ${GENERIC_USB_HID}/unbind

# bind to riemann
echo -n ${TOUCH_INTERFACE} > ${RIEMANN}/bind

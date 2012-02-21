#!/bin/sh

GENERIC_USB_HID=/sys/bus/hid/drivers/generic-usb
RIEMANN=/sys/bus/hid/drivers/riemann

# find the holly touch interface
TOUCH_INTERFACE=`ls ${GENERIC_USB_HID} | grep 1926 | head -n 1`
echo found ${TOUCH_INTERFACE}

# unbind our touch interface from generic hid
echo unbind ${TOUCH_INTERFACE} from ${GENERIC_USB_HID} 
echo -n ${TOUCH_INTERFACE} > ${GENERIC_USB_HID}/unbind

# bind to riemann
echo bind ${TOUCH_INTERFACE} to ${RIEMANN} 
echo -n ${TOUCH_INTERFACE} > ${RIEMANN}/bind

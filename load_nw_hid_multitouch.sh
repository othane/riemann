#!/bin/sh

GENERIC_USB_HID=/sys/bus/hid/drivers/generic-usb
MULTITOUCH=/sys/bus/hid/drivers/hid-multitouch

# find the nitro touch interface and kick it off
TOUCH_INTERFACE=`ls ${GENERIC_USB_HID} | grep 1926 | head -n 1`
echo found ${TOUCH_INTERFACE}
echo unbind ${TOUCH_INTERFACE} from ${GENERIC_USB_HID} 
echo -n ${TOUCH_INTERFACE} > ${GENERIC_USB_HID}/unbind

# force hid-multitouch to load for this device
echo load hid-multitouch
modprobe hid-multitouch
PAT='^\([0-9A-Fa-f]\+\):\([0-9A-Fa-f]\+\):\([0-9A-Fa-f]\+\).\([0-9A-Fa-f]\+\).*'
BUS=3
VID=`echo ${TOUCH_INTERFACE} | sed "s/${PAT}/\2/"` 
PID=`echo ${TOUCH_INTERFACE} | sed "s/${PAT}/\3/"` 
MT_CLASS="0"
echo new_id BUS=${BUS} VID=${VID} PID=${PID} MT_CLASS=${MT_CLASS}
echo ${BUS} ${VID} ${PID} ${MT_CLASS} > ${MULTITOUCH}/new_id


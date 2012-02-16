#!/usr/bin/env python

import os

GENERIC_USB_HID = '/sys/bus/hid/drivers/generic-usb'
RIEMANN = '/sys/bus/hid/drivers/riemann'

RIEMANN_PIDS = ('0008', '00FF', '025E', '0262')

# insert hid-nwriemann.ko
os.system('"modprobe" hid-nwriemann')

# find the holly multitouch interface
files = os.listdir(GENERIC_USB_HID)
riemann_multitouch_hids = []
for filename in files:
    # limit to hid (0003) devices with nw vendor id (1926)
    if filename.find('0003:1926') == 0:
        for pid in RIEMANN_PIDS:
            # limit to hids with riemann product ids (RIEMANN_PIDS)
            if filename.find(pid) == 10:
                filename_full = "%s%s%s" % (GENERIC_USB_HID ,'/' ,filename)
                uevent = open(filename_full + '/uevent').read()
                # limit to the first hid collection (it is not easy to read hid usages yet so this will do)
                if uevent.find('HID_PHYS=usb-0000:00:1d.1-2/input0') != -1:
		    print 'found riemann hid:', filename
		    riemann_multitouch_hids.append(filename)

for filename in riemann_multitouch_hids:
    # unbind our multitouch hids from generic hid
    print 'unbind "%s" from %s' % (filename, GENERIC_USB_HID)
    open(GENERIC_USB_HID + '/unbind', 'w').write(filename)

    # bind to riemann
    print 'bind "%s" to %s' % (filename, RIEMANN)
    open(RIEMANN + '/bind', 'w').write(filename)

Currently the best way to install the driver is to use::

    # echo 0003:VENDORID:DEVICEID.n > /sys/module/usbhid/drivers/hid\:generic-usb/unbind

Then load your driver with a "modprobe" and do a::

    # echo 0003:VENDORID:DEVICEID.n > /sys/module/YOURDRIVER/drivers/INERNAL_NAME/bind

For example::

    # echo 0003:1926:0008.0001 > /sys/module/usbhid/drivers/hid\:generic-usb/unbind
    # echo 0003:1926:0008.0001 > /sys/module/hid_riemann/drivers/hid\:riemann/bind


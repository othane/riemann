obj-m += hid-nwriemann.o
KBUILD_CFLAGS += -DxDEBUG
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

all module:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean

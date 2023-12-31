CC = gcc
CFLAGS = -I.
DEPS = UsbMonitorService_Server.h
OBJ = UsbMonitorService_Server.o
KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

.PHONY: all clean

UsbMonitorService_APP: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

obj-m := usb_monitor_driver.o

all: UsbMonitorService_APP usb_monitor_driver.ko

usb_monitor_driver.ko:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	sudo rm -rf UsbMonitorService_APP *.o
	$(MAKE) -C $(KDIR) M=$(PWD) clean

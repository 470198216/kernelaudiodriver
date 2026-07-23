# SPDX-License-Identifier: GPL-2.0
# WWJAudio Driver Makefile

obj-m += wwjaudio-codec.o
wwjaudio-codec-y := wwjaudio_codec.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all clean install uninstall

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

install: all
	cp wwjaudio-codec.ko /lib/modules/$(shell uname -r)/extra/
	/sbin/depmod -a

uninstall:
	rm -f /lib/modules/$(shell uname -r)/extra/wwjaudio-codec.ko
	/sbin/depmod -a

test: all
	sudo insmod wwjaudio-codec.ko
	sleep 1
	dmesg | tail -10
	sudo rmmod wwjaudio-codec
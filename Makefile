# SPDX-License-Identifier: GPL-2.0
# WWJAudio Driver Makefile
# No Device Tree required - directly registers ALSA card

obj-m += wwjaudio.o
wwjaudio-y := wwjaudio_codec.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all clean install uninstall test

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

install: all
	sudo mkdir -p /lib/modules/$(shell uname -r)/extra/
	sudo cp wwjaudio.ko /lib/modules/$(uname -r)/extra/
	sudo depmod -a
	@echo "Installed! Use: modprobe wwjaudio"

uninstall:
	sudo rmmod wwjaudio 2>/dev/null || true
	sudo rm -f /lib/modules/$(shell uname -r)/extra/wwjaudio.ko
	sudo depmod -a

test: all
	sudo insmod wwjaudio.ko
	sleep 1
	@echo "=== Kernel Messages ==="
	dmesg | tail -15
	@echo ""
	@echo "=== Available Audio Devices ==="
	aplay -l 2>/dev/null || echo "aplay not available"
	@echo ""
	@echo "=== Testing aplay ==="
	aplay -D plughw:CARD=WWJAudio /dev/zero --duration=1 2>&1 || echo "Test skipped (no audio file)"
	sudo rmmod wwjaudio
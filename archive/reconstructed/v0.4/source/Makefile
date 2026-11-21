# XD OS ARM32 V0.4 reconstruction, Circle for Raspberry Pi 3B/3B+ (AArch32)
# WIFI=1 uses onboard SDIO WLAN. WIFI=0 uses LAN9514 wired Ethernet.
CIRCLEHOME = ../..
WIFI ?= 1
OBJS = main.o kernel.o desktop.o xdos_gui.o webtask.o
ifeq ($(strip $(WIFI)),1)
CPPFLAGS += -DXDOS_USE_WIFI
LIBS = $(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
       $(CIRCLEHOME)/addon/wlan/libwlan.a \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/net/libnet.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a
else
LIBS = $(CIRCLEHOME)/lib/net/libnet.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a
endif
include $(CIRCLEHOME)/Rules.mk
-include $(DEPS)

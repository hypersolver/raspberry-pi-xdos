# XD OS ARM32 Assembly V0.5.2 - ONE image, LAN9514 Ethernet + SDIO WiFi
# Target Pi 3B / 3B+, AARCH=32, RASPPI=3. Circle selects one network on boot.
CIRCLEHOME = ../..
OBJS = main.o kernel.o desktop.o xdos_gui.o webtask.o boot_splash.o
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
include $(CIRCLEHOME)/Rules.mk
-include $(DEPS)

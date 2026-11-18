# XD OS ARM32 V0.3: Circle for Pi3B, AArch32, LAN9514, HTTP browser
CIRCLEHOME = ../..
OBJS = main.o kernel.o desktop.o xdos_gui.o webtask.o
LIBS = $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/net/libnet.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/libcircle.a
include $(CIRCLEHOME)/Rules.mk
-include $(DEPS)

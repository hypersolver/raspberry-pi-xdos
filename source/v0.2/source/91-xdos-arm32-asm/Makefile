# Built inside existing Circle repository configured with ./configure -r 3 -p arm-none-eabi-
CIRCLEHOME = ../..
OBJS = main.o kernel.o desktop.o xdos_gui.o
LIBS = $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a
include $(CIRCLEHOME)/Rules.mk
-include $(DEPS)

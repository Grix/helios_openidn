obj-m += spi-helios.o

KDIR = /lib/modules/$(shell uname -r)/build

all:
        make -C $(KDIR)  M=$(shell pwd) modules

clean:
        make -C $(KDIR)  M=$(shell pwd) clean
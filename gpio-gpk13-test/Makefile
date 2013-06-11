ifneq ($(KERNELRELEASE),)
obj-m:=gpk13.o
else
KERNELDIR ?=/sourcecode/arm-workspace/linux-2.6.38-used
PWD:=$(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm -rf *.o *.mod.c *.mod.o *.ko *.symvers *.order nbproject
endif
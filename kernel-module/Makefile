#KERNEL_SRC=/opt/poky/2.5.1/sysroots/cortexa8hf-neon-poky-linux-gnueabi/usr/src/kernel
#LDFLAGS=""

obj-m += wattmeter-km.o



all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

modules_install:
	make -C $(KERNEL_SRC) M=$(PWD) modules_install

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean


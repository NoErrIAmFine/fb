CROSS_COMPILE:=arm-linux-gnueabihf-

CC:=$(CROSS_COMPILE)gcc
LD:=$(CROSS_COMPILE)ld

TARGET=tftlcd
KERN_DIR=/home/luo/linux/kernel/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek

obj-m+=$(TARGET).o
# obj-m+=tftlcd_all.o
# tftlcd_all-objs:=tftlcd.o cfbcopyarea.o cfbfillrect.o cfbimgblt.o videomode_helper.o

all:
	$(MAKE) -C $(KERN_DIR) M=`pwd` modules
	sudo cp $(TARGET).ko /home/luo/mnt/nfs/rootfs/home/luo/drivers/fb
clean:
	$(MAKE) -C $(KERN_DIR) M=`pwd` clean

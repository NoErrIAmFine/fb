KERN_DIR=/home/luo/linux/kernel/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek

obj-m+=d_tube.o
d_tube-objs:=digital_tube.o cfbcopyarea.o cfbfillrect.o cfbimgblt.o


all:
	arm-linux-gnueabihf-gcc -o u_tube_test test_app.c
	$(MAKE) -C $(KERN_DIR) M=`pwd` modules

clean:
	$(MAKE) -C $(KERN_DIR) M=`pwd` clean
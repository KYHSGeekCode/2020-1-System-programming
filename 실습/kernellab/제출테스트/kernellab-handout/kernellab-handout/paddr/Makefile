KDIR = /lib/modules/$(shell uname -r)/build

obj-m := dbfs_paddr.o

all : 
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules;
	gcc -o app app.c;
	sudo insmod dbfs_paddr.ko

clean : 
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean;
	rm app;
	sudo rmmod dbfs_paddr.ko

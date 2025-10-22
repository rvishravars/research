# Makefile for the asgn2 kernel module and user-space utilities

# The final module will be named asgn2.ko
# It is built from asgn2_main.o and gpio.o
obj-m += asgn2.o
asgn2-y := asgn2_main.o gpio.o

all: modules userspace

# Target to build the kernel module(s)
modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# Target to build the user-space programs
userspace: sendhalfbyte data_generator

sendhalfbyte: sendhalfbyte.c
	$(CC) $(CFLAGS) $^ -o $@

data_generator: data_generator.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f sendhalfbyte data_generator
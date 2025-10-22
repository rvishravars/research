# 1.3
Exercise: using ioctl to pass data

Write a simple module that uses the ioctl directional information to pass a data buffer of fixed size back and forth between the
driver and the user-space program.
The size and directions(s) of the data transfer should be encoded in the command number.
You will need to write a user-space application to test this.
# 1.4

Exercise: using ioctl() to pass data of variable length

Extend the previous exercise to send a buffer whose length is determined at run time. You will probably need to use the _IOC
macro directly in the user-space program. (See linux/ioctl.h.)

# ioctl_fixed Kernel Module

This module demonstrates using `ioctl` to transfer a fixed-size buffer between user space and kernel space.

## Build and Load the Module

```sh
make
sudo insmod ioctl.ko
```

## Create the Device Node

Find the major number:

```sh
MAJOR=$(awk '$2=="ioctl_fixed" {print $1}' /proc/devices)
```

Create the device node:

```sh
sudo mknod /dev/ioctl_fixed c $MAJOR 0
sudo chmod 666 /dev/ioctl_fixed
```

## Run the Test

```sh
./test_ioctl_fixed
```

You should see output similar to:

```
Kernel buffer: 'Hello from kernel!'
Sent buffer to kernel.
Kernel buffer (after set): 'Hello from user!'
```

## Cleanup

```sh
sudo rmmod
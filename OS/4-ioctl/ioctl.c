#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "mycdrv"
#define CLASS_NAME  "ioctlclass"
#define BUF_SIZE    32

#define MYDRBASE 0xF0

#define MYDRVR_RESET    _IO(MYDRBASE, 0)
#define MYDRVR_OFFLINE  _IO(MYDRBASE, 1)
#define MYDRVR_GETSTATE _IOR(MYDRBASE, 2, char[BUF_SIZE])
#define MYDRVR_SETSTATE _IOW(MYDRBASE, 3, char[BUF_SIZE])

#define MAX_VARBUF 256
#define MYDRVR_VARBUF _IOWR(MYDRBASE, 4, struct mydrvr_varbuf)

static int major;
static struct class *cls;
static struct cdev cdev;
static char mydrvr_state_struct[BUF_SIZE] = "Hello from kernel!";

struct mydrvr_varbuf {
    size_t len;
    char buf[MAX_VARBUF];
};

static int dev_open(struct inode *inode, struct file *file) {
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    return 0;
}

static long mydrvr_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (_IOC_TYPE(cmd) != MYDRBASE)
        return -EINVAL;
    switch (cmd) {
    case MYDRVR_RESET:
        memset(mydrvr_state_struct, 0, BUF_SIZE);
        return 0;
    case MYDRVR_OFFLINE:
        strncpy(mydrvr_state_struct, "OFFLINE", BUF_SIZE);
        return 0;
    case MYDRVR_GETSTATE:
        if (copy_to_user((void __user *)arg, &mydrvr_state_struct, sizeof(mydrvr_state_struct))) {
            return -EFAULT;
        }
        return 0;
    case MYDRVR_SETSTATE:
        if (copy_from_user(&mydrvr_state_struct, (void __user *)arg, sizeof(mydrvr_state_struct))) {
            return -EFAULT;
        }
        mydrvr_state_struct[BUF_SIZE-1] = '\0';
        return 0;
    case MYDRVR_VARBUF: {
        struct mydrvr_varbuf tmp;
        if (copy_from_user(&tmp, (void __user *)arg, sizeof(size_t)))
            return -EFAULT;
        if (tmp.len > MAX_VARBUF)
            return -EINVAL;
        // Example: echo back the buffer in uppercase
        if (copy_from_user(tmp.buf, (void __user *)(arg + sizeof(size_t)), tmp.len))
            return -EFAULT;
        for (size_t i = 0; i < tmp.len; ++i)
            if (tmp.buf[i] >= 'a' && tmp.buf[i] <= 'z')
                tmp.buf[i] -= 32;
        if (copy_to_user((void __user *)arg, &tmp, sizeof(size_t) + tmp.len))
            return -EFAULT;
        return 0;
    }
    default:
        return -ENOTTY;
    }
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = mydrvr_ioctl,
};

static int __init ioctl_init(void) {
    dev_t dev;
    int ret;

    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;
    major = MAJOR(dev);

    cdev_init(&cdev, &fops);
    cdev.owner = THIS_MODULE;
    ret = cdev_add(&cdev, dev, 1);
    if (ret) goto unregister;

    cls = class_create(CLASS_NAME);
    if (IS_ERR(cls)) {
        ret = PTR_ERR(cls);
        goto del_cdev;
    }
    device_create(cls, NULL, dev, NULL, DEVICE_NAME);
    pr_info("ioctl: loaded, major=%d\n", major);
    return 0;

del_cdev:
    cdev_del(&cdev);
unregister:
    unregister_chrdev_region(dev, 1);
    return ret;
}

static void __exit ioctl_exit(void) {
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    pr_info("ioctl: unloaded\n");
}

module_init(ioctl_init);
module_exit(ioctl_exit);

MODULE_AUTHOR("Vishravars R");
MODULE_DESCRIPTION("Create a read-only proc entry in Lab 4 of COSC440");
MODULE_LICENSE("GPL v2");

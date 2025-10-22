/**
 * File: asgn2_main.c
 *
 * This is the main driver file for the dummy GPIO port device.
 * It implements a character device /dev/asgn2 that provides read-only access
 * to data received from a dummy GPIO port.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/list.h>

/* Forward declarations for functions in gpio.c */
int gpio_dummy_init(void);
void gpio_dummy_exit(void);
u8 read_half_byte(void);

#define DEVICE_NAME "asgn2"
#define CLASS_NAME  "asgn2"

/* --- Global vars --- */

static dev_t dev_num;
static struct class *dev_class;
static struct cdev my_cdev;

/* single open file req */
static struct semaphore open_sem;

/* open session state */
static bool session_read_done;

/* Wait queue for the reader(s) */
static DECLARE_WAIT_QUEUE_HEAD(read_wq);

/* --- Data Buffering controls --- */

/* 1. Interrupt to Tasklet: Circular buffer */
#define CIRC_BUF_SIZE 4096
static char circ_buf[CIRC_BUF_SIZE];
static int circ_head;
static int circ_tail;
static DEFINE_SPINLOCK(circ_buf_lock);

/* 2. Lightweight Tasklet to Read: Buffer pool */
struct data_node {
    struct list_head list;
    char *buffer;
    size_t len;
    size_t read_pos;
};
static LIST_HEAD(data_pool);
static DEFINE_SPINLOCK(data_pool_lock);
static size_t data_pool_bytes;

/* --- Bottom Half (Tasklet) --- */

/* Prototype for the interrupt handler, called from gpio.c */
irqreturn_t dummyport_interrupt(int irq, void *dev_id);

static void bottom_half_tasklet(struct tasklet_struct *t);
static DECLARE_TASKLET(my_tasklet, bottom_half_tasklet);

/* --- Interrupt Handler --- */

static bool have_first_nibble;
static u8 first_nibble;

/**
 * dummyport_interrupt() - The top-half interrupt handler for the GPIO port.
 */
irqreturn_t dummyport_interrupt(int irq, void *dev_id)
{
    u8 nibble;
    unsigned long flags;

    nibble = read_half_byte();

    spin_lock_irqsave(&circ_buf_lock, flags);

    if (!have_first_nibble) {
        first_nibble = nibble;
        have_first_nibble = true;
    } else {
        char byte = (first_nibble << 4) | (nibble & 0x0F);
        int next_head = (circ_head + 1) % CIRC_BUF_SIZE;

        if (next_head != circ_tail) {
            circ_buf[circ_head] = byte;
            circ_head = next_head;
            // ### LOG 1: Confirm a byte was assembled and tasklet is scheduled ###
            pr_info("asgn2_debug: IRQ assembled byte 0x%02x, scheduling tasklet.\n", byte);
            tasklet_schedule(&my_tasklet);
        } else {
            pr_warn("asgn2: Circular buffer overflow, data lost!\n");
        }
        have_first_nibble = false;
    }

    spin_unlock_irqrestore(&circ_buf_lock, flags);
    return IRQ_HANDLED;
}

/**
 * bottom_half_tasklet() - The bottom-half (deferred work) for the driver.
 */
static void bottom_half_tasklet(struct tasklet_struct *t)
{
    char *temp_buf;
    int bytes_to_move = 0;
    int i;
    unsigned long flags;
    struct data_node *new_node;

    spin_lock_irqsave(&circ_buf_lock, flags);
    if (circ_head >= circ_tail)
        bytes_to_move = circ_head - circ_tail;
    else
        bytes_to_move = CIRC_BUF_SIZE - circ_tail + circ_head;
    spin_unlock_irqrestore(&circ_buf_lock, flags);

    if (bytes_to_move == 0)
        return;

    // ### LOG 2: Check if tasklet is moving the correct amount of data ###
    pr_info("asgn2_debug: Tasklet running, moving %d bytes from circ_buf.\n", bytes_to_move);

    temp_buf = kmalloc(bytes_to_move, GFP_ATOMIC);
    if (!temp_buf) {
        pr_err("asgn2: kmalloc failed in tasklet for temp_buf\n");
        return;
    }

    spin_lock_irqsave(&circ_buf_lock, flags);
    for (i = 0; i < bytes_to_move; i++) {
        temp_buf[i] = circ_buf[circ_tail];
        circ_tail = (circ_tail + 1) % CIRC_BUF_SIZE;
    }
    spin_unlock_irqrestore(&circ_buf_lock, flags);

    new_node = kmalloc(sizeof(struct data_node), GFP_ATOMIC);
    if (!new_node) {
        kfree(temp_buf);
        pr_err("asgn2: kmalloc failed in tasklet for data_node\n");
        return;
    }
    new_node->buffer = temp_buf;
    new_node->len = bytes_to_move;
    new_node->read_pos = 0;

    spin_lock_irqsave(&data_pool_lock, flags);
    list_add_tail(&new_node->list, &data_pool);
    data_pool_bytes += bytes_to_move;
    spin_unlock_irqrestore(&data_pool_lock, flags);

    // ### LOG 3: Confirm that the reader process is being woken up ###
    pr_info("asgn2_debug: Tasklet moved data, waking up read_wq. Total data_pool_bytes = %zu\n", data_pool_bytes);
    wake_up_interruptible(&read_wq);
}

/* --- File Operations --- */

/**
 * asgn2_open() - Called when a process opens the /dev/asgn2 device file.
 */
static int asgn2_open(struct inode *inode, struct file *filp)
{
    if (down_interruptible(&open_sem))
        return -ERESTARTSYS;

    session_read_done = false;
    pr_info("asgn2: device opened\n");
    return 0;
}

/**
 * asgn2_release() - Called when a process closes the device file.
 */
static int asgn2_release(struct inode *inode, struct file *filp)
{
    if (!session_read_done) {
        unsigned long flags;
        struct data_node *node, *tmp;
        bool found_null = false;

        pr_info("asgn2: device closed before session end, cleaning up.\n");

        spin_lock_irqsave(&data_pool_lock, flags);
        list_for_each_entry_safe(node, tmp, &data_pool, list) {
            char *node_data = node->buffer + node->read_pos;
            size_t node_rem = node->len - node->read_pos;
            char *null_char = memchr(node_data, '\0', node_rem);

            if (null_char) {
                size_t to_discard = (null_char - node_data) + 1;
                node->read_pos += to_discard;
                data_pool_bytes -= to_discard;
                found_null = true;
            } else {
                data_pool_bytes -= node_rem;
                node->read_pos = node->len;
            }

            if (node->read_pos >= node->len) {
                list_del(&node->list);
                kfree(node->buffer);
                kfree(node);
            }

            if (found_null)
                break;
        }
        spin_unlock_irqrestore(&data_pool_lock, flags);
    }

    up(&open_sem);
    pr_info("asgn2: device released\n");
    return 0;
}

/**
 * asgn2_read() - Called when a process reads from the device file.
 */
static ssize_t asgn2_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t bytes_read = 0;
    unsigned long flags;

    // ### LOG 4: Check if read is called and if it exits early ###
    pr_info("asgn2_debug: read() called. session_read_done=%d\n", session_read_done);

    if (session_read_done)
        return 0;

    // ### LOG 5: See if the process is about to sleep ###
    pr_info("asgn2_debug: read() preparing to wait. data_pool_bytes=%zu\n", data_pool_bytes);
    if (wait_event_interruptible(read_wq, data_pool_bytes > 0 || session_read_done))
        return -ERESTARTSYS;

    // ### LOG 6: Confirm the process woke up and why ###
    pr_info("asgn2_debug: read() woke up! data_pool_bytes=%zu, session_read_done=%d\n", data_pool_bytes, session_read_done);

    if (session_read_done)
        return 0;

    spin_lock_irqsave(&data_pool_lock, flags);

    while (bytes_read < count && !list_empty(&data_pool)) {
        struct data_node *node = list_first_entry(&data_pool, struct data_node, list);
        char *node_data = node->buffer + node->read_pos;
        size_t node_rem = node->len - node->read_pos;
        size_t to_copy;
        bool found_null = false;
        char *null_char;

        null_char = memchr(node_data, '\0', node_rem);
        if (null_char) {
            to_copy = null_char - node_data;
            found_null = true;
        } else {
            to_copy = node_rem;
        }

        to_copy = min(to_copy, count - bytes_read);

        if (to_copy > 0) {
            spin_unlock_irqrestore(&data_pool_lock, flags);
            if (copy_to_user(buf + bytes_read, node_data, to_copy)) {
                pr_warn("asgn2: copy_to_user failed\n");
                return bytes_read > 0 ? bytes_read : -EFAULT;
            }
            spin_lock_irqsave(&data_pool_lock, flags);
        }

        node->read_pos += to_copy;
        data_pool_bytes -= to_copy;
        bytes_read += to_copy;

        if (found_null) {
            node->read_pos++;
            data_pool_bytes--;
            session_read_done = true;
        }

        if (node->read_pos >= node->len) {
            list_del(&node->list);
            kfree(node->buffer);
            kfree(node);
        }

        if (session_read_done)
            break;
    }

    spin_unlock_irqrestore(&data_pool_lock, flags);
    *f_pos += bytes_read;
    
    // ### LOG 7: See how many bytes are being returned to the user ###
    pr_info("asgn2_debug: read() finished, returning %zd bytes.\n", bytes_read);
    return bytes_read;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = asgn2_open,
    .release = asgn2_release,
    .read = asgn2_read,
};

/* --- Module Init/Exit --- */

/**
 * asgn2_module_init() - The initialization function for the kernel module.
 */
static int __init asgn2_module_init(void)
{
    int ret;
    pr_info("Loading asgn2 module.\n");
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("asgn2: Failed to allocate major number\n");
        return ret;
    }
    dev_class = class_create(CLASS_NAME);
    if (IS_ERR(dev_class)) {
        pr_err("asgn2: Failed to create device class\n");
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(dev_class);
    }
    if (IS_ERR(device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        pr_err("asgn2: Failed to create device file\n");
        class_destroy(dev_class);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }
    cdev_init(&my_cdev, &fops);
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("asgn2: Failed to add cdev\n");
        device_destroy(dev_class, dev_num);
        class_destroy(dev_class);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    ret = gpio_dummy_init();
    if (ret) {
        pr_err("asgn2: gpio_dummy_init failure\n");
        cdev_del(&my_cdev);
        device_destroy(dev_class, dev_num);
        class_destroy(dev_class);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    sema_init(&open_sem, 1);
    pr_info("asgn2 module load success.\n");
    return 0;
}

/**
 * asgn2_module_exit() - The cleanup function for the kernel module.
 */
static void __exit asgn2_module_exit(void)
{
    struct data_node *node, *tmp;
    pr_info("Unloading asgn2 module...\n");
    tasklet_kill(&my_tasklet);
    gpio_dummy_exit();
    cdev_del(&my_cdev);
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    unregister_chrdev_region(dev_num, 1);
    list_for_each_entry_safe(node, tmp, &data_pool, list) {
        list_del(&node->list);
        kfree(node->buffer);
        kfree(node);
    }
    pr_info("asgn2 module unloaded...\n");
}

module_init(asgn2_module_init);
module_exit(asgn2_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vishravars R");
MODULE_DESCRIPTION("Character device driver for a dummy GPIO port.");
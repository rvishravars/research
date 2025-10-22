#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/highmem.h> 

#define DRV_NAME        "asgn1"
#define DRV_DESC        "Virtual Ramdisk (paged, list-backed)"
#define DRV_AUTHOR      "Vishravars Ramasubramanian"
#define DRV_LICENSE     "GPL"

static int asgn1_major = 0;            // dynamically allocate a major
static const char *asgn1_name = DRV_NAME;

#define ASGN1_IOCTL_BASE    0xF1

// Set maximum concurrent opens (processes)
#define ASGN1_IOCTL_SET_MAX_USERS   _IOW(ASGN1_IOCTL_BASE, 0x01, int)

// Get maximum concurrent opens
#define ASGN1_IOCTL_GET_MAX_USERS   _IOR(ASGN1_IOCTL_BASE, 0x02, int)

// Get current open count
#define ASGN1_IOCTL_GET_OPEN_COUNT  _IOR(ASGN1_IOCTL_BASE, 0x03, int)

#define asgn1_kmap_local(page)      kmap_local_page(page)
#define asgn1_kunmap_local(addr)    kunmap_local(addr)


struct page_node {

    struct list_head list;     // kernel's doubly-lnk list node
    struct page *page;         // Pointer to the kernel page.. 4KB?
};

/*
 * struct Represents the state of the ramdisk device.
 * 1. pages: The head of a doubly-linked list of page_node structures.
 * 2. size_bytes: The current logical size of the ramdisk content.
 * 3. lock, max_users, open_count: For synchronization and access control.
 */
struct asgn1_dev {
    struct list_head pages;
    size_t size_bytes;
    struct mutex lock;
    int max_users;
    atomic_t open_count;
};

static struct asgn1_dev gdev;

/* ---------- page lst manage fns ---------- */

/*
* 1. Frees all the pages
*/
static void asgn1_free_all_pages_locked(struct asgn1_dev *dev)
{
    struct page_node *pn, *tmp;

    // Use tmp for safe delete while iteration
    list_for_each_entry_safe(pn, tmp, &dev->pages, list) {
        if (pn->page)
            __free_page(pn->page);

	list_del(&pn->list);

        kfree(pn);
    }
    // Update the metadata
    dev->size_bytes = 0;
}

/*
 * Find the Nth page_node in the list.
 * 1. Traverse the doubly-linked list of pages from the beginning.
 * 2. Locate the node corresponding to the zero-based page_index.
 * 3. Return a pointer to the node on success, or NULL if the index is out of bounds.
 */
static struct page_node *asgn1_get_nth_page_locked(struct asgn1_dev *dev, size_t page_index)
{
    size_t idx = 0;
    struct page_node *pn;

    list_for_each_entry(pn, &dev->pages, list) {
        if (idx == page_index)
            return pn;
        idx++;
    }
    return NULL;
}

/*
* 1. Create/Ensure page for use
*/
static int asgn1_ensure_pages_locked(struct asgn1_dev *dev, size_t needed_pages)
{
    size_t have = 0;
    struct page_node *pn;

    list_for_each_entry(pn, &dev->pages, list)
        have++;

    while (have < needed_pages) {
	// Avoid random garbage value
        pn = kzalloc(sizeof(*pn), GFP_KERNEL);
        if (!pn)
            return -ENOMEM;
	// Create the actual page
        pn->page = alloc_page(GFP_KERNEL);
	// If unsuccessful
        if (!pn->page) {
            kfree(pn);
            return -ENOMEM;
        }
        list_add_tail(&pn->list, &dev->pages);
        have++;
    }
    return 0;
}

/* ---------- mmap support ---------- */

/*
 * asgn1_vma_fault - VMA fault handler for mmap'd regions.
 * 1. Find the page in the device list corresponding to the fault offset.
 * 2. Verify the fault is within the device's size, return SIGBUS on error.
 * 3. Increment the page's refcount and assigns it to the VMF to be mapped.
 */
static vm_fault_t asgn1_vma_fault(struct vm_fault *vmf)
{
	struct page_node *pn;
	size_t page_index = vmf->pgoff;
	vm_fault_t ret = VM_FAULT_SIGBUS; /* Default error */

	mutex_lock(&gdev.lock);

	/*
	 * Check if the fault is within the logical size of the device.
	 * The VMA may be larger than the current file size.
	 */
	if (page_index * PAGE_SIZE >= gdev.size_bytes) {
		goto out;
	}

	pn = asgn1_get_nth_page_locked(&gdev, page_index);
	if (!pn || !pn->page) {
		goto out; /* Should not happen if size check is correct */
	}

	get_page(pn->page); /* Increment page reference count before handing to MM */
	vmf->page = pn->page;
	ret = 0; /* Success (VM_FAULT_NOPAGE) */

out:
	mutex_unlock(&gdev.lock);
	return ret;
}

static const struct vm_operations_struct asgn1_vm_ops = {
	.fault = asgn1_vma_fault,
};

static int asgn1_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &asgn1_vm_ops;
	return 0;
}

/* ---------- File ops related ---------- */

/*
* 1. Open file, validate max concurrent users
*/
static int asgn1_open(struct inode *inode, struct file *filp)
{
    int flags = filp->f_flags;
    int rc = 0;

    mutex_lock(&gdev.lock);

    if (gdev.max_users > 0 && atomic_read(&gdev.open_count) >= gdev.max_users) {
        rc = -EBUSY;
        goto out;
    }

    atomic_inc(&gdev.open_count);

    // fresh write and no append (free all pages)
    if ((flags & O_ACCMODE) == O_WRONLY) {
        asgn1_free_all_pages_locked(&gdev);
    }

out:
    mutex_unlock(&gdev.lock);
    return rc;
}

static int asgn1_release(struct inode *inode, struct file *filp)
{
    atomic_dec(&gdev.open_count);
    return 0;
}

/*
* 1. Handles start, current and end positions
* 2. Validate the new position
*/
static loff_t asgn1_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t newpos;

    mutex_lock(&gdev.lock);

    switch (whence) {
    case SEEK_SET:
        newpos = off;
        break;
    case SEEK_CUR:
        newpos = filp->f_pos + off;
        break;
    case SEEK_END:
        newpos = (loff_t)gdev.size_bytes + off;
        break;
    default:
        mutex_unlock(&gdev.lock);
        return -EINVAL;
    }

    if (newpos < 0) {
        mutex_unlock(&gdev.lock);
        return -EINVAL;
    }

    filp->f_pos = newpos;
    mutex_unlock(&gdev.lock);
    return newpos;
}

/*
 * Read data from the ramdisk into a user-space buffer.
 * 1. Handle EOF by returning 0 if the read position is at or beyond file size.
 * 2. Iterate through the pages, copying data chunks to the user buffer.
 * 3. Update the file position pointer (*ppos) by the number of bytes read.
 */
static ssize_t asgn1_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t read_total = 0;
    size_t pos, remaining;
    int rc = 0;

    if (!count)
        return 0;

    mutex_lock(&gdev.lock);

    // If reading beyond current logical size, return 0 (EOF)
    if (*ppos >= gdev.size_bytes) {
        mutex_unlock(&gdev.lock);
        return 0;
    }

    // Read only upto available data
    if (*ppos + count > gdev.size_bytes)
        count = gdev.size_bytes - *ppos;

    remaining = count;
    pos = (size_t)*ppos;

    while (remaining) {
        size_t page_index = pos >> PAGE_SHIFT;
        size_t page_off   = pos & (PAGE_SIZE - 1);
        size_t chunk      = min(remaining, PAGE_SIZE - page_off);
        struct page_node *pn = asgn1_get_nth_page_locked(&gdev, page_index);
        void *kaddr;

        if (!pn || !pn->page) { // should not happen if size_bytes is correct
            rc = -EIO;
            break;
        }

	// Physical address
        kaddr = asgn1_kmap_local(pn->page);

	// Copy from kernel memory space to user memory space
        if (copy_to_user(buf + read_total, (char *)kaddr + page_off, chunk)) {
            kunmap_local(kaddr);
            rc = -EFAULT;
            break;
        }
        asgn1_kunmap_local(kaddr);

        pos += chunk;
        read_total += chunk;
        remaining -= chunk;
    }

    if (read_total > 0)
        *ppos += read_total;

    mutex_unlock(&gdev.lock);
    return rc ? rc : read_total;
}


/*
 * asgn1_write - Write data to the ramdisk.
 * 1. Dynamic allocate new pages if the write exceeds capacity.
 * 2. Copy data from user space into the correct page(s) at the offset.
 * 3. Update the file position and the total size of the ramdisk.
 */
static ssize_t asgn1_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t written_total = 0;
    size_t pos;
    int rc = 0;

    if (!count)
        return 0;

    mutex_lock(&gdev.lock);

    pos = (size_t)*ppos;

    // Ensure pages exist up to the end of this write
    {
        size_t end_pos = pos + count;
        size_t needed_pages = (end_pos + PAGE_SIZE - 1) >> PAGE_SHIFT;
	// Dynamically expand the ramdisk if necessary
	// Avoid incomplete writes
        rc = asgn1_ensure_pages_locked(&gdev, needed_pages);
        if (rc) {
            mutex_unlock(&gdev.lock);
            return rc;
        }
    }

    // Perform paged write
    {
        size_t remaining = count;

        while (remaining) {
            size_t page_index = pos >> PAGE_SHIFT;
            size_t page_off   = pos & (PAGE_SIZE - 1);
            size_t chunk      = min(remaining, PAGE_SIZE - page_off);
            struct page_node *pn = asgn1_get_nth_page_locked(&gdev, page_index);
            void *kaddr;

            if (!pn || !pn->page) {
                rc = -EIO;
                break;
            }

            kaddr = asgn1_kmap_local(pn->page);

	        // Copy the chunk from the user space to kern
            if (copy_from_user((char *)kaddr + page_off, buf + written_total, chunk)) {
                kunmap_local(kaddr);
                rc = -EFAULT;
                break;
            }
            asgn1_kunmap_local(kaddr);

            pos += chunk;
            written_total += chunk;
            remaining -= chunk;
        }
    }

    if (written_total > 0) {
        *ppos += written_total;
        if ((size_t)*ppos > gdev.size_bytes)
            gdev.size_bytes = (size_t)*ppos;
    }

    mutex_unlock(&gdev.lock);
    return rc ? rc : written_total;
}
/*
* 1. Set the max users and open count
* 2. filep is not used in this case
*/
static long asgn1_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long rc = 0;
    int val = 0;

    mutex_lock(&gdev.lock);

    switch (cmd) {
    case ASGN1_IOCTL_SET_MAX_USERS:
        if (copy_from_user(&val, (void __user *)arg, sizeof(val))) {
            rc = -EFAULT;
            break;
        }
        if (val < 0) {
            rc = -EINVAL;
            break;
        }
        // If decreasing below current open_count, we still allow current holders;
        // New opens will be denied until open_count < max_users.
        gdev.max_users = val;
        break;

    case ASGN1_IOCTL_GET_MAX_USERS:
        val = gdev.max_users;
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            rc = -EFAULT;
        break;

    case ASGN1_IOCTL_GET_OPEN_COUNT:
        val = atomic_read(&gdev.open_count);
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            rc = -EFAULT;
        break;

    default:
        rc = -ENOTTY;
        break;
    }

    mutex_unlock(&gdev.lock);
    return rc;
}

/*
1. file_operations is like an interface
2. Kernel intercepts all the calls b/w user <-> driver
*/
static const struct file_operations asgn1_fops = {
    .owner          = THIS_MODULE,
    .open           = asgn1_open,
    .release        = asgn1_release,
    .read           = asgn1_read,
    .write          = asgn1_write,
    .llseek         = asgn1_llseek,
    .unlocked_ioctl = asgn1_unlocked_ioctl,
    .mmap           = asgn1_mmap,
};

/*
 * asgn1_init - Module initialization function
 * 1. Initializes the global device (gdev).
 * 2. Registers the character device and gets major number.
 */
static int __init asgn1_init(void)
{
    int rc;

    INIT_LIST_HEAD(&gdev.pages);
    mutex_init(&gdev.lock);
    gdev.size_bytes = 0;
    gdev.max_users = 0;   // 0 == unlimited
    atomic_set(&gdev.open_count, 0);

    rc = register_chrdev(0, asgn1_name, &asgn1_fops);
    if (rc < 0) {
        pr_err(DRV_NAME ": register_chrdev failed: %d\n", rc);
        return rc;
    }
    asgn1_major = rc;

    pr_info(DRV_NAME ": loaded. Major=%d. Create node with: mknod /dev/%s c %d 0\n",
            asgn1_major, asgn1_name, asgn1_major);
    pr_info(DRV_NAME ": " DRV_DESC "\n");
    return 0;
}

/*
 * asgn1_exit - Module cleanup function.
 * 1. Free all allocated pages to prevent memory leaks.
 * 2. Unregister the character device from the kernel.
 */
static void __exit asgn1_exit(void)
{
    mutex_lock(&gdev.lock);
    asgn1_free_all_pages_locked(&gdev);
    mutex_unlock(&gdev.lock);

    if (asgn1_major > 0)
        unregister_chrdev(asgn1_major, asgn1_name);

    pr_info(DRV_NAME ": unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);

MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
MODULE_LICENSE(DRV_LICENSE);

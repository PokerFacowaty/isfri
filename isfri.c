#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <asm/errno.h>

/* Most of the comments are here to remind me what a particular thing does
as I'm still learning :) */

// Prototypes
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user*, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "isfri"
#define BUF_LEN 80

// Global vars

static int major; // Major number assigned to our device driver

// atomic Compare-And-Swap (CAS) that determines whether the file is currently open or not
enum {
	CDEV_NOT_USED = 0,
	CDEV_EXCLUSIVE_OPEN = 1,
};

// Is the device open?
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

// The buffer to write the message to
static char msg[BUF_LEN + 1];

static struct class *cls;

// https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html
static struct file_operations chardev_fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release,
};

static char *isfri_devnode(const struct device *dev, umode_t *mode){
	if (mode) *mode = 0666;
	return NULL;
}

/* __init is a macro that causes the init function to be discarded
 * (and its memory freed) once the init function finishes for built-in
 * drivers burtt no loadable modules */
static int __init chardev_init(void){

	/* TODO: possible improvement includes switching to the recommended
	 * cdev interface instead of register_chrdev  */
	major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

	if (major < 0){
		pr_alert("Registering char device failed with %d\n", major);
		return major;
	}

	pr_info("I was assigned major number %d.\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	cls = class_create(DEVICE_NAME);
#else
	/* THIS_MODULE field tells who is the owner of struct file_operations,
	 * which prevents the module from getting unloaded when it's in operation 
	 * https://stackoverflow.com/questions/19467150 */
	cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif
	cls->devnode = isfri_devnode;

	/* One of the three methods to have a /dev/isfri file after the device
	 * has been registered */
	device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

	pr_info("Device created on /dev/%s\n", DEVICE_NAME);
	
	return SUCCESS;
}

static void __exit chardev_exit(void){
	device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);

	unregister_chrdev(major, DEVICE_NAME);

	pr_info("Shutting down.");
	pr_info("I don't hate you.");
}

// Methods

// Called when a procces tries to open the device file
static int device_open(struct inode *inode, struct file *file){

	if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
		return -EBUSY;

	// Increment the usage count
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

// Called when a process closes the device file
static int device_release(struct inode *inode, struct file *file){
	// Ready for the next caller
	atomic_set(&already_open, CDEV_NOT_USED);

	// Decrement the usage count
	module_put(THIS_MODULE);

	return SUCCESS;
}

// Called when a process that already opened the file attempts to read from it
static ssize_t device_read(struct file *filp,
			   char __user *buffer,
			   size_t length,
			   loff_t *offset){
	
	struct timespec64 now;
	struct tm tm_now;
	// Number of bytes actually written to the buffer
	int bytes_read = 0;
	const char *msg_ptr = msg;

	/* Moved here from device_open as per jpodster comment:
	 * https://www.reddit.com/r/linux/comments/16hipvg/comment/k0ev8ds/
	 * Thank you! */

	/* Yes, the time is in UTC, since I spent way too much time
	 * on checking if TZ info can be obtained from the kernel
	 * and the only solution that seemed viable kept telling
	 * me I'm in UTC, so screw it, I guess I was after all. 
	 *
	 * This could maybe be done with a parameter at least? */
	ktime_get_real_ts64(&now);
	time64_to_tm(now.tv_sec, 0, &tm_now);
	
	switch(tm_now.tm_wday){
		case 0:
			sprintf(msg, "No, but it's still the weekend!\n");
			break;
		case 6:
			sprintf(msg, "You just missed it!\n");
			break;
		case 5:
			sprintf(msg, "IT IS!\n");
			break;
		default:
			sprintf(msg, "Nope.\n");
			break;
	}

	if (!*(msg_ptr + *offset)){
			// End of message
		*offset = 0;
		return 0;
	}

	msg_ptr += *offset;

	// Put the data into the buffer
	while (length && *msg_ptr){
		/* The buffer is in the user data segment, not the kernel
		 * segment, so "*" assignment won't work. We have to use
		 * put_user which copies data from the kernel data segment to
		 * the user data segment.
		 */
		put_user(*(msg_ptr++), buffer++);
		length--;
		bytes_read++;
	}

	*offset += bytes_read;

	// Most read functions return the number of bytes put into the buffer.
	return bytes_read;
}

// Called when a process writes to dev file: echo "hi" > /dev/isfri
static ssize_t device_write(struct file *filp, const char __user *buff,
			    size_t len, loff_t *off){
	pr_alert("Sorry, this operation is not supported.\n");
	return -EINVAL;
}

module_init(chardev_init);
module_exit(chardev_exit);
MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("PokerFacowaty");
MODULE_DESCRIPTION("Simple module making a char driver /dev/isfri that will tell you if it's Friday already (in UTC for now)");

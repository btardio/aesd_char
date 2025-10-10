/**
 * 
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "aesd" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Brandon Tardio
 * @date 09-30-2025
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/random.h>
#include <linux/uaccess.h>	/* copy_*_user */

#include "src/aesd.h"		/* local definitions */
#include "src/aesd-circular-buffer.h"
#include "src/aesd-driver.h"
#include "access_ok_version.h"
#include "proc_ops_version.h"




int aesd_p_buffer =  SCULL_P_BUFFER;

int aesd_major =   MAJOR_NUM;
int aesd_minor =   0;
int aesd_nr_devs = SCULL_NR_DEVS;	// number of bare aesd devices

unsigned long ioctl_ptr;
unsigned long *as_ptr_ioctl_ptr;
module_param(aesd_major, int, S_IRUGO);
module_param(aesd_minor, int, S_IRUGO);
module_param(aesd_nr_devs, int, S_IRUGO);

//module_param(ioctl_ptr, ulong, S_IRUGO);

MODULE_AUTHOR("Brandon Tardio");
MODULE_LICENSE("GPL");

struct aesd_dev *aesd_devices;	/* allocated in aesd_init_module */



inline int min_int(int a, int b) {
	return (a < b) ? a : b;
}

inline int max_int(int a, int b) {
	return (a > b) ? a : b;
}

/*
 * Open and close
 */

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */

	int pid_index;

	printk(KERN_WARNING "aesd_open  The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);

	/* now trim to 0 the length of the device if open was write-only */
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	pid_index = get_open_pid_or_index(dev);
	dev->pids[pid_index].pid = current->pid;
	dev->pids[pid_index].fpos = 0;
	dev->pids[pid_index].completed = 1;
	kfree(dev->pids[pid_index].fpos_buffer);
	dev->pids[pid_index].fpos_buffer = NULL;
	print_pids(dev);

	mutex_unlock(&dev->lock);

	return 0;          /* success */
}

int aesd_release(struct inode *inode, struct file *filp)
{
	int pid_index;
	struct aesd_dev *dev;

	dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	printk(KERN_WARNING "aesd_release  The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);

	pid_index = get_open_pid_or_index(dev);

	if (pid_index != -1) {
		dev->pids[pid_index].pid = 0;
		dev->pids[pid_index].fpos = 0;
		dev->pids[pid_index].completed = 1;
		kfree(dev->pids[pid_index].fpos_buffer); // TODO this needs more work, if reader fails to get to close()
		dev->pids[pid_index].fpos_buffer = NULL;
	}


	print_pids(dev);
	mutex_unlock(&dev->lock);

	return 0;
}

void print_pids(struct aesd_dev *dev) {

	int pid_index;
	for ( pid_index = 0; pid_index < PIDS_ARRAY_SIZE; pid_index++ ) {
		printk(KERN_WARNING "dev->pids[i] {.pid .fpos .completed }: %d %d %d\n", dev->pids[pid_index].pid, dev->pids[pid_index].fpos, dev->pids[pid_index].completed);
	}
	return;
}

int get_open_pid_or_index(struct aesd_dev *dev ) {
	int openindex = -1;
	int t;
	for ( t = 0; t < PIDS_ARRAY_SIZE; t++ ) {

		if (current->pid == dev->pids[t].pid ) {
			return t;
		} else if (dev->pids[t].pid == 0) {
			openindex = t;

		}
	}
	return openindex;
}


ssize_t aesd_cat_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {


	int openindex;
	printk(KERN_INFO "aesd_cat_read The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);
	struct aesd_dev *dev = filp->private_data;
	struct aesd_circular_buffer *buffer = &dev->buffer;
	struct aesd_qset *dptr;	/* the first listitem */
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset; /* how many bytes in the listitem */
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;
	printk(KERN_WARNING "f_pos: %d\n", *f_pos);
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;


	int b;
	printk(KERN_WARNING "CCC buffer->s_cb: %d\n", buffer->s_cb);
	for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {
		printk(KERN_WARNING "CCC buffer->entry[b].size: %d\n", buffer->entry[b].size);
		printk(KERN_WARNING "CCC buffer->entry[b].buffptr %.*s", buffer->entry[b].size, buffer->entry[b].buffptr);

	}


	// keep track of the pid, if the process id is the same ( cat for example ), and it read once
	// return 0 to get it to stop its loop, this only works because the size is small and will
	// never be over the 131072 mark, this can be improved
	int pid_index;
	pid_index = get_open_pid_or_index(dev );

	if ( dev->pids[pid_index].pid != 0 ) {
		dev->pids[pid_index].pid = pid_index;
	} else {

		mutex_unlock(&dev->lock);
		return 0;
	}


	int total_size = 0;
	int old_count = buffer->count;
	int old_out_offs = buffer->out_offs;

	// iterate the buffer once to find the total size that it is writing, should be redone to use s_cb
	for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {

		buffer->count--;
		total_size += buffer->entry[buffer->out_offs].size;
		buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

	}

	buffer->count = old_count;
	buffer->out_offs = old_out_offs;

	printk(KERN_WARNING "total_size: %d\n", total_size);
	printk(KERN_WARNING "count: %d\n", count);

	char* temp_buffer = kmalloc(sizeof(char) * max_int(count, total_size), GFP_KERNEL);

	memset(temp_buffer, 0, ksize(temp_buffer));

	int b_offset = 0;

	// iterate again copying the contents to temp_buffer
	for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {
		buffer->count--;

		// write to temp_buffer
		if (buffer->entry[buffer->out_offs].buffptr != NULL) {
			memcpy(temp_buffer + b_offset, 
					buffer->entry[(buffer->out_offs + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].buffptr, 
					buffer->entry[(buffer->out_offs + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size);
		}


		b_offset += buffer->entry[ (buffer->out_offs + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;

		buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		printk(KERN_WARNING "cat temp_buffer: %s\n", temp_buffer);

	}

	//buffer.count = old_count;
	buffer->out_offs = old_out_offs;

	if ( copy_to_user(buf, temp_buffer, count) ) {
		retval = -EFAULT;
		kfree(temp_buffer);
		goto out;
	}


	kfree(temp_buffer);

	*f_pos += count;
	retval = total_size;

out:
	mutex_unlock(&dev->lock);
	return buffer->s_cb;

}

/*
int create_pid_buffer(struct aesd_dev* dev, struct aesd_circular_buffer* buffer, char __user *buf, int pid_index) {

	int total_size = 0;
	int b;

	int outoffset = buffer->out_offs;
	int b_offset;

	if( dev->pids[pid_index].fpos_buffer == NULL || dev->pids[pid_index].completed == 1 ) {
		// b_offset = outoffset;
		b_offset = get_index(buffer, dev->pids[pid_index].index_offset);

		// this loop can probably be improved placing it in the cirular buffer, keep track of total entry size
		for(b = b_offset; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {

			total_size += buffer->entry[b + 1 % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
			//outoffset = (outoffset + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

		}

		printk(KERN_WARNING "total_size: %d\n", total_size);
		printk(KERN_WARNING "buffer->s_cb: %d\n", buffer->s_cb);

		// get the size of all entries in buffer
		dev->pids[pid_index].s_fpos_buffer = total_size;

		// handle 0 size
		if (dev->pids[pid_index].s_fpos_buffer == 0){
			return 0;
		}

		printk(KERN_WARNING "NULL and 1\n");        

		dev->pids[pid_index].fpos_buffer = kmalloc(sizeof(char) * (total_size), GFP_KERNEL);

		memset(dev->pids[pid_index].fpos_buffer, 0, ksize(dev->pids[pid_index].fpos_buffer));

		int b_offset = 0;



		// iterate again copying the contents to temp_buffer
		printk(KERN_WARNING "!@#$ dev->pids[pid_index].index_offset: %d\n", dev->pids[pid_index].index_offset);	
		for(b = b_offset; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {
			
			// write to temp_buffer
			if (buffer->entry[outoffset].buffptr != NULL) {
				memcpy(dev->pids[pid_index].fpos_buffer + b_offset, 
						buffer->entry[(outoffset + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].buffptr, 
						buffer->entry[(outoffset + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size);
			}
			printk(KERN_WARNING "completed write to temp_buffer\n");

			//b_offset += buffer->entry[buffer->out_offs].size;
			b_offset += buffer->entry[ (outoffset + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
			printk(KERN_WARNING "new b_offset: %d\n", b_offset);

			printk(KERN_WARNING "temp_buffer at %d: %.*s\n", outoffset, b_offset, dev->pids[pid_index].fpos_buffer);

			outoffset = (outoffset + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

		}
		printk(KERN_WARNING "000 temp_buffer: %.*s\n", b_offset, dev->pids[pid_index].fpos_buffer);

		printk(KERN_WARNING "000 buffadd: %d\n", *buf);
		printk(KERN_WARNING "000 dev->pids[pid_index].fpos_buffer[0]: %c\n", dev->pids[pid_index].fpos_buffer[0]);

		printk(KERN_WARNING "000 fpos: %d\n", dev->pids[pid_index].fpos);

		if ( copy_to_user(buf, dev->pids[pid_index].fpos_buffer, dev->buffer.s_cb ) ) { // TODO min_int(count, s_read_buffer - dev->pids[pid_index].fpos));
			kfree(dev->pids[pid_index].fpos_buffer);
			return -1;
		}

		// need to cpy pointer for kfree !!!

		//buffer->out_offs = old_out_offs;

	}

	return dev->pids[pid_index].s_fpos_buffer;
}
*/


/*
 * Data management: read and write
 */

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{

	if (current->comm[0] == 'c' && current->comm[1] == 'a' && current->comm[2] == 't') {
		return aesd_cat_read(filp, buf, count, f_pos);
	}

	int i;

	int openindex;
	printk(KERN_INFO "aesd_read The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);
	int t;
	struct aesd_dev *dev = filp->private_data;
	struct aesd_circular_buffer *buffer = &dev->buffer;
	struct aesd_qset *dptr;	/* the first listitem */
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset; /* how many bytes in the listitem */
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;
	int fpos;
	int pid_index;

	print_pids(dev);

	printk(KERN_WARNING "f_pos: %d\n", *f_pos);
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	int b;
	printk(KERN_WARNING "CCC buffer->s_cb: %d\n", buffer->s_cb);
	for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {
		printk(KERN_WARNING "CCC buffer->entry[b].size: %d\n", buffer->entry[b].size);
		printk(KERN_WARNING "CCC buffer->entry[b].buffptr %.*s", buffer->entry[b].size, buffer->entry[b].buffptr);

	}


	// keep track of the pid, if the process id is the same ( cat for example ), and it read once
	pid_index = get_open_pid_or_index(dev );

	printk(KERN_WARNING "openindex: %d\n", openindex);
	printk(KERN_WARNING "pid fpos: %d\n", fpos);

	int old_count = buffer->count;
	int old_out_offs = buffer->out_offs;

	// iterate the buffer once to find the total size that it is writing, should be redone to use s_cb

	buffer->count = old_count;
	buffer->out_offs = old_out_offs;

	printk(KERN_WARNING "count: %d\n", count);


	if (dev->pids[pid_index].fpos_buffer == NULL){
		printk(KERN_WARNING "dev->pids[pid_index].fpos_buffer is null");
	} else {
		printk(KERN_WARNING "dev->pids[pid_index].fpos_buffer is NOT null");
	}

	printk(KERN_WARNING "dev->pids[pid_index].fpos: %d\n", dev->pids[pid_index].fpos);
	printk(KERN_WARNING "buffer->s_cb: %d\n", buffer->s_cb);
	printk(KERN_WARNING "dev->pids[pid_index].completed: %d\n", dev->pids[pid_index].completed);

	printk(KERN_WARNING "000 f_pos: %d\n", *f_pos);
	int s_read_buffer = create_pid_buffer(dev, buffer, buf, pid_index);

	if (s_read_buffer <= 0) {
		goto out;
	}

	if ( dev->pids[pid_index].fpos <= s_read_buffer ) { //buffer->s_cb ) {
		printk(KERN_WARNING "111 buffaddr: %d\n", *buf);
		printk(KERN_WARNING "111 copy_to_user: %.*s\n", buffer->s_cb - dev->pids[pid_index].fpos, dev->pids[pid_index].fpos_buffer + dev->pids[pid_index].fpos);
		printk(KERN_WARNING "111 dev->pids[pid_index].fpos_buffer[0]: %c\n", dev->pids[pid_index].fpos_buffer[0]);
		printk(KERN_WARNING "111 fpos: %d\n", dev->pids[pid_index].fpos);
		printk(KERN_WARNING "111 f_pos: %d\n", *f_pos);

		memcpy(buf, dev->pids[pid_index].fpos_buffer + dev->pids[pid_index].fpos, min_int(count, s_read_buffer - dev->pids[pid_index].fpos));
	}

	printk(KERN_WARNING "~~~ temp_buffer: %s\n", dev->pids[pid_index].fpos_buffer);
	printk(KERN_WARNING "buf: %.*s\n", buffer->s_cb, buf);

out:
	int rval;

	if (dev->pids[pid_index].fpos >= s_read_buffer ) { //buffer->s_cb ){
		rval = 0;
		dev->pids[pid_index].fpos = dev->pids[pid_index].fpos + count;
		//kfree(dev->pids[pid_index].fpos_buffer);
		//dev->pids[pid_index].fpos_buffer = NULL;

	} else {	  
		printk(KERN_WARNING "buffer->s_cb: %d\n", buffer->s_cb);
		printk(KERN_WARNING "dev->pids[pid_index].fpos: %d\n", dev->pids[pid_index].fpos);	
		rval = min_int(count, s_read_buffer - dev->pids[pid_index].fpos ); //  count; // dev->pids[pid_index].fpos; //buffer->s_cb;
		dev->pids[pid_index].fpos = dev->pids[pid_index].fpos + count;

	}

	mutex_unlock(&dev->lock);
	printk(KERN_WARNING "Returning: %d\n", rval);	
	return rval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	// TODO: reset temp buffer variable
	//printk(KERN_WARNING "111\n");

	struct aesd_dev *dev = filp->private_data;
	struct aesd_qset *dptr;
	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */
	struct aesd_circular_buffer *buffer = &dev->buffer;

	printk(KERN_INFO "The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	struct aesd_buffer_entry *buffer_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
	memset(buffer_entry, 0, ksize(buffer_entry));

	const char* mychars;

	mychars = kmalloc(3+ count * sizeof(char), GFP_KERNEL);

	if (mychars) {
		// slab	
		memset(mychars, 0, ksize(mychars));
	}


	if (copy_from_user(mychars, buf, count + 3)) {
		retval = -EFAULT;
		goto out;
	}

	buffer_entry->buffptr = mychars;
	buffer_entry->size=count;


	int foundNewline = 0; // Flag to indicate if newline is found
	int i;

	// Iterate through the string until the null terminator ('\0')
	for (i = 0; mychars[i] != '\0'; i++) {
		if (mychars[i] == '\n') {
			foundNewline = 1; // Set flag if newline is found
			break; // Exit the loop as we've found it
		}
	}


	newline_structure_add(
			dev,
			buffer_entry,
			buffer,
			mychars,
			count,
			foundNewline

			);
	int b;
	printk(KERN_WARNING "@@@ buffer->s_cb: %d\n", buffer->s_cb);
	for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {
		printk(KERN_WARNING "@@@ buffer->entry[b].size: %d\n", buffer->entry[b].size);
		printk(KERN_WARNING "@@@ buffer->entry[b].buffptr %.*s", buffer->entry[b].size, buffer->entry[b].buffptr);

	} 

	// these aren't really used
	*f_pos += count;
	retval = count;
	if (dev->size < *f_pos)
		dev->size = *f_pos;


out:
	mutex_unlock(&dev->lock);
	return retval;
}

// NYI
/*
 * The ioctl() implementation
 */

long aesd_ioctl(struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param)
{

	struct aesd_dev *dev = filp->private_data;

	int i;
	int *temp;
	char ch;
	int pid_index;


	printk(KERN_INFO "aesd_ioctl The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);

	print_pids(dev);


	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;


	pid_index = get_open_pid_or_index(dev );


	/* 
	 * Switch according to the ioctl called 
	 */
	switch (ioctl_num) {
		case IOCTL_SET_MSG:
			/* 
			 * Receive a pointer to a message (in user space) and set that
			 * to be the device's message.  Get the parameter given to 
			 * ioctl by the process. 
			 */
			temp = (int *)ioctl_param;

			/* 
			 * Find the length of the message 
			 */
			get_user(ch, temp);
			//for (i = 0; ch && i < BUF_LEN; i++, temp++)
			//get_user(ch, temp);

			//device_write(file, (char *)ioctl_param, i, 0);
			printk(KERN_WARNING "temp: %d\n", *temp);

			dev->pids[pid_index].index_offset = *temp % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

			break;

	}
out:
	mutex_unlock(&dev->lock);

	return 0;
}



/*
 * The "extended" operations -- only seek
 */

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
	printk(KERN_INFO "aesd_llseek The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);

	printk(KERN_WARNING "loff_t off: %d\n", off);
	printk(KERN_WARNING "whence: %d\n", whence);
	int pid_index;
	struct aesd_dev *dev;
	dev = filp->private_data;
	loff_t newpos;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	pid_index = get_open_pid_or_index(dev);

	switch(whence) {
		case 0: /* SEEK_SET */
			newpos = off;
			break;

		case 1: /* SEEK_CUR */
			//newpos = filp->f_pos + off;
			newpos = dev->pids[pid_index].fpos + off;
			break;

		case 2: /* SEEK_END */
			newpos = dev->buffer.s_cb;
			break;

		default: /* can't happen */
			mutex_unlock(&dev->lock);
			return -EINVAL;
	}
	if (newpos < 0) { 
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	// if position is greater than the end
	if (newpos > dev->buffer.s_cb) { 
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	dev->pids[pid_index].fpos = newpos;

	filp->f_pos = newpos;
	print_pids(dev);
	mutex_unlock(&dev->lock);

	return newpos;
}



struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.llseek =   aesd_llseek,
	.read =     aesd_read,
	.write =    aesd_write,
	.unlocked_ioctl = aesd_ioctl, // .unlocked_ioctl
	.open =     aesd_open,
	.release =  aesd_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void aesd_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	/* Get rid of our char dev entries */
	if (aesd_devices) {
		for (i = 0; i < aesd_nr_devs; i++) {
			//aesd_trim(aesd_devices + i);
			cdev_del(&aesd_devices[i].cdev);
		}
		kfree(aesd_devices);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, aesd_nr_devs);

	aesd_access_cleanup();

	//kfree(buffer);

}


/*
 * Set up the char_dev structure for this device.
 */
static void aesd_setup_cdev(struct aesd_dev *dev, int index)
{
	int err, devno = MKDEV(aesd_major, aesd_minor + index);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding aesd%d", err, index);
}




int aesd_init_module(void)
{

	int b;
	int result, i;
	dev_t dev = 0;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */

	if (aesd_major) {
		dev = MKDEV(aesd_major, aesd_minor);
		printk(KERN_WARNING "shouldnt be here 000\n");
		result = register_chrdev_region(dev, aesd_nr_devs, "aesdchar");
	} else {
		result = alloc_chrdev_region(&dev, aesd_minor, aesd_nr_devs,
				"aesdchar");
		aesd_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "aesd: can't get major %d\n", aesd_major);
		return result;
	}


	int ret_val;

	/* 
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	aesd_devices = kmalloc(aesd_nr_devs * sizeof(struct aesd_dev), GFP_KERNEL);
	if (!aesd_devices) {
		result = -ENOMEM;
		goto fail;  /* Make this more graceful */
	}
	memset(aesd_devices, 0, aesd_nr_devs * sizeof(struct aesd_dev));

	/* Initialize each device. */
	for (i = 0; i < aesd_nr_devs; i++) {
		mutex_init(&aesd_devices[i].lock);
		aesd_setup_cdev(&aesd_devices[i], i);
		aesd_devices[i].buffer.in_offs = aesd_devices[i].buffer.out_offs = 0;
		for ( b = 0; b < PIDS_ARRAY_SIZE; b++) {
			aesd_devices[i].pids[b].pid = 0;
			aesd_devices[i].pids[b].completed = 1;
			aesd_devices[i].pids[b].fpos = 0;
			aesd_devices[i].pids[b].fpos_buffer = NULL;
			aesd_devices[i].pids[b].index_offset = 0;
		}

	}

	/* At this point call the init function for any friend device */
	dev = MKDEV(aesd_major, aesd_minor + aesd_nr_devs);

	dev += aesd_access_init(dev);

	return 0; /* succeed */

fail:
	printk(KERN_WARNING "fail goto reached\n");
	aesd_cleanup_module();
	return result;

}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

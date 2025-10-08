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
//	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
		// aesd_trim(dev); /* ignore errors 
		
	pid_index = get_open_pid_or_index(dev);
	dev->pids[pid_index].pid = current->pid;
	dev->pids[pid_index].fpos = 0;
	dev->pids[pid_index].completed = 0;

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
		dev->pids[pid_index].completed = 0;
		kfree(dev->pids[pid_index].fpos_buffer); // this needs more work, if reader fails to get to close()
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
	//int t;
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
/*	
	openindex = -1;
	if (current->comm[0] == 'c' && current->comm[1] == 'a' && current->comm[2] == 't') {
		for ( t = 0; t < PIDS_ARRAY_SIZE; t++ ) {
			printk(KERN_WARNING "dev->pids[t].pid: %d\n", dev->pids[t].pid);
			
			if (current->pid == dev->pids[t].pid ) { //&& dev->pids[t].completed >= dev->buffer.s_cb) {
				dev->pids[t].pid = 0;
				mutex_unlock(&dev->lock);
				return 0;
			} else if (dev->pids[t].pid == 0) {
				openindex = t;
			}
		}
		printk(KERN_WARNING "openindex: %d\n", openindex);
		if (openindex == 0) {
			mutex_unlock(&dev->lock);
			return -1;
		} else {
			dev->pids[openindex].pid = current->pid;
		}
	}
*/
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

	char* temp_buffer = kmalloc(sizeof(char) * MAX(count, total_size), GFP_KERNEL);

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
		printk(KERN_WARNING "temp_buffer: %s\n", temp_buffer);

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

inline int min_int(int a, int b) {
	return (a < b) ? a : b;
}

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

	//	for ( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
	//		printk(KERN_WARNING "buffer[%d]: %s\n",i, buffer.entry[i].buffptr);
	//	}
	
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

	//dev->pids[pid_index].fpos = dev->pids[pid_index].fpos + count;
	

	// keep track of the pid, if the process id is the same ( cat for example ), and it read once
	// return 0 to get it to stop its loop, this only works because the size is small and will
	// never be over the 131072 mark, this can be improved
/*	
	openindex = -1;
	
	for ( t = 0; t < PIDS_ARRAY_SIZE; t++ ) {
		printk(KERN_WARNING "dev->pids[t].pid: %d\n", dev->pids[t].pid);
		
		if (current->pid == dev->pids[t].pid ) { //&& dev->pids[t].completed >= dev->buffer.s_cb) {
			dev->pids[t].pid = 0;
			fpos = dev->pids[t].fpos;
			mutex_unlock(&dev->lock);
			return 0;
		} else if (dev->pids[t].pid == 0) {
			openindex = t;

		}
	}
	
*/	


	printk(KERN_WARNING "openindex: %d\n", openindex);
	printk(KERN_WARNING "pid fpos: %d\n", fpos);
/*
	if (openindex == 0) {
		mutex_unlock(&dev->lock);
		return -1;
	} else {
		dev->pids[openindex].pid = current->pid;
	}
*/
	//	}


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

	// remove me
//	if (dev->pids[pid_index].fpos > 3 ) {
//		mutex_unlock(&dev->lock);
//
//		return 0;
//	}

	if( dev->pids[pid_index].fpos_buffer == NULL && dev->pids[pid_index].fpos <= buffer->s_cb ){

		dev->pids[pid_index].fpos_buffer = kmalloc(sizeof(char) * MAX(count, total_size), GFP_KERNEL);

		memset(dev->pids[pid_index].fpos_buffer, 0, ksize(dev->pids[pid_index].fpos_buffer));
	
		int b_offset = 0;
	
		// iterate again copying the contents to temp_buffer
		printk(KERN_WARNING "!@#$ dev->pids[pid_index].index_offset: %d\n", dev->pids[pid_index].index_offset);	
		for(b = dev->pids[pid_index].index_offset; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {
			buffer->count--;
			// write to temp_buffer
			if (buffer->entry[buffer->out_offs].buffptr != NULL) {
				memcpy(dev->pids[pid_index].fpos_buffer + b_offset, 
					buffer->entry[(buffer->out_offs + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].buffptr, 
					buffer->entry[(buffer->out_offs + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size);
			}
		
		
			//b_offset += buffer->entry[buffer->out_offs].size;
			b_offset += buffer->entry[ (buffer->out_offs + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;

//			// write to temp_buffer
//			if (buffer->entry[buffer->out_offs].buffptr != NULL) {
//				memcpy(dev->pids[pid_index].fpos_buffer + b_offset, buffer->entry[buffer->out_offs].buffptr, buffer->entry[buffer->out_offs].size);
//			}
			
//			b_offset += buffer->entry[buffer->out_offs].size;
		
			buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
	//		printk(KERN_WARNING "temp_buffer: %s\n", temp_buffer);

		}
		printk(KERN_WARNING "000 temp_buffer: %s\n", dev->pids[pid_index].fpos_buffer);

		printk(KERN_WARNING "000 buffadd: %d\n", *buf);
//		printk(KERN_WARNING "000 copy_to_user: %.*s\n", MIN(count, buffer->s_cb - dev->pids[pid_index].fpos), dev->pids[pid_index].fpos_buffer + dev->pids[pid_index].fpos);
		printk(KERN_WARNING "000 dev->pids[pid_index].fpos_buffer[0]: %c\n", dev->pids[pid_index].fpos_buffer[0]);

		printk(KERN_WARNING "000 fpos: %d\n", dev->pids[pid_index].fpos);
		printk(KERN_WARNING "000 f_pos: %d\n", *f_pos);

		if ( copy_to_user(buf, dev->pids[pid_index].fpos_buffer /* + dev->pids[pid_index].fpos */, dev->buffer.s_cb ) ) { //MIN(count, buffer->s_cb - dev->pids[pid_index].fpos ) ) ) {
			retval = -EFAULT;
			kfree(dev->pids[pid_index].fpos_buffer);
			goto out;
		}

		// need to cpy pointer for kfree !!!

		buffer->out_offs = old_out_offs;

//		if (dev->pids[pid_index].fpos + count <= buffer->s_cb ){
//			*buf += dev->pids[pid_index].fpos; // note this always at the beginning of the __user buffer
//		} else {
//
//		}

//		*f_pos = 0;
	} 
	if ( dev->pids[pid_index].fpos <= buffer->s_cb ) {
		printk(KERN_WARNING "111 buffaddr: %d\n", *buf);
		//printk(KERN_WARNING "111 copy_to_user: %.*s\n", MIN(count, buffer->s_cb - dev->pids[pid_index].fpos), dev->pids[pid_index].fpos_buffer + dev->pids[pid_index].fpos);
		printk(KERN_WARNING "111 dev->pids[pid_index].fpos_buffer[0]: %c\n", dev->pids[pid_index].fpos_buffer[0]);
		printk(KERN_WARNING "111 fpos: %d\n", dev->pids[pid_index].fpos);
		printk(KERN_WARNING "111 f_pos: %d\n", *f_pos);

		memcpy(buf, dev->pids[pid_index].fpos_buffer + dev->pids[pid_index].fpos, min_int(count, buffer->s_cb - dev->pids[pid_index].fpos));	
		//		return 0;
		
//		copy_to_user(buf + dev->pids[pid_index].fpos, dev->pids[pid_index].fpos_buffer + dev->pids[pid_index].fpos,  MIN(count, buffer->s_cb - dev->pids[pid_index].fpos ) ); // count

//		*buf += count; // MIN(count, buffer->s_cb - dev->pids[pid_index].fpos ); 
		//			printk(KERN_WARNING "erroring\n");
//			retval = -EFAULT;
			//kfree(dev->pids[pid_index].fpos_buffer);
//			goto out;
//		}

//		mutex_unlock(&dev->lock);
//		return 1;
//		*f_pos += count;
		
		
//		if (dev->pids[pid_index].fpos + count <= buffer->s_cb ){
//			*buf += count;
//			*f_pos += count;
//		} else {
//			// todo: possible set new position to last position of s_cb
//
//		}

	}

	printk(KERN_WARNING "temp_buffer: %s\n", dev->pids[pid_index].fpos_buffer);
	printk(KERN_WARNING "buf: %.*s\n", buffer->s_cb, buf);
	//buffer.count = old_count;
//	buffer->out_offs = old_out_offs;
//	*buf += 1;
/*	
	if ( copy_to_user(buf, dev->pids[pid_index].fpos_buffer + dev->pids[pid_index].fpos, count) ) {
		retval = -EFAULT;
		kfree(dev->pids[pid_index].fpos_buffer);
		goto out;
	}
*/

//	kfree(temp_buffer);


	//retval = total_size;

  out:
	int rval;

	if (dev->pids[pid_index].fpos >= buffer->s_cb ){
		rval = 0;
		dev->pids[pid_index].fpos = dev->pids[pid_index].fpos + count;
		//kfree(dev->pids[pid_index].fpos_buffer);
		//dev->pids[pid_index].fpos_buffer = NULL;

	} else {	  
	   	printk(KERN_WARNING "buffer->s_cb: %d\n", buffer->s_cb);
		printk(KERN_WARNING "dev->pids[pid_index].fpos: %d\n", dev->pids[pid_index].fpos);	
		rval = MIN(count, buffer->s_cb - dev->pids[pid_index].fpos ); //  count; // dev->pids[pid_index].fpos; //buffer->s_cb;
		dev->pids[pid_index].fpos = dev->pids[pid_index].fpos + count;

	}

	mutex_unlock(&dev->lock);
	printk(KERN_WARNING "Returning: %d\n", rval);	
	return rval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{

	printk(KERN_WARNING "111\n");

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

//	printk(KERN_INFO "aesd_ioctl The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);
	
//	unsigned long *mioctl_ptr = kmalloc(sizeof(unsigned long), GFP_KERNEL);


//	unsigned long* sss;	
	//void __user *argp = (void __user *) arg;
//	printk(KERN_WARNING "receiving cmd: %lX\n", cmd);
//	printk(KERN_WARNING "receiving address: %lX\n", arg);
//	printk(KERN_WARNING "*receiving address: %lX\n", *arg);
//	printk(KERN_WARNING "__get_user(ioctl_ptr, (unsigned long __user *)arg): %lX\n", __get_user(mioctl_ptr, (ulong __user *)arg));
//	printk(KERN_WARNING "ioctl_ptr: %lX\n", mioctl_ptr);
//	as_ptr_ioctl_ptr = (void*) mioctl_ptr;
//	printk(KERN_WARNING "as_ptr_ioctl_ptr: %lX\n", as_ptr_ioctl_ptr);
//	printk(KERN_WARNING "as_ptr_ioctl_ptr: %lu\n", *as_ptr_ioctl_ptr);
	
//	unsigned long *asval = kmalloc(sizeof(unsigned long), GFP_KERNEL);


//	*asval = 0L;

//	printk(KERN_WARNING "asval: %lX\n", *asval);

//	memcpy(asval, as_ptr_ioctl_ptr, sizeof(unsigned long));

//	printk(KERN_WARNING "asval: %lX\n", *asval);

	//printk(KERN_WARNING "*ioctl_ptr: %lX\n", *(unsigned long*)ioctl_ptr);

//	copy_from_user(sss, (unsigned long __user *)arg, sizeof(unsigned long));

//	printk(KERN_WARNING "aesd_ioctl arg: %lX\n", sss);
	

	
//	int err = 0, tmp;
//	int retval = 0;
    
	//
	 // extract the type and number bitfields, and don't decode
	 // wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 //
//	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
//	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	//
	 // the direction is a bitmask, and VERIFY_WRITE catches R/W
	 // transfers. `Type' is user-oriented, while
	 // access_ok is kernel-oriented, so the concept of "read" and
	 // "write" is reversed
	 //
//	if (_IOC_DIR(cmd) & _IOC_READ)
//		err = !access_ok_wrapper(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
//	else if (_IOC_DIR(cmd) & _IOC_WRITE)
//		err =  !access_ok_wrapper(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
//	if (err) return -EFAULT;
	
//	printk(KERN_WARNING "cmd ioctl: %d\n", cmd);
//	return 0;
/*
	switch(cmd) {

		case SCULL_IOCRESET:
			aesd_quantum = SCULL_QUANTUM;
			aesd_qset = SCULL_QSET;
		break;
        
		case SCULL_IOCSQUANTUM: // Set: arg points to the value 
			if (! capable (CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			retval = __get_user(aesd_quantum, (int __user *)arg);
		break;

		case SCULL_IOCTQUANTUM: // Tell: arg is the value 

			if (! capable (CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			aesd_quantum = arg;
		break;

		case SCULL_IOCGQUANTUM: // Get: arg is pointer to result 

			retval = __put_user(aesd_quantum, (int __user *)arg);
		break;

		case SCULL_IOCQQUANTUM: // Query: return it (it's positive)
	
			return aesd_quantum;

		case SCULL_IOCXQUANTUM: // eXchange: use arg as pointer 

			if (! capable (CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			tmp = aesd_quantum;
			retval = __get_user(aesd_quantum, (int __user *)arg);
			if (retval == 0) {
				retval = __put_user(tmp, (int __user *)arg);
			}
		break;

		case SCULL_IOCHQUANTUM: // sHift: like Tell + Query 

			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
			tmp = aesd_quantum;
			aesd_quantum = arg;
			return tmp;
        
		case SCULL_IOCSQSET:

			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
			retval = __get_user(aesd_qset, (int __user *)arg);
		break;

		case SCULL_IOCTQSET:

			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
			aesd_qset = arg;
		break;

		case SCULL_IOCGQSET:
	
			retval = __put_user(aesd_qset, (int __user *)arg);
		break;

		case SCULL_IOCQQSET:
	
			return aesd_qset;

		case SCULL_IOCXQSET:
	
			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
			tmp = aesd_qset;
			retval = __get_user(aesd_qset, (int __user *)arg);
			if (retval == 0)
				retval = put_user(tmp, (int __user *)arg);
		break;

		case SCULL_IOCHQSET:
	
			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
			tmp = aesd_qset;
			aesd_qset = arg;
			return tmp;

        //
         // The following two change the buffer size for aesdpipe.
         // The aesdpipe device uses this same ioctl method, just to
         // write less code. Actually, it's the same driver, isn't it?
         //

	  case SCULL_P_IOCTSIZE:

		aesd_p_buffer = arg;
		break;

	  case SCULL_P_IOCQSIZE:

		return aesd_p_buffer;


	  default:  // redundant, as cmd was checked against MAXNR 
		return -ENOTTY;
	}
	return retval;
*/
//}



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

#ifdef SCULL_DEBUG /* use proc only if debugging */
	aesd_remove_proc();
#endif

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
	 * Register the character device (atleast try) 
	 */
	//ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);

	/* 
	 * Negative values signify an error 
	 */
/*	if (ret_val < 0) {
		printk(KERN_ALERT "%s failed with %d\n",
		       "Sorry, registering the character device ", ret_val);
		return ret_val;
	}


	printk(KERN_INFO "%s The major device number is %d.\n",
	       "Registeration is a success", MAJOR_NUM);
	printk(KERN_INFO "If you want to talk to the device driver,\n");
	printk(KERN_INFO "you'll have to create a device file. \n");
	printk(KERN_INFO "We suggest you use:\n");
	printk(KERN_INFO "mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
	printk(KERN_INFO "The device file name is important, because\n");
	printk(KERN_INFO "the ioctl program assumes that's the\n");
	printk(KERN_INFO "file you'll use.\n");
*/
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
			aesd_devices[i].pids[b].completed = 0;
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

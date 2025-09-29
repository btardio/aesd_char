/**
 * 
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "aesd" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */



// todo look at what happens when mknod /dev/aesd c 508 3 instead of 1

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

#include <linux/uaccess.h>	/* copy_*_user */

#include "src/aesd.h"		/* local definitions */
#include "src/aesd-circular-buffer.h"
#include "access_ok_version.h"
#include "proc_ops_version.h"

#define MAX(a, b) ( ( a > b ) ? ( a ) : ( b ))

int aesd_p_buffer =  SCULL_P_BUFFER;

/*
 * Our parameters which can be set at load time.
 */

int aesd_major =   SCULL_MAJOR;
int aesd_minor =   0;
int aesd_nr_devs = SCULL_NR_DEVS;	/* number of bare aesd devices */
int aesd_quantum = SCULL_QUANTUM;
int aesd_qset =    SCULL_QSET;

module_param(aesd_major, int, S_IRUGO);
module_param(aesd_minor, int, S_IRUGO);
module_param(aesd_nr_devs, int, S_IRUGO);
module_param(aesd_quantum, int, S_IRUGO);
module_param(aesd_qset, int, S_IRUGO);

MODULE_AUTHOR("Brandon Tardio");
MODULE_LICENSE("GPL");

struct aesd_dev *aesd_devices;	/* allocated in aesd_init_module */

//static struct aesd_circular_buffer buffer;

/*
 * Empty out the aesd device; must be called with the device
 * semaphore held.
 */
int aesd_trim(struct aesd_dev *dev)
{
	struct aesd_qset *next, *dptr;
	int qset = dev->qset;   /* "dev" is not-null */
	int i;

	for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
		if (dptr->data) {
			for (i = 0; i < qset; i++)
				kfree(dptr->data[i]);
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		kfree(dptr);
	}
	dev->size = 0;
	dev->quantum = aesd_quantum;
	dev->qset = aesd_qset;
	dev->data = NULL;
	return 0;
}
#ifdef SCULL_DEBUG /* use proc only if debugging */
/*
 * The proc filesystem: function to read and entry
 */

int aesd_read_procmem(struct seq_file *s, void *v)
{
        int i, j;
        int limit = s->size - 80; /* Don't print more than this */

        for (i = 0; i < aesd_nr_devs && s->count <= limit; i++) {
                struct aesd_dev *d = &aesd_devices[i];
                struct aesd_qset *qs = d->data;
                if (mutex_lock_interruptible(&d->lock))
                        return -ERESTARTSYS;
                seq_printf(s,"\nDevice %i: qset %i, q %i, sz %li\n",
                             i, d->qset, d->quantum, d->size);
                for (; qs && s->count <= limit; qs = qs->next) { /* scan the list */
                        seq_printf(s, "  item at %p, qset at %p\n",
                                     qs, qs->data);
                        if (qs->data && !qs->next) /* dump only the last item */
                                for (j = 0; j < d->qset; j++) {
                                        if (qs->data[j])
                                                seq_printf(s, "    % 4i: %8p\n",
                                                             j, qs->data[j]);
                                }
                }
                mutex_unlock(&aesd_devices[i].lock);
        }
        return 0;
}



/*
 * Here are our sequence iteration methods.  Our "position" is
 * simply the device number.
 */
static void *aesd_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= aesd_nr_devs)
		return NULL;   /* No more to read */
	return aesd_devices + *pos;
}

static void *aesd_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= aesd_nr_devs)
		return NULL;
	return aesd_devices + *pos;
}

static void aesd_seq_stop(struct seq_file *s, void *v)
{
	/* Actually, there's nothing to do here */
}

static int aesd_seq_show(struct seq_file *s, void *v)
{
	struct aesd_dev *dev = (struct aesd_dev *) v;
	struct aesd_qset *d;
	int i;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
	seq_printf(s, "\nDevice %i: qset %i, q %i, sz %li\n",
			(int) (dev - aesd_devices), dev->qset,
			dev->quantum, dev->size);
	for (d = dev->data; d; d = d->next) { /* scan the list */
		seq_printf(s, "  item at %p, qset at %p\n", d, d->data);
		if (d->data && !d->next) /* dump only the last item */
			for (i = 0; i < dev->qset; i++) {
				if (d->data[i])
					seq_printf(s, "    % 4i: %8p\n",
							i, d->data[i]);
			}
	}
	mutex_unlock(&dev->lock);
	return 0;
}
	
/*
 * Tie the sequence operators up.
 */
static struct seq_operations aesd_seq_ops = {
	.start = aesd_seq_start,
	.next  = aesd_seq_next,
	.stop  = aesd_seq_stop,
	.show  = aesd_seq_show
};

/*
 * Now to implement the /proc files we need only make an open
 * method which sets up the sequence operators.
 */
static int aesdmem_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, aesd_read_procmem, NULL);
}

static int aesdseq_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &aesd_seq_ops);
}

/*
 * Create a set of file operations for our proc files.
 */
static struct file_operations aesdmem_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = aesdmem_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static struct file_operations aesdseq_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = aesdseq_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};
	

/*
 * Actually create (and remove) the /proc file(s).
 */

static void aesd_create_proc(void)
{
	proc_create_data("aesdmem", 0 /* default mode */,
			NULL /* parent dir */, proc_ops_wrapper(&aesdmem_proc_ops, aesdmem_pops),
			NULL /* client data */);
	proc_create("aesdseq", 0, NULL, proc_ops_wrapper(&aesdseq_proc_ops, aesdseq_pops));
}

static void aesd_remove_proc(void)
{
	/* no problem if it was not registered */
	remove_proc_entry("aesdmem", NULL /* parent dir */);
	remove_proc_entry("aesdseq", NULL);
}


#endif /* SCULL_DEBUG */





/*
 * Open and close
 */

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */

	/* now trim to 0 the length of the device if open was write-only */
	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
		aesd_trim(dev); /* ignore errors */
		mutex_unlock(&dev->lock);
	}
	return 0;          /* success */
}

int aesd_release(struct inode *inode, struct file *filp)
{
	return 0;
}
/*
 * Follow the list
 */
struct aesd_qset *aesd_follow(struct aesd_dev *dev, int n)
{
	struct aesd_qset *qs = dev->data;

        /* Allocate first qset explicitly if need be */
	if (! qs) {
		qs = dev->data = kmalloc(sizeof(struct aesd_qset), GFP_KERNEL);
		if (qs == NULL)
			return NULL;  /* Never mind */
		memset(qs, 0, sizeof(struct aesd_qset));
	}

	/* Then follow the list */
	while (n--) {
		if (!qs->next) {
			qs->next = kmalloc(sizeof(struct aesd_qset), GFP_KERNEL);
			if (qs->next == NULL)
				return NULL;  /* Never mind */
			memset(qs->next, 0, sizeof(struct aesd_qset));
		}
		qs = qs->next;
		continue;
	}
	return qs;
}

/*
 * Data management: read and write
 */

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
//	struct aesd_circular_buffer buffer;
	int i;
//	for ( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
//		printk(KERN_WARNING "buffer[%d]: %s\n",i, buffer.entry[i].buffptr);
//	}

	printk(KERN_INFO "The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);

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
//	if (*f_pos >= dev->size)
//		goto out;
//	if (*f_pos + count > dev->size)
//		count = dev->size - *f_pos;

	/* find listitem, qset index, and offset in the quantum */
//	item = (long)*f_pos / itemsize;
//	rest = (long)*f_pos % itemsize;
//	s_pos = rest / quantum; q_pos = rest % quantum;

	/* follow the list up to the right position (defined elsewhere) */
//	dptr = aesd_follow(dev, item);

//	if (dptr == NULL || !dptr->data || ! dptr->data[s_pos])
//		goto out; /* don't fill holes */

	/* read only up to the end of this quantum */
//	if (count > quantum - q_pos)
//		count = quantum - q_pos;

	
	int total_size = 0;
	int old_count = buffer->count;
	int old_out_offs = buffer->out_offs;
	// empty buffer get sizes
	//while (buffer.count > 0){
	int b;
	for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {

		buffer->count--;
		total_size += buffer->entry[buffer->out_offs].size;
		buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		//printk(KERN_WARNING "total_size: %d\n", total_size);
	

	}

	buffer->count = old_count;
	buffer->out_offs = old_out_offs;

	printk(KERN_WARNING "total_size: %d\n", total_size);
	printk(KERN_WARNING "count: %d\n", count);

	char* temp_buffer = kmalloc(sizeof(char) * MAX(count, total_size), GFP_KERNEL);

	memset(temp_buffer, 0, ksize(temp_buffer));
	
	int b_offset = 0;
	//while (buffer.count > 0) {
	
	for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; b++) {
		buffer->count--;

		// write to temp_buffer
		if (buffer->entry[buffer->out_offs].buffptr != NULL) {
			memcpy(temp_buffer + b_offset, buffer->entry[buffer->out_offs].buffptr, buffer->entry[buffer->out_offs].size);
		}
		
		
		b_offset += buffer->entry[buffer->out_offs].size;
		
		buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		printk(KERN_WARNING "temp_buffer: %s\n", temp_buffer);

	}

	//buffer.count = old_count;
	buffer->out_offs = old_out_offs;

	//dev->quantum = -1;
	
	if ( copy_to_user(buf, temp_buffer, count) ) {
		retval = -EFAULT;
		kfree(temp_buffer);
		goto out;
	}



//	if (copy_to_user(buf, buffer.entry[buffer.out_offs].buffptr, buffer.entry[buffer.out_offs].size)) {
//		retval = -EFAULT;
//		goto out;
//	}

	kfree(temp_buffer);

	// memset null the read entries
	//memset(buffer.entry[buffer.out_offs].buffptr, 0, buffer.entry[buffer.out_offs].size);
		
	//buffer.out_offs = (buffer.out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
	*f_pos += count;
	retval = total_size;

  out:
	mutex_unlock(&dev->lock);
	return buffer->s_cb;
/*
	//mutex_unlock(&dev->lock);
	if (dev->quantum == -1){
		dev->quantum = 1;
		mutex_unlock(&dev->lock);
		return 0;
	}
	else{
		dev->quantum = -1;	
		mutex_unlock(&dev->lock);
		return retval;
	}
*/
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{

	printk(KERN_WARNING "111\n");

	struct aesd_dev *dev = filp->private_data;
	struct aesd_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */
	struct aesd_circular_buffer *buffer = &dev->buffer;

	printk(KERN_INFO "The calling process is \"%s\" (pid %i)\n", current->comm, current->pid);


	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	/* find listitem, qset index and offset in the quantum */
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	


	/* follow the list up to the right position */

	

	

	// replace with the write pointer of circ buffer
	dptr = aesd_follow(dev, item);


	


	if (dptr == NULL)
		goto out;



	// empty initialized first time case

	if (!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		if (!dptr->data)
			goto out;
		memset(dptr->data, 0, qset * sizeof(char *));
	}



	// not empty
	if (!dptr->data[s_pos]) {
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if (!dptr->data[s_pos])
			goto out;
	}

	// write only up to the end of this quantum 
	if (count > quantum - q_pos)
		count = quantum - q_pos;


	//count = 56535;

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
	if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count)) {
		retval = -EFAULT;
		goto out;
	}




	


	buffer_entry->buffptr = mychars;
	buffer_entry->size=count;

	// TODO: dont go until you see a newline, use the filp structure



	int foundNewline = 0; // Flag to indicate if newline is found
	int i;
        // Iterate through the string until the null terminator ('\0')
	for (i = 0; mychars[i] != '\0'; i++) {
	        if (mychars[i] == '\n') {
	            foundNewline = 1; // Set flag if newline is found
	            break; // Exit the loop as we've found it
        	}
	}


//	aesd_circular_buffer_add_entry(&buffer, &entry);


	newline_structure_add(
			dev,
			buffer_entry,
			buffer,
			mychars,
			count,
			foundNewline

			);


/*
	if (foundNewline) {
		if (dev->newlineb == NULL) {
			printk(KERN_WARNING "foundnewline=true dev->newlineb: %s entry: %s", dev->newlineb, entry.buffptr);
			aesd_circular_buffer_add_entry(&buffer, &entry);
		} else {
			printk(KERN_WARNING "foundnewline=true count + dev->newlineb: %s entry: %s\n", dev->newlineb, entry.buffptr);
			mychars = krealloc(dev->newlineb, count + dev->s_newlineb, GFP_KERNEL);
			
			
			
			printk(KERN_WARNING "entry.buffptr: %s\n", entry.buffptr);
			printk(KERN_WARNING "dev->newlineb: %s\n", dev->newlineb);

			memcpy(dev->newlineb + dev->s_newlineb, entry.buffptr, entry.size);
			dev->newlineb[dev->s_newlineb + entry.size] = '\0';

			printk(KERN_WARNING "entry.buffptr: %s\n", entry.buffptr);
			printk(KERN_WARNING "dev->newlineb: %s\n", dev->newlineb);

			entry.buffptr = dev->newlineb;
			entry.size = entry.size + dev->s_newlineb + 1; // newline and null
			aesd_circular_buffer_add_entry(&buffer, &entry);
			kfree(mychars);
			dev->newlineb = NULL;
		}
	}
	else {
		if(dev->newlineb == NULL) {
			printk(KERN_WARNING "foundnewline=false dev->newlineb: %s entry: %s\n", dev->newlineb, entry.buffptr);
			dev->newlineb = kmalloc(count * sizeof(char), GFP_KERNEL);
			dev->s_newlineb = count; //ksize(dev->newlineb);
			memcpy(dev->newlineb, mychars, count);
			dev->newlineb[count] = '\0';
			kfree(mychars);
			
		} else {
			printk(KERN_WARNING "foundlewline=false dev->newlineb: %s entry: %s\n", dev->newlineb);
			dev->newlineb = krealloc(dev->newlineb, count + dev->s_newlineb, GFP_KERNEL);
			memcpy(dev->newlineb + dev->s_newlineb, mychars, count);
			dev->newlineb[count] = '\0';
			dev->s_newlineb = count + dev->s_newlineb;
			kfree(mychars);
		}
	}
*/





	*f_pos += count;
	retval = count;

        // update the size
	if (dev->size < *f_pos)
		dev->size = *f_pos;


  out:
	mutex_unlock(&dev->lock);
	return retval;
}

/*
 * The ioctl() implementation
 */

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0, tmp;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok_wrapper(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok_wrapper(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;
	
	printk(KERN_WARNING "cmd ioctl: %d\n", cmd); 

	switch(cmd) {

		case SCULL_IOCRESET:
			aesd_quantum = SCULL_QUANTUM;
			aesd_qset = SCULL_QSET;
		break;
        
		case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
			if (! capable (CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			retval = __get_user(aesd_quantum, (int __user *)arg);
		break;

		case SCULL_IOCTQUANTUM: /* Tell: arg is the value */

			if (! capable (CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			aesd_quantum = arg;
		break;

		case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */

			retval = __put_user(aesd_quantum, (int __user *)arg);
		break;

		case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
	
			return aesd_quantum;

		case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */

			if (! capable (CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			tmp = aesd_quantum;
			retval = __get_user(aesd_quantum, (int __user *)arg);
			if (retval == 0) {
				retval = __put_user(tmp, (int __user *)arg);
			}
		break;

		case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */

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

        /*
         * The following two change the buffer size for aesdpipe.
         * The aesdpipe device uses this same ioctl method, just to
         * write less code. Actually, it's the same driver, isn't it?
         */

	  case SCULL_P_IOCTSIZE:

		aesd_p_buffer = arg;
		break;

	  case SCULL_P_IOCQSIZE:

		return aesd_p_buffer;


	  default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;

}



/*
 * The "extended" operations -- only seek
 */

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{

	struct aesd_dev *dev = filp->private_data;
	loff_t newpos;

	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off;
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	  case 2: /* SEEK_END */
		// count all non null in circular buffer and add size

		newpos = dev->buffer.s_cb;
		
		//newpos = dev->size + off;
		
		break;

	  default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0) return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}



struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.llseek =   aesd_llseek,
	.read =     aesd_read,
	.write =    aesd_write,
	.unlocked_ioctl = aesd_ioctl,
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
			aesd_trim(aesd_devices + i);
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

	printk(KERN_WARNING "~!@#\n");

	int result, i;
	dev_t dev = 0;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */

	if (aesd_major) {
		dev = MKDEV(aesd_major, aesd_minor);
		printk(KERN_WARNING "shouldnt be here 000\n");
		result = register_chrdev_region(dev, aesd_nr_devs, "aesd");
	} else {
		result = alloc_chrdev_region(&dev, aesd_minor, aesd_nr_devs,
				"aesd");
		aesd_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "aesd: can't get major %d\n", aesd_major);
		return result;
	}

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
		aesd_devices[i].quantum = aesd_quantum;
		aesd_devices[i].qset = aesd_qset;
		mutex_init(&aesd_devices[i].lock);
		aesd_setup_cdev(&aesd_devices[i], i);
		aesd_devices[i].buffer.in_offs = aesd_devices[i].buffer.out_offs = 0;
	}
printk(KERN_WARNING "000");
        /* At this point call the init function for any friend device */
	dev = MKDEV(aesd_major, aesd_minor + aesd_nr_devs);
	
	// removing the pipe devices, hoping they arent the /proc devices
	//dev += aesd_p_init(dev);
	dev += aesd_access_init(dev);

#ifdef SCULL_DEBUG /* only when debugging */
//	aesd_create_proc();
#endif

	//buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);	
	
//	buffer.in_offs = buffer.out_offs = 0;

	return 0; /* succeed */

  fail:
	printk(KERN_WARNING "fail goto reached\n");
	aesd_cleanup_module();
	return result;

}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

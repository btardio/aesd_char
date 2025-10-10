
#ifdef __KERNEL__

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

#endif

#ifdef __KERNEL__
#include "src/aesd.h"		/* local definitions */
#include "src/aesd-circular-buffer.h"
#include "src/aesd-driver.h"
#else
#include "aesd.h"
#include "aesd-circular-buffer.h"
#include "aesd-driver.h"
#include <stdio.h>
#endif

//#include "access_ok_version.h"
//#include "proc_ops_version.h"

#ifndef __KERNEL__
int copy_to_user(char* ubuf, char* fbuf, int size){
	return 0;
}		
#endif


#ifdef __KERNEL__
int create_pid_buffer(struct aesd_dev* dev, struct aesd_circular_buffer* buffer, char __user *buf, int pid_index) {
#else
int create_pid_buffer(struct aesd_dev* dev, struct aesd_circular_buffer* buffer, char *buf, int pid_index) {
#endif
	int total_size = 0;
	int b;

	int outoffset = buffer->out_offs;
	int b_offset;
	int buff_index;	
#ifdef __KERNEL__
	printk(KERN_WARNING "create_pid_buffer pid_index: %d\n", pid_index);
#else
	
	printf("create_pid_buffer pid_index: %d\n", pid_index);
	printf("create_pid_buffer complete: %d\n", dev->pids[pid_index].completed);
#endif



	if( dev->pids[pid_index].fpos_buffer == NULL || dev->pids[pid_index].completed == 1 ) {
		// b_offset = outoffset;
		b_offset = get_index(buffer, dev->pids[pid_index].index_offset);
		//printf("b_offset: %d\n", b_offset);
		// this loop can probably be improved placing it in the cirular buffer, keep track of total entry size
		for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - dev->pids[pid_index].index_offset; b++) {
			
			buff_index = (b + b_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;	

			
			//printf("b: %d\n", b);
			//printf("index: %d\n", buff_index);
			total_size += buffer->entry[buff_index].size;
			//outoffset = (outoffset + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

		}

#ifdef __KERNEL__
		printk(KERN_WARNING "total_size: %d\n", total_size);
		printk(KERN_WARNING "buffer->s_cb: %d\n", buffer->s_cb);
		printk(KERN_WARNING "!@#$ dev->pids[pid_index].index_offset: %d\n", dev->pids[pid_index].index_offset);	
#else
		test_variable_total_size = total_size;
		printf("total_size: %d\n", total_size);
		printf("buffer->s_cb: %d\n", buffer->s_cb);
		printf("dev->pids[pid_index].index_offset: %d\n", dev->pids[pid_index].index_offset);	
#endif


		// get the size of all entries in buffer
		dev->pids[pid_index].s_fpos_buffer = total_size;

		// handle 0 size
		if (dev->pids[pid_index].s_fpos_buffer == 0){
			return 0;
		}

#ifdef __KERNEL__
		dev->pids[pid_index].fpos_buffer = kmalloc(sizeof(char) * (total_size), GFP_KERNEL);
		memset(dev->pids[pid_index].fpos_buffer, 0, ksize(dev->pids[pid_index].fpos_buffer));
#else
		dev->pids[pid_index].fpos_buffer = malloc(sizeof(char) * (total_size));
		memset(dev->pids[pid_index].fpos_buffer, 0, total_size);
#endif


		int b_offset = 0;


		printf("dev->pids[pid_index].index_offset: %d\n", dev->pids[pid_index].index_offset);
		// iterate again copying the contents to temp_buffer
		for(b = 0; b < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - dev->pids[pid_index].index_offset; b++) {
			
			buff_index = (b + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;	

			// write to temp_buffer
			if (buffer->entry[ buff_index ].buffptr != NULL ) { //outoffset].buffptr != NULL) {
				memcpy(dev->pids[pid_index].fpos_buffer + b_offset, 
						buffer->entry[buff_index].buffptr,
						buffer->entry[buff_index].size);
						//buffer->entry[(outoffset + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].buffptr, 
						//buffer->entry[(outoffset + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size);
			}
#ifdef __KERNEL__
			printk(KERN_WARNING "completed write to temp_buffer\n");
#else
			printf("completed write to temp_buffer\n");
#endif

			//b_offset += buffer->entry[buffer->out_offs].size;
			b_offset += buffer->entry[ buff_index ].size;
				//(outoffset + dev->pids[pid_index].index_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
#ifdef __KERNEL__
			printk(KERN_WARNING "new b_offset: %d\n", b_offset);
#else
			printf("new b_offset: %d\n", b_offset);
#endif

#ifdef __KERNEL__
			printk(KERN_WARNING "temp_buffer at %d: %.*s\n", outoffset, b_offset, dev->pids[pid_index].fpos_buffer);
#else
			printf("temp_buffer at %d: %.*s\n", outoffset, b_offset, dev->pids[pid_index].fpos_buffer);
#endif

			//outoffset = (outoffset + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

		}
#ifdef __KERNEL__
		printk(KERN_WARNING "000 temp_buffer: %.*s\n", b_offset, dev->pids[pid_index].fpos_buffer);
		printk(KERN_WARNING "000 dev->pids[pid_index].fpos_buffer[0]: %c\n", dev->pids[pid_index].fpos_buffer[0]);
		printk(KERN_WARNING "000 fpos_: %d\n", dev->pids[pid_index].fpos);
#else
		test_variable_offset = b_offset;
//			dev->pids[pid_index].s_fpos_buffer == b_offset);
		printf("000 temp_buffer: %.*s\n", b_offset, dev->pids[pid_index].fpos_buffer);
		printf("000 dev->pids[pid_index].fpos_buffer[0]: %c\n", dev->pids[pid_index].fpos_buffer[0]);
		printf("000 fpos: %d\n", dev->pids[pid_index].fpos);
#endif

		if ( copy_to_user(buf, dev->pids[pid_index].fpos_buffer /* + dev->pids[pid_index].fpos */, dev->buffer.s_cb ) ) { // TODO min_int(count, s_read_buffer - dev->pids[pid_index].fpos));
#ifdef __KERNEL__
			kfree(dev->pids[pid_index].fpos_buffer);
#else
			free(dev->pids[pid_index].fpos_buffer);
#endif

			return -1;
		}

		// need to cpy pointer for kfree !!!

		//buffer->out_offs = old_out_offs;

	}

	return dev->pids[pid_index].s_fpos_buffer;
}


#ifndef AESD_DRIVER_H

#ifdef __KERNEL__
int create_pid_buffer(struct aesd_dev* dev, struct aesd_circular_buffer* buffer, char __user *userbuf, int pid_index);
#else
int create_pid_buffer(struct aesd_dev* dev, struct aesd_circular_buffer* buffer, char *userbuf, int pid_index);
extern int test_variable_offset;
extern int test_variable_total_size;
#endif

#endif

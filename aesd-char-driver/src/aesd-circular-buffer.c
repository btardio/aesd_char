/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Brandon Tardio
 * @date 2025-09-28
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "src/aesd.h" //-circular-buffer.h"


#else
#include <stdlib.h>
#include <string.h>

#include "aesd.h" //-circular-buffer.h"


#endif






void newline_structure_add(
		struct aesd_dev *dev,
		struct aesd_buffer_entry *entry, 
		struct aesd_circular_buffer* buffer,
		char* in_chars,
		int s_in_chars,
		int foundNewline) {




	if (foundNewline) {
		if (dev->newlineb == NULL) {
#ifdef __KERNEL__
			printk(KERN_WARNING "foundnewline=true dev->newlineb: %s entry: %s", dev->newlineb, entry->buffptr);
#endif
			aesd_circular_buffer_add_entry(buffer, entry);
			
			// buffer->s_cb += entry->size; moved to add funct

		} else {
#ifdef __KERNEL__
			printk(KERN_WARNING "foundnewline=true count + dev->newlineb: %s entry: %s\n", dev->newlineb, entry->buffptr);
#else
			printf("03 foundnewline=true count + dev->newlineb: %s entry: %s\n", dev->newlineb, entry->buffptr);
#endif

#ifdef __KERNEL__
			if ( dev->newlineb == NULL ) {
				dev->newlineb = kmalloc(sizeof(char) * (s_in_chars + dev->s_newlineb), GFP_KERNEL);
			} else {
				dev->newlineb = krealloc(dev->newlineb, sizeof(char) * (s_in_chars + dev->s_newlineb), GFP_KERNEL);
			}
#else
			if ( dev->newlineb == NULL ) {
				dev->newlineb = malloc(sizeof(char) * (s_in_chars + dev->s_newlineb));
			} else {
				dev->newlineb = realloc(dev->newlineb, sizeof(char) * (s_in_chars + dev->s_newlineb));
			}
#endif

#ifdef __KERNEL__			
			printk(KERN_WARNING "entry.buffptr: %.*s\n", entry->size, entry->buffptr);
			printk(KERN_WARNING "dev->newlineb: %s\n", dev->newlineb);
#else
			printf("entry.buffptr: %.*s\n", entry->size, entry->buffptr);
			printf("dev->newlineb: %.*s\n", dev->s_newlineb, dev->newlineb);
#endif
			memcpy(dev->newlineb + dev->s_newlineb, entry->buffptr, entry->size);
			dev->newlineb[dev->s_newlineb + entry->size] = '\0';
			dev->s_newlineb = entry->size = entry->size + dev->s_newlineb;
			entry->buffptr = dev->newlineb;
#ifdef __KERNEL__
			printk(KERN_WARNING "entry.buffptr: %s\n", entry->buffptr);
			printk(KERN_WARNING "dev->newlineb: %.*s\n", dev->s_newlineb, dev->newlineb);
#else
			printf("entry.buffptr: %s\n", entry->buffptr);
			printf("dev->newlineb: %.*s\n", dev->s_newlineb, dev->newlineb);

#endif
			//entry->size = entry->size + dev->s_newlineb + 1; // newline and null
			aesd_circular_buffer_add_entry(buffer, entry);

			// buffer->s_cb += entry->size; // moved to add method

#ifdef __KERNEL__
			kfree(in_chars);
#else
			free(in_chars);
#endif
			dev->newlineb = NULL;
		}
	}
	else {
		if(dev->newlineb == NULL) {
#ifdef __KERNEL__
			printk(KERN_WARNING "00 foundnewline=false dev->newlineb: %s entry: %s\n", dev->newlineb, entry->buffptr);
#else
			printf("00 foundnewline=false dev->newlineb: %s entry: %s\n", dev->newlineb, entry->buffptr);
#endif

#ifdef __KERNEL__
			dev->newlineb = kmalloc((s_in_chars + 1) * sizeof(char), GFP_KERNEL);
#else
			dev->newlineb = malloc((s_in_chars + 1) * sizeof(char));
#endif
			dev->s_newlineb = s_in_chars; //ksize(dev->newlineb);
			memcpy(dev->newlineb, in_chars, s_in_chars);
			dev->newlineb[s_in_chars] = '\0';
#ifdef __KERNEL__
			kfree(in_chars);
#else
			free(in_chars);
#endif
		} else {
#ifdef __KERNEL__
			printk(KERN_WARNING "01 foundlewline=false dev->newlineb: %s entry: %s\n", dev->newlineb);
#else
			printf("01 foundlewline=false dev->newlineb: %s entry: %s\n", dev->newlineb);
#endif


#ifdef __KERNEL__
			dev->newlineb = krealloc(dev->newlineb, s_in_chars + dev->s_newlineb, GFP_KERNEL);
#else
			dev->newlineb = realloc(dev->newlineb, s_in_chars + dev->s_newlineb);
#endif

			memcpy(dev->newlineb + dev->s_newlineb, in_chars, s_in_chars);
			dev->newlineb[s_in_chars + dev->s_newlineb] = '\0';
			
			dev->s_newlineb = s_in_chars + dev->s_newlineb;
#ifdef __KERNEL__
			kfree(in_chars);
#else
			free(in_chars);
#endif
		}

	}




}




/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.

 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end

 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.

 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
		struct aesd_circular_buffer *buffer,
    		size_t char_offset, 
		size_t *entry_offset_byte_rtn 
		)
{

    struct aesd_buffer_entry *rtn_value = &buffer->entry[buffer->out_offs + 1 % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
    int count = char_offset;
    int out_offs_count = buffer->out_offs;
    struct aesd_buffer_entry *counting_entry = rtn_value;
    int loops_completed = 0;
    

    if( char_offset < 0 ) {
	*entry_offset_byte_rtn = -1;
	return NULL;
    }

    while (count > -1) {
	
	count -= counting_entry->size;
	counting_entry = &buffer->entry[(out_offs_count + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
	out_offs_count++;
	loops_completed++;

    }
    
    counting_entry = &buffer->entry[(out_offs_count - 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];

    if(loops_completed > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){

	*entry_offset_byte_rtn = -1;
    
        return NULL;

    } else {
    
	*entry_offset_byte_rtn = count + counting_entry->size;
    
    	return &buffer->entry[(out_offs_count - 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
    }
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{

#ifdef __KERNEL__
    printk(KERN_WARNING "adding entry: %s\n", add_entry->buffptr);
    printk(KERN_WARNING "adding entry size: %d\n", add_entry->size);
#endif

    if (add_entry == NULL){
        return;
    }

    if( buffer->full && (buffer->in_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED == buffer->out_offs)
    {
	buffer->out_offs = (buffer->in_offs + 1 ) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
 
#ifdef __KERNEL__
    buffer->s_cb -= buffer->entry[buffer->in_offs].size;
    buffer->s_cb += add_entry->size; 
    printk(KERN_WARNING "buffer size: %d\n", buffer->s_cb);
    kfree(buffer->entry[buffer->in_offs].buffptr);
#else
    // test is using stack, but keeping above b/c user can't manage slab
    if (buffer->entry[buffer->in_offs].buffptr != NULL) { 
    	//free(buffer->entry[buffer->in_offs].buffptr);
    }

#endif
    
    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
#ifdef __KERNEL__
    printk(KERN_WARNING "buffer->entry[buffer->in_offs].buffptr: %s\n", buffer->entry[buffer->in_offs].buffptr);
#endif

    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    if((buffer->in_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED == buffer->out_offs) {
        buffer->full = 1;
    } else {
        buffer->full = 0;
    }

    buffer->count++;

    return;
}



/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    
    memset(buffer,0,sizeof(struct aesd_circular_buffer));

    buffer->s_cb = 0;


    int i;

    for ( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
	    buffer->entry[i].buffptr = NULL;
	    buffer->entry[i].size = 0;
    }

}

#include <stdlib.h>

#include "unity.h"
#include "aesd-circular-buffer.h"
#include "aesd.h"
#include "string.h"
#include "aesd-driver.h"

int test_variable_offset;
int test_variable_total_size;
 


void write_circular_buffer_packet(struct aesd_circular_buffer *buffer,
                                         const char *writestr)
{
    _aesd_buffer_entry entry;
    entry.buffptr = writestr;
    entry.size=strlen(writestr);
    aesd_circular_buffer_add_entry(buffer,&entry);
}

void setUp(void) {

}

void tearDown(void) {

}

void test_sane(void) {
    TEST_ASSERT_EQUAL(5, 5);
}

void test_create_pid_buffer() {
    
}

void test_init(void) {
	

	

}

void test_read_create_buffer() {



		_aesd_dev *ddv = malloc(sizeof(_aesd_dev));

	_aesd_buffer_entry *entry = malloc(sizeof(_aesd_buffer_entry));

	_aesd_circular_buffer *buffer = malloc(sizeof(_aesd_circular_buffer));

	memset(ddv, 0, sizeof(_aesd_dev));

	ddv->newlineb = NULL;

	aesd_circular_buffer_init(buffer);

	char* chars = malloc(sizeof(char) * 4);
	memcpy(chars, "abcd", 4);


	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 0);


	TEST_ASSERT_EQUAL_STRING("abcd\0", ddv->newlineb);

	int i;

	chars = malloc(sizeof(char) * 4);
	memcpy(chars, "efgh", 4);
	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 0);

	TEST_ASSERT_EQUAL_STRING("abcdefgh\0", ddv->newlineb);

	chars = malloc(sizeof(char) * 4);
	memcpy(chars, "ijk\n", 4);
	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 1);

	//TEST_ASSERT_EQUAL_STRING("abcdefghijk\n", ddv->);


	for ( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
		printf("buffer[%d]: %s\n",i, buffer->entry[i].buffptr);
	}


	ddv->pids[0].fpos_buffer = NULL;
	ddv->pids[0].completed = 1;

	char outbuff[4096];
	
	int out_s_fpos_buffer;

	out_s_fpos_buffer = create_pid_buffer(ddv, buffer, &outbuff, 0);

	TEST_ASSERT_EQUAL(12, test_variable_offset);
	TEST_ASSERT_EQUAL(12, out_s_fpos_buffer);

}







void test_read_create_buffer_000() {



		_aesd_dev *ddv = malloc(sizeof(_aesd_dev));

	_aesd_buffer_entry *entry = malloc(sizeof(_aesd_buffer_entry));

	_aesd_circular_buffer *buffer = malloc(sizeof(_aesd_circular_buffer));

	memset(ddv, 0, sizeof(_aesd_dev));

	ddv->newlineb = NULL;

	aesd_circular_buffer_init(buffer);

	char* chars = malloc(sizeof(char) * 4);
	memcpy(chars, "abcd", 4);


	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 0);


	TEST_ASSERT_EQUAL_STRING("abcd\0", ddv->newlineb);

	int i;

	chars = malloc(sizeof(char) * 4);
	memcpy(chars, "efgh", 4);
	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 0);

	TEST_ASSERT_EQUAL_STRING("abcdefgh\0", ddv->newlineb);

	chars = malloc(sizeof(char) * 4);
	memcpy(chars, "ijk\n", 4);
	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);

	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);

	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);


	for ( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
		printf("buffer[%d]: %s\n",i, buffer->entry[i].buffptr);
	}


	ddv->pids[0].fpos_buffer = NULL;
	ddv->pids[0].completed = 1;

	char outbuff[4096];
	
	int out_s_fpos_buffer;

	out_s_fpos_buffer = create_pid_buffer(ddv, buffer, &outbuff, 0);

	TEST_ASSERT_EQUAL(120, test_variable_offset);
	TEST_ASSERT_EQUAL(120, out_s_fpos_buffer);

}







void test_read_create_buffer_get_index_000() {



		_aesd_dev *ddv = malloc(sizeof(_aesd_dev));

	_aesd_buffer_entry *entry = malloc(sizeof(_aesd_buffer_entry));

	_aesd_circular_buffer *buffer = malloc(sizeof(_aesd_circular_buffer));

	memset(ddv, 0, sizeof(_aesd_dev));

	ddv->newlineb = NULL;

	aesd_circular_buffer_init(buffer);

	char* chars = malloc(sizeof(char) * 4);
	memcpy(chars, "abcd", 4);


	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 0);


	TEST_ASSERT_EQUAL_STRING("abcd\0", ddv->newlineb);

	int i;

	chars = malloc(sizeof(char) * 4);
	memcpy(chars, "efgh", 4);
	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 0);

	TEST_ASSERT_EQUAL_STRING("abcdefgh\0", ddv->newlineb);

	chars = malloc(sizeof(char) * 4);
	memcpy(chars, "ijk\n", 4);
	entry->buffptr = chars;
	entry->size = 4;

	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);

	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);

	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);
	newline_structure_add(ddv, entry, buffer, chars, 4, 1);


	for ( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
		printf("buffer[%d]: %s\n",i, buffer->entry[i].buffptr);
	}


	ddv->pids[0].fpos_buffer = NULL;
	ddv->pids[0].completed = 1;
	ddv->pids[0].index_offset = 1;
	char outbuff[4096];
	
	int out_s_fpos_buffer;

	out_s_fpos_buffer = create_pid_buffer(ddv, buffer, &outbuff, 0);

	TEST_ASSERT_EQUAL(108, test_variable_total_size);
	TEST_ASSERT_EQUAL(108, test_variable_offset);
	TEST_ASSERT_EQUAL(108, out_s_fpos_buffer);

}

void test_read_create_buffer_000_sequential() {
	int i;

	_aesd_dev *ddv = malloc(sizeof(_aesd_dev));

	_aesd_buffer_entry *entry = malloc(sizeof(_aesd_buffer_entry));

	_aesd_circular_buffer *buffer = malloc(sizeof(_aesd_circular_buffer));

	    TEST_MESSAGE("Write strings 1 to 10 to the circular buffer");
    write_circular_buffer_packet(buffer,"write1\n");
    write_circular_buffer_packet(buffer,"write2\n");
    write_circular_buffer_packet(buffer,"write3\n");
    write_circular_buffer_packet(buffer,"write4\n");
    write_circular_buffer_packet(buffer,"write5\n");
    write_circular_buffer_packet(buffer,"write6\n");
    write_circular_buffer_packet(buffer,"write7\n");
    write_circular_buffer_packet(buffer,"write8\n");
    write_circular_buffer_packet(buffer,"write9\n");
    write_circular_buffer_packet(buffer,"writeA\n");



	for ( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
		printf("buffer[%d]: %s\n",i, buffer->entry[i].buffptr);
	}


	ddv->pids[0].fpos_buffer = NULL;
	ddv->pids[0].completed = 1;
	ddv->pids[0].index_offset = 1;
	char outbuff[4096];

	memset(outbuff, 0, 4096);

	int out_s_fpos_buffer;

	out_s_fpos_buffer = create_pid_buffer(ddv, buffer, &outbuff, 0);

	printf("outbuff: %.*s\n\n\n===\n", 1024, outbuff);

	TEST_ASSERT_EQUAL(63, test_variable_total_size);
	TEST_ASSERT_EQUAL(63, test_variable_offset);
	TEST_ASSERT_EQUAL(63, out_s_fpos_buffer);

}



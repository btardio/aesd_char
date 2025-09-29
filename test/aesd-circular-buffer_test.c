#include <stdlib.h>

#include "unity.h"
#include "aesd-circular-buffer.h"
#include "aesd.h"
#include "string.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_sane(void) {
    TEST_ASSERT_EQUAL(5, 5);
}

static void write_circular_buffer_packet(struct aesd_circular_buffer *buffer,
                                         const char *writestr)
{
    _aesd_buffer_entry entry;
    entry.buffptr = writestr;
    entry.size=strlen(writestr);
    aesd_circular_buffer_add_entry(buffer,&entry);
}

/**
* Verify we can find an entry in @param buffer corresponding to a zero referenced byte offset @param entry_offest_byte
* and verify the resulting string at the corresponding offset matches @param expectstring
*/
static void verify_find_entry(struct aesd_circular_buffer *buffer, size_t entry_offset_byte, const char *expectstring)
{
    size_t offset_rtn=0;
    char message[250];
    _aesd_buffer_entry *rtnentry = aesd_circular_buffer_find_entry_offset_for_fpos(buffer,
                                                entry_offset_byte,
                                                &offset_rtn);
    snprintf(message,sizeof(message),"null pointer unexpected when verifying offset %zu with expect string %s",
                                        entry_offset_byte, expectstring);
    TEST_ASSERT_NOT_NULL_MESSAGE(rtnentry,message);
    snprintf(message,sizeof(message),"entry string does not match expected value at offset %zu",
                                        entry_offset_byte);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expectstring,&rtnentry->buffptr[offset_rtn],message);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)rtnentry->size,(uint32_t)strlen(rtnentry->buffptr),
            "size parameter in buffer entry should match total entry length");
}

/**
* Verify we cannot find an entry in @param buffer at offset @param entry_offset_byte (this represents an
* offset past the end of the buffer
*/
static void verify_find_entry_not_found(struct aesd_circular_buffer *buffer, size_t entry_offset_byte)
{
    size_t offset_rtn;
    char message[150];
    _aesd_buffer_entry *rtnentry = aesd_circular_buffer_find_entry_offset_for_fpos(buffer,
                                                entry_offset_byte,
                                                &offset_rtn);
    snprintf(message,sizeof(message),"Expected null pointer when trying to validate entry offset %zu",entry_offset_byte);
    TEST_ASSERT_NULL_MESSAGE(rtnentry,message);
}


void test_init(void) {


    struct aesd_circular_buffer buffer;

    buffer.in_offs = buffer.out_offs = 0;

    aesd_circular_buffer_init(&buffer);

    for ( int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++ ) {
	TEST_ASSERT_EQUAL('\0', buffer.entry[i].buffptr);
    }

    TEST_ASSERT_EQUAL(0, buffer.in_offs);
    TEST_ASSERT_EQUAL(0, buffer.out_offs);
    TEST_ASSERT_EQUAL(0, buffer.full);

}


void test_add_entry(void) {
    _aesd_buffer_entry A;
    _aesd_buffer_entry B;
    _aesd_buffer_entry C;

    const char c_A = 'A';
    const char c_B = 'B';
    const char c_C = 'C';

    A.buffptr = &c_A;
    B.buffptr = &c_B;
    C.buffptr = &c_C;

    struct aesd_circular_buffer buffer;

    buffer.in_offs = buffer.out_offs = 0;

    aesd_circular_buffer_init(&buffer);

    aesd_circular_buffer_add_entry(&buffer, &A);
    aesd_circular_buffer_add_entry(&buffer, &B);
    aesd_circular_buffer_add_entry(&buffer, &C);

    TEST_ASSERT_EQUAL('A', *buffer.entry[0].buffptr);
    TEST_ASSERT_EQUAL('B', *buffer.entry[1].buffptr);
    TEST_ASSERT_EQUAL('C', *buffer.entry[2].buffptr);

    for ( int i = 3; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++ ) {
        TEST_ASSERT_EQUAL('\0', buffer.entry[i].buffptr);
    }

}


void test_aesd_circular_buffer_find_entry_offset_for_fpos(void) {

    _aesd_buffer_entry A;
    _aesd_buffer_entry B;
    _aesd_buffer_entry C;

    const char c_A = 'A';
    const char c_B = 'B';
    const char c_C = 'C';

    A.buffptr = &c_A;
    B.buffptr = &c_B;
    C.buffptr = &c_C;

    A.size = 1;
    B.size = 1;
    C.size = 1;

    struct aesd_circular_buffer buffer;

    buffer.in_offs = buffer.out_offs = 0;

    aesd_circular_buffer_init(&buffer);

    aesd_circular_buffer_add_entry(&buffer, &A);
/*
    aesd_circular_buffer_add_entry(&buffer, &B);
    aesd_circular_buffer_add_entry(&buffer, &C);

*/
    _aesd_buffer_entry *rval;

    rval = malloc(sizeof(_aesd_buffer_entry));

    aesd_circular_buffer_find_entry_offset_for_fpos(&buffer, 0, &rval);
/*
    TEST_ASSERT_EQUAL('A', rval->buffptr);
*/
}

/*
void test_aesd_circular_buffer_find_entry_offset_string(void) {

    _aesd_buffer_entry A;
    _aesd_buffer_entry B;
    _aesd_buffer_entry C;

    const char *c_A = "abcdefghijklm";

    A.buffptr = c_A;

    A.size = 13;

    struct aesd_circular_buffer buffer;

    buffer.in_offs = buffer.out_offs = 0;

    aesd_circular_buffer_init(&buffer);

    aesd_circular_buffer_add_entry(&buffer, &A);
    
    _aesd_buffer_entry *rval;

    size_t char_offset;

    for ( int i = 0; i < 13; i++ ) {
        rval = aesd_circular_buffer_find_entry_offset_for_fpos(&buffer, i, &char_offset);

	printf("\n");
        fwrite(char_offset, sizeof(char), 1, stdout);
	printf("\n");	
	TEST_ASSERT_EQUAL(*(char*)char_offset, c_A[i]);
    }

    //rval = aesd_circular_buffer_find_entry_offset_for_fpos(&buffer, AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED, &char_offset);
    //TEST_ASSERT_EQUAL(*(char*)char_offset, c_A[0]);

}

*/



void test_circular_buffer(void)
{
    struct aesd_circular_buffer buffer;
    aesd_circular_buffer_init(&buffer);
    
    
    
    TEST_MESSAGE("Write strings 1 to 10 to the circular buffer");
    write_circular_buffer_packet(&buffer,"write1\n"); 
    write_circular_buffer_packet(&buffer,"write2\n"); 
    write_circular_buffer_packet(&buffer,"write3\n"); 
    write_circular_buffer_packet(&buffer,"write4\n"); 
    write_circular_buffer_packet(&buffer,"write5\n"); 
    write_circular_buffer_packet(&buffer,"write6\n"); 
    write_circular_buffer_packet(&buffer,"write7\n"); 
    write_circular_buffer_packet(&buffer,"write8\n"); 
    write_circular_buffer_packet(&buffer,"write9\n"); 
    write_circular_buffer_packet(&buffer,"write10\n"); 
    
    
    
    
    TEST_MESSAGE("Verify strings 1 through 10 exist in the circular buffer");
    verify_find_entry(&buffer,0,"write1\n");
/*
    verify_find_entry(&buffer,7,"write2\n");
    verify_find_entry(&buffer,14,"write3\n");
    verify_find_entry(&buffer,21,"write4\n");
    verify_find_entry(&buffer,28,"write5\n");
    verify_find_entry(&buffer,35,"write6\n");
    verify_find_entry(&buffer,42,"write7\n");
    verify_find_entry(&buffer,49,"write8\n");
    verify_find_entry(&buffer,56,"write9\n");
    verify_find_entry(&buffer,63,"write10\n");
    */
}






/**
* Tests the circular buffer by writing a set of 10 strings, verifying each request for
* associated offset returns the correct location in the buffer
*/
void test_circular_buffer_assignment7(void)
{
    struct aesd_circular_buffer buffer;
    aesd_circular_buffer_init(&buffer);
    TEST_MESSAGE("Write strings 1 to 10 to the circular buffer");
    write_circular_buffer_packet(&buffer,"write1\n"); 
    write_circular_buffer_packet(&buffer,"write2\n"); 
    write_circular_buffer_packet(&buffer,"write3\n"); 
    write_circular_buffer_packet(&buffer,"write4\n"); 
    write_circular_buffer_packet(&buffer,"write5\n"); 
    write_circular_buffer_packet(&buffer,"write6\n"); 
    write_circular_buffer_packet(&buffer,"write7\n"); 
    write_circular_buffer_packet(&buffer,"write8\n"); 
    write_circular_buffer_packet(&buffer,"write9\n"); 
    write_circular_buffer_packet(&buffer,"write10\n"); 
    TEST_MESSAGE("Verify strings 1 through 10 exist in the circular buffer");
    verify_find_entry(&buffer,0,"write1\n");
    verify_find_entry(&buffer,7,"write2\n");
    verify_find_entry(&buffer,14,"write3\n");
    verify_find_entry(&buffer,21,"write4\n");
    verify_find_entry(&buffer,28,"write5\n");
    verify_find_entry(&buffer,35,"write6\n");
    verify_find_entry(&buffer,42,"write7\n");
    verify_find_entry(&buffer,49,"write8\n");
    verify_find_entry(&buffer,56,"write9\n");
    verify_find_entry(&buffer,63,"write10\n");
    TEST_MESSAGE("Verify a request for the last byte in the circular buffer succeeds");
    verify_find_entry(&buffer,70,"\n");
    TEST_MESSAGE("Verify a request for one offset past the last byte in the circular buffer returns not found");
    verify_find_entry_not_found(&buffer,71);
    TEST_MESSAGE("Write one more packet to the circular buffer, causing the first entry to be removed");
    write_circular_buffer_packet(&buffer,"write11\n"); 
    TEST_MESSAGE("Verify the first entry is removed, and all other remaining entries are found at the correct offsets");
    verify_find_entry(&buffer,0,"write2\n");
    verify_find_entry(&buffer,7,"write3\n");
    verify_find_entry(&buffer,14,"write4\n");
    verify_find_entry(&buffer,21,"write5\n");
    verify_find_entry(&buffer,28,"write6\n");
    verify_find_entry(&buffer,35,"write7\n");
    verify_find_entry(&buffer,42,"write8\n");
    verify_find_entry(&buffer,49,"write9\n");
    verify_find_entry(&buffer,56,"write10\n");
    verify_find_entry(&buffer,64,"write11\n");
    verify_find_entry(&buffer,71,"\n");
    verify_find_entry_not_found(&buffer,72);
}



void test_find_bug(void) {

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

	// probably kprint? test \0 is on everything
	// or
	// test everything is allocating enough space

/*
	write_circular_buffer_packet(buffer,"write1\n"); 
	write_circular_buffer_packet(buffer,"write2\n"); 
	write_circular_buffer_packet(buffer,"write3\n"); 
	write_circular_buffer_packet(buffer,"write4\n"); 
	write_circular_buffer_packet(buffer,"write5\n"); 
	write_circular_buffer_packet(buffer,"write6\n"); 
	write_circular_buffer_packet(buffer,"write7\n"); 
	write_circular_buffer_packet(buffer,"write8\n"); 
	write_circular_buffer_packet(buffer,"write9\n"); 
	write_circular_buffer_packet(buffer,"write10\n"); 
*/


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


}	

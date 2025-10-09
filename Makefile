# To build modules outside of the kernel tree, we run "make"
# in the kernel source tree; the Makefile these then includes this
# Makefile once again.
# This conditional selects whether we are being included from the
# kernel Makefile or not.




ifeq ($(KERNELRELEASE),)
    $(info !!!)
    # Assume the source tree is where the running kernel was built
    # You should set KERNELDIR in the environment if it's elsewhere
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    # The current directory is passed to sub-makes as argument
    PWD := $(shell pwd)


UNITY_ROOT=test/Unity/

TARGET_TEST_DRIVER = aesd-driver_test

TARGET_TEST_BUFFER = aesd-circular-buffer_test


TEST_SRC_FILES_DRIVER=$(UNITY_ROOT)/src/unity.c test/aesd-driver_test.c test/test_runners/aesd-driver_test_runner.c

TEST_SRC_FILES_BUFFER=$(UNITY_ROOT)/src/unity.c src/aesd-circular-buffer.c test/aesd-circular-buffer_test.c test/test_runners/aesd-circular-buffer_test_runner.c 


INC_DIRS=-Isrc -I$(UNITY_ROOT)/src




modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

test: test_driver test_buffer

test_driver: test_runner_driver
	$(CC) $(CFLAGS) $(INC_DIRS) $(SYMBOLS) -g $(TEST_SRC_FILES_DRIVER) -o $(TARGET_TEST_DRIVER)


test_buffer: test_runner_buffer
	$(CC) $(CFLAGS) $(INC_DIRS) $(SYMBOLS) -g $(TEST_SRC_FILES_BUFFER) -o $(TARGET_TEST_BUFFER)


test_runner_driver: test/aesd-circular-buffer_test.c test/aesd-driver_test.c
	ruby $(UNITY_ROOT)/auto/generate_test_runner.rb test/aesd-driver_test.c test/test_runners/aesd-driver_test_runner.c


test_runner_buffer: test/aesd-circular-buffer_test.c
	ruby $(UNITY_ROOT)/auto/generate_test_runner.rb test/aesd-circular-buffer_test.c test/test_runners/aesd-circular-buffer_test_runner.c



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions aesd-driver_test aesd-circular-buffer_test

.PHONY: modules modules_install clean

else
    # called from kernel build system: just declare what our modules are

	aesdchar-objs := main.o access.o aesd-circular-buffer.o

	obj-m := aesdchar.o
endif






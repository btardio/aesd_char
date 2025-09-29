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

TARGET_TEST = aesd-cirular-buffer_test

TEST_SRC_FILES=$(UNITY_ROOT)/src/unity.c src/aesd-circular-buffer.c test/aesd-circular-buffer_test.c test/test_runners/aesd-circular-buffer_test_runner.c
INC_DIRS=-Isrc -I$(UNITY_ROOT)/src




modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install


test: test_runner
	$(CC) $(CFLAGS) $(INC_DIRS) $(SYMBOLS) -g $(TEST_SRC_FILES) -o $(TARGET_TEST)

test_runner: test/aesd-circular-buffer_test.c
	ruby $(UNITY_ROOT)/auto/generate_test_runner.rb test/aesd-circular-buffer_test.c test/test_runners/aesd-circular-buffer_test_runner.c


clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

.PHONY: modules modules_install clean

else
    # called from kernel build system: just declare what our modules are
#    obj-m := hello.o hellop.o seq.o jit.o jiq.o sleepy.o complete.o \
#             silly.o faulty.o kdatasize.o kdataalign.o

	aesdcircular-objs := main.o access.o aesd-circular-buffer.o

	obj-m := aesdcircular.o
endif






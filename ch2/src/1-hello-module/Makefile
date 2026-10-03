## Kbuild part of Makefile
obj-m := 1-hello_module.o

#following is just an exmpple
#ldflags-y := -T foo_sections.lds

# normal makefile
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

all default: modules
install: modules_install

modules modules_install help clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) $@
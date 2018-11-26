CFILES = my_module.c

obj-m := MyModule.o
MyModule-objs := $(CFILES:.c=.o)

ccflags-y += -std=gnu99 -Wall -Wno-declaration-after-statement -I/opt/vc/include -L/opt/vc/lib -lbcm_host -Werror=implicit-function-declaration

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

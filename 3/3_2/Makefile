TARGET_MODULE := My_Kernel
obj-m := $(TARGET_MODULE).o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC := gcc
# Add -pthread and -D_GNU_SOURCE for thread support and sigaction
CFLAGS := -Wall -pthread -D_GNU_SOURCE 

# --- Kernel Space ---
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# --- User Space Tasks ---
# We compile scheduler.c and task.c together directly to avoid rule conflicts
Prog_1thread:
	@rm -f 3_2.txt
	@echo "#define THREAD_NUMBER 1" > 3_2_Config.h
	@$(CC) $(CFLAGS) -o 3_2.out scheduler.c task.c
	@sudo ./3_2.out RR
	@rm -f 3_2.out 3_2_Config.h
Prog_2thread:
	@rm -f 3_2.txt
	@echo "#define THREAD_NUMBER 2" > 3_2_Config.h
	@$(CC) $(CFLAGS) -o 3_2.out scheduler.c task.c
	@sudo ./3_2.out RR
	@rm -f 3_2.out 3_2_Config.h
Prog_5thread:
	@rm -f 3_2.txt
	@echo "#define THREAD_NUMBER 5" > 3_2_Config.h
	@$(CC) $(CFLAGS) -o 3_2.out scheduler.c task.c
	@sudo ./3_2.out RR
	@rm -f 3_2.out 3_2_Config.h

Prog_10thread:
	@rm -f 3_2.txt
	@echo "#define THREAD_NUMBER 10" > 3_2_Config.h
	@$(CC) $(CFLAGS) -o 3_2.out scheduler.c task.c
	@sudo ./3_2.out RR
	@rm -f 3_2.out 3_2_Config.h

Prog_20thread:
	@rm -f 3_2.txt
	@echo "#define THREAD_NUMBER 20" > 3_2_Config.h
	@$(CC) $(CFLAGS) -o 3_2.out scheduler.c task.c
	@sudo ./3_2.out RR
	@rm -f 3_2.out 3_2_Config.h

# --- Utilities ---
load:
	@sudo insmod $(TARGET_MODULE).ko

unload:
	@sudo rmmod $(TARGET_MODULE).ko

check:
	@echo "Comparing 3_2.txt with correct_answer.txt..."
	@diff -w 3_2.txt correct_answer.txt && echo "Result: MATCH!" || echo "Result: FAILED!"

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -f *.o *.ko *.mod.* *.symvers *.order *.mod.cmd *.mod .*.mod.* .*.*.cmd 3_2.txt 3_2.out 3_2_Config.h
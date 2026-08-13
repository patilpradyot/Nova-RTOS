TOOLCHAIN = arm-none-eabi-

CC = $(TOOLCHAIN)gcc
AS = $(TOOLCHAIN)as
LD = $(TOOLCHAIN)ld

CFLAGS = -mcpu=arm926ej-s -g -ffreestanding -I. -Ikernel -Idrivers
ASFLAGS = -mcpu=arm926ej-s -g

BUILD_DIR = build

OBJS = $(BUILD_DIR)/boot.o \
       $(BUILD_DIR)/irq_handler.o \
       $(BUILD_DIR)/kernel.o \
       $(BUILD_DIR)/task.o \
       $(BUILD_DIR)/scheduler.o \
       $(BUILD_DIR)/mutex.o \
       $(BUILD_DIR)/kalman.o \
       $(BUILD_DIR)/timer.o \
       $(BUILD_DIR)/uart.o

all: $(BUILD_DIR)/kernel.elf

$(BUILD_DIR)/boot.o: boot/boot.s
	$(AS) $(ASFLAGS) boot/boot.s -o $(BUILD_DIR)/boot.o

$(BUILD_DIR)/irq_handler.o: boot/irq_handler.s
	$(AS) $(ASFLAGS) boot/irq_handler.s -o $(BUILD_DIR)/irq_handler.o

$(BUILD_DIR)/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c kernel/kernel.c -o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/task.o: kernel/task.c
	$(CC) $(CFLAGS) -c kernel/task.c -o $(BUILD_DIR)/task.o

$(BUILD_DIR)/scheduler.o: kernel/scheduler.c
	$(CC) $(CFLAGS) -c kernel/scheduler.c -o $(BUILD_DIR)/scheduler.o

$(BUILD_DIR)/mutex.o: kernel/mutex.c
	$(CC) $(CFLAGS) -c kernel/mutex.c -o $(BUILD_DIR)/mutex.o

$(BUILD_DIR)/kalman.o: kernel/kalman.c
	$(CC) $(CFLAGS) -c kernel/kalman.c -o $(BUILD_DIR)/kalman.o

$(BUILD_DIR)/timer.o: drivers/timer.c
	$(CC) $(CFLAGS) -c drivers/timer.c -o $(BUILD_DIR)/timer.o

$(BUILD_DIR)/uart.o: drivers/uart.c
	$(CC) $(CFLAGS) -c drivers/uart.c -o $(BUILD_DIR)/uart.o

# IMPORTANT: linking ab $(CC) [gcc] se ho rahi hai, $(LD) [raw ld] se nahi.
# Wajah: Kalman filter float math + integer division use karta hai,
# jinke liye ARM926 (bina hardware FPU/divider ke) ko libgcc ke
# software routines (__aeabi_fadd, __aeabi_idivmod, etc) chahiye.
# gcc khud sahi libgcc path dhoondh ke link kar deta hai -lgcc se.
$(BUILD_DIR)/kernel.elf: $(OBJS)
	$(CC) $(CFLAGS) -nostartfiles -T boot/linker.ld $(OBJS) -lgcc -o $(BUILD_DIR)/kernel.elf

run: all
	qemu-system-arm -M versatilepb -m 128M -nographic -kernel $(BUILD_DIR)/kernel.elf

clean:
	del /Q $(BUILD_DIR)\*.o $(BUILD_DIR)\*.elf 2>nul || true

.PHONY: all run clean